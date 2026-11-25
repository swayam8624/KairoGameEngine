#ifndef KAIRO_RUNTIME_PLUGIN_ABI_H
#define KAIRO_RUNTIME_PLUGIN_ABI_H
#include <stdint.h>

/* A small C ABI: no C++ objects, allocation ownership, exceptions or STL
 * cross the library boundary. Both host and plugin must use the same ABI.
 * Native plugins run with the host's privileges; this ABI is not a sandbox. */
#define KAIRO_PLUGIN_ABI_VERSION 1u
#if defined(_WIN32)
#define KAIRO_PLUGIN_EXPORT __declspec(dllexport)
#else
#define KAIRO_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*KairoPluginAction)(int32_t input, int32_t* output);
typedef struct KairoPluginV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    char id[64];
    KairoPluginAction action;
} KairoPluginV1;
/* Query writes only when requested ABI and output size are compatible.
 * Zero is success; nonzero is rejection. The action uses caller-owned output. */
typedef int32_t (*KairoPluginQuery)(uint32_t abi, uint32_t size, KairoPluginV1* out);
#ifdef __cplusplus
}
#endif
#endif
