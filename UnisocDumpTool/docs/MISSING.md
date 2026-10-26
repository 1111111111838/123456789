# What is missing to complete the read engine per chip

The specification requires that where a protocol is not fully known, the tool
states **exactly** what is missing instead of pretending. This is that list.

## The core gap: FDL images

The BSL protocol itself is implemented (see `PROTOCOL.md`). What the protocol
*cannot* supply by itself, on almost every Unisoc chip, is the ability to read
flash from the Boot ROM alone. That ability lives in the chip-specific **FDL**
loaders:

* **FDL1** — small loader that runs from Boot ROM and brings up DRAM.
* **FDL2** — larger loader that runs from DRAM and exposes the flash read/write
  commands (`READ_FLASH_START/MIDST/END`).

FDL1/FDL2 are:

1. **Proprietary** — they ship inside each device's flashing package (PAC) and
   are not redistributable here.
2. **Chip- and often model-specific** — an FDL for one SoC/flash combination
   will not work on another.
3. **Loaded at specific addresses** — the load addresses also come from the PAC
   (the `fdl1_addr` / `fdl2_addr` values).

So, to complete a real read for a given device you must supply, via
`UnisocDumpTool.ini`:

| value | where it comes from |
|-------|---------------------|
| `fdl1` + `fdl1_addr` | extracted from the device's PAC |
| `fdl2` + `fdl2_addr` | extracted from the device's PAC |
| `size` (flash size)  | the device's spec / PAC partition table |
| `base_addr`          | 0 for a full linear dump, or a partition base |

### How to obtain these from a PAC
A PAC is a container of the firmware images plus an XML describing each file's
name, type (FDL1/FDL2/…) and load/run address. Use a PAC
unpacker (e.g. community `pacextractor`/`unpac` tools) to extract the FDL images
and read the addresses from the XML. Point the INI at those files/addresses.

## Per-chip status

| chip | protocol path | what is still needed |
|------|---------------|----------------------|
| SC6531E  | linear + FDL2 implemented | FDL1/FDL2 + addresses from PAC; hardware validation |
| SC6531DA | linear + FDL2 implemented | FDL1/FDL2 + addresses; hardware validation |
| SC6530   | linear implemented | FDL + addresses; some variants may need legacy quirks |
| SC7731   | FDL2 implemented | FDL1/FDL2 + eMMC partition table; hardware validation |
| SC9832E  | FDL2 implemented | FDL1/FDL2 + eMMC partition table; hardware validation |
| other Unisoc | generic BSL | add a `chipdb` row; confirm command quirks + FDL |

## Secondary unknowns to verify on real hardware

* The exact moment a specific FDL switches to the additive checksum (the code
  switches after FDL2 bring-up — confirm per loader).
* Whether a given loader ACKs `EXEC_DATA` or jumps silently (handled as a soft
  success on timeout — confirm).
* Maximum safe read chunk size per loader.
* For NAND/SPI-NAND: whether reads include spare/OOB bytes, and how bad blocks
  are represented in the returned stream.
* The precise `READ_FLASH_START` argument semantics for partition-by-id vs.
  absolute address on newer FDL2 builds.

Until a chip is confirmed on real hardware (`TESTED_DEVICES.md`), its capability
is reported as **unverified**, never "supported".
