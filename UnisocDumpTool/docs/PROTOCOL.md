# Spreadtrum / Unisoc download protocol (as implemented here)

This file documents the protocol this tool speaks and the publicly available,
reverse-engineered sources it is based on. Nothing here is invented: the command
codes, framing and checksums below are the documented BSL (Boot Stage Loader)
protocol used by Spreadtrum/Unisoc Boot ROMs and FDL loaders.

> Everything in this tool must still be **validated against the specific chip**
> you target. See `MISSING.md`.

## 1. Physical/transport layer

The device, in download mode, enumerates as a USB device with vendor id
`0x1782` (Spreadtrum) and exposes a virtual COM port via its download driver.
The host talks 8N1 serial over that port. This tool opens it with overlapped
I/O and large buffers (`src/comm/serial.c`).

## 2. Framing (HDLC-like) — `src/protocol/hdlc.c`

Every message is delimited by the flag byte `0x7E`:

```
0x7E  <escaped message>  0x7E
```

Inside the frame two bytes are escaped so they cannot be confused with a flag:

```
0x7E -> 0x7D 0x5E
0x7D -> 0x7D 0x5D        (escaped byte = original XOR 0x20, preceded by 0x7D)
```

Some FDLs can disable transcoding; the session tracks this (`transcode`).

## 3. Message layout — `src/protocol/sprd_proto.c`

The bytes that get escaped are:

```
+--------+--------+-----------------+--------+
| type   | length | data[length]    | crc    |
| u16 BE | u16 BE |                 | u16 BE |
+--------+--------+-----------------+--------+
```

`crc` covers `type + length + data`.

## 4. Checksums — `src/common/crc16.c`

Two algorithms, selected by device phase:

* **Boot ROM phase:** CRC-16/CCITT, polynomial `0x1021`, MSB-first, init `0`
  (`spd_crc16`).
* **FDL phase (most loaders):** a 16-bit additive checksum — sum of
  little-endian 16-bit words, fold the carries, one's complement, byte-swap
  (`spd_fdl_crc`).

The exact point at which a loader switches checksum is loader-specific; the
code switches after FDL2 bring-up and documents the assumption.

## 5. Commands and responses

Host → device (`bsl_cmd`):

| code | name | payload |
|------|------|---------|
| 0x00 | CONNECT | — |
| 0x01 | START_DATA | addr(4 BE), size(4 BE) |
| 0x02 | MIDST_DATA | chunk bytes |
| 0x03 | END_DATA | — |
| 0x04 | EXEC_DATA | — (jump to downloaded loader) |
| 0x05 | NORMAL_RESET | — |
| 0x06 | READ_FLASH (linear) | addr(4), size(4), pad(4) |
| 0x09 | CHANGE_BAUD | baud |
| 0x10 | READ_FLASH_START | addr/part(4), total(4), offset(4) |
| 0x11 | READ_FLASH_MIDST | size(4), offset(4) |
| 0x12 | READ_FLASH_END | — |
| 0x13 | KEEP_CHARGE | — |

Device → host (`bsl_rep`):

| code | name |
|------|------|
| 0x80 | ACK |
| 0x81 | VER (version string; Boot ROM handshake reply) |
| 0x82 | INVALID_CMD |
| 0x83 | UNKNOWN_CMD |
| 0x84 | OPERATION_FAILED |
| 0x93 | READ_FLASH (carries the read-back bytes) |

(Additional download-status replies 0x85–0x8C are defined in the header.)

## 6. Flows

### Handshake
Send a run of `0x7E` flag bytes to let the Boot ROM synchronise; it answers
with a `VER` frame (or ACKs a following `CONNECT`). Boot ROM CRC is in effect.

### FDL bring-up (`src/protocol/fdl.c`)
Per image:
```
START_DATA{addr,size} -> ACK
MIDST_DATA{chunk}      -> ACK   (repeat)
END_DATA               -> ACK
EXEC_DATA              -> ACK   (device jumps into the loader)
```
Typically FDL1 first (initialises DRAM), then FDL2 (exposes flash read).

### Reading flash (`src/flash/flash_dump.c`)
* **Linear:** loop `READ_FLASH{base+off, n, 0}` → `READ_FLASH` reply with data.
* **Partitioned (FDL2):** `READ_FLASH_START{base,total,0}` → ACK, then loop
  `READ_FLASH_MIDST{n, off}` → `READ_FLASH` reply, then `READ_FLASH_END`.

Each returned chunk is streamed to disk and hashed; the read range is logged.
A short/zero read stops the loop and the dump is marked incomplete.

## 7. Sources

This implementation mirrors the behaviour documented by well-known open-source
Spreadtrum tools and community protocol write-ups, among them:

* `spreadtrum_flash` / `spd_dump` (ilyakurdyukov) — command set, framing, the
  two checksum variants, FDL staging and the read commands.
* `spdflashtool` / community PAC/FDL documentation — USB identity, FDL load
  addresses living inside each device's PAC.

Because FDL binaries and their exact load addresses are device-specific and
proprietary, they are **not** reproduced here; the tool consumes ones the user
supplies.
