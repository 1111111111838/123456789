# Tested devices

Honest status: **no physical device has been validated yet** in this
environment. The engine has been verified end-to-end against the in-process
simulator (byte-exact dump + SHA-256) and by unit tests, and the full Windows
GUI build compiles cleanly, but real-hardware confirmation is pending.

When a device is tested on real hardware, add a row here and (only then) raise
its `capability` in `src/detect/chipdb.c` to `CAP_SUPPORTED`.

## Template

| date | chip | model | mode | driver (signed?) | result | notes |
|------|------|-------|------|------------------|--------|-------|
| YYYY-MM-DD | SC6531E | <phone> | Boot ROM / download | <driver + version> | full / partial / fail | FDL used, flash size, SHA-256, anomalies |

For each real test, record:

* the exact FDL1/FDL2 files and load addresses used (from the PAC),
* the flash size and whether the full range read back,
* the dump's SHA-256 and whether a re-dump reproduced it,
* any short reads / bad blocks / timeouts and how they appeared,
* the driver package and whether it installed without manual steps.
