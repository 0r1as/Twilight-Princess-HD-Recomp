// Vulkan entry points loaded at run time (loader.h).
#include "loader.h"
#include <stdexcept>
#include <string>

namespace gfxvk {
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion = nullptr;
PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties = nullptr;
#define TPHD_VK_DEFINE(name) PFN_##name name = nullptr;
TPHD_VK_GLOBAL_FUNCTIONS(TPHD_VK_DEFINE)
TPHD_VK_INSTANCE_FUNCTIONS(TPHD_VK_DEFINE)
TPHD_VK_DEVICE_FUNCTIONS(TPHD_VK_DEFINE)
TPHD_VK_RENDERING_FUNCTIONS(TPHD_VK_DEFINE)
#undef TPHD_VK_DEFINE

namespace {
[[noreturn]] void missing(const char *name) {
  throw std::runtime_error(std::string("the Vulkan driver does not provide ") + name +
                           "; update the graphics driver");
}
}  // namespace

void load_global_functions(PFN_vkGetInstanceProcAddr gipa) {
  if (!gipa)
    throw std::runtime_error("the Vulkan loader has no vkGetInstanceProcAddr");
  vkGetInstanceProcAddr = gipa;
#define TPHD_VK_LOAD(name) \
  if (!(name = reinterpret_cast<PFN_##name>(gipa(nullptr, #name)))) missing(#name);
  TPHD_VK_GLOBAL_FUNCTIONS(TPHD_VK_LOAD)
#undef TPHD_VK_LOAD
  // Vulkan 1.1 loaders and newer; a 1.0 loader has none (the caller reports it)
  vkEnumerateInstanceVersion =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(gipa(nullptr, "vkEnumerateInstanceVersion"));
  // only for the log (the layers in the process); a loader without it is not refused
  vkEnumerateInstanceLayerProperties = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
      gipa(nullptr, "vkEnumerateInstanceLayerProperties"));
}

void load_instance_functions(VkInstance instance) {
#define TPHD_VK_LOAD(name) \
  if (!(name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name)))) missing(#name);
  TPHD_VK_INSTANCE_FUNCTIONS(TPHD_VK_LOAD)
#undef TPHD_VK_LOAD
}

void load_device_functions(VkDevice device, bool khrDynamicRendering) {
#define TPHD_VK_LOAD(name) \
  if (!(name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name)))) missing(#name);
  TPHD_VK_DEVICE_FUNCTIONS(TPHD_VK_LOAD)
#undef TPHD_VK_LOAD
  // the core 1.3 function or the extension's: same parameters (VkRenderingInfoKHR = VkRenderingInfo)
#define TPHD_VK_LOAD(name)                                                                          \
  {                                                                                                 \
    const char *n = khrDynamicRendering ? #name "KHR" : #name;                                     \
    if (!(name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, n)))) missing(n);        \
  }
  TPHD_VK_RENDERING_FUNCTIONS(TPHD_VK_LOAD)
#undef TPHD_VK_LOAD
}
}  // namespace gfxvk
