# Upstream and reference commits

ZEN-1 is built on FuMi-1, itself a fork of FoMni.

| Repository | Role | Commit | Date |
|---|---|---|---|
| https://github.com/cartesive/fumi-1 (FuMi-1) | **Base.** ZEN-1 started from FuMi-1 1.0.3; its instrument is kept as a mode | `8282ef9ac1bfd6307727780abf2835ee9927585d` | 2026-10-10 |
| https://github.com/charlesvestal/fm1-fomni (FoMni) | **Platform.** Platform code is kept byte-for-byte (see `tools/guard_platform.sh`) | `b81eb6b661c35948ec2d1059664d4e5790999305` | 2026-10-08 |
| https://github.com/charlesvestal/fm1-fomni (FoMni) | **The installers** (`tools/fm1_install.py`, `web/fm1ota.js`): back to stock from M-VAVE's FM1 Open firmware | `6c326230af03c66a77ba68e1af72cf3341e34208` | 2026-10-14 |
| https://github.com/cartesive/fumi-1 (FuMi-1) | Taken from 1.0.4-1.0.7: Kokyu, Hyoshigi, Mokugyo, the new Shakuhachi, the kotos' velocity, MIDI in velocity, fixes (not the looper or the trill's rework) | `7900e76` | 2026-10-13 |
| https://github.com/hugelton/Felucca | Reference and parts bin: FM6 engine (msfa port, Apache-2.0) | `f21d6d69e88e2e70908beaa97179fa8818ccae4e` (Felucca 1.4.1) | 2026-10-09 |

`tools/guard_platform.sh` diffs the protected paths against the FoMni commit above (the two installers against the later
one) and fails if they differ. One line
may differ: the USB product name in `firmware/src/usb.c` (`STR2`, "ZEN-1 FM-1" for FoMni's "FoMni FM-1", the same
length). It
needs that commit's objects: `git remote add fomni https://github.com/charlesvestal/fm1-fomni && git fetch fomni`.
The SMK-37 target (`SMK37.md`) changes some of the protected files; the guard takes each of those only as the version
it records (`SMK37_FILES`).

For reference, unmodified FoMni at that commit builds to `build/omni.fwsc`, 609657 B, identity `FM-1_800`, SHA-256
`dc0759191447442db64eabbbf25f7a1e1a2a7746501f01402a7ebd1cd1446723` (JieLi toolchain
`jieli-linux-toolchains-20260730.1`, SDK `AC79NN_SDK_V1.2.1_2023-12-13`).
