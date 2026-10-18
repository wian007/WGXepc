# WGXepc

Hardware control utility for Watchguard Firebox appliances: fan speed, the
arm/disarm LED, LCD backlight and temperature readings, by talking directly
to the SuperIO and chipset GPIO registers.

This fork adds a **Linux (Ubuntu) port** in [`linux/`](./linux). The original
FreeBSD/pfSense version by stephenw10 is kept unchanged in
[`WGXepc.c`](./WGXepc.c).

> [!WARNING]
> **The Linux port was made with the help of AI (Claude, by Anthropic).**
> It has been tested by hand on a Firebox XTM800/1500 and an M470 running
> Ubuntu 24.04, but not on any other model. The tool writes directly to
> hardware I/O ports as root. Use it at your own risk, and read the source
> before running it on hardware you care about.

## What works on Linux

| Model              | LED (`-l`) | Fan (`-f`)    | Notes |
| ------------------ | ---------- | ------------- | ----- |
| M370/470/570/670   | ✅ tested  | ✅ tested     | No LCD, no flashing LED |
| XTM800/1500        | ❌         | ❌            | Fan registers are overridden by firmware (see below). `-dump` and `-rpm` work for diagnostics. LCD works through LCDproc. |
| Other models       | ❓         | ❓            | Code paths ported from the original but untested on Linux |

### Why fan control doesn't work on the XTM800/1500

The XTM800/1500 uses a Winbond/Nuvoton W83627DHG-P SuperIO chip. Following
the datasheet, the fan output can be switched to manual mode and given a
value, but on this unit the value never sticks: the fan configuration
register reverts on its own and the measured RPM never changes. That points
to a BIOS/SMI routine continuously reasserting the registers, which userspace
can't override. The full investigation is written up in the comments of
`linux/WGXepc.c`. Two read-only diagnostic commands were added for anyone who
wants to dig further:

- `-dump <bank>` dumps all hardware-monitor registers of a W83627 bank
- `-rpm` shows the measured fan speeds from the chip's tachometer inputs

## Building and installing

```sh
cd linux
make
sudo make install            # installs /usr/local/sbin/wgxepc
sudo wgxepc                  # shows detected model and usage
```

To set the LED and minimum fan speed automatically at boot:

```sh
sudo make install-service
sudo systemctl daemon-reload
sudo systemctl enable --now wgxepc-fansandled.service
journalctl -u wgxepc-fansandled   # shows the result per boot
```

Edit `/usr/local/sbin/99-fansandled.sh` to change the LED colour or fan value.

## LCD display

The front-panel LCD and buttons on the XTM series are handled by the
`sdeclcd` driver in LCDproc, not by this tool. On Ubuntu:

```sh
sudo apt install lcdproc lcdproc-extra-drivers
```

Then in `/etc/LCDd.conf` set `Driver=sdeclcd` and add, under `[server]`:

```ini
DriverPath=/usr/lib/x86_64-linux-gnu/lcdproc/
```

and enable it with `sudo systemctl enable --now lcdproc.service`. The boot
service above is ordered to start after LCDproc. Don't use `wgxepc -b` while
the LCD driver is running.

## Changes from the FreeBSD version

- `machine/cpufunc.h` / `/dev/io` replaced by `sys/io.h` and `ioperm()`
- `outb()` argument order adjusted (glibc uses `outb(value, port)`)
- `kenv()` SMBIOS lookups replaced by `/sys/class/dmi/id/product_name` and
  `board_name`
- Root check added, since `ioperm()` needs root
- `-dump` and `-rpm` diagnostics added, XTM800/1500 fan findings documented
- FreeBSD rc-script replaced by a systemd oneshot service

## Credits and sources

- **Original WGXepc** by stephenw10:
  <https://github.com/stephenw10/WGXepc>. All credit for the original code
  and the reverse engineering of the Firebox hardware goes to him and the
  pfSense community. The I/O helper functions come from LCDproc.
- **Nuvoton W83627DHG-P datasheet**: register map used for the XTM800/1500
  investigation and the `-dump` / `-rpm` diagnostics.
- **LCDproc `sdeclcd` driver**: <https://github.com/lcdproc/lcdproc>, used for
  the LCD and buttons.
- **Claude (Anthropic)**: assisted with the Linux port, the XTM800/1500
  investigation and this documentation.

## License

The original repository does not include a license, so its licensing terms
are unclear. This fork makes no claim on stephenw10's code. If you are the
original author and want this fork changed or taken down, please open an
issue.
