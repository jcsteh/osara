#include "installer_app.h"
#include "resource.h"

#include "swell.h"

// Forward declarations for Mac-specific functions
extern int mac_main(int argc, char* argv[]);
extern void mac_initialize_swell();

// Global variables
HWND g_hwnd = NULL;
HINSTANCE g_hInst = NULL;
InstallerApp* g_installer = NULL;

// Include SWELL-generated resources for macOS
#include "swell-dlggen.h"
#include "installer.rc_mac_dlg"
#include "swell-menugen.h"
#include "installer.rc_mac_menu"

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

