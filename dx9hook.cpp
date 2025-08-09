#include "dx9hook.h"
#include <d3d9.h>
#include <iostream>
#include <cstdint>

#include "MinHook.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace DX9 {
    typedef HRESULT(WINAPI* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
    typedef HRESULT(WINAPI* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    typedef HRESULT(WINAPI* EndScene_t)(IDirect3DDevice9*);
    typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

    Present_t oPresent = nullptr;
    Reset_t oReset = nullptr;
    EndScene_t oEndScene = nullptr;
    WNDPROC oWndProc = nullptr;

    bool initialized = false;
    bool showMenu = true;
    HWND window = nullptr;

    LRESULT CALLBACK hkWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (showMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
            return true;
        }

        // Toggle menu med INSERT key
        if (uMsg == WM_KEYUP && wParam == VK_HOME) {
            showMenu = !showMenu;
            std::cout << "[*] Menu toggled: " << (showMenu ? "ON" : "OFF") << std::endl;
        }

        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    void InitImGui(IDirect3DDevice9* device) {
        std::cout << "[*] Initializing ImGui..." << std::endl;

        // Find window
        D3DDEVICE_CREATION_PARAMETERS params;
        device->GetCreationParameters(&params);
        window = params.hFocusWindow;

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX9_Init(device);

        // Hook WndProc for input
        oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        initialized = true;
        std::cout << "[+] ImGui initialized successfully" << std::endl;
    }

    void CleanupImGui() {
        if (initialized) {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            if (window && oWndProc) {
                SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
            }
        }
    }

    HRESULT WINAPI hkEndScene(IDirect3DDevice9* device) {
        if (!initialized) {
            InitImGui(device);
        }

        if (initialized && showMenu) {
            // Start ImGui frame
            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Demo window
            ImGui::ShowDemoWindow();

            // Render ImGui
            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }

        return oEndScene(device);
    }

    HRESULT WINAPI hkPresent(IDirect3DDevice9* device, const RECT* src, const RECT* dest, HWND window, const RGNDATA* region) {
        return oPresent(device, src, dest, window, region);
    }

    HRESULT WINAPI hkReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
        std::cout << "[!] Device Reset called" << std::endl;

        ImGui_ImplDX9_InvalidateDeviceObjects();
        HRESULT hr = oReset(device, params);
        ImGui_ImplDX9_CreateDeviceObjects();

        return hr;
    }

    bool GetD3D9Device(void** pTable, size_t Size) {
        if (!pTable) {
            return false;
        }

        IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);

        if (!pD3D) {
            std::cout << "[-] Failed to create D3D9 interface" << std::endl;
            return false;
        }

        IDirect3DDevice9* pDummyDevice = nullptr;

        HWND window = FindWindowA("Direct3DWindowClass", NULL);
        if (!window) {
            window = GetForegroundWindow();
        }
        if (!window) {
            window = GetDesktopWindow();
        }

        D3DPRESENT_PARAMETERS d3dpp = {};
        d3dpp.Windowed = TRUE;
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
        d3dpp.hDeviceWindow = window;

        HRESULT hr = pD3D->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            d3dpp.hDeviceWindow,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &d3dpp,
            &pDummyDevice
        );

        if (FAILED(hr)) {
            std::cout << "[*] HAL device failed, trying NULLREF..." << std::endl;

            hr = pD3D->CreateDevice(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_NULLREF,
                d3dpp.hDeviceWindow,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                &d3dpp,
                &pDummyDevice
            );

            if (FAILED(hr)) {
                std::cout << "[-] Failed to create device. HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
                pD3D->Release();
                return false;
            }
        }

        std::cout << "[+] Dummy device created successfully" << std::endl;

        memcpy(pTable, *(void***)pDummyDevice, Size);

        pDummyDevice->Release();
        pD3D->Release();

        return true;
    }

    void Hook() {
        std::cout << "[*] Starting DirectX9 Hook..." << std::endl;

        void* d3d9Device[119];
        if (!GetD3D9Device(d3d9Device, sizeof(d3d9Device))) {
            std::cout << "[-] Failed to get D3D9 device VTable" << std::endl;
            return;
        }

        std::cout << "[+] Got D3D9 VTable" << std::endl;

        MH_STATUS status = MH_Initialize();
        if (status != MH_OK) {
            std::cout << "[-] MinHook initialize failed: " << MH_StatusToString(status) << std::endl;
            return;
        }

        std::cout << "[+] MinHook initialized" << std::endl;

        status = MH_CreateHook(d3d9Device[42], &hkEndScene, reinterpret_cast<void**>(&oEndScene));
        if (status != MH_OK) {
            std::cout << "[-] Failed to create EndScene hook: " << MH_StatusToString(status) << std::endl;
        }

        status = MH_CreateHook(d3d9Device[17], &hkPresent, reinterpret_cast<void**>(&oPresent));
        if (status != MH_OK) {
            std::cout << "[-] Failed to create Present hook: " << MH_StatusToString(status) << std::endl;
        }

        status = MH_CreateHook(d3d9Device[16], &hkReset, reinterpret_cast<void**>(&oReset));
        if (status != MH_OK) {
            std::cout << "[-] Failed to create Reset hook: " << MH_StatusToString(status) << std::endl;
        }

        status = MH_EnableHook(MH_ALL_HOOKS);
        if (status != MH_OK) {
            std::cout << "[-] Failed to enable hooks: " << MH_StatusToString(status) << std::endl;
            return;
        }

        std::cout << "[+] All hooks enabled successfully!" << std::endl;
        std::cout << "[*] Press INSERT to toggle menu" << std::endl;
    }

    void Unhook() {
        std::cout << "[*] Unhooking..." << std::endl;

        CleanupImGui();

        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        std::cout << "[+] Unhooked successfully" << std::endl;
    }
}