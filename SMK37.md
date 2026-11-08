# ZEN-1 on the M-Vave SMK-37

ZEN-1 also builds for the M-Vave SMK-37, a 37-key controller with the same chip as the FM-1. The FM-1 build is
unchanged; everything for the SMK-37 is behind `BOARD_SMK37`. On the SMK-37 it is the same ZEN-1: the same scenes,
moods, pages, koto instrument, screen and guide.

## Supported models

- **SMK-37 Pro**: the default build (package identity `SMK-37 Pro_460`; a release X.Y.Z is `SMK-37 Pro_46XXYZ`: the
  major version in two digits, the minor and the patch in one each, so 1.0.2 is `SMK-37 Pro_460102`).
- **SMK-37 Elite**: `MODEL=elite` (identity `SMK-37 Elite_460`).
- **MKE-P37**: `MODEL=mke` (identity `MKE-P37_460`).
- **Starrykey 37 Play**: uses the default build.

All of them are the same hardware. Each build carries its model's name, so M-Vave's updater still takes that
unit's stock package later: install the package for your model.

## Controls

The FM-1's controls keep their ZEN-1 meaning, so the guide applies; the screen keeps the FM-1's names (HOME, SEL,
FX, ...).

| SMK-37 | ZEN-1 |
| --- | --- |
| K1..K4 | KNOB 1..4 (the page's knobs) |
| K5 | MASTER (volume) |
| K6 / K7 / K8 | SELECT / ALGORITHM / PRESETS: Activity, the mood, the scene |
| Keys F3..G5 (the middle 27) | the FM-1's 27 keys |
| Keys C3..E3 and G#5..C6 | the same keys an octave in (C4..E4, G#4..C5) |
| 16 pads | the 16 white keys F3..G5, low notes on the bottom row |
| Pad lights | in a scene every pad dark orange; in the koto the tonics light blue; a key sounding or held bright. Dark with the lights off and while a scene sleeps |
| FX / SCALE | FX / SEL: the next / the previous knob page |
| < / > | the previous / the next knob page too (in the koto: HOME, SOUND, BEND, TUNING) |
| GLOBE | GLO: painterly or pixel art |
| SEQ PLAY | PLAY: a new picture |
| PATCH | HOME: tapped, the status card; held, the koto (held again, back) |
| SEQ | SEQ: the sleep timer |
| BT | SAVE (held 3 s: reset everything) |
| NOTE REPEAT / ARP / CHORD / PARA / SEQ REC | LFO / ARP / ENV / EDIT / REC |
| OCTAVE- / OCTAVE+ | OCT- / OCT+ |
| Faders 1..4 | a scene: its Volume page (Notes, Drone, Air, Water); the koto: its HOME page (余韻, trill rate, vibrato depth, reverb). A fader takes over only once it moves (also after switching between a scene and the koto) |
| Pitch wheel | the koto: up or down bends what sounds, as OCT+ / OCT- held. A scene: nothing |
| Mod wheel | the koto: vibrato while it is up. A scene: nothing |
| OCTAVE- + OCTAVE+ held 5 s | UPDATE MODE |
| OCTAVE- + OCTAVE+ held at power-on | HARDWARE CALIBRATION: teach the buttons and knobs again (it asks by the FM-1's names, KNOB 1-4 as K1-K4) |

KNOB BANK and FADER BANK have no use. The keys' and pads' velocity is read but not used (the FM-1's keys have none).
The SMK-37's TRS jack is a MIDI output and this build does not use it; MIDI comes in over USB only. The battery
reading on the status card uses the FM-1's scale.

## Build

See [BUILDING.md](BUILDING.md) for the toolchain.

```sh
BOARD=smk37 ./build.sh                 # build/zen1-smk37.fwsc (SMK-37 Pro, Starrykey 37 Play)
BOARD=smk37 MODEL=elite ./build.sh     # build/zen1-smk37-elite.fwsc
BOARD=smk37 MODEL=mke ./build.sh       # build/zen1-smk37-mke.fwsc
BOARD=smk37 ./build.sh --release 1.0.2 # build/zen-1-smk37-1.0.2.fwsc, identity SMK-37 Pro_460102
./build.sh                             # the FM-1 build again before tests/run_tests.sh (the tests use its files)
```

## Install

Connect the keyboard by USB and run, with the package for your model:

```sh
python3 tools/fm1_install.py build/zen1-smk37.fwsc
```

`fm1_install.py` needs `mido` and `python-rtmidi` (`pip3 install mido python-rtmidi`). It checks that the package
is for the connected keyboard.

## Going back to the stock firmware

Install M-Vave's firmware package for your model with M-Vave's updater, M-UPGRADE. It works directly, because
the installed package keeps the keyboard's model name.

## Safety

- Back up first: have M-Vave's stock package for your model and M-UPGRADE ready before you install.
- Keep the USB cable connected until the install has finished and the keyboard has restarted.
- ZEN-1 keeps its settings and save journal in the stock firmware's VM area (0x9D000-0xAFFFF). The first start
  clears the stock firmware's settings, and any other firmware's data, there.
- The firmware never writes the keyboard's calibration, Bluetooth pairing, stock key mapping (USRFLASH) or
  resources; the flash map refuses them at compile time and again in the flash driver.
- Installing other firmware is at your own risk.
