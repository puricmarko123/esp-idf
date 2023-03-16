#!/bin/bash

set -e -x

patch -d components/lwip/lwip -p 1 < lwip.patch
patch -d components/lwip/lwip -p 1 < lwip_napt_fix.patch
patch -d components/bt/host/nimble/nimble -p 1 < nimble_reset_handle_on_restart.patch
