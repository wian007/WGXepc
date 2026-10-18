#!/bin/sh
# Ported from the original FreeBSD 99-fansandled rc-script.
# Sets the arm/disarm LED to green and the minimum fan speed at boot.
#
# Behaviour differs per Firebox model:
#   - M370/470/570/670: LED (-l) and fan control (-f) work normally.
#   - XTM800/1500: LED (-l) is not implemented, and fan control (-f) is
#     blocked by a BIOS/SMI routine that keeps reasserting the fan
#     registers. Both calls just print a message and exit cleanly.
#     See linux/WGXepc.c source comments for the full investigation.
#
# The detected model and the result of each call end up in the journal:
#   journalctl -u wgxepc-fansandled
/usr/local/sbin/wgxepc -l green
/usr/local/sbin/wgxepc -f 00
