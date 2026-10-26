# Drivers — packaging, signing and honest limits

The specification asks the installer to set up the USB drivers so the user does
not hunt for them. This document states plainly what can and cannot be shipped,
and how the installer handles it, without weakening Windows security.

## What driver is needed

A Unisoc device in **download / Boot ROM mode** presents as a Spreadtrum USB
serial/diagnostic device (VID `0x1782`). Windows needs the matching driver so a
virtual **COM port** appears; this tool then talks the BSL protocol over that
port. Note:

* An ordinary USB-serial, ADB or MTP driver does **not** give access to the
  Spreadtrum download protocol. The download-mode driver is specific.
* The device may expose different identities for normal mode vs. Boot ROM vs.
  download mode; only the download/Boot ROM identity is relevant here.

## What this project does and does not ship

* This repository does **not** bundle any third-party driver binary. Driver
  packages (`.inf`/`.cat`/`.sys`) are redistributable only under their own
  license, and a `.cat` is only valid with a signature chain that Windows
  trusts.
* The installer (`installer/setup.iss`) is structured to install a driver
  package **if one is placed in `drivers/`** by whoever builds the installer and
  is legally entitled to redistribute it. It uses Windows' own mechanisms
  (`pnputil /add-driver … /install`) and never disables security.

## Hard rules the installer follows

* **Never** disable Driver Signature Enforcement or any other protection.
* Only install **signed** packages via `pnputil`; Windows rejects unsigned or
  untrusted packages, and the installer surfaces that error rather than working
  around it.
* **Never** replace an existing driver without a compatibility check.
* If a required driver is **not present** in `drivers/` (or is not legally
  distributable / not signed for this Windows version), the installer shows a
  clear message and lets the user proceed — the app still runs and can use a
  driver the user installs themselves — rather than bypassing anything.
* Request elevation only through the standard UAC prompt, and only for the
  driver step.
* Do not force a reboot unless Windows actually requires one.

## Where to get a legitimate driver

The driver ships with the device vendor's official flashing/upgrade package for
the specific model, or from the SoC vendor's official tooling. Obtain it from a
source you are authorised to use, confirm it is signed and matches your Windows
version/architecture, place the package under `drivers/`, and rebuild the
installer.

## Uninstall

Uninstalling removes the application's own files. A shared driver is removed
only if Windows reports it is unused by other devices/software, via the standard
uninstall path — never forcibly.

## Acceptance test (clean machine)

On a clean, compatible Windows 10/11 machine: after installing, connecting a
supported device in download mode should make a COM port appear and the app
should detect it with no manual driver steps — **only** when a valid, signed,
compatible driver package was included in `drivers/`. Record the models and
connection modes actually verified in `TESTED_DEVICES.md`.
