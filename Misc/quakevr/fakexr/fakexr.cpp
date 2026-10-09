// fakexr.cpp -- a fake OpenXR runtime for the headless tests of the runtime choice (vr_xr_runtime Auto, its fallback):
// the loader loads it from a manifest like any runtime. It has no headset: xrCreateInstance works (or fails, as asked)
// and xrGetSystem always fails, so the game goes on to the next runtime. Copies of the DLL under other names
// (fakexr_a.dll, fakexr_b.dll) are other runtimes: the name it reports is its file's.
//
//   FAKEXR_LOG            a file each copy appends to: loaded, instance made, instance destroyed, unloaded
//   FAKEXR_FAIL_INSTANCE  copies (file names without .dll, comma-separated) whose xrCreateInstance fails
//   FAKEXR_D3D11          copies that load d3d11.dll at xrCreateInstance and free it at xrDestroyInstance (as VDXR
//                         does), logging whether it is still loaded after (the game keeps it: vr_backend_openxr.cpp)
//
// Built by Misc/quakevr/fakexr/build.sh into <worktree>/scratch/fakexr (docs/vr-port/TESTING.md, "OpenXR runtime
// choice").

#include <windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{

char moduleName[64] = "fakexr";

void log(const char* what)
{
    char path[1024];
    if(GetEnvironmentVariableA("FAKEXR_LOG", path, sizeof(path)) == 0)
    {
        return;
    }
    FILE* f = fopen(path, "a");
    if(f)
    {
        fprintf(f, "%s %s\n", moduleName, what);
        fclose(f);
    }
}

bool listed(const char* variable)
{
    char list[1024];
    if(GetEnvironmentVariableA(variable, list, sizeof(list)) == 0)
    {
        return false;
    }
    for(char* token = strtok(list, ","); token; token = strtok(nullptr, ","))
    {
        if(!_stricmp(token, moduleName))
        {
            return true;
        }
    }
    return false;
}

HMODULE d3d11 = nullptr; // FAKEXR_D3D11

const XrInstance fakeInstance = reinterpret_cast<XrInstance>(static_cast<uintptr_t>(0x5eed));

XRAPI_ATTR XrResult XRAPI_CALL enumerateInstanceExtensionProperties(
    const char* layerName, uint32_t capacity, uint32_t* count, XrExtensionProperties* properties)
{
    if(layerName)
    {
        return XR_ERROR_API_LAYER_NOT_PRESENT;
    }
    *count = 1;
    if(capacity == 0)
    {
        return XR_SUCCESS;
    }
    strcpy(properties[0].extensionName, "XR_KHR_opengl_enable"); // (the game's graphics binding)
    properties[0].extensionVersion = 10;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL createInstance(const XrInstanceCreateInfo* /* info */, XrInstance* instance)
{
    if(listed("FAKEXR_FAIL_INSTANCE"))
    {
        log("xrCreateInstance failed (FAKEXR_FAIL_INSTANCE)");
        return XR_ERROR_RUNTIME_UNAVAILABLE;
    }
    log("xrCreateInstance");
    if(listed("FAKEXR_D3D11"))
    {
        d3d11 = LoadLibraryA("d3d11.dll");
        log(d3d11 ? "d3d11.dll loaded" : "d3d11.dll not loaded");
    }
    *instance = fakeInstance;
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL destroyInstance(XrInstance /* instance */)
{
    log("xrDestroyInstance");
    if(d3d11)
    {
        FreeLibrary(d3d11);
        d3d11 = nullptr;
        log(GetModuleHandleA("d3d11.dll") ? "d3d11.dll freed: still loaded" : "d3d11.dll freed: unloaded");
    }
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getInstanceProperties(XrInstance /* instance */, XrInstanceProperties* properties)
{
    properties->runtimeVersion = XR_MAKE_VERSION(1, 0, 0);
    _snprintf_s(properties->runtimeName, XR_MAX_RUNTIME_NAME_SIZE, _TRUNCATE, "FakeXR %s", moduleName);
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getSystem(XrInstance /* instance */, const XrSystemGetInfo* /* info */, XrSystemId* /* id */)
{
    log("xrGetSystem: no headset");
    return XR_ERROR_FORM_FACTOR_UNAVAILABLE;
}

XRAPI_ATTR XrResult XRAPI_CALL resultToString(XrInstance /* instance */, XrResult value, char buffer[XR_MAX_RESULT_STRING_SIZE])
{
    _snprintf_s(buffer, XR_MAX_RESULT_STRING_SIZE, _TRUNCATE, "XR_RESULT_%d", static_cast<int>(value));
    return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL getInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);

XRAPI_ATTR XrResult XRAPI_CALL unsupported()
{
    log("an unsupported function called");
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

struct Entry
{
    const char* name;
    PFN_xrVoidFunction function;
};

const Entry entries[] = {
    {"xrGetInstanceProcAddr", reinterpret_cast<PFN_xrVoidFunction>(getInstanceProcAddr)},
    {"xrEnumerateInstanceExtensionProperties", reinterpret_cast<PFN_xrVoidFunction>(enumerateInstanceExtensionProperties)},
    {"xrCreateInstance", reinterpret_cast<PFN_xrVoidFunction>(createInstance)},
    {"xrDestroyInstance", reinterpret_cast<PFN_xrVoidFunction>(destroyInstance)},
    {"xrGetInstanceProperties", reinterpret_cast<PFN_xrVoidFunction>(getInstanceProperties)},
    {"xrGetSystem", reinterpret_cast<PFN_xrVoidFunction>(getSystem)},
    {"xrResultToString", reinterpret_cast<PFN_xrVoidFunction>(resultToString)},
};

XRAPI_ATTR XrResult XRAPI_CALL getInstanceProcAddr(XrInstance /* instance */, const char* name, PFN_xrVoidFunction* function)
{
    for(const Entry& e : entries)
    {
        if(!strcmp(e.name, name))
        {
            *function = e.function;
            return XR_SUCCESS;
        }
    }
    // Any other function: one that fails (the loader wants every core function; the game calls none of them without
    // a system).
    *function = reinterpret_cast<PFN_xrVoidFunction>(unsupported);
    return XR_SUCCESS;
}

} // namespace

extern "C" __declspec(dllexport) XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
    const XrNegotiateLoaderInfo* loaderInfo, XrNegotiateRuntimeRequest* request)
{
    if(!loaderInfo || !request || loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_RUNTIME_VERSION)
    {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    request->runtimeInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
    request->runtimeApiVersion = XR_MAKE_VERSION(1, 0, 0);
    request->getInstanceProcAddr = getInstanceProcAddr;
    log("negotiated");
    return XR_SUCCESS;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID /* reserved */)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(module, path, sizeof(path));
        const char* base = strrchr(path, '\\');
        base = base ? base + 1 : path;
        strncpy_s(moduleName, base, _TRUNCATE);
        if(char* dot = strrchr(moduleName, '.'))
        {
            *dot = '\0';
        }
        log("loaded");
    }
    else if(reason == DLL_PROCESS_DETACH)
    {
        log("unloaded");
    }
    return TRUE;
}
