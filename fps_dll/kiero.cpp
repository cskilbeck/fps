//////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <d3d9.h>
#include <dxgi.h>
#include <d3d10_1.h>
#include <d3d10.h>
#include <d3d11.h>
#include <utility>

#include "minhook/include/MinHook.h"
#include "kiero.h"
#include "fps_dll.h"
#include "defer.h"

namespace {

//////////////////////////////////////////////////////////////////////

using kiero::Status;
using kiero::DXType;

//////////////////////////////////////////////////////////////////////

typedef LPDIRECT3D9(__stdcall *Direct3DCreate9_FN)(uint32_t);

typedef long(__stdcall *D3D11CreateDeviceAndSwapChain_FN)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT, const DXGI_SWAP_CHAIN_DESC *,
                                                          IDXGISwapChain **, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **);

//////////////////////////////////////////////////////////////////////
// See METHODSTABLE.txt for more information

uint_ptr *g_methodsTable[DXType::dx_num_types] = {};

//////////////////////////////////////////////////////////////////////

Status open_d3d9()
{
    auto return_status = Status::UnknownError;

    WNDCLASSEX windowClass;
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = DefWindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = GetModuleHandle(null);
    windowClass.hIcon = null;
    windowClass.hCursor = null;
    windowClass.hbrBackground = null;
    windowClass.lpszMenuName = null;
    windowClass.lpszClassName = "Kiero";
    windowClass.hIconSm = null;

    if(!RegisterClassEx(&windowClass)) {
        return Status::RegisterClassFailed;
    }
    defer(UnregisterClass(windowClass.lpszClassName, windowClass.hInstance));

    HWND window = CreateWindow(windowClass.lpszClassName, "Kiero DirectX Window", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, null, null, windowClass.hInstance, null);
    if(window == null) {
        return Status::CreateWindowFailed;
    }
    defer(DestroyWindow(window));

    LPDIRECT3D9 direct3D9 = null;
    LPDIRECT3DDEVICE9 device9 = null;
    Direct3DCreate9_FN Direct3DCreate9 = null;

    HMODULE libD3D9 = GetModuleHandle("d3d9.dll");
    if(libD3D9 == null) {
        return Status::ModuleNotFoundError;
    }

    Direct3DCreate9 = reinterpret_cast<Direct3DCreate9_FN>(GetProcAddress(libD3D9, "Direct3DCreate9"));
    if(Direct3DCreate9 == null) {
        return Status::FunctionNotFound;
    }

    direct3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if(direct3D9 == null) {
        return Status::CreateD3DFailed;
    }
    defer(direct3D9->Release());

    D3DDISPLAYMODE displayMode;
    if(FAILED(direct3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode))) {
        return Status::GetDisplayModeFailed;
    }

    D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth = 0;
    params.BackBufferHeight = 0;
    params.BackBufferFormat = displayMode.Format;
    params.BackBufferCount = 0;
    params.MultiSampleType = D3DMULTISAMPLE_NONE;
    params.MultiSampleQuality = 0;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = window;
    params.Windowed = 1;
    params.EnableAutoDepthStencil = 0;
    params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    params.Flags = 0;
    params.FullScreen_RefreshRateInHz = 0;
    params.PresentationInterval = 0;

    if(FAILED(direct3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &params, &device9))) {
        return Status::CreateDeviceFailed;
    }
    defer(device9->Release());

    g_methodsTable[DXType::dx9] = new uint_ptr[119];
    memcpy(g_methodsTable[DXType::dx9], *(uint_ptr **)device9, 119 * sizeof(uint_ptr));
    return Status::Success;
}

//////////////////////////////////////////////////////////////////////

Status open_d3d11()
{
    HMODULE libD3D11 = null;

    WNDCLASSEX windowClass;
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = DefWindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = GetModuleHandle(null);
    windowClass.hIcon = null;
    windowClass.hCursor = null;
    windowClass.hbrBackground = null;
    windowClass.lpszMenuName = null;
    windowClass.lpszClassName = "Kiero";
    windowClass.hIconSm = null;

    if(!RegisterClassEx(&windowClass)) {
        return Status::RegisterClassFailed;
    }
    defer(UnregisterClass(windowClass.lpszClassName, windowClass.hInstance));

    HWND window = CreateWindow(windowClass.lpszClassName, "Kiero DirectX Window", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, null, null, windowClass.hInstance, null);
    if(window == null) {
        return Status::CreateWindowFailed;
    }
    defer(DestroyWindow(window));

    D3D11CreateDeviceAndSwapChain_FN D3D11CreateDeviceAndSwapChain_PTR = null;
    IDXGISwapChain *swapChain = null;
    ID3D11Device *device11 = null;
    ID3D11DeviceContext *context = null;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1 };

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    if((libD3D11 = GetModuleHandle("d3d11.dll")) == null) {
        return Status::ModuleNotFoundError;
    }

    if((D3D11CreateDeviceAndSwapChain_PTR = reinterpret_cast<D3D11CreateDeviceAndSwapChain_FN>(GetProcAddress(libD3D11, "D3D11CreateDeviceAndSwapChain"))) == null) {
        return Status::FunctionNotFound;
    }

    DXGI_RATIONAL refreshRate;
    refreshRate.Numerator = 60;
    refreshRate.Denominator = 1;

    DXGI_MODE_DESC bufferDesc;
    bufferDesc.Width = 100;
    bufferDesc.Height = 100;
    bufferDesc.RefreshRate = refreshRate;
    bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    DXGI_SAMPLE_DESC sampleDesc;
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChainDesc.BufferDesc = bufferDesc;
    swapChainDesc.SampleDesc = sampleDesc;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 1;
    swapChainDesc.OutputWindow = window;
    swapChainDesc.Windowed = 1;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    if(FAILED(D3D11CreateDeviceAndSwapChain_PTR(null, D3D_DRIVER_TYPE_HARDWARE, null, 0, featureLevels, _countof(featureLevels), D3D11_SDK_VERSION, &swapChainDesc, &swapChain,
                                                &device11, &featureLevel, &context))) {
        return Status::CreateDeviceFailed;
    }
    scoped[=]
    {
        swapChain->Release();
        device11->Release();
        context->Release();
    };

    g_methodsTable[DXType::dx11] = new uint_ptr[205];

    memcpy(g_methodsTable[DXType::dx11], *(uint_ptr **)swapChain, 18 * sizeof(uint_ptr));
    memcpy(g_methodsTable[DXType::dx11] + 18, *(uint_ptr **)device11, 43 * sizeof(uint_ptr));
    memcpy(g_methodsTable[DXType::dx11] + 18 + 43, *(uint_ptr **)context, 144 * sizeof(uint_ptr));

    return Status::Success;
}

//////////////////////////////////////////////////////////////////////

void close_d3d9()
{
    delete[] g_methodsTable[DXType::dx9];
    g_methodsTable[DXType::dx9] = null;
}

//////////////////////////////////////////////////////////////////////

void close_d3d11()
{
    delete[] g_methodsTable[DXType::dx11];
    g_methodsTable[DXType::dx11] = null;
}

}    // namespace

//////////////////////////////////////////////////////////////////////

kiero::Status kiero::open()
{
    if(MH_Initialize() != MH_OK) {
        return Status::InitHookerFailed;
    }
    Status s9 = open_d3d9();
    if(s9 == Status::Success) {
        s9 = open_d3d11();
    }
    if(s9 != Status::Success) {
        close();
    }
    return s9;
}

//////////////////////////////////////////////////////////////////////

void kiero::close()
{
    MH_Uninitialize();
    close_d3d9();
    close_d3d11();
}

//////////////////////////////////////////////////////////////////////

MH_STATUS kiero::bind(DXType type, uint16_t _index, void **_original, void *_function)
{
    if(g_methodsTable[type] == null) {
        return MH_ERROR_NOT_INITIALIZED;
    }
    MH_STATUS c = MH_CreateHook((void *)g_methodsTable[type][_index], _function, _original);
    if(c != MH_OK) {
        return c;
    }
    return MH_EnableHook((void *)g_methodsTable[type][_index]);
}
