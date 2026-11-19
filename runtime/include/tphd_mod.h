/* TPHD recomp frame-mod ABI v1. Plain C; game bytes are never part of the SDK. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TPHD_MOD_ABI_VERSION 1
#if defined(_WIN32)
#define TPHD_MOD_EXPORT __declspec(dllexport)
#else
#define TPHD_MOD_EXPORT __attribute__((visibility("default")))
#endif
typedef struct TPHDModHostV1 {
    uint32_t size, abi_version;
    void* context;
    const char* game_id;
    const char* package_dir;
    /* Called only from the game-thread callbacks. Bytes use guest big-endian order. */
    int (*read_guest)(void*, uint32_t address, void* output, size_t size);
    int (*write_guest)(void*, uint32_t address, const void* input, size_t size);
    void (*log)(void*, const char* message);
    void (*status)(void*, const char* message);
    const char* (*get_string)(void*, const char* option_id);
    double (*get_number)(void*, const char* option_id);
    int (*get_bool)(void*, const char* option_id);
} TPHDModHostV1;
typedef struct TPHDModV1 {
    uint32_t size, abi_version;
    void* instance;
    /* Once per original logic step, after actor execution. No callback on interpolated draws. */
    void (*on_frame)(void*, uint64_t logic_step);
    void (*on_config_changed)(void*);
    /* Runs after the last frame callback, before the library unloads. */
    void (*on_unload)(void*);
} TPHDModV1;
typedef int (*TPHDModInitV1)(const TPHDModHostV1*, TPHDModV1*);
/* Each native mod exports this entry point and returns nonzero on successful init. */
TPHD_MOD_EXPORT int tphd_mod_init_v1(const TPHDModHostV1*, TPHDModV1*);
#ifdef __cplusplus
}
#endif
