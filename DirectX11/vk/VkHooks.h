#pragma once

#include <windows.h>

// A Vulkan game gets its functions from vulkan-1.dll, the loader: either its
// exports, which vkGetInstanceProcAddr hands out as well, or what
// vkGetDeviceProcAddr returns, which leads past those exports straight into
// the first layer or the driver. Both are covered here, the exports with code
// hooks and the latter by answering vkGetDeviceProcAddr with our functions.
//
// Registering as a layer of the loader would be the documented way in, but
// that is done in the registry for every Vulkan program on the machine, or
// with environment variables the loader ignores in an elevated process.
//
// Vulkan handles are not wrapped: the game keeps the loader's, and our
// functions find where to continue from the dispatch table pointer the loader
// keeps at the start of every device, queue and command buffer.

// Hooks the vulkan-1.dll exports we need if the DLL is in the process and
// they are not hooked yet, or no longer: it may have been unloaded and loaded
// again since. Called from DllMain and from the LoadLibraryExW hook, so it
// has to stay within what is permitted under the loader lock.
void VkHookExports();
// The Vulkan loader, or NULL when it is not in the process. A module named
// vulkan-1.dll can be a proxy of it instead: EFF puts one next to the game
// whose exports forward to "C:\Windows\System32\vulkan-1.vkXxx". The
// proxy is what is found by name, the hook library cannot follow forwarders
// given with a path, and the game's calls go to the real loader regardless,
// so the loader is found where the proxy's vkGetInstanceProcAddr leads. With
// reference, the caller is given a reference it has to FreeLibrary.
HMODULE VkLoaderModule(bool reference = false);
// Releases deferred Vulkan destroys when a D3D11 present rules out its mod path.
void VkDisableModsForD3D11();
// For DLL_PROCESS_DETACH: no more being told of vulkan-1.dll being unloaded
void VkUnwatchUnloads();
