#include "installer_app.h"
#include "resource.h"

#ifndef _WIN32
#include "swell.h"
// Forward declarations for Mac-specific functions
extern int mac_main(int argc, char* argv[]);
extern void mac_initialize_swell();
#endif

// Global variables
HWND g_hwnd = NULL;
HINSTANCE g_hInst = NULL;
InstallerApp* g_installer = NULL;

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nShowCmd)
{
    g_hInst = hInstance;
    InitCommonControls();
    
    // Create installer instance
    g_installer = new InstallerApp();

    // Start with welcome dialog
    g_hwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_WELCOME), GetDesktopWindow(), WelcomeDlgProc);
    if (g_hwnd)
    {
        ShowWindow(g_hwnd, SW_SHOW);
    }

    // Message loop
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        if (!g_hwnd || !IsDialogMessage(g_hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    delete g_installer;
    return (int)msg.wParam;
}

#else // macOS using SWELL

// Include SWELL-generated resources for macOS
#include "swell-dlggen.h"
#include "installer.rc_mac_dlg"
#include "swell-menugen.h"
#include "installer.rc_mac_menu"

// SWELL app main function - required for standalone SWELL apps
int SWELL_dllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpReserved)
{
    return 1;
}

// Cross-platform main function for macOS
int main(int argc, char* argv[])
{
    // Call Mac-specific initialization
    mac_initialize_swell();
    
    // Set g_hInst for SWELL (required for CreateDialog to work)
    g_hInst = (HINSTANCE)1;
    
    // Create installer instance
    g_installer = new InstallerApp();
    
    // Create and show welcome dialog
    g_hwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_WELCOME), NULL, WelcomeDlgProc);
    if (g_hwnd)
    {
        ShowWindow(g_hwnd, SW_SHOW);
        SetFocus(g_hwnd);
        
        // Call Mac-specific main loop
        return mac_main(argc, argv);
    }
    else
    {
        delete g_installer;
        return 1;
    }
}

#endif // _WIN32
