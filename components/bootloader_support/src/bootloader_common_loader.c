/*
 * SPDX-FileCopyrightText: 2020-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "string.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#if CONFIG_IDF_TARGET_ESP32
#include "esp32/rom/spi_flash.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/spi_flash.h"
#include "esp32s2/rom/ets_sys.h"
#endif
#include "esp_rom_crc.h"
#include "esp_rom_gpio.h"
#include "esp_flash_partitions.h"
#include "bootloader_flash.h"
#include "bootloader_common.h"
#include "soc/gpio_periph.h"
#include "soc/rtc.h"
#include "soc/efuse_reg.h"
#include "soc/soc_memory_types.h"
#include "hal/gpio_ll.h"
#include "esp_image_format.h"
#include "bootloader_sha.h"
#include "sys/param.h"
#include "bootloader_flash_priv.h"

#define ESP_PARTITION_HASH_LEN 32 /* SHA-256 digest length */

static const char* TAG = "boot_comm";

uint32_t bootloader_common_ota_select_crc(const esp_ota_select_entry_t *s)
{
    esp_ota_select_entry_t tmp = *s;
    tmp.crc = 0;
    return esp_rom_crc32_le(UINT32_MAX, (uint8_t *) &tmp, sizeof(tmp));
}

bool bootloader_common_ota_select_invalid(const esp_ota_select_entry_t *s)
{
    return !bootloader_common_ota_select_valid(s);
}

bool bootloader_common_ota_select_valid(const esp_ota_select_entry_t *s)
{
    return s->seq != UINT32_MAX && s->crc == bootloader_common_ota_select_crc(s);
}

int bootloader_common_get_active_otadata(esp_ota_select_entry_t *two_otadata)
{
    if (two_otadata == NULL) {
        return -1;
    }
    bool valid_two_otadata[2];
    valid_two_otadata[0] = bootloader_common_ota_select_valid(&two_otadata[0]);
    valid_two_otadata[1] = bootloader_common_ota_select_valid(&two_otadata[1]);
    return bootloader_common_select_otadata(two_otadata, valid_two_otadata, true);
}

esp_err_t bootloader_common_check_chip_validity(const esp_image_header_t* img_hdr, esp_image_type type)
{
    esp_err_t err = ESP_OK;
    esp_chip_id_t chip_id = CONFIG_IDF_FIRMWARE_CHIP_ID;
    if (chip_id != img_hdr->chip_id) {
        ESP_LOGE(TAG, "mismatch chip ID, expected %d, found %d", chip_id, img_hdr->chip_id);
        err = ESP_FAIL;
    }

#ifndef CONFIG_IDF_ENV_FPGA
    uint8_t revision = bootloader_common_get_chip_revision();
    if (revision < img_hdr->min_chip_rev) {
        /* To fix this error, please update mininum supported chip revision from configuration,
         * located in TARGET (e.g. ESP32) specific options under "Component config" menu */
        ESP_LOGE(TAG, "This chip is revision %d but the application is configured for minimum revision %d. Can't run.", revision, img_hdr->min_chip_rev);
        err = ESP_FAIL;
    } else if (revision != img_hdr->min_chip_rev) {
#ifdef BOOTLOADER_BUILD
        ESP_LOGI(TAG, "chip revision: %d, min. %s chip revision: %d", revision, type == ESP_IMAGE_BOOTLOADER ? "bootloader" : "application", img_hdr->min_chip_rev);
#endif
    }
#endif // CONFIG_IDF_ENV_FPGA

    return err;
}

int bootloader_common_select_otadata(const esp_ota_select_entry_t *two_otadata, bool *valid_two_otadata, bool max)
{
    if (two_otadata == NULL || valid_two_otadata == NULL) {
        return -1;
    }
    int active_otadata = -1;
    const esp_ota_select_entry_t *s0 = &two_otadata[0];
    const esp_ota_select_entry_t *s1 = &two_otadata[1];
    const bool s0_valid = valid_two_otadata[0];
    const bool s1_valid = valid_two_otadata[1];
    if (s0_valid && s1_valid) {
        if (max) {
            active_otadata = (s0->seq > s1->seq ? 0 : 1);
        } else {
            active_otadata = (s0->seq < s1->seq ? 0 : 1);
        }
    } else if (s0_valid) {
        active_otadata = 0;
    } else if (s1_valid) {
        active_otadata = 1;
    }
    return active_otadata;
}

esp_err_t bootloader_common_get_partition_description(const esp_partition_pos_t *partition, esp_app_desc_t *app_desc)
{
    if (partition == NULL || app_desc == NULL || partition->offset == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint32_t app_desc_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    const uint32_t mmap_size = app_desc_offset + sizeof(esp_app_desc_t);
    const uint8_t *image = bootloader_mmap(partition->offset, mmap_size);
    if (image == NULL) {
        ESP_LOGE(TAG, "bootloader_mmap(0x%x, 0x%x) failed", partition->offset, mmap_size);
        return ESP_FAIL;
    }

    memcpy(app_desc, image + app_desc_offset, sizeof(esp_app_desc_t));
    bootloader_munmap(image);

    if (app_desc->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

#if defined( CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP ) || defined( CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC )

#define RTC_RETAIN_MEM_ADDR (SOC_RTC_DRAM_HIGH - sizeof(rtc_retain_mem_t))

rtc_retain_mem_t *const rtc_retain_mem = (rtc_retain_mem_t *)RTC_RETAIN_MEM_ADDR;

#ifndef BOOTLOADER_BUILD
#include "heap_memory_layout.h"
/* The app needs to be told this memory is reserved, important if configured to use RTC memory as heap.

   Note that keeping this macro here only works when other symbols in this file are referenced by the app, as
   this feature is otherwise 100% part of the bootloader. However this seems to happen in all apps.
 */
SOC_RESERVE_MEMORY_REGION(RTC_RETAIN_MEM_ADDR, RTC_RETAIN_MEM_ADDR + sizeof(rtc_retain_mem_t), rtc_retain_mem);
#endif

static bool check_rtc_retain_mem(void)
{
    return esp_rom_crc32_le(UINT32_MAX, (uint8_t*)rtc_retain_mem, sizeof(rtc_retain_mem_t) - sizeof(rtc_retain_mem->crc)) == rtc_retain_mem->crc && rtc_retain_mem->crc != UINT32_MAX;
}

static void update_rtc_retain_mem_crc(void)
{
    rtc_retain_mem->crc = esp_rom_crc32_le(UINT32_MAX, (uint8_t*)rtc_retain_mem, sizeof(rtc_retain_mem_t) - sizeof(rtc_retain_mem->crc));
}

void bootloader_common_reset_rtc_retain_mem(void)
{
    memset(rtc_retain_mem, 0, sizeof(rtc_retain_mem_t));
}

uint16_t bootloader_common_get_rtc_retain_mem_reboot_counter(void)
{
    if (check_rtc_retain_mem()) {
        return rtc_retain_mem->reboot_counter;
    }
    return 0;
}

esp_partition_pos_t* bootloader_common_get_rtc_retain_mem_partition(void)
{
    if (check_rtc_retain_mem()) {
        return &rtc_retain_mem->partition;
    }
    return NULL;
}

void bootloader_common_update_rtc_retain_mem(esp_partition_pos_t* partition, bool reboot_counter)
{
    if (reboot_counter) {
        if (!check_rtc_retain_mem()) {
            bootloader_common_reset_rtc_retain_mem();
        }
        if (++rtc_retain_mem->reboot_counter == 0) {
            // do not allow to overflow. Stop it.
            --rtc_retain_mem->reboot_counter;
        }

    }

    if (partition != NULL) {
        rtc_retain_mem->partition.offset = partition->offset;
        rtc_retain_mem->partition.size   = partition->size;
    }

    update_rtc_retain_mem_crc();
}

rtc_retain_mem_t* bootloader_common_get_rtc_retain_mem(void)
{
    return rtc_retain_mem;
}
#endif // defined( CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP ) || defined( CONFIG_BOOTLOADER_CUSTOM_RESERVE_RTC )
