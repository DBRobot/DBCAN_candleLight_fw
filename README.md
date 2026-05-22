# DBCAN_candleLight_fw

A downstream fork of [candleLight_fw](https://github.com/candle-usb/candleLight_fw),
based on [marckleinebudde's multichannel branch](https://github.com/marckleinebudde/candleLight_fw/tree/multichannel).

This fork adds:
- Board support for the **DBRobot DBCAN V1.0** (STM32G0B1KBT6, dual FDCAN)
- A fix for the STM32G0B1 USB enumeration failure after `dfu-util :leave`
  (upstream [#195](https://github.com/candle-usb/candleLight_fw/issues/195))

For the list of other supported MCUs and boards, build options, and
general usage, see the [upstream README](https://github.com/marckleinebudde/candleLight_fw/blob/multichannel/README.md).
The sections below cover DBCAN-specific build and flash instructions.

## Limitations

STM32G431-based devices (e.g. CANable-MKS 2.0) are not supported by this project yet.

Currently, the firmware sends back an echo frame to the host when the frame is written to the CAN peripheral, and not when the frame is actually sent successfully on the bus. This affects timestamps, one-shot mode, and other edge cases.

## Known issues

Be aware that there is a bug in the gs_usb module in linux<4.5 that can crash the kernel on device removal.

Here is a fixed version that should also work for older kernels:
  https://github.com/HubertD/socketcan_gs_usb

The Firmware also implements WCID USB descriptors and thus can be used on recent Windows versions without installing a driver.

## Building

Requires `cmake` and the `arm-none-eabi-gcc` toolchain.

```shell
sudo apt-get install cmake gcc-arm-none-eabi

# Configure (uses the system-installed toolchain on $PATH)
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake

# Build just the DBCAN target
cmake --build build --target DBCAN_V1_0_fw

# ...or build every supported board
cmake --build build

# To list all available targets
cmake --build build --target help
```

The output binary lands at `build/DBCAN_V1_0_fw.bin`.

## Download Binaries
Prebuilt binaries can be downloaded by clicking [![CI](https://github.com/candle-usb/candleLight_fw/actions/workflows/ci.yml/badge.svg)](https://github.com/candle-usb/candleLight_fw/actions). On the workflow overview page, select the latest workflow that ran on master branch. The firmware artifacts can downloaded by clicking them at the bottom of the page.

## Flashing

Flashing uses `dfu-util` over USB.

```shell
sudo apt install dfu-util
```

### One-time setup: install the udev rule

Lets `dfu-util` talk to a running adapter without `sudo`:

```shell
sudo cp 70-candle-usb.rules /etc/udev/rules.d/
sudo udevadm control --reload
sudo udevadm trigger
```

### Flashing an already-running adapter

```shell
# Reboot the running firmware into its DFU bootloader
dfu-util -d 1d50:606f -e

# Wait a moment for the ROM bootloader to enumerate as 0483:df11, then flash
sudo dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D build/DBCAN_V1_0_fw.bin
```

This fork includes a fix for the STM32G0B1 DFU-leave bug, so the board
re-enumerates cleanly after `:leave` — no unplug/replug needed.

The CMake-generated convenience target wraps both commands:

```shell
cmake --build build --target flash-DBCAN_V1_0_fw
```

### Reflashing a specific device when several are connected

```shell
dfu-util -l                    # find the right serial number
sudo dfu-util -d 0483:df11 -S <serial> -a 0 -s 0x08000000:leave \
    -D build/DBCAN_V1_0_fw.bin
```



## Associating persistent device names
With udev on linux, it is possible to assign a device name to a certain serial number (see udev manpages and [systemd.link](https://www.freedesktop.org/software/systemd/man/systemd.link.html)).
This can be useful when multiple devices are connected at the same time.

An example for a DBCAN V1.0 adapter:

```
 $ cat /etc/systemd/network/60-persistent-candev.link
[Match]
Property=ID_MODEL=DBCAN-V1.0_gs_usb ID_SERIAL_SHORT="004E002E4156501720383937"

[Link]
# from systemd.link manpage:
# Note that specifying a name that the kernel might use for another interface (for example "eth0") is dangerous because the name assignment done by udev will race with the assignment done by the kernel, and only one
#   interface may use the name. Depending on the order of operations, either udev or the kernel will win, making the naming unpredictable. It is best to use some different prefix

Name=dbcan0
```

(Find the serial number with `lsusb -v -d 1d50:606f | grep iSerial` or `udevadm info /sys/class/net/can0`.) After reloading systemd units and re-plugging the board:

```
 $ ip a
....
59: dbcan0: <NOARP,ECHO> mtu 16 qdisc noop state DOWN group default qlen 10
    link/can
 $
```


## Hacking
### Submitting pull requests
- Each commit must not contain unrelated changes (e.g. functional and whitespace changes)
- Project must be compilable (with default options) and functional, at each commit.
- Squash any "WIP" or other temporary commits.
- Make sure your editor is not messing up whitespace or line-ends.
- We include both a `.editorconfig` and `uncrustify.cfg` which should help with whitespace.

Typical command to run uncrustify on all source files (ignoring HAL and third-party libs):
`uncrustify -c ./uncrustify.cfg --replace --no-backup $(find include src -name "*.[ch]")`

### Profiling
Not great on cortex-M0 cores (F042, F072 targets etc) since they lack hardware support (ITM and SWO). However, it's possible to randomly sample the program counter and get some coarse profiling info.

For example, openocd has the `profile` command (see https://openocd.org/doc/html/General-Commands.html#Misc-Commands), e.g.

```profile 5 test.out 0x8000000 0x8100000```

(from inside gdb, the command needs to be prefixed with `monitor` to forward it to openocd, i.e. `monitor profile 5 .....`.

The .out file can then be processed with `gprof <firmware_name> -l test.out`
