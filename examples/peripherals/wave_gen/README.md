| Supported Targets | ESP32 |
| ----------------- | ----- |

# Wave Generator Example

(See the README.md file in the upper level 'examples' directory for more information about examples)

This example demonstrates how to implement a software controlled signal generator by utilizing the DAC and Timer Group drivers. All waveforms demonstrated in this example are generated by software.

Users can connect DAC output channel to their devices and use it as a simple analog signal output source.

## How to use this example

### Hardware Required

* A development board with ESP32 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
* A USB cable for power supply and programming
* Target device or oscilloscope (not required)

Make sure the DAC output pin (which is the GPIO25 if channel 1 set, GPIO26 if channel 2 set) is connected to the target device correctly.

### Configure the project

Open the project configuration menu (`idf.py menuconfig`). 

In the `Example Configuration` menu:

* Use `DAC Channel Num` to select the DAC number.
* Use `Waveform` to select the waveform type.
    * Select `Sine` (*default*), `Triangle`, `Sawtooth` or `Square` wave type.
* Select `Wave frequency` from the available range.
* Set the `Enable output voltage log` if you want to print the log in the terminal.

#### Wave frequency

For this example, the range of frequency is from 1 kHz to 17 kHz. **3 kHz is selected by default.**

If you modify the frequency, you will change the number of DAC output points. This will affect the smoothness of the curve as well.
Each output point value is calculated by the DAC resolution of 8-bits (0~255). All of these raw values are stored in an array.

Based on the given frequency, the number of DAC output points for each cycle can be calculated through the following formula:
```num_of_output_points = 1000000(us)/(7 us * frequency)```

For example, with high frequency, 20 kHz will generate only 10 output points; the curve will be edgy.

On the other hand, 500 Hz, a relative low frequency, will result in many DAC output points and the array will not be able to store all of the generated data.

Thus, there will be less output points per cycle in higher frequency, and more points in lower frequency.

After the raw value calculation, the real output voltage can be converted through the following formula (VDD is 3.3 V):

```points_voltage = (VDD * DAC_OUTPUT / 255)```

The voltage is within the range of 0~3300 mV.

#### Enable output voltage log

**Disabled selected by default.**

If enabled, the expected voltage of each point will be printed on the terminal.

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

Run `idf.py -p PORT flash monitor` to build, flash and monitor the project.

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for all the steps to configure and use the ESP-IDF to build projects.

* [ESP-IDF Getting Started Guide on ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

## Example Output

If an oscilloscope is available, the target wave will be displayed in it after running this example.

#### Sine:
![Sine](image/sine.png)
#### Triangle:
![Triangle](image/triangle.png)
#### Sawtooth:
![Sawtooth](image/sawtooth.png)
#### Square:
![Square](image/square.png)

The output log will be shown in the terminal as the following (if `Enable output voltage log` is enabled):

```
I (318) Wave generator: DAC output channel: 1
I (318) Wave generator: GPIO:25
I (328) Wave generator: Waveform: Sine
I (328) Wave generator: Frequency(Hz): 3000
I (338) Wave generator: Output points num: 47

I (438) Wave generator: Output voltage(mV): 1656
I (538) Wave generator: Output voltage(mV): 1863
I (638) Wave generator: Output voltage(mV): 2083
I (738) Wave generator: Output voltage(mV): 2290
I (838) Wave generator: Output voltage(mV): 2484
I (938) Wave generator: Output voltage(mV): 2678
I (1038) Wave generator: Output voltage(mV): 2834
I (1138) Wave generator: Output voltage(mV): 2976
I (1238) Wave generator: Output voltage(mV): 3092
I (1338) Wave generator: Output voltage(mV): 3183
....

```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub. We will get back to you soon.