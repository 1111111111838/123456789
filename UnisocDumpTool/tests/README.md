# Tests

`test_runner.c` exercises the portable core (no Windows, no hardware):

* **SHA-256** against FIPS 180-4 known-answer vectors, including streaming.
* **Checksums** (`spd_crc16`, `spd_fdl_crc`) for determinism and sensitivity.
* **HDLC framing** round-trip, including escaping of `0x7E`/`0x7D`.
* **Protocol loopback** against the simulator: handshake, version, a linear
  read compared byte-for-byte against the synthetic flash model.
* **A full simulated dump** through the real read engine + file writer, with
  the written `.bin` verified byte-exact against the model.
* **FDL gating**: reads refused until an FDL is "executed", then allowed.

## Run

```
cmake -S .. -B ../build
cmake --build ../build --target udt_tests
ctest --test-dir ../build --output-on-failure
```

Or directly with any C compiler:

```
cc -std=c11 -I.. tests/test_runner.c \
   src/common/*.c src/comm/transport.c src/protocol/*.c \
   src/detect/chipdb.c src/detect/device_detect.c src/sim/sim_device.c \
   src/fileout/dump_writer.c src/flash/flash_dump.c -o udt_tests
./udt_tests
```

Set `UDT_TEST_VERBOSE=1` to see engine log output during the tests.

## Not covered here

The Win32 GUI, the real serial transport and SetupAPI detection are
Windows-only and require interactive/hardware validation; see
`docs/TESTED_DEVICES.md`.
