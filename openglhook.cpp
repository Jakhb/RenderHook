#include "openglhook.h"
#include <Windows.h>
#include <iostream>
#include <gl/GL.h>
#include "MinHook.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "opengl32.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace OpenGL {
    typedef BOOL(WINAPI* WglSwapBuffers_t)(HDC hdc);
    typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

    WglSwapBuffers_t oWglSwapBuffers = nullptr;
    WNDPROC oWndProc = nullptr;

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

    void InitImGui(HDC hdc) {
        std::cout << "[*] Initializing ImGui for OpenGL..." << std::endl;

        window = WindowFromDC(hdc);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(window);
        ImGui_ImplOpenGL3_Init("#version 130");

        oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        initialized = true;
        std::cout << "[+] ImGui initialized successfully for OpenGL" << std::endl;
    }

    BOOL WINAPI hkWglSwapBuffers(HDC hdc) {
        if (!initialized && hdc) {
            InitImGui(hdc);
        }

        if (initialized && showMenu) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow();

            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        return oWglSwapBuffers(hdc);
    }

    void Hook() {
        std::cout << "[*] Starting OpenGL Hook..." << std::endl;

        while (!GetModuleHandleA("opengl32.dll")) {
            Sleep(100);
        }

        HMODULE openGL32 = GetModuleHandleA("opengl32.dll");
        void* fnWglSwapBuffers = GetProcAddress(openGL32, "wglSwapBuffers");

        MH_Initialize();
        MH_CreateHook(fnWglSwapBuffers, &hkWglSwapBuffers, reinterpret_cast<void**>(&oWglSwapBuffers));
        MH_EnableHook(fnWglSwapBuffers);

        std::cout << "[+] OpenGL hook enabled successfully!" << std::endl;
        std::cout << "[*] Press HOME to toggle menu" << std::endl;
    }

    void Unhook() {
        std::cout << "[*] Unhooking OpenGL..." << std::endl;

        if (initialized) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            if (window && oWndProc) {
                SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
            }
        }

        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        std::cout << "[+] OpenGL unhooked successfully" << std::endl;
    }
}