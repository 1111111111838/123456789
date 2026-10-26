/*
 * sim_device.h - In-process simulated Unisoc device (TEST MODE ONLY).
 *
 * This provides a udt_transport that speaks the real BSL protocol on the device
 * side against a synthetic, deterministic flash image. It exists so the whole
 * pipeline - framing, FDL staging, the read loop, chunking, hashing and file
 * output - can be exercised and unit-tested without hardware.
 *
 * CRITICAL SEPARATION (per specification): the simulator is never used on a
 * real dump. Its transport name begins with "SIM:" and every dump produced
 * through it is labelled SIMULATED in the manifest. Code paths that require a
 * real dump check for this and refuse to mislabel simulated data as real.
 */
#ifndef UDT_SIM_DEVICE_H
#define UDT_SIM_DEVICE_H

#include <stdint.h>

#include "../comm/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *chip_name;   /* reported version/chip string */
    uint32_t    flash_size;  /* synthetic flash size in bytes */
    int         require_fdl; /* 1 = refuse reads until an FDL is "executed" */
} sim_config;

/* Open a simulated device. Release with t->close(t). */
udt_status udt_sim_open(const sim_config *cfg, udt_transport **out);

/* 1 if this transport is the simulator (name starts with "SIM:"). */
int udt_transport_is_sim(const udt_transport *t);

/* Deterministic synthetic flash byte at absolute address - used by tests to
 * verify a simulated dump read back the expected content. */
uint8_t sim_flash_byte(uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif /* UDT_SIM_DEVICE_H */
