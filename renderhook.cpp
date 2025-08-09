#include "renderhook.h"
#include "dx9hook.h"
#include "dx11hook.h"
#include "dx12hook.h"
#include "openglhook.h"
#include "vulkanhook.h"
#include <iostream>

namespace RenderHook {
    RenderBackend currentBackend;
    bool isHooked = false;

    void SetRenderingBackend(RenderBackend backend) {
        if (isHooked) {
            std::cout << "[!] Already hooked!" << std::endl;
            return;
        }

        currentBackend = backend;
        isHooked = true;

        if (backend == DIRECTX9) {
            DX9::Hook();
            return;
        }

        if (backend == DIRECTX11) {
            DX11::Hook();
            return;
        }

        if (backend == DIRECTX12) {
            DX12::Hook();
            return;
        }

        if (backend == OPENGL) {
            OpenGL::Hook();
            return;
        }

        //if (backend == VULKAN) {
        //    Vulkan::Hook();
        //    return;
        //}
    }

    void Unhook() {
        if (!isHooked) {
            return;
        }

        if (currentBackend == DIRECTX9) {
            DX9::Unhook();
        }

        if (currentBackend == DIRECTX11) {
            DX11::Unhook();
        }

        if (currentBackend == DIRECTX12) {
            DX12::Unhook();
        }

        if (currentBackend == OPENGL) {
            OpenGL::Unhook();
        }

        //if (currentBackend == VULKAN) {
        //    Vulkan::Unhook();
        //}

        isHooked = false;
    }
}