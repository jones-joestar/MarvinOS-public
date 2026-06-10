# Third-Party Notices

MarvinOS as a whole is licensed under the GNU General Public License, version 2
(see the top-level `LICENSE` file).

It additionally incorporates third-party components listed below. Each is the
copyright of its respective authors and is used under the terms of its own
license. The notices below are reproduced to satisfy those licenses; they do
not change the license of MarvinOS's own source code.

---

## 1. doomgeneric — GPL-2.0

- Component: `doomgeneric/` and the Doom source files built from it
  (`d_main.c`, `g_game.c`, `p_*.c`, `r_*.c`, etc.), plus the MarvinOS platform
  backend `doomgeneric_marvinos.c` and `snd_mixer.c`.
- Source: https://github.com/ozkl/doomgeneric (a fork of `maximevince/fbDOOM`,
  which is derived from the id Software DOOM source release; the GitHub network
  root of the fork is `id-Software/DOOM`).
- License: GNU General Public License, version 2 (GPL-2.0).

doomgeneric is derived from the DOOM source code. The Doom source files carry
id Software's original copyright notice:

    Copyright (C) 1993-1996 id Software, Inc.

(id Software re-licensed the Doom source code under the GNU GPL, version 2, on
October 3, 1999; that is the license under which doomgeneric is distributed.)

The Doom-related binaries in this project (`rootfs/bin/doom`) are a derivative
work of GPL-2.0 code and are distributed under the GPL-2.0. The full text of
the GNU General Public License, version 2, is included in this repository as
`LICENSES/GPL-2.0.txt` (copy it verbatim from doomgeneric's `LICENSE` file).

If you distribute the Doom binary, you must also make the corresponding source
code available under the same terms.

NOTE: `doom1.wad` is game *data*, not part of this project, and is not
redistributed here. It is governed by its own license from id Software.

---

## 2. gnu-efi — BSD-style

- Component: linked into the UEFI bootloader (`build/main.efi` /
  `BOOTX64.EFI`) via `libefi.a`, `libgnuefi.a`, and `crt0-efi-x86_64.o`.
- Source: https://sourceforge.net/projects/gnu-efi/
  (built against the Arch Linux `gnu-efi` package).
- License: BSD-style (permissive), with the `gnuefi` glue additionally
  available under the GPL.

gnu-efi is assembled from code under more than one permissive notice. The two
notices relevant to the parts statically linked into the bootloader are
reproduced below verbatim from the upstream source.

### 2a. `inc/` and `lib/` (linked as `libefi.a`)

From gnu-efi's `README.efilib`:

    The files in the "lib" and "inc" subdirectories are using the EFI Application
    Toolkit distributed by Intel at http://developer.intel.com/technology/efi

    This code is covered by the following agreement:

    Copyright (c) 1998-2000 Intel Corporation

    Redistribution and use in source and binary forms, with or without modification, are permitted
    provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this list of conditions and
    the following disclaimer.

    Redistributions in binary form must reproduce the above copyright notice, this list of conditions
    and the following disclaimer in the documentation and/or other materials provided with the
    distribution.

    THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
    FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE. THE EFI SPECIFICATION AND ALL OTHER INFORMATION
    ON THIS WEB SITE ARE PROVIDED "AS IS" WITH NO WARRANTIES, AND ARE SUBJECT
    TO CHANGE WITHOUT NOTICE.

### 2b. `gnuefi/` glue, incl. `crt0-efi-x86_64.o` (linked as `libgnuefi.a` + `crt0`)

The `gnuefi` glue (startup code and self-relocator) was contributed by
Hewlett-Packard and is dual-licensed (3-clause BSD or GPL). The header of
`gnuefi/crt0-efi-x86_64.S`, the startup object linked into the bootloader,
reads:

    Copyright (C) 1999 Hewlett-Packard Co.
        Contributed by David Mosberger <davidm@hpl.hp.com>.
    Copyright (C) 2005 Intel Co.
        Contributed by Fenghua Yu <fenghua.yu@intel.com>.

    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials
      provided with the distribution.
    * Neither the name of Hewlett-Packard Co. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

(The other `gnuefi/` source files carry the same notice; some files also bear a
`Copyright (c) 2014 Linaro Ltd.` notice for ARM-only code, which is not linked
into the x86-64 bootloader.)

---

## 3. Tetris game engine — MIT

- Component: `src/user/tetris/tetris.c` and `src/user/tetris/tetris.h`
  (the platform-agnostic game logic). The MarvinOS display/input driver
  `src/user/tetris/marvinos_tetris.c` is MarvinOS's own code.
- Author: Jacob Bokor
- Source: https://github.com/0xjmux/tetris
- License: MIT.

    MIT License

    Copyright (c) 2024 Jacob Bokor

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
