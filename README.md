# MarvinOS

A custom 64-bit operating system built from scratch, capable of running Doom (1993).

## Overview

MarvinOS is a monolithic kernel operating system targeting x86-64 hardware with UEFI boot support. It implements a custom UEFI bootloader, a higher-half kernel, process isolation via paging, a FAT filesystem driver, PS/2 keyboard input, sound output (Sound Blaster 16 / PC speaker fallback), and a port of [doomgeneric](https://github.com/ozkl/doomgeneric) as its flagship application. To play doom, you need a WAD file containing the game data. Put the file into the (`rootfs/`) directory.

**Key components:**
- UEFI bootloader (`src/bootloader/`) — sets up GOP framebuffer, loads kernel at `0x200000`, exits boot services
- Monolithic kernel (`src/kernel/`) — paging (4-level), IDT, PIC, ATA/AHCI + FAT filesystem, system calls, round-robin scheduler, binary loader
- User space (`src/user/`) — custom C runtime (`crt0`), partial libc (malloc, stdio, string, math), shell, Doom, Tetris, and several test programs
- Doom WAD (`rootfs/doom1.wad`) — required to run Doom; must be provided separately (shareware WAD works)

## Prerequisites

### Native build (Arch Linux / x86-64 Linux)

```
gcc  nasm  binutils  gnu-efi  mtools  xxd  qemu-system-x86  gdb xorriso
```

On Arch Linux:
```sh
sudo pacman -S gcc nasm binutils gnu-efi mtools qemu-system-x86 gdb xorriso
# `xxd` is not its own package: it ships with `vim`/`gvim` (likely already installed)
# or `tinyxxd`. Note that `tinyxxd` conflicts with `vim` (both provide the xxd binary).
```

### Docker (any OS / architecture)

Only Docker is required so no host toolchain is needed.

> **macOS / Apple Silicon note:** The Makefile detects `arm64` and passes `--platform linux/amd64` automatically. QEMU audio uses `coreaudio` on macOS and `pipewire` on Linux.


## Quick Start

```sh
# 1. Build the image (native)
make all

# 2. Run in QEMU
make run
```

Or with Docker:
```sh
make docker-setup   # one-time image build
make docker-build   # compile inside container
make docker-run     # run QEMU inside container (VNC on :5900)
```

> The OS boots to a shell without any extra files. To play Doom, drop a `doom1.wad`
> (shareware works) into `rootfs/` before building — see [Audio](#audio) and the
> note in [Project Structure](#project-structure).


## Build Commands

| Command | Description |
|---|---|
| `make` / `make all` | Build everything (bootloader, kernel, all user programs) |
| `make kernel` | Build only the kernel binary |
| `make shell` | Build only the user shell |
| `make doom` | Build only the Doom binary |
| `make clean` | Remove the `build/` directory and generated headers |
| `make clean-build` | Clean & build & run & clean |


## Run Commands

| Command | Description |
|---|---|
| `make run` | Launch the OS in QEMU with audio (Sound Blaster 16) |
| `make debug` | Rebuild with debug symbols (`-g`) and start QEMU paused, waiting for GDB on port 1234 |
| `make gdb` | Attach GDB to a running `make debug` session |
| `make marvin` | Build everything then immediately run in QEMU |


## Docker Commands

| Command | Description |
|---|---|
| `make docker-setup` | Build the `marvin-build` Docker image (Arch Linux + toolchain) |
| `make docker-build` | Compile the project inside the container |
| `make docker-run` | Run QEMU inside the container; connect via VNC at `localhost:5900` |


## ISO / Hardware Commands

| Command | Description |
|---|---|
| `make iso` | Create a bootable hybrid ISO at `build/marvin.iso` |
| `make iso_usb` | Write the ISO to an auto-detected USB stick (auto-increments test number) |


## Debugging

```sh
# Terminal 1 starts QEMU paused, GDB stub on :1234
make debug

# Terminal 2 attaches GDB with source + register layout
make gdb
```

GDB connects to `localhost:1234`, loads `build/kernel.elf`, and opens split source/register TUI views automatically.


## Project Structure

```
src/
  bootloader/   UEFI bootloader (main.c → BOOTX64.EFI)
  kernel/       Monolithic kernel
    helper/     Subsystems: paging, IDT, PIC, ATA, FAT, scheduler, syscalls, ...
  user/
    lib/        Partial libc (malloc, stdio, string, math, ctype)
    shell/      Interactive shell
    doom/       doomgeneric platform layer
    tetris/     Tetris game
    count/      Counter demo
    meminfo/    Memory info utility
    spawn10/    Multi-process spawn test
    pf_test/    Page-fault test
doomgeneric/    Upstream doomgeneric source (with MarvinOS backend)
rootfs/         Files placed on the virtual FAT disk at boot
  bin/          User ELF binaries (built by make)
  doom1.wad     Doom shareware WAD (not included, add manually!)
  snd/          Sound assets
build/          Compiled artifacts (generated)
qemu/OVMF/      OVMF firmware for UEFI emulation
```

## Kernel Memory Map

| Virtual Base Address | Region |
|---|---|
| `0xFFFFFFFF80000000` | Kernel binary |
| `0xFFFFFF0000000000` | Kernel stacks |
| `0xFFFFFE8000000000` | GOP framebuffer |
| `0xFFFFC90000000000` | Kernel heap |
| `0xFFFF888000000000` | Direct physical memory map |

User space spans `0x0` – `0x00007FFFFFFFFFFF` (128 TB). Each process gets its own page table; kernel space is mapped identically in all of them.

## User Programs

Once booted, the shell provides the following commands:

| Command | Description |
|---|---|
| `doom` | Launch Doom (requires `doom1.wad` on the FAT disk) |
| `tetris` | Tetris |
| `count` | Simple counter demo |
| `meminfo` | Print memory usage |
| `spawn10` | Spawn 10 concurrent processes |
| `pf_test` | Trigger and recover from a page fault |
| `test` | Run kernel self-tests |

## Audio

The kernel supports two audio backends, selected automatically at runtime:

- **Sound Blaster 16** — full audio via DMA; used when present (QEMU emulates it with `-device sb16`)
- **PC speaker** — square-wave fallback; works on any x86 hardware
