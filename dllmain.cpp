#include <Windows.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include "renderhook.h"

void SetupConsole() {
    AllocConsole();

    FILE* pCout;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    FILE* pCerr;
    freopen_s(&pCerr, "CONOUT$", "w", stderr);
    FILE* pCin;
    freopen_s(&pCin, "CONIN$", "r", stdin);

    SetConsoleTitleA("Hook Console");

    std::cout << "[+] Console allocated successfully" << std::endl;
}

void CleanupConsole() {
    fclose(stdout);
    fclose(stderr);
    fclose(stdin);
    FreeConsole();
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    SetupConsole();
    std::cout << "[+] DLL Injected successfully" << std::endl;

    // Set the rendering backend
    RenderHook::SetRenderingBackend(DIRECTX11);

    std::cout << "[*] Press END to unload" << std::endl;

    while (!GetAsyncKeyState(VK_END)) {
        Sleep(100);
    }

    std::cout << "[*] Unloading..." << std::endl;
    RenderHook::Unhook();

    Sleep(1000);
    CleanupConsole();

    FreeLibraryAndExitThread((HMODULE)lpParam, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }

    return TRUE;
}