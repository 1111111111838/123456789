# Third-party components, dependencies and licenses

## Build/runtime dependencies

* **Win32 API / Common Controls v6** — part of Windows; linked via
  `comctl32`, `setupapi`, `shell32`, `ole32`, `uuid`. No redistribution needed.
* **C standard library** — from the toolchain (MSVC CRT or MinGW).
* **CMake** (build-time only).
* **Inno Setup** (optional, to build the installer) — distributed under its own
  license; not bundled here.

This project does **not** statically or dynamically bundle any third-party
library in source form. All non-trivial code here is original to the project.

## SHA-256

`src/common/sha256.c` is an original, from-scratch implementation of FIPS 180-4
SHA-256, placed in the **public domain** by this project. No external SHA code
was copied.

## Protocol knowledge

The Spreadtrum/Unisoc BSL protocol details (`src/protocol/*`, `docs/PROTOCOL.md`)
are based on publicly available, community reverse-engineered documentation and
open-source tools (e.g. `spreadtrum_flash`/`spd_dump`). Only the *protocol
behaviour* (command codes, framing, checksums) is reproduced — no third-party
source code is copied into this project. Those projects are typically available
under permissive licenses; consult them directly if you reuse their code.

## Drivers

No driver binaries are included. Any driver package added under `drivers/` is
governed by that package's own license and signature — see `docs/DRIVERS.md`.

## This project's license

See `LICENSE` at the repository root.
