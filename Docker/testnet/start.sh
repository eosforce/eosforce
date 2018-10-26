#!/bin/sh
cd /opt/eosforce/
./bios_boot_eosforce.py -a --root ./
tail -f ./nodes/biosbpa.log