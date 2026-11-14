// A stand-in for the vulkan-1.dll proxy EFF puts next to the game: nothing
// but exports forwarding to the system loader by its full path.
#pragma comment(linker, "/export:vkGetInstanceProcAddr=C:\\Windows\\System32\\vulkan-1.vkGetInstanceProcAddr")
#pragma comment(linker, "/export:vkGetDeviceProcAddr=C:\\Windows\\System32\\vulkan-1.vkGetDeviceProcAddr")
#pragma comment(linker, "/export:vkCreateInstance=C:\\Windows\\System32\\vulkan-1.vkCreateInstance")
#pragma comment(linker, "/export:vkDestroyInstance=C:\\Windows\\System32\\vulkan-1.vkDestroyInstance")
#pragma comment(linker, "/export:vkCreateDevice=C:\\Windows\\System32\\vulkan-1.vkCreateDevice")
#pragma comment(linker, "/export:vkEnumerateInstanceExtensionProperties=C:\\Windows\\System32\\vulkan-1.vkEnumerateInstanceExtensionProperties")
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) { return TRUE; }
