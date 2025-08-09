#include "dx12hook.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <iostream>
#include <vector>

#include "MinHook.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace DX12 {
    // Function typedefs
    typedef HRESULT(WINAPI* Present_t)(IDXGISwapChain3*, UINT, UINT);
    typedef void(WINAPI* ExecuteCommandLists_t)(ID3D12CommandQueue*, UINT, ID3D12CommandList*);
    typedef HRESULT(WINAPI* ResizeBuffers_t)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

    // Original function pointers
    Present_t oPresent = nullptr;
    ExecuteCommandLists_t oExecuteCommandLists = nullptr;
    ResizeBuffers_t oResizeBuffers = nullptr;
    WNDPROC oWndProc = nullptr;

    // D3D12 objects
    ID3D12Device* pDevice = nullptr;
    ID3D12DescriptorHeap* pDescriptorHeapBackBuffers = nullptr;
    ID3D12DescriptorHeap* pDescriptorHeapImGuiRender = nullptr;
    ID3D12GraphicsCommandList* pCommandList = nullptr;
    ID3D12CommandQueue* pCommandQueue = nullptr;
    ID3D12CommandAllocator* pAllocator = nullptr;

    struct FrameContext {
        ID3D12CommandAllocator* CommandAllocator;
        ID3D12Resource* Resource;
        D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle;
    };

    UINT bufferCount = 0;
    std::vector<FrameContext> frameContexts;

    bool initialized = false;
    bool imguiInitialized = false;
    bool showMenu = true;
    HWND window = nullptr;

    LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (showMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
            return true;
        }

        if (uMsg == WM_KEYUP && wParam == VK_HOME) {
            showMenu = !showMenu;
            std::cout << "[*] Menu toggled: " << (showMenu ? "ON" : "OFF") << std::endl;
        }

        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    void InitializeImGuiContext() {
        if (imguiInitialized) {
            return;
        }

        std::cout << "[*] Creating ImGui context..." << std::endl;

        // Create ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup ImGui style
        ImGui::StyleColorsDark();

        // Initialize ImGui for Win32
        ImGui_ImplWin32_Init(window);

        // Initialize ImGui for DX12 (but don't create device objects yet)
        ImGui_ImplDX12_Init(
            pDevice,
            bufferCount,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            pDescriptorHeapImGuiRender,
            pDescriptorHeapImGuiRender->GetCPUDescriptorHandleForHeapStart(),
            pDescriptorHeapImGuiRender->GetGPUDescriptorHandleForHeapStart()
        );

        imguiInitialized = true;
        std::cout << "[+] ImGui context created" << std::endl;
    }

    void CreateDeviceObjects() {
        if (!imguiInitialized || !pCommandQueue) {
            return;
        }

        std::cout << "[*] Building font atlas..." << std::endl;

        // Build font atlas
        ImGuiIO& io = ImGui::GetIO();
        if (!io.Fonts->IsBuilt()) {
            io.Fonts->AddFontDefault();
            io.Fonts->Build();
        }

        // Create device objects (this will upload the font texture)
        ImGui_ImplDX12_CreateDeviceObjects();

        std::cout << "[+] Font atlas built successfully" << std::endl;
    }

    void InitD3D12Resources(IDXGISwapChain3* pSwapChain) {
        if (initialized) {
            return;
        }

        std::cout << "[*] Initializing D3D12 resources..." << std::endl;

        // Get device from swapchain
        HRESULT hr = pSwapChain->GetDevice(IID_PPV_ARGS(&pDevice));
        if (FAILED(hr)) {
            std::cout << "[-] Failed to get device from swapchain" << std::endl;
            return;
        }

        // Get swap chain description
        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        window = desc.OutputWindow;
        bufferCount = desc.BufferCount;

        // Resize frame contexts
        frameContexts.resize(bufferCount);

        // Create descriptor heap for ImGui (need extra for font texture)
        D3D12_DESCRIPTOR_HEAP_DESC descriptorImGuiRender = {};
        descriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorImGuiRender.NumDescriptors = bufferCount + 1;
        descriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        hr = pDevice->CreateDescriptorHeap(&descriptorImGuiRender, IID_PPV_ARGS(&pDescriptorHeapImGuiRender));
        if (FAILED(hr)) {
            std::cout << "[-] Failed to create ImGui descriptor heap" << std::endl;
            return;
        }

        // Create command allocator
        hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pAllocator));
        if (FAILED(hr)) {
            std::cout << "[-] Failed to create command allocator" << std::endl;
            return;
        }

        // Set allocators for all frames
        for (UINT i = 0; i < bufferCount; i++) {
            frameContexts[i].CommandAllocator = pAllocator;
        }

        // Create command list
        hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocator, nullptr, IID_PPV_ARGS(&pCommandList));
        if (FAILED(hr)) {
            std::cout << "[-] Failed to create command list" << std::endl;
            return;
        }

        hr = pCommandList->Close();
        if (FAILED(hr)) {
            std::cout << "[-] Failed to close command list" << std::endl;
            return;
        }

        // Create descriptor heap for back buffers
        D3D12_DESCRIPTOR_HEAP_DESC descriptorBackBuffers = {};
        descriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descriptorBackBuffers.NumDescriptors = bufferCount;
        descriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        descriptorBackBuffers.NodeMask = 1;

        hr = pDevice->CreateDescriptorHeap(&descriptorBackBuffers, IID_PPV_ARGS(&pDescriptorHeapBackBuffers));
        if (FAILED(hr)) {
            std::cout << "[-] Failed to create back buffer descriptor heap" << std::endl;
            return;
        }

        // Create render target views
        const UINT rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = pDescriptorHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < bufferCount; i++) {
            ID3D12Resource* pBackBuffer = nullptr;
            frameContexts[i].DescriptorHandle = rtvHandle;
            pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
            pDevice->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
            frameContexts[i].Resource = pBackBuffer;
            rtvHandle.ptr += rtvDescriptorSize;
        }

        // Hook WndProc
        oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        initialized = true;
        std::cout << "[+] D3D12 resources initialized" << std::endl;

        // Initialize ImGui context
        InitializeImGuiContext();
    }

    void CleanupImGui() {
        if (!initialized) {
            return;
        }

        // Shutdown ImGui
        if (imguiInitialized) {
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            imguiInitialized = false;
        }

        // Restore WndProc
        if (window && oWndProc) {
            SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        }

        // Clean up frame contexts
        for (auto& context : frameContexts) {
            if (context.Resource) {
                context.Resource->Release();
                context.Resource = nullptr;
            }
        }
        frameContexts.clear();

        // Release allocator
        if (pAllocator) {
            pAllocator->Release();
            pAllocator = nullptr;
        }

        // Release command list
        if (pCommandList) {
            pCommandList->Release();
            pCommandList = nullptr;
        }

        // Release descriptor heaps
        if (pDescriptorHeapBackBuffers) {
            pDescriptorHeapBackBuffers->Release();
            pDescriptorHeapBackBuffers = nullptr;
        }

        if (pDescriptorHeapImGuiRender) {
            pDescriptorHeapImGuiRender->Release();
            pDescriptorHeapImGuiRender = nullptr;
        }

        // Release device
        if (pDevice) {
            pDevice->Release();
            pDevice = nullptr;
        }

        pCommandQueue = nullptr;
        initialized = false;
    }

    HRESULT WINAPI hkPresent(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags) {
        if (!initialized) {
            InitD3D12Resources(pSwapChain);
        }

        // Create device objects once we have command queue
        if (initialized && pCommandQueue && imguiInitialized) {
            static bool deviceObjectsCreated = false;
            if (!deviceObjectsCreated) {
                CreateDeviceObjects();
                deviceObjectsCreated = true;
            }
        }

        if (!pCommandQueue || !initialized || !imguiInitialized) {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }

        if (showMenu) {
            // Start new ImGui frame
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Render ImGui windows
            ImGui::ShowDemoWindow();

            // Render ImGui
            ImGui::EndFrame();

            UINT backBufferIndex = pSwapChain->GetCurrentBackBufferIndex();
            FrameContext& currentFrameContext = frameContexts[backBufferIndex];

            // Reset allocator
            currentFrameContext.CommandAllocator->Reset();

            // Transition to render target
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = currentFrameContext.Resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

            pCommandList->Reset(currentFrameContext.CommandAllocator, nullptr);
            pCommandList->ResourceBarrier(1, &barrier);
            pCommandList->OMSetRenderTargets(1, &currentFrameContext.DescriptorHandle, FALSE, nullptr);
            pCommandList->SetDescriptorHeaps(1, &pDescriptorHeapImGuiRender);

            // Render ImGui draw data
            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);

            // Transition back to present
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            pCommandList->ResourceBarrier(1, &barrier);
            pCommandList->Close();

            // Execute command list
            ID3D12CommandList* ppCommandLists[] = { pCommandList };
            pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
        }

        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    void WINAPI hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists) {
        if (!pCommandQueue) {
            pCommandQueue = queue;
        }

        oExecuteCommandLists(queue, NumCommandLists, ppCommandLists);
    }

    HRESULT WINAPI hkResizeBuffers(IDXGISwapChain3* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT Flags) {
        std::cout << "[!] ResizeBuffers called" << std::endl;

        // Invalidate device objects before resize
        if (initialized) {
            ImGui_ImplDX12_InvalidateDeviceObjects();

            // Release old resources
            for (auto& context : frameContexts) {
                if (context.Resource) {
                    context.Resource->Release();
                    context.Resource = nullptr;
                }
            }
        }

        HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, Format, Flags);

        // Recreate resources after resize
        if (initialized && SUCCEEDED(hr)) {
            // Update buffer count if changed
            bufferCount = BufferCount;
            frameContexts.resize(bufferCount);

            // Re-create render target views
            const UINT rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = pDescriptorHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();

            for (UINT i = 0; i < bufferCount; i++) {
                ID3D12Resource* pBackBuffer = nullptr;
                frameContexts[i].DescriptorHandle = rtvHandle;
                frameContexts[i].CommandAllocator = pAllocator;
                pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
                pDevice->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
                frameContexts[i].Resource = pBackBuffer;
                rtvHandle.ptr += rtvDescriptorSize;
            }

            ImGui_ImplDX12_CreateDeviceObjects();
        }

        return hr;
    }

    bool GetD3D12CommandQueueVTable(void** vtable, size_t size) {
        // Create dummy window
        WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
        RegisterClassExA(&wc);
        HWND hwnd = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

        HMODULE D3D12Module = GetModuleHandleA("d3d12.dll");
        HMODULE DXGIModule = GetModuleHandleA("dxgi.dll");
        if (!D3D12Module || !DXGIModule) {
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        // Create DXGI Factory
        auto fnCreateDXGIFactory = (decltype(&CreateDXGIFactory))GetProcAddress(DXGIModule, "CreateDXGIFactory");
        if (!fnCreateDXGIFactory) {
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        IDXGIFactory* factory;
        if (FAILED(fnCreateDXGIFactory(IID_PPV_ARGS(&factory)))) {
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        IDXGIAdapter* adapter;
        if (factory->EnumAdapters(0, &adapter) == DXGI_ERROR_NOT_FOUND) {
            factory->Release();
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        // Create D3D12 Device
        auto fnD3D12CreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(D3D12Module, "D3D12CreateDevice");
        if (!fnD3D12CreateDevice) {
            adapter->Release();
            factory->Release();
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        ID3D12Device* device;
        if (FAILED(fnD3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
            adapter->Release();
            factory->Release();
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        // Create Command Queue
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        ID3D12CommandQueue* commandQueue;
        if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)))) {
            device->Release();
            adapter->Release();
            factory->Release();
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        // Create allocator and command list
        ID3D12CommandAllocator* commandAllocator;
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)))) {
            commandQueue->Release();
            device->Release();
            adapter->Release();
            factory->Release();
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        ID3D12GraphicsCommandList* commandList;
        if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList)))) {
            commandAllocator->Release();
            commandQueue->Release();
            device->Release();
            adapter->Release();
            factory->Release();
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        // Create swap chain
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hwnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        IDXGISwapChain* swapChain;
        if (FAILED(factory->CreateSwapChain(commandQueue, &swapChainDesc, &swapChain))) {
            commandList->Release();
            commandAllocator->Release();
            commandQueue->Release();
            device->Release();
            adapter->Release();
            factory->Release();
            DestroyWindow(hwnd);
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        // Copy vtables
        memcpy(vtable, *(void***)commandQueue, 19 * sizeof(void*));  // CommandQueue vtable
        memcpy((char*)vtable + 19 * sizeof(void*), *(void***)swapChain, 18 * sizeof(void*));  // SwapChain vtable

        // Cleanup
        swapChain->Release();
        commandList->Release();
        commandAllocator->Release();
        commandQueue->Release();
        device->Release();
        adapter->Release();
        factory->Release();
        DestroyWindow(hwnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);

        return true;
    }

    void Hook() {
        std::cout << "[*] Starting DirectX12 Hook..." << std::endl;

        // Wait for D3D12
        while (!GetModuleHandleA("d3d12.dll") || !GetModuleHandleA("dxgi.dll")) {
            Sleep(100);
        }

        std::cout << "[+] DirectX12 modules found!" << std::endl;

        void* d3d12VTable[37];  // 19 CommandQueue + 18 SwapChain
        if (!GetD3D12CommandQueueVTable(d3d12VTable, sizeof(d3d12VTable))) {
            std::cout << "[-] Failed to get D3D12 VTable" << std::endl;
            return;
        }

        std::cout << "[+] Got D3D12 VTable" << std::endl;

        MH_STATUS status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
            std::cout << "[-] MinHook initialize failed: " << MH_StatusToString(status) << std::endl;
            return;
        }

        // Hook ExecuteCommandLists (CommandQueue vtable index 10)
        status = MH_CreateHook(d3d12VTable[10], &hkExecuteCommandLists, reinterpret_cast<void**>(&oExecuteCommandLists));
        if (status != MH_OK) {
            std::cout << "[-] Failed to create ExecuteCommandLists hook: " << MH_StatusToString(status) << std::endl;
        }

        // Hook Present (SwapChain vtable index 8, offset by 19 CommandQueue methods)
        status = MH_CreateHook(d3d12VTable[19 + 8], &hkPresent, reinterpret_cast<void**>(&oPresent));
        if (status != MH_OK) {
            std::cout << "[-] Failed to create Present hook: " << MH_StatusToString(status) << std::endl;
        }

        // Hook ResizeBuffers (SwapChain vtable index 13, offset by 19 CommandQueue methods)
        status = MH_CreateHook(d3d12VTable[19 + 13], &hkResizeBuffers, reinterpret_cast<void**>(&oResizeBuffers));
        if (status != MH_OK) {
            std::cout << "[-] Failed to create ResizeBuffers hook: " << MH_StatusToString(status) << std::endl;
        }

        status = MH_EnableHook(MH_ALL_HOOKS);
        if (status != MH_OK) {
            std::cout << "[-] Failed to enable hooks: " << MH_StatusToString(status) << std::endl;
            return;
        }

        std::cout << "[+] DirectX12 hooks enabled successfully!" << std::endl;
        std::cout << "[*] Press HOME to toggle menu" << std::endl;
    }

    void Unhook() {
        std::cout << "[*] Unhooking DirectX12..." << std::endl;

        CleanupImGui();

        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        std::cout << "[+] DirectX12 unhooked successfully" << std::endl;
    }
}