# Development environment

What the build scripts expect, set up on macOS (Apple silicon). Linux x86-64 works too, without Docker.

| Need | Suggested place | Why |
|---|---|---|
| Python 3.10+ venv with Pillow (with raqm), numpy, scipy, mido, python-rtmidi | e.g. `~/.venvs/zen-1` | macOS's system Python's Pillow lacks raqm, which `tools/gen_font.py` needs. Put the venv's `bin` first on `PATH` (or set `PYTHON`) before building or testing |
| Emscripten | `~/emsdk` (`source ~/emsdk/emsdk_env.sh`) | The browser emulator and the audition bench. The official emsdk works; it needs Python 3.10+ on `PATH` |
| JieLi toolchain | `~/.jieli/toolchain` (from `tools/get_toolchain.sh`) | The firmware build, inside Docker (linux/amd64) on a Mac |
| JieLi AC79 SDK | `~/fw-AC79_AIoT_SDK` (tag `AC79NN_SDK_V1.2.1_2023-12-13`) | Three files go into every package. Set `AC79_SDK` for the update-loader tests |
| Docker | Docker Desktop, OrbStack or podman | `./build.sh` runs the toolchain in a `debian:bookworm-slim` amd64 container |
| Node.js | optional | The browser build's tests |

A shell for building:

```
export PATH=~/.venvs/zen-1/bin:$PATH
export AC79_SDK=~/fw-AC79_AIoT_SDK
source ~/emsdk/emsdk_env.sh
tests/run_tests.sh      # host tests, simulator scenarios, emulator
./build.sh              # build/zen1.fwsc (Docker)
```

Keep a copy of the toolchain tarball and the SDK checkout: if either download disappears the firmware cannot be
built.
