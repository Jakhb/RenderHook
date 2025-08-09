#pragma once

enum RenderBackend {
    DIRECTX9,
    DIRECTX11,
    DIRECTX12,
    OPENGL,
    // VULKAN
};

namespace RenderHook {
    void SetRenderingBackend(RenderBackend backend);
    void Unhook();
}