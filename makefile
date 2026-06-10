ARCH          = x86_64
SYSROOT       = sysroot

DOCKER_IMAGE = marvin-build

# Source paths
BOOT_SRC      = src/bootloader
KERN_SRC      = src/kernel
HELPER_SRC    = $(KERN_SRC)/helper
USER_SRC      = src/user
LOADER_SRC    = $(HELPER_SRC)/loader

DOOM_LIBC_SRCS = string.c malloc.c stdio.c stdlib.c ctype.c math.c
DOOM_LIBC_OBJS = $(patsubst %.c,$(BUILD)/libc_%.o,$(DOOM_LIBC_SRCS))

HELPER_SRCS   = $(wildcard $(HELPER_SRC)/**/*.c) $(wildcard $(HELPER_SRC)/*.c)
HELPER_HDRS   = $(wildcard $(HELPER_SRC)/**/*.h) $(wildcard $(HELPER_SRC)/*.h)
HELPER_OBJS   = $(patsubst %.c,$(BUILD)/helper_%.o,$(notdir $(HELPER_SRCS)))

ASM_SRCS      = $(wildcard $(KERN_SRC)/*.asm) $(wildcard $(HELPER_SRC)/**/*.asm) $(wildcard $(HELPER_SRC)/*.asm)
ASM_OBJS      = $(patsubst %.asm,$(BUILD)/%.o,$(notdir $(ASM_SRCS)))

# Build output
BUILD         = build
ESP_DIR       = $(BUILD)/esp/EFI/BOOT
ESP_ROOT      = $(BUILD)/esp

EFI_INCLUDES= -I /usr/include/efi \
              -I /usr/include/efi/$(ARCH)

EFI_CFLAGS    = $(EFI_INCLUDES) \
              -fno-stack-protector \
              -fpic \
              -fshort-wchar \
              -mno-red-zone \
              -maccumulate-outgoing-args \
              $(DEBUG_C_FLAGS)

EFI_LDFLAGS   = -nostdlib \
              -znocombreloc \
              -T /usr/lib/elf_$(ARCH)_efi.lds \
              -shared \
              -Bsymbolic \
              -L /usr/lib \
              -l:libgnuefi.a \
              -l:libefi.a

KERN_CFLAGS = --sysroot=$(SYSROOT) \
		  -ffreestanding \
	      -mno-red-zone \
	      -mno-sse -mno-mmx -mno-sse2 -mno-80387 \
	      -fno-stack-protector \
	      -fno-delete-null-pointer-checks \
	      -nostdlib -O2 $(DEBUG_C_FLAGS)

# User-space programs: position-dependent code linked at USER_LOAD_BASE (0x10000).
USER_BASE_CFLAGS = -ffreestanding -nostdlib -m64 \
                -mno-red-zone -fno-stack-protector \
                -fno-pic -fno-pie -O2 $(DEBUG_C_FLAGS) \
                -fno-builtin -I $(USER_SRC)/include

USER_CFLAGS   = $(USER_BASE_CFLAGS) -nostdinc \
                -I $(shell gcc -print-file-name=include) \
				--sysroot=src/user

#DOOM
DOOM_DIR      = doomgeneric
# -I . lets doomgeneric_marvinos.c resolve "src/user/lib/syscall.h" from the project root
DOOM_CFLAGS   = $(USER_BASE_CFLAGS) -I $(DOOM_DIR) -I $(USER_SRC) -I . \
                -isystem $(USER_SRC)/include \
                -DNORMALUNIX -DLINUX -DFEATURE_SOUND -DMARVIN_SOUND -w

OVMF        = qemu/OVMF/OVMF.4m.fd

EFI_CRT0      = /usr/lib/crt0-efi-$(ARCH).o

VPATH         = $(sort $(dir $(HELPER_SRCS) $(ASM_SRCS) $(KERN_SRC)/kernel.c))

UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),arm64)
  DOCKER_PLATFORM = --platform linux/amd64
else
  DOCKER_PLATFORM =
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  QEMU_AUDIODEV = coreaudio
else
  QEMU_AUDIODEV = pipewire
endif

ifeq ($(DEBUG), 1)
  DEBUG_C_FLAGS = -g
  DEBUG_ASM_FLAGS = -g -F dwarf
else
  DEBUG_C_FLAGS =
  DEBUG_ASM_FLAGS =
endif

.PHONY: all run clean docker-build docker-setup docker-run debug gdb kernel iso marvin clean-build iso_usb


all: $(BUILD)/main.efi $(BUILD)/kernel.bin rootfs/bin/sh rootfs/bin/test rootfs/bin/doom rootfs/bin/count rootfs/bin/meminfo rootfs/bin/tetris rootfs/bin/spawn10 rootfs/bin/pf_test
#User-space

$(BUILD)/user_crt0.o: $(USER_SRC)/crt0.asm
	@mkdir -p $(BUILD)
	nasm -f elf64 $(DEBUG_ASM_FLAGS) $< -o $@

$(BUILD)/user_shell.o: $(USER_SRC)/shell/shell.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC) -c $< -o $@

$(BUILD)/user_gif.o: $(USER_SRC)/gif/gif.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC) -c $< -o $@

$(BUILD)/user_lzw.o: $(USER_SRC)/gif/lzw.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC) -c $< -o $@

$(BUILD)/shell.elf: $(BUILD)/user_crt0.o $(BUILD)/user_shell.o $(BUILD)/user_gif.o $(BUILD)/user_lzw.o $(BUILD)/libc_malloc.o $(USER_SRC)/user.ld
	ld -T $(USER_SRC)/user.ld -nostdlib $(BUILD)/user_crt0.o $(BUILD)/user_shell.o $(BUILD)/user_gif.o $(BUILD)/user_lzw.o $(BUILD)/libc_malloc.o -o $@

# --pad-to forces objcopy to include BSS as zeroes in the flat binary.
# Without this, objcopy skips .bss (SHT_NOBITS) and the file is too small.
$(BUILD)/shell.bin: $(BUILD)/shell.elf
	@BSS_END=$$(nm $< | awk '$$3 == "__bss_end" {print "0x"$$1}'); \
	objcopy -O binary --pad-to=$$BSS_END $< $@

# Generate a C header with the shell binary as a byte array.
# sed normalises the variable names so loader.c can use shell_bin / shell_bin_len
# regardless of the build path.
$(LOADER_SRC)/shell_bin.h: $(BUILD)/shell.bin
	@printf '#pragma once\n' > $@
	xxd -i $< | sed \
		-e 's/^unsigned char [a-zA-Z0-9_]*\[\]/static const unsigned char shell_bin[]/' \
		-e 's/^unsigned int [a-zA-Z0-9_]*/static const unsigned int shell_bin_len/' \
		>> $@

# loader.c #includes shell_bin.h — ensure it exists before the .o is compiled.
$(BUILD)/helper_loader.o: $(LOADER_SRC)/shell_bin.h

rootfs/bin/sh: $(BUILD)/shell.elf
	@mkdir -p rootfs/bin
	cp $< $@

$(BUILD)/user_test.o: $(USER_SRC)/test/test.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC) -c $< -o $@

$(BUILD)/test.elf: $(BUILD)/user_crt0.o $(BUILD)/user_test.o $(DOOM_LIBC_OBJS) $(USER_SRC)/user.ld
	ld -T $(USER_SRC)/user.ld -nostdlib $(BUILD)/user_crt0.o $(BUILD)/user_test.o $(DOOM_LIBC_OBJS) -o $@

rootfs/bin/test: $(BUILD)/test.elf
	@mkdir -p rootfs/bin
	cp $< $@

$(BUILD)/user_count.o: $(USER_SRC)/count/count.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC) -c $< -o $@

$(BUILD)/count.elf: $(BUILD)/user_crt0.o $(BUILD)/user_count.o $(DOOM_LIBC_OBJS) $(USER_SRC)/user.ld
	ld -T $(USER_SRC)/user.ld -nostdlib $(BUILD)/user_crt0.o $(BUILD)/user_count.o $(DOOM_LIBC_OBJS) -o $@

rootfs/bin/count: $(BUILD)/count.elf
	@mkdir -p rootfs/bin
	cp $< $@

$(BUILD)/user_pf_test.o: $(USER_SRC)/pf_test/pf_test.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC) -c $< -o $@

$(BUILD)/pf_test.elf: $(BUILD)/user_crt0.o $(BUILD)/user_pf_test.o $(DOOM_LIBC_OBJS) $(USER_SRC)/user.ld
	ld -T $(USER_SRC)/user.ld -nostdlib $(BUILD)/user_crt0.o $(BUILD)/user_pf_test.o $(DOOM_LIBC_OBJS) -o $@

rootfs/bin/pf_test: $(BUILD)/pf_test.elf
	@mkdir -p rootfs/bin
	cp $< $@

$(BUILD)/user_spawn10.o: $(USER_SRC)/spawn10/spawn10.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC) -c $< -o $@

$(BUILD)/spawn10.elf: $(BUILD)/user_crt0.o $(BUILD)/user_spawn10.o $(DOOM_LIBC_OBJS) $(USER_SRC)/user.ld
	ld -T $(USER_SRC)/user.ld -nostdlib $(BUILD)/user_crt0.o $(BUILD)/user_spawn10.o $(DOOM_LIBC_OBJS) -o $@

rootfs/bin/spawn10: $(BUILD)/spawn10.elf
	@mkdir -p rootfs/bin
	cp $< $@

$(BUILD)/user_meminfo.o: $(USER_SRC)/meminfo/meminfo.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC) -c $< -o $@

$(BUILD)/meminfo.elf: $(BUILD)/user_crt0.o $(BUILD)/user_meminfo.o $(DOOM_LIBC_OBJS) $(USER_SRC)/user.ld
	ld -T $(USER_SRC)/user.ld -nostdlib $(BUILD)/user_crt0.o $(BUILD)/user_meminfo.o $(DOOM_LIBC_OBJS) -o $@

rootfs/bin/meminfo: $(BUILD)/meminfo.elf
	@mkdir -p rootfs/bin
	cp $< $@

$(BUILD)/user_tetris_driver.o: $(USER_SRC)/tetris/marvinos_tetris.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC)/tetris -I $(USER_SRC) -c $< -o $@

$(BUILD)/user_tetris_engine.o: $(USER_SRC)/tetris/tetris.c
	@mkdir -p $(BUILD)
	gcc $(USER_CFLAGS) -I $(USER_SRC)/tetris -I $(USER_SRC) -c $< -o $@

$(BUILD)/tetris.elf: $(BUILD)/user_crt0.o $(BUILD)/user_tetris_driver.o $(BUILD)/user_tetris_engine.o $(DOOM_LIBC_OBJS) $(USER_SRC)/user.ld
	ld -T $(USER_SRC)/user.ld -nostdlib $(BUILD)/user_crt0.o $(BUILD)/user_tetris_driver.o $(BUILD)/user_tetris_engine.o $(DOOM_LIBC_OBJS) -o $@

rootfs/bin/tetris: $(BUILD)/tetris.elf
	@mkdir -p rootfs/bin
	cp $< $@
#DOOM

DOOM_C_SRCS = am_map.c d_event.c d_items.c d_iwad.c d_loop.c d_main.c \
    d_mode.c d_net.c doomdef.c doomstat.c dstrings.c dummy.c f_finale.c \
    f_wipe.c g_game.c gusconf.c hu_lib.c hu_stuff.c i_cdmus.c i_endoom.c \
    i_input.c i_joystick.c i_scale.c i_sound.c i_system.c i_timer.c i_video.c \
    info.c memio.c m_argv.c m_bbox.c m_cheat.c m_config.c m_controls.c \
    m_fixed.c m_menu.c m_misc.c m_random.c mus2mid.c p_ceilng.c p_doors.c \
    p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c p_maputl.c p_mobj.c \
    p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c p_spec.c p_switch.c \
    p_telept.c p_tick.c p_user.c r_bsp.c r_data.c r_draw.c r_main.c \
    r_plane.c r_segs.c r_sky.c r_things.c sha1.c sounds.c s_sound.c \
    statdump.c st_lib.c st_stuff.c tables.c v_video.c w_checksum.c w_file.c \
    w_file_stdc.c w_main.c w_wad.c wi_stuff.c z_zone.c \
    doomgeneric.c doomgeneric_marvinos.c snd_mixer.c

DOOM_OBJS = $(patsubst %.c,$(BUILD)/doom_%.o,$(DOOM_C_SRCS))

$(BUILD)/doom_%.o: $(DOOM_DIR)/%.c
	@mkdir -p $(BUILD)
	gcc $(DOOM_CFLAGS) -c $< -o $@

$(BUILD)/doom_main.o: $(USER_SRC)/doom/main.c
	@mkdir -p $(BUILD)
	gcc $(DOOM_CFLAGS) -c $< -o $@

$(BUILD)/libc_%.o: $(USER_SRC)/lib/%.c
	@mkdir -p $(BUILD)
	gcc $(DOOM_CFLAGS) -c $< -o $@

$(BUILD)/doom.elf: $(BUILD)/user_crt0.o $(BUILD)/doom_main.o $(DOOM_OBJS) $(DOOM_LIBC_OBJS) $(USER_SRC)/user.ld
	ld -T $(USER_SRC)/user.ld -nostdlib \
	    $(BUILD)/user_crt0.o $(BUILD)/doom_main.o $(DOOM_OBJS) $(DOOM_LIBC_OBJS) -o $@

rootfs/bin/doom: $(BUILD)/doom.elf
	@mkdir -p rootfs/bin
	cp $< $@

#Bootloader

$(BUILD)/main.o: $(BOOT_SRC)/main.c
	@mkdir -p $(BUILD)
	gcc $(EFI_CFLAGS) -c $< -o $@

$(BUILD)/main.so: $(BUILD)/main.o
	ld $(EFI_LDFLAGS) $(EFI_CRT0) $< -l:libgnuefi.a -l:libefi.a -o $@

$(BUILD)/main.efi: $(BUILD)/main.so
	objcopy -j .text \
	        -j .sdata \
	        -j .data \
	        -j .rodata \
	        -j .dynamic \
	        -j .dynsym \
	        -j .rel \
	        -j .rela \
	        -j .reloc \
	        --output-target=efi-app-$(ARCH) \
	        $< $@

#Kernel

$(BUILD)/%.o: %.asm
	@mkdir -p $(BUILD)
	nasm -f elf64 -w+error=label-orphan $(DEBUG_ASM_FLAGS) $< -o $@

$(BUILD)/helper_%.o: %.c $(HELPER_HDRS)
	@mkdir -p $(BUILD)
	gcc $(KERN_CFLAGS) -I $(KERN_SRC) -I $(HELPER_SRC) -c $< -o $@

$(BUILD)/kernel.o: $(KERN_SRC)/kernel.c $(HELPER_HDRS)
	@mkdir -p $(BUILD)
	gcc $(KERN_CFLAGS) -I $(KERN_SRC) -I $(HELPER_SRC) -c $< -o $@

$(BUILD)/kernel.elf: $(ASM_OBJS) $(BUILD)/kernel.o $(HELPER_OBJS) $(KERN_SRC)/kernel.ld 
	ld -T $(KERN_SRC)/kernel.ld -nostdlib $(ASM_OBJS) $(BUILD)/kernel.o $(HELPER_OBJS) -o $@
	
$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	objcopy -O binary $< $@

docker-build:
	docker run --rm $(DOCKER_PLATFORM) -u $$(id -u):$$(id -g) -v "$(CURDIR)":/project $(DOCKER_IMAGE) make all

docker-setup:
	docker build $(DOCKER_PLATFORM) -t $(DOCKER_IMAGE) .

docker-run:
	rm -rf $(ESP_ROOT)
	@mkdir -p $(ESP_DIR)
	cp $(BUILD)/main.efi $(ESP_DIR)/BOOTX64.EFI
	cp $(BUILD)/kernel.bin $(ESP_ROOT)/
	cp -r rootfs/. $(ESP_ROOT)/
	docker run --rm $(DOCKER_PLATFORM) -v "$(CURDIR)":/project \
		-p 5900:5900 \
		$(DOCKER_IMAGE) \
		qemu-system-x86_64 \
			-bios $(OVMF) \
			-drive format=raw,file=fat:rw:$(ESP_ROOT),if=ide \
			-net none \
			-rtc base=localtime \
			-m 512M \
			-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
			-vnc :0

run:
	rm -rf $(ESP_ROOT)
	@mkdir -p $(ESP_DIR)
	cp $(BUILD)/main.efi $(ESP_DIR)/BOOTX64.EFI
	cp $(BUILD)/kernel.bin $(ESP_ROOT)/
	cp -r rootfs/. $(ESP_ROOT)/
	qemu-system-x86_64 \
		-bios $(OVMF) \
		-drive format=raw,file=fat:rw:$(ESP_ROOT),if=ide \
		-net none \
		-rtc base=localtime \
		-m 512M \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-serial stdio \
		-audiodev $(QEMU_AUDIODEV),id=snd0 \
		-machine pc,pcspk-audiodev=snd0 \
		-device sb16,dma=1,dma16=5,audiodev=snd0

debug:
	$(MAKE) clean
	$(MAKE) all DEBUG=1
	@mkdir -p $(ESP_DIR)
	cp $(BUILD)/main.efi $(ESP_DIR)/BOOTX64.EFI
	cp $(BUILD)/kernel.bin $(ESP_ROOT)/
	qemu-system-x86_64 \
		-bios $(OVMF) \
		-drive format=raw,file=fat:rw:$(ESP_ROOT),if=ide \
		-net none \
		-rtc base=localtime \
		-s -S \
		-m 512M \
		-serial stdio \
		-audiodev $(QEMU_AUDIODEV),id=snd0 \
		-machine pc,pcspk-audiodev=snd0 \
		-device sb16,dma=1,dma16=5,audiodev=snd0

gdb:
	gdb build/kernel.elf -ex "target remote localhost:1234" \
	       -ex "layout src" \
	       -ex "layout reg" \

clean:
	rm -rf $(BUILD)
	rm -f $(LOADER_SRC)/shell_bin.h rootfs/bin/sh

iso: all
	@mkdir -p $(BUILD)/isoroot
	dd if=/dev/zero of=$(BUILD)/isoroot/efi.img bs=1M count=64
	mformat -i $(BUILD)/isoroot/efi.img -F ::
	mmd -i $(BUILD)/isoroot/efi.img ::/EFI ::/EFI/BOOT
	mcopy -i $(BUILD)/isoroot/efi.img $(BUILD)/main.efi ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $(BUILD)/isoroot/efi.img $(BUILD)/kernel.bin ::
	for f in rootfs/*; do mcopy -i $(BUILD)/isoroot/efi.img "$$f" ::; done
	xorriso -as mkisofs \
		-o $(BUILD)/marvin.iso \
		-e efi.img \
		-no-emul-boot \
		-isohybrid-gpt-basdat \
		$(BUILD)/isoroot/

kernel: $(BUILD)/kernel.bin

bootloader: $(BUILD)/main.efi

shell: rootfs/bin/sh

doom: rootfs/bin/doom

marvin:
	$(MAKE) all
	$(MAKE) run

clean-build:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) run
	$(MAKE) clean

iso_usb: iso
	@USB_PART=$$(lsblk -o PATH,RM,TYPE -nr | awk '$$2=="1" && $$3=="part" {print $$1; exit}'); \
	if [ -z "$$USB_PART" ]; then echo "Error: no USB stick found."; exit 1; fi; \
	ALREADY_MNT=$$(lsblk -no MOUNTPOINT "$$USB_PART" 2>/dev/null | tr -d ' '); \
	if [ -n "$$ALREADY_MNT" ]; then \
		MNT=$$ALREADY_MNT; UNMOUNT=0; \
	else \
		MNT=$$(udisksctl mount -b "$$USB_PART" | sed 's/.* at //;s/\.$$//'); \
		UNMOUNT=1; \
	fi; \
	LAST=$$(ls "$$MNT"/marvin_test_*.iso 2>/dev/null | sort -V | tail -1); \
	if [ -n "$$LAST" ]; then \
		NUM=$$(basename "$$LAST" .iso | grep -oE '[0-9]+$$'); \
		NEXT=$$((NUM + 1)); \
		rm -f "$$LAST"; \
	else \
		NEXT=1; \
	fi; \
	cp $(BUILD)/marvin.iso "$$MNT/marvin_test_$$NEXT.iso"; \
	sync; \
	if [ "$$UNMOUNT" = "1" ]; then udisksctl unmount -b "$$USB_PART"; fi; \
	echo "Stick ready — marvin_test_$$NEXT.iso"