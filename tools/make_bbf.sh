#!/usr/bin/env bash
# Pack a raw firmware .bin (linked at 0x08020200) into a .bbf the Prusa
# CORE One bootloader accepts via USB stick (appendix must be broken).
#
# Usage: tools/make_bbf.sh build/bringup.bin
set -e
BIN="${1:?usage: make_bbf.sh <firmware.bin>}"
PACK="$(dirname "$0")/../../Prusa-Firmware-Buddy/utils/pack_fw.py"

# CORE One: printer-type 7, version 1, subversion 0.  --no-sign: unsigned custom
# firmware (accepted because the appendix is broken).  High version so the
# bootloader always offers it as an update.
python "$PACK" "$BIN" --no-sign --version 99.0.0+99 \
    --printer-type 7 --printer-version 1 --printer-subversion 0 --board 0

echo "Created ${BIN%.bin}.bbf"
