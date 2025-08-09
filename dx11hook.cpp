#include "dx11hook.h"
#include <d3d11.h>
#include <dxgi.h>
#include <iostream>
#include <cstdint>

#include <MinHook.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace DX11 {
    typedef HRESULT(WINAPI* Present_t)(IDXGISwapChain*, UINT, UINT);
    typedef HRESULT(WINAPI* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

    Present_t oPresent = nullptr;
    ResizeBuffers_t oResizeBuffers = nullptr;
    WNDPROC oWndProc = nullptr;

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    ID3D11RenderTargetView* pRenderTargetView = nullptr;

    bool initialized = false;
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

    void InitImGui(IDXGISwapChain* pSwapChain) {
        std::cout << "[*] Initializing ImGui for DirectX11..." << std::endl;

        // Get device from swapchain
        HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice);
        if (FAILED(hr)) {
            std::cout << "[-] Failed to get device from swapchain" << std::endl;
            return;
        }

        pDevice->GetImmediateContext(&pContext);

        // Get window
        DXGI_SWAP_CHAIN_DESC desc;
        pSwapChain->GetDesc(&desc);
        window = desc.OutputWindow;

        // Create render target view
        ID3D11Texture2D* pBackBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        if (pBackBuffer) {
            pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pRenderTargetView);
            pBackBuffer->Release();
        }

        // Setup ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX11_Init(pDevice, pContext);

        // Hook WndProc
        oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        initialized = true;
        std::cout << "[+] ImGui initialized successfully for DirectX11" << std::endl;
    }

    void CleanupRenderTarget() {
        if (pRenderTargetView) {
            pRenderTargetView->Release();
            pRenderTargetView = nullptr;
        }
    }

    void CreateRenderTarget(IDXGISwapChain* pSwapChain) {
        ID3D11Texture2D* pBackBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        if (pBackBuffer) {
            pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pRenderTargetView);
            pBackBuffer->Release();
        }
    }

    void CleanupImGui() {
        if (!initialized) {
            return;
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (window && oWndProc) {
            SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        }

        CleanupRenderTarget();

        if (pContext) {
            pContext->Release();
            pContext = nullptr;
        }

        if (pDevice) {
            pDevice->Release();
            pDevice = nullptr;
        }

        initialized = false;
    }

    HRESULT WINAPI hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        if (!initialized) {
            InitImGui(pSwapChain);
        }

        if (initialized && showMenu) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow();

            ImGui::EndFrame();
            ImGui::Render();

            pContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    HRESULT WINAPI hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT Flags) {
        std::cout << "[!] ResizeBuffers called" << std::endl;

        CleanupRenderTarget();
        HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, Format, Flags);
        CreateRenderTarget(pSwapChain);

        return hr;
    }

    bool GetD3D11SwapchainVTable(void** pTable, size_t Size) {
        DXGI_SWAP_CHAIN_DESC swapChainDesc;
        ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
        swapChainDesc.BufferCount = 1;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = GetForegroundWindow();
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL featureLevel;
        ID3D11Device* pDummyDevice = nullptr;
        ID3D11DeviceContext* pDummyContext = nullptr;
        IDXGISwapChain* pDummySwapchain = nullptr;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &pDummySwapchain,
            &pDummyDevice,
            &featureLevel,
            &pDummyContext
        );

        if (FAILED(hr)) {
            std::cout << "[-] Failed to create dummy device. HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
            return false;
        }

        std::cout << "[+] Dummy device created successfully" << std::endl;

        memcpy(pTable, *(void***)pDummySwapchain, Size);

        pDummySwapchain->Release();
        pDummyContext->Release();
        pDummyDevice->Release();

        return true;
    }

    void Hook() {
        std::cout << "[*] Starting DirectX11 Hook..." << std::endl;

        while (!GetModuleHandleA("d3d11.dll")) {
            Sleep(100);
        }

        std::cout << "[+] DirectX11 found!" << std::endl;

        void* d3d11SwapChainVTable[18];
        if (!GetD3D11SwapchainVTable(d3d11SwapChainVTable, sizeof(d3d11SwapChainVTable))) {
            std::cout << "[-] Failed to get D3D11 SwapChain VTable" << std::endl;
            return;
        }

        std::cout << "[+] Got D3D11 SwapChain VTable" << std::endl;

        MH_STATUS status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
            std::cout << "[-] MinHook initialize failed: " << MH_StatusToString(status) << std::endl;
            return;
        }

        // Hook Present (index 8)
        status = MH_CreateHook(d3d11SwapChainVTable[8], &hkPresent, reinterpret_cast<void**>(&oPresent));
        if (status != MH_OK) {
            std::cout << "[-] Failed to create Present hook: " << MH_StatusToString(status) << std::endl;
        }

        // Hook ResizeBuffers (index 13)
        status = MH_CreateHook(d3d11SwapChainVTable[13], &hkResizeBuffers, reinterpret_cast<void**>(&oResizeBuffers));
        if (status != MH_OK) {
            std::cout << "[-] Failed to create ResizeBuffers hook: " << MH_StatusToString(status) << std::endl;
        }

        status = MH_EnableHook(MH_ALL_HOOKS);
        if (status != MH_OK) {
            std::cout << "[-] Failed to enable hooks: " << MH_StatusToString(status) << std::endl;
            return;
        }

        std::cout << "[+] DirectX11 hooks enabled successfully!" << std::endl;
        std::cout << "[*] Press HOME to toggle menu" << std::endl;
    }

    void Unhook() {
        std::cout << "[*] Unhooking DirectX11..." << std::endl;

        CleanupImGui();

        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        std::cout << "[+] DirectX11 unhooked successfully" << std::endl;
    }
}