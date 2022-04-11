#define _CRT_SECURE_NO_WARNINGS

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <exception>
#include <cassert>

#include <algorithm>
#include <tuple>

#include <stdint.h>
#include <cwchar>

#include <wrl.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <string>
#include <vector>
#include <sstream>

#include "vertex.h"
#include "ObjectConstants.h"

using namespace Microsoft::WRL;

const wchar_t* gWindowClassName = L"SkullSample"; //Windows title
uint32_t gClientWidth = 1280; // Width
uint32_t gClientHeight = 720; // Height
bool gUseWarp = false; // Use WARP adapter
bool gVSync = true; // Can be toggled with the V key.
bool gTearingSupported = false;
HWND gHwnd; // Window handle.
RECT gWindowRect; // Window rectangle (used to toggle fullscreen state).
const uint8_t gNumFrames = 3; // The number of swap chain back buffers.

// DirectX 12 Objects
ComPtr<ID3D12Device2> gDevice;
ComPtr<ID3D12Fence> gFence;
ComPtr<ID3D12CommandQueue> gCommandQueue;
ComPtr<IDXGISwapChain4> gSwapChain;
ComPtr<ID3D12Resource> gBackBuffers[gNumFrames];
ComPtr<ID3D12Resource> gDSVBuffers;
ComPtr<ID3D12GraphicsCommandList> gCommandList;
ComPtr<ID3D12CommandAllocator> gCommandAllocator; 
ComPtr<ID3D12DescriptorHeap> gRTVDescriptorHeap, gDSVDescriptorHeap, gCBVDescriptorHeap;
ComPtr<ID3D12RootSignature> gRootSignature;
ComPtr<ID3D12PipelineState> gPSO;
ComPtr<ID3DBlob> gVSShader, gPSShader;
std::vector<D3D12_INPUT_ELEMENT_DESC> gInputLayout;
D3D12_VIEWPORT gScreenViewport;
D3D12_RECT gScissorRect;
UINT gRTVDescriptorSize, gDSVDescriptorSize, gCBVDescriptorSize;
UINT gCurrentBackBufferIndex = 0;
UINT64 gCurrentFence = 0;

std::vector<Vertex> gVertexes;
std::vector<std::uint16_t> gIndexes;
//ComPtr<ID3DBlob> gVertexBufferCPU = nullptr;
//ComPtr<ID3DBlob> gIndexBufferCPU = nullptr;
ComPtr<ID3D12Resource> gVertexBufferGPU = nullptr;
ComPtr<ID3D12Resource> gIndexBufferGPU = nullptr;
ComPtr<ID3D12Resource> gVertexBufferUploader = nullptr;
ComPtr<ID3D12Resource> gIndexBufferUploader = nullptr;

struct UploadBuffer {

    ComPtr<ID3D12Resource> mUploadResource;
    std::uint8_t* mBuffer = nullptr;

} gUploadConstantsBuffer;



LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

inline void ThrowIfFailed(HRESULT hr) {

    if (FAILED(hr)) throw std::exception();
}

void EnableDebugLayer() {

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif
}

bool CheckTearingSupport() {

    // Rather than create the DXGI 1.5 factory interface directly, we create the DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
    // graphics debugging tools which will not support the 1.5 factory interface until a future update.
    BOOL allowTearing = FALSE;
    ComPtr<IDXGIFactory4> factory4;
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
        if (SUCCEEDED(factory4.As(&factory5)))
            if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
                allowTearing = FALSE;

    return allowTearing == TRUE;
}

void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName) {

    // Register a window class for creating our render window with.
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &WndProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInst;
    windowClass.hIcon = ::LoadIcon(hInst, NULL); //  MAKEINTRESOURCE(APPLICATION_ICON));
    windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = windowClassName;
    windowClass.hIconSm = ::LoadIcon(hInst, NULL); //  MAKEINTRESOURCE(APPLICATION_ICON));

    static HRESULT hr = RegisterClassExW(&windowClass);
    assert(SUCCEEDED(hr));
}

HWND CreateWindowHwnd(const wchar_t* windowClassName, HINSTANCE hInst, const wchar_t* windowTitle, uint32_t width, uint32_t height) {

    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
    int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
    int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

    HWND hWnd = CreateWindowExW(
        NULL,
        windowClassName,
        windowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,//windowX,
        CW_USEDEFAULT, //windowY,
        windowWidth,
        windowHeight,
        0,
        0,
        hInst,
        0
    );
    auto lastError = GetLastError();


    assert(hWnd && "Failed to create window");

    return hWnd;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    default:
        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }


    return 0;
}

auto GetAdapter(bool useWarp) {

    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT createFactoryFlags = 0;
//#if defined(_DEBUG)
//    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
//#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if (useWarp)
    {
        ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
    }
    else
    {
        SIZE_T maxDedicatedVideoMemory = 0;
        for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
            dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

            // Check to see if the adapter can create a D3D12 device without actually 
            // creating it. The adapter with the largest dedicated video memory
            // is favored.
            if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
                    D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
                dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
            {
                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
            }
        }
    }

    return dxgiAdapter4;
}

auto CreateDevice(ComPtr<IDXGIAdapter4> adapter) {

    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        //NewFilter.DenyList.NumCategories = _countof(Categories);
        //NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif

    return d3d12Device2;
}

auto CreateDescriptorHeapSizes(ComPtr<ID3D12Device2> device) {

    auto size_rtv = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto size_dsv = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    auto size_cbv = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return std::make_tuple(size_rtv, size_dsv, size_cbv);
}

auto CreateCommandObjects(ComPtr<ID3D12Device2> device) {

    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12CommandAllocator> commandListAlloc;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandListAlloc.GetAddressOf())));
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, 
                                            commandListAlloc.Get(), // Associated command allocator
                                            nullptr,                   // Initial PipelineStateObject
                                            IID_PPV_ARGS(&commandList)));

    // Start off in a closed state.  This is because the first time we refer 
    // to the command list we will Reset it, and it needs to be closed before
    // calling Reset.
    ThrowIfFailed(commandList->Close());

    return std::make_tuple(commandQueue, commandList, commandListAlloc);
}

auto CreateSwapChain(HWND hWnd, ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount) {

    ComPtr<IDXGISwapChain4> dxgiSwapChain4;
    ComPtr<IDXGIFactory4> dxgiFactory4;
    UINT createFactoryFlags = 0;
//#if defined(_DEBUG)
//    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
//#endif
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = bufferCount;
    sd.OutputWindow = hWnd;
    sd.Windowed = true;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ComPtr<IDXGISwapChain> swapChain;
    ThrowIfFailed(dxgiFactory4->CreateSwapChain(
        commandQueue.Get(),
        &sd,
        &swapChain));

    // Disable the Alt+Enter fullscreen toggle feature
    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&dxgiSwapChain4));

    return dxgiSwapChain4;
}

auto CreateDescriptorHeaps(ComPtr<ID3D12Device2> device, uint32_t numRTVDescriptors) {

    ComPtr<ID3D12DescriptorHeap> descriptorHeapRTV, descriptorHeapDSV, descriptorHeapCBV;

    D3D12_DESCRIPTOR_HEAP_DESC descRTV = {};
    descRTV.NumDescriptors = numRTVDescriptors;
    descRTV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(device->CreateDescriptorHeap(&descRTV, IID_PPV_ARGS(&descriptorHeapRTV)));

    D3D12_DESCRIPTOR_HEAP_DESC descDSV;
    descDSV.NumDescriptors = 1;
    descDSV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    descDSV.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    descDSV.NodeMask = 0;
    ThrowIfFailed(device->CreateDescriptorHeap(&descDSV, IID_PPV_ARGS(&descriptorHeapDSV)));

    D3D12_DESCRIPTOR_HEAP_DESC descCBV;
    descCBV.NumDescriptors = 1;
    descCBV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descCBV.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descCBV.NodeMask = 0;
    ThrowIfFailed(device->CreateDescriptorHeap(&descCBV, IID_PPV_ARGS(&descriptorHeapCBV)));

    return std::make_tuple(descriptorHeapRTV, descriptorHeapDSV, descriptorHeapCBV);
}

auto GetTransitionBarrier(ID3D12Resource* pResource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter, UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE) {

    D3D12_RESOURCE_BARRIER returnBarrier;
    ZeroMemory(&returnBarrier, sizeof(returnBarrier));
    returnBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    returnBarrier.Flags = flags;
    returnBarrier.Transition.pResource = pResource;
    returnBarrier.Transition.StateBefore = stateBefore;
    returnBarrier.Transition.StateAfter = stateAfter;
    returnBarrier.Transition.Subresource = subresource;
    return returnBarrier;
}

auto UploadResource(ID3D12GraphicsCommandList* pCmdList, _In_ ID3D12Resource* pDestinationResource, ID3D12Resource* pIntermediate, D3D12_SUBRESOURCE_DATA* pSrcData) {

    std::uint8_t* pData = nullptr;
    auto hr = pIntermediate->Map(0, NULL, reinterpret_cast<void**>(&pData));
    if (FAILED(hr)) return false;
    memcpy(pData, pSrcData->pData, pSrcData->RowPitch);
    pIntermediate->Unmap(0, NULL);
    pCmdList->CopyBufferRegion(pDestinationResource, 0, pIntermediate, 0, pSrcData->RowPitch);
    return true;
}

auto CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* data, UINT64 size, ComPtr<ID3D12Resource>& uploadBuffer) {

    ComPtr<ID3D12Resource> returnBuffer;

    D3D12_HEAP_PROPERTIES defaultHeap;
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeap.CreationNodeMask = 1;
    defaultHeap.VisibleNodeMask = 1;
    defaultHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_HEAP_PROPERTIES uploadHeap = defaultHeap;
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDescr;
    resourceDescr.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDescr.Alignment = 0;
    resourceDescr.Width = size;
    resourceDescr.Height = 1;
    resourceDescr.DepthOrArraySize = 1;
    resourceDescr.MipLevels = 1;
    resourceDescr.Format = DXGI_FORMAT_UNKNOWN;
    resourceDescr.SampleDesc.Count = 1;
    resourceDescr.SampleDesc.Quality = 0;
    resourceDescr.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDescr.Flags = D3D12_RESOURCE_FLAG_NONE;

    ThrowIfFailed(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &resourceDescr, D3D12_RESOURCE_STATE_COMMON,       nullptr, IID_PPV_ARGS(returnBuffer.GetAddressOf())));
    ThrowIfFailed(device->CreateCommittedResource(&uploadHeap,  D3D12_HEAP_FLAG_NONE, &resourceDescr, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = data;
    subResourceData.RowPitch = size;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    auto beforeBarrier = GetTransitionBarrier(returnBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,    D3D12_RESOURCE_STATE_COPY_DEST);
    auto afterBarrier  = GetTransitionBarrier(returnBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &beforeBarrier);
    UploadResource(gCommandList.Get(), returnBuffer.Get(), uploadBuffer.Get(), &subResourceData);
    cmdList->ResourceBarrier(1, &afterBarrier);

    return returnBuffer;
}

void BuildRootSignature() {

    D3D12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvTable.NumDescriptors = 1;
    cbvTable.BaseShaderRegister = 0;
    cbvTable.RegisterSpace = 0;
    cbvTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    //D3D12_ROOT_PARAMETER slotRootParameter[1];
    //slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    //slotRootParameter[0].DescriptorTable.NumDescriptorRanges = 1;
    //slotRootParameter[0].DescriptorTable.pDescriptorRanges = &cbvTable;
    //slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_PARAMETER slotRootParameter[1];
    slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    slotRootParameter[0].Constants.Num32BitValues = 16;
    slotRootParameter[0].Constants.RegisterSpace = 0;
    slotRootParameter[0].Constants.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC rootDescr;
    rootDescr.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootDescr.NumParameters = 1;
    rootDescr.NumStaticSamplers = 0;
    rootDescr.pParameters = slotRootParameter;
    rootDescr.pStaticSamplers = nullptr;

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootDescr, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(gDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(gRootSignature.GetAddressOf())));
}

void BuildShadersAndInputLayout() {

    HRESULT hr = S_OK;
    ComPtr<ID3DBlob> errors;

    hr = D3DCompileFromFile(L"default.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "VS", "vs_5_1", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &gVSShader, &errors);

    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

    ThrowIfFailed(hr);

    hr = D3DCompileFromFile(L"default.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PS", "ps_5_1", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &gPSShader, &errors);

    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

    ThrowIfFailed(hr);

    gInputLayout = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

}

void BuildShapeGeometry() {
    
    FILE* fp = fopen("skull.txt", "r");
    char buffer[1024];
    bool startReading = false;
    bool endReading = false;

    struct FileVertexData {
        FileVertexData(
            float px, float py, float pz,
            float nx, float ny, float nz) :
            mPosition(px, py, pz),
            mNormal(nx, ny, nz)
        { }
        DirectX::XMFLOAT3 mPosition;
        DirectX::XMFLOAT3 mNormal;
    };

    std::vector<FileVertexData> vertexes;
    std::vector<int> indexes;

    while (fgets(buffer, 1024, fp)) {
        if (strcmp(buffer, "{\n") == 0) {
            startReading = true;
        }
        else if (strcmp(buffer, "}\n") == 0) {
            startReading = false;
            endReading = true;
        }
        else if (endReading) {
            break;
        }
        else if (startReading) {
            std::stringstream ss{ buffer };
            float arr[6];
            ss >> arr[0];
            ss >> arr[1];
            ss >> arr[2];
            ss >> arr[3];
            ss >> arr[4];
            ss >> arr[5];
            FileVertexData v{ arr[0], arr[1], arr[2], arr[3], arr[4], arr[5] };
            vertexes.push_back(v);
        }
    }

    endReading = false;

    while (fgets(buffer, 1024, fp)) {
        if (strcmp(buffer, "{\n") == 0) {
            startReading = true;
        }
        else if (strcmp(buffer, "}\n") == 0) {
            startReading = false;
            endReading = true;
        }
        else if (endReading) {
            break;
        }
        else if (startReading) {
            std::stringstream ss{ buffer };
            int a, b, c;
            ss >> a;
            ss >> b;
            ss >> c;
            indexes.push_back(a);
            indexes.push_back(b);
            indexes.push_back(c);
        }
    }

    gVertexes.reserve(vertexes.size());
    gIndexes.reserve(indexes.size());

    std::for_each(vertexes.begin(), vertexes.end(), [](FileVertexData& item) {
        Vertex v;
        v.SetColor(DirectX::Colors::Yellow);
        v.SetPosition(item.mPosition);
        v.Scale(0.05);
        gVertexes.push_back(v);
        });

    std::for_each(indexes.begin(), indexes.end(), [](int& idx) {
        gIndexes.push_back(static_cast<std::uint16_t>(idx));
        });

    auto vbByteSize = gVertexes.size() * sizeof(Vertex);
    auto ibByteSize = gIndexes.size() * sizeof(std::uint16_t);

    //ThrowIfFailed(D3DCreateBlob(vbByteSize, &gVertexBufferCPU));
    //memcpy(gVertexBufferCPU->GetBufferPointer(), gVertexes.data(), vbByteSize);

    //ThrowIfFailed(D3DCreateBlob(ibByteSize, &gIndexBufferCPU));
    //memcpy(gIndexBufferCPU->GetBufferPointer(), gIndexes.data(), ibByteSize);

    gVertexBufferGPU = CreateDefaultBuffer(gDevice.Get(), gCommandList.Get(), gVertexes.data(), vbByteSize, gVertexBufferUploader);
    gIndexBufferGPU  = CreateDefaultBuffer(gDevice.Get(), gCommandList.Get(), gIndexes.data(),  ibByteSize, gIndexBufferUploader);
}

void BuildCBV(UINT byteSize) {

    // Constant buffers must be a multiple of the minimum hardware allocation size (usually 256 bytes). So round up to nearest multiple of 256.
    UINT cbvSize = (byteSize + 255) & ~255;

    UINT objCount = 1; //skull model

    D3D12_HEAP_PROPERTIES uploadHeap;
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;
    uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDescr;
    resourceDescr.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDescr.Alignment = 0;
    resourceDescr.Width = cbvSize;
    resourceDescr.Height = 1;
    resourceDescr.DepthOrArraySize = 1;
    resourceDescr.MipLevels = 1;
    resourceDescr.Format = DXGI_FORMAT_UNKNOWN;
    resourceDescr.SampleDesc.Count = 1;
    resourceDescr.SampleDesc.Quality = 0;
    resourceDescr.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDescr.Flags = D3D12_RESOURCE_FLAG_NONE;

    ThrowIfFailed(gDevice->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &resourceDescr, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&gUploadConstantsBuffer.mUploadResource)));
    ThrowIfFailed(gUploadConstantsBuffer.mUploadResource->Map(0, nullptr, reinterpret_cast<void**>(&gUploadConstantsBuffer.mBuffer)));

    //ObjectConstants objConstants;
    using namespace DirectX;
    XMFLOAT4X4 World4x4 = Identity4x4();
    XMMATRIX world = XMLoadFloat4x4(&World4x4);
    XMFLOAT4X4 resultWorld;
    XMStoreFloat4x4(&resultWorld, XMMatrixTranspose(world));
    memcpy(gUploadConstantsBuffer.mBuffer, &resultWorld, sizeof(XMFLOAT4X4));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = gUploadConstantsBuffer.mUploadResource->GetGPUVirtualAddress();
    
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = cbvSize;

    auto handle = gCBVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    gDevice->CreateConstantBufferView(&cbvDesc, handle);
}

void BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { gInputLayout.data(), (UINT)gInputLayout.size() };
    psoDesc.pRootSignature = gRootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(gVSShader->GetBufferPointer()),
        gVSShader->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(gPSShader->GetBufferPointer()),
        gPSShader->GetBufferSize()
    };

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.DepthClipEnable = 1;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
    {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;

    D3D12_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthDesc.StencilEnable = FALSE;
    depthDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
    { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    depthDesc.FrontFace = defaultStencilOp;
    depthDesc.BackFace = defaultStencilOp;

    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = depthDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ThrowIfFailed(gDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&gPSO)));
}

void FlushCommandQueue()
{
    gCurrentFence++;
    ThrowIfFailed(gCommandQueue->Signal(gFence.Get(), gCurrentFence));

    if (gFence->GetCompletedValue() < gCurrentFence) {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(gFence->SetEventOnCompletion(gCurrentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void OnResize()
{
    // Flush before changing any resources.
    FlushCommandQueue();

    ThrowIfFailed(gCommandList->Reset(gCommandAllocator.Get(), nullptr));

    // Release the previous resources we will be recreating.
    for (int i = 0; i < gNumFrames; ++i)
        gBackBuffers[i].Reset();

    gDSVBuffers.Reset();

    ThrowIfFailed(gSwapChain->ResizeBuffers(gNumFrames, gClientWidth, gClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    gCurrentBackBufferIndex = 0;

    auto rtvHeapHandle = gRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < gNumFrames; i++)
    {
        ThrowIfFailed(gSwapChain->GetBuffer(i, IID_PPV_ARGS(&gBackBuffers[i])));
        gDevice->CreateRenderTargetView(gBackBuffers[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.ptr += gRTVDescriptorSize;
    }

    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = gClientWidth;
    depthStencilDesc.Height = gClientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DXGI_FORMAT_R24G8_TYPELESS
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heapProperties{
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        1, 1
    };

    ThrowIfFailed(gDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(gDSVBuffers.GetAddressOf())));

    // Create descriptor to mip level 0 of entire resource using the format of the resource.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.Texture2D.MipSlice = 0;

    gDevice->CreateDepthStencilView(gDSVBuffers.Get(), &dsvDesc, gDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_RESOURCE_BARRIER transitionBarrier;
    transitionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transitionBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    transitionBarrier.Transition.pResource = gDSVBuffers.Get();
    transitionBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    transitionBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    transitionBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    gCommandList->ResourceBarrier(1, &transitionBarrier);

    ThrowIfFailed(gCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { gCommandList.Get() };
    gCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until resize is complete.
    FlushCommandQueue();

    // Update the viewport transform to cover the client area.
    gScreenViewport.TopLeftX = 0;
    gScreenViewport.TopLeftY = 0;
    gScreenViewport.Width = static_cast<float>(gClientWidth);
    gScreenViewport.Height = static_cast<float>(gClientHeight);
    gScreenViewport.MinDepth = 0.0f;
    gScreenViewport.MaxDepth = 1.0f;

    gScissorRect = { 0, 0, static_cast<long>(gClientWidth), static_cast<long>(gClientHeight)};
}

void STAGE1_InitMainWindow(HINSTANCE hInstance) {
    
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RegisterWindowClass(hInstance, gWindowClassName);
    gHwnd = CreateWindowHwnd(gWindowClassName, hInstance, L"Skull sample", gClientWidth, gClientHeight);
    // Initialize the global window rect variable.
    GetWindowRect(gHwnd, &gWindowRect);
    ShowWindow(gHwnd, SW_SHOW);
    UpdateWindow(gHwnd);
}

void STAGE2_InitDirect3D() {

    //EnableDebugLayer();
    gTearingSupported = CheckTearingSupport();
    ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(gUseWarp);
    gDevice = CreateDevice(dxgiAdapter4);
    ThrowIfFailed(gDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gFence)));
    std::tie(gRTVDescriptorSize, gDSVDescriptorSize, gCBVDescriptorSize) = CreateDescriptorHeapSizes(gDevice);
    std::tie(gCommandQueue, gCommandList, gCommandAllocator) = CreateCommandObjects(gDevice);
    gSwapChain = CreateSwapChain(gHwnd, gCommandQueue, gClientWidth, gClientHeight, gNumFrames);
    std::tie(gRTVDescriptorHeap, gDSVDescriptorHeap, gCBVDescriptorHeap) = CreateDescriptorHeaps(gDevice, gNumFrames);
    OnResize();
}

void STAGE3_InitDirect3D() {

    ThrowIfFailed(gCommandList->Reset(gCommandAllocator.Get(), nullptr));
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
    BuildCBV(sizeof(DirectX::XMFLOAT4X4));
    BuildPSOs();

    ThrowIfFailed(gCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { gCommandList.Get() };
    gCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();
}

void Draw() {

    if (gFence->GetCompletedValue() < gCurrentFence) {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(gFence->SetEventOnCompletion(gCurrentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    ThrowIfFailed(gCommandAllocator->Reset());
    ThrowIfFailed(gCommandList->Reset(gCommandAllocator.Get(), gPSO.Get()));

    gCommandList->RSSetViewports(1, &gScreenViewport);
    gCommandList->RSSetScissorRects(1, &gScissorRect);

    auto beforeBarrier = GetTransitionBarrier(gBackBuffers[gCurrentBackBufferIndex].Get(), D3D12_RESOURCE_STATE_COMMON,    D3D12_RESOURCE_STATE_COPY_DEST);
    auto afterBarrier  = GetTransitionBarrier(gBackBuffers[gCurrentBackBufferIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

    gCommandList->ResourceBarrier(1, &beforeBarrier);
    auto currentBackBuferView = gRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    currentBackBuferView.ptr += gCurrentBackBufferIndex * gRTVDescriptorSize;
    gCommandList->ClearRenderTargetView(currentBackBuferView, DirectX::Colors::LightSteelBlue, 0, nullptr);
    gCommandList->ClearDepthStencilView(gDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    auto currentDSVView = gDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    gCommandList->OMSetRenderTargets(1, &currentBackBuferView, true, &currentDSVView);

    //ID3D12DescriptorHeap* descriptorHeaps[] = { gCBVDescriptorHeap.Get() };
    //gCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    gCommandList->SetGraphicsRootSignature(gRootSignature.Get());

    //auto passCbvHandle = gCBVDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    //gCommandList->SetGraphicsRootDescriptorTable(0, passCbvHandle);

    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = gVertexBufferGPU->GetGPUVirtualAddress();
    vbv.StrideInBytes = sizeof(Vertex);
    vbv.SizeInBytes = vbv.StrideInBytes * gVertexes.size();
    gCommandList->IASetVertexBuffers(0, 1, &vbv);

    D3D12_INDEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = gIndexBufferGPU->GetGPUVirtualAddress();
    ibv.Format = DXGI_FORMAT_R16_UINT;
    ibv.SizeInBytes = sizeof(std::uint16_t) * gIndexes.size();
    gCommandList->IASetIndexBuffer(&ibv);

    gCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    {
        using namespace DirectX;

        DirectX::XMFLOAT4X4 mWorld = Identity4x4();
        DirectX::XMFLOAT4X4 mView = Identity4x4();
        DirectX::XMFLOAT4X4 mProj = Identity4x4();

        float radius = 3;
        float theta = 1.5f * XM_PI;
        float mPhi = XM_PIDIV4;

        // Convert Spherical to Cartesian coordinates.
        float x = radius * sinf(mPhi) * cosf(theta);
        float z = radius * sinf(mPhi) * sinf(theta);
        float y = radius * cosf(mPhi);

        // Build the view matrix.
        XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
        XMVECTOR target = XMVectorZero();
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
        XMStoreFloat4x4(&mView, view);

        XMMATRIX world = XMLoadFloat4x4(&mWorld);
        XMMATRIX proj = XMLoadFloat4x4(&mProj);
        XMMATRIX worldViewProj = world * view * proj;

        //auto identityMtx = Identity4x4();
        //DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&tempIdentityMtx);
        //DirectX::XMFLOAT4X4 World;
        //XMStoreFloat4x4(&World, XMMatrixTranspose(world));

        gCommandList->SetGraphicsRoot32BitConstants(0, sizeof(DirectX::XMMATRIX) / 4, &worldViewProj, 0);
    }

    gCommandList->DrawIndexedInstanced(gIndexes.size(), 1, 0, 0, 0);

    gCommandList->ResourceBarrier(1, &afterBarrier);

    ThrowIfFailed(gCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { gCommandList.Get() };
    gCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(gSwapChain->Present(0, 0));
    gCurrentBackBufferIndex = (gCurrentBackBufferIndex + 1) % gNumFrames;

    gCurrentFence++;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    gCommandQueue->Signal(gFence.Get(), gCurrentFence);
}

int Run() {

    MSG msg = { 0 };

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Draw();
        }
    }

    return (int)msg.wParam;
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {

    STAGE1_InitMainWindow(hInstance);
    STAGE2_InitDirect3D();
    STAGE3_InitDirect3D();

    Run();
}