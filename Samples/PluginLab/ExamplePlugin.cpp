#include "RuntimePluginABI.h"
#include <cstring>
#include <limits>

// All exceptions and allocator ownership stay inside the native extension.
// Check before multiplication: signed integer overflow is undefined behavior.
extern "C" int32_t DoubleValue(int32_t input,int32_t* output)
{
    if(!output||input>std::numeric_limits<int32_t>::max()/2||input<std::numeric_limits<int32_t>::min()/2)return 1;
    *output=input*2;return 0;
}
extern "C" KAIRO_PLUGIN_EXPORT int32_t kairo_plugin_query(uint32_t abi,uint32_t size,KairoPluginV1* out)
{
    if(!out||abi!=KAIRO_PLUGIN_ABI_VERSION||size!=sizeof(KairoPluginV1))return 1;
    KairoPluginV1 result{};result.struct_size=sizeof(result);
#ifdef KAIRO_PLUGIN_BAD_ABI
    result.abi_version=2;
#else
    result.abi_version=KAIRO_PLUGIN_ABI_VERSION;
#endif
    std::memcpy(result.id,"double-value",sizeof("double-value"));result.action=&DoubleValue;
    *out=result;return 0;
}
