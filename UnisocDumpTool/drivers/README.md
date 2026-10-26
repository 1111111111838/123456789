# drivers/

Place the **signed** Unisoc / Spreadtrum download-mode driver package here
(`.inf`, `.cat`, `.sys`) before building the installer, if you are legally
entitled to redistribute it.

* No driver binaries are included in this repository on purpose (license +
  signature constraints). See `../docs/DRIVERS.md` for the full rationale and
  the hard security rules.
* The installer (`../installer/setup.iss`) installs any `*.inf` found here using
  Windows' own `pnputil /add-driver … /install`. Windows installs only trusted
  (signed) packages and refuses the rest; the installer never disables Driver
  Signature Enforcement.
* If this folder has no `.inf`, the installer still succeeds and the app still
  runs — it simply tells the user to install a driver themselves.

Required identity: the Spreadtrum download/Boot ROM device (USB VID `0x1782`).
A plain USB-serial / ADB / MTP driver is **not** a substitute.
