# RenderHook

Minimal render-hooking framework for **Windows** that injects a small **ImGui** overlay into apps using **DirectX 9/11/12** or **OpenGL**. Built with **MinHook**. Educational use.

## What it does
- Installs function hooks on the app’s render path.
- Boots ImGui (Win32 + backend) and draws per-frame UI.
- Unhooks cleanly and restores original state.

> Not for protected/anti‑cheat environments.


## Backends
- **DX9** – `Present`, `Reset`
- **DX11** – `IDXGISwapChain::Present`, `ResizeBuffers` (vtable via dummy device)
- **DX12** – swap chain present path, RTV heap, per‑frame contexts
- **OpenGL** – `wglSwapBuffers`


## Quick start

**Requirements**
- Windows 10/11, Visual Studio 2022 (C++17+), Windows SDK
- Deps: [MinHook], [Dear ImGui] (`imgui_impl_win32`, `imgui_impl_dx9/dx11/dx12`, `imgui_impl_opengl3`)

**Build**
1) Open the project in VS.  
2) Select **x86**/**x64** to match the target process.  
3) Build the **DLL** in *Release*.

**Inject**
- Inject the DLL into a process using the desired API.
- Choose a backend in `dllmain.cpp`:

```cpp
RenderHook::SetRenderingBackend(DIRECTX11);
```

**Controls**
- **HOME** – toggle overlay  
- **END**  – unhook + unload


## How it’s wired (short)
1) `DllMain` spawns a worker thread + console logs.  
2) `RenderHook::SetRenderingBackend(...)` installs API‑specific hooks.  
3) Vtable/function resolution per backend.  
4) ImGui init: Win32 + renderer backend.  
5) Hooked `Present/SwapBuffers` → new frame → draw → call original.


## Files
```
/dllmain.cpp
/renderhook.h|.cpp
/dx9hook.cpp
/dx11hook.cpp
/dx12hook.cpp
/openglhook.cpp
```

## API
```cpp
enum RenderBackend { DIRECTX9, DIRECTX11, DIRECTX12, OPENGL };

namespace RenderHook {
  void SetRenderingBackend(RenderBackend backend);
  void Unhook();
}
```

## Roadmap
- Automatic API detection.  
- Shared helpers across backends.  
- Vulkan backend.

## Images

![image](https://github.com/user-attachments/assets/53478bf0-e700-4eaa-b453-81e28253da84)
<img width="1633" height="1268" alt="image2" src="https://github.com/user-attachments/assets/81672f5d-3942-45de-9db3-5898dc7da6b8" />

## Credits
- [TsudaKageyu/minhook] – function hooking
- [ocornut/imgui] – UI

## License
Pick a permissive license (MIT/BSD/Apache‑2.0).

[MinHook]: https://github.com/TsudaKageyu/minhook
[Dear ImGui]: https://github.com/ocornut/imgui
[TsudaKageyu/minhook]: https://github.com/TsudaKageyu/minhook
[ocornut/imgui]: https://github.com/ocornut/imgui
