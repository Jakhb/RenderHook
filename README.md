# RenderHook
The idea was to be able to learn how DirectX, Vulkan and OpenGL worked and be able to use ImGUI. I have tested it for different x86 and x64 games using the APIs, and it seems to work as intended. 

## Status
Work in progress. May rewrite parts and may or may not try implement the Vulkan API. 

## Building


I am no C++ genius, and its not a language I have been working that much with, so there may or may not be some code smells. However, you should be able to open the project and then just build the project. You may need to install the different API SDKs (Uncertain). 

Within the DllMain you set the rendering API. (Stole the idea from bruhmoment21)
```c++
DWORD WINAPI MainThread(LPVOID lpParam) {
   ...

    // Set the rendering backend
    RenderHook::SetRenderingBackend(DIRECTX11);

    ...
}
```

![image](https://github.com/user-attachments/assets/53478bf0-e700-4eaa-b453-81e28253da84)
<img width="1633" height="1268" alt="image2" src="https://github.com/user-attachments/assets/81672f5d-3942-45de-9db3-5898dc7da6b8" />

## References
http://www.directxtutorial.com <br>
https://www.opengl.org/ <br>
https://github.com/Rebzzel/kiero <br>
https://github.com/bruhmoment21/UniversalHookX <br>
https://github.com/TsudaKageyu/minhook <br>
