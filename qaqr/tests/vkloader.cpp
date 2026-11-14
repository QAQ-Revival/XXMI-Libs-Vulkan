// Offline probe for a vulkan-1.dll that is a proxy of the loader, as EFF
// installs one: its exports forward to "C:\Windows\System32\vulkan-1.vkXxx",
// so the loader's exports have to be hooked in the module they lead to, not
// in the one found by name. Runs the real DLL; the Vulkan call needs no GPU,
// only that the system loader is there.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
namespace fs = std::filesystem;
static void require(bool ok, const std::string& what) { if (!ok) throw std::runtime_error(what); }
static bool logHas(const fs::path& log, const std::string& needle) {
    std::ifstream f(log); std::string line;
    while (std::getline(f, line)) if (line.find(needle) != std::string::npos) return true;
    return false;
}
static std::string pointer(const void* at) { char text[32]; snprintf(text, sizeof(text), "%p", at); return text; }
// Just what vkCreateInstance reads of it, as in vulkan_core.h
struct InstanceCreateInfo {
    int sType; const void* pNext; unsigned flags; const void* pApplicationInfo;
    unsigned enabledLayerCount; const char* const* ppEnabledLayerNames;
    unsigned enabledExtensionCount; const char* const* ppEnabledExtensionNames;
};
typedef int (__stdcall *CreateInstance)(const InstanceCreateInfo*, const void*, void**);
typedef void (__stdcall *DestroyInstance)(void*, const void*);
int wmain(int argc, wchar_t** argv) {
    try {
        require(argc >= 3, "Usage: vkloader.exe DLL PROXY");
        const auto work = fs::current_path();
        for (auto ancestor = work;; ancestor = ancestor.parent_path()) {
            require(lstrcmpiW(ancestor.filename().c_str(), L"Mods") != 0 && lstrcmpiW(ancestor.filename().c_str(), L"Core") != 0 &&
                    lstrcmpiW(ancestor.filename().c_str(), L"ShaderFixes") != 0 && lstrcmpiW(ancestor.filename().c_str(), L"ShaderCache") != 0 &&
                    !fs::exists(ancestor / "d3dx.ini"), "Test outputs must be outside live importer and Mod directories");
            if (ancestor == ancestor.parent_path()) break;
        }
        wchar_t system[MAX_PATH]; GetSystemDirectoryW(system, MAX_PATH);
        if (!fs::exists(fs::path(system) / L"vulkan-1.dll")) { std::cout << "SKIP No Vulkan loader on this machine\n"; return 0; }
        require(GetModuleHandleW(L"vulkan-1.dll") == nullptr, "vulkan-1.dll is in the process before the test loads it");
        fs::path dir = work / ("vkloader-test-" + std::to_string(GetCurrentProcessId()));
        fs::create_directories(dir);
        fs::copy_file(fs::absolute(argv[1]), dir / "d3d11.dll", fs::copy_options::overwrite_existing);
        fs::copy_file(fs::absolute(argv[2]), dir / "vulkan-1.dll", fs::copy_options::overwrite_existing);
        {
            std::ofstream ini(dir / "d3dx.ini");
            ini << "[System]\ncheck_foreground_window = 0\n[Hunting]\nhunting = 0\n"
                   "[Logging]\nlog_level = debug\nunbuffered = 1\nshow_warnings = 0\n[Rendering]\nextended_rendering = 1\ncache_shaders = 0\n";
        }
        HMODULE dll = LoadLibraryW((dir / L"d3d11.dll").c_str()); require(dll != nullptr, "Load XXMI DLL");
        const fs::path log = dir / "d3d11_log.txt";
        // The config and the log are only read and opened by the first D3D11 call
        auto createDevice = (decltype(&D3D11CreateDevice))GetProcAddress(dll, "D3D11CreateDevice"); require(createDevice != nullptr, "Find create export");
        ID3D11Device* device = nullptr; ID3D11DeviceContext* context = nullptr; D3D_FEATURE_LEVEL level;
        require(SUCCEEDED(createDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context)), "Create WARP through XXMI");

        // As the game loads it: by a path that finds the proxy first
        HMODULE proxy = LoadLibraryExW((dir / L"vulkan-1.dll").c_str(), nullptr, 0); require(proxy != nullptr, "Load the proxy");
        auto create = (CreateInstance)GetProcAddress(proxy, "vkCreateInstance"); require(create != nullptr, "The proxy's vkCreateInstance");
        HMODULE real = nullptr;
        require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)create, &real) && real != proxy,
                "The proxy's vkCreateInstance does not lead into another module");
        wchar_t realPath[MAX_PATH]; GetModuleFileNameW(real, realPath, MAX_PATH);
        require(!lstrcmpiW(fs::path(realPath).filename().c_str(), L"vulkan-1.dll"), "The proxy forwards to something other than the loader");
        require(GetModuleHandleW(L"vulkan-1.dll") != nullptr, "No vulkan-1.dll by name");

        require(logHas(log, "forwards to the Vulkan loader at " + pointer(real)), "The proxy was not followed to the loader at " + pointer(real));
        require(logHas(log, "Hooked 3 exports of vulkan-1.dll at " + pointer(real) + " for instances and devices, 0 failed"),
                "The loader's exports were not hooked at " + pointer(real));

        // A call the game makes through the proxy reaches the hook
        InstanceCreateInfo info = {}; info.sType = 1;
        void* instance = nullptr;
        int result = create(&info, nullptr, &instance);
        require(logHas(log, "vkCreateInstance, 0 extensions"), "vkCreateInstance through the proxy did not reach the hook");
        if (result == 0 && instance) {
            auto destroy = (DestroyInstance)GetProcAddress(proxy, "vkDestroyInstance");
            if (destroy) destroy(instance, nullptr);
        }
        context->Release(); device->Release();
        std::cout << "RESULT {\"pass\":true,\"proxy\":\"" << pointer(proxy) << "\",\"loader\":\"" << pointer(real)
                  << "\",\"vkCreateInstance\":" << result << ",\"actual_dll_loaded\":true}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
