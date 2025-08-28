#include "installer_app.h"
#include "resource.h"
#include "../../src/translation.h"

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

    // Initialize translations now that g_installer is set and GetResourcePath() works.
    initTranslation();

    // Show the welcome screen via ShowScreen so it is recorded in navigation
    // history, allowing Back from the Mode Selection screen to return here.
    g_installer->ShowScreen(IDD_WELCOME);
    if (g_hwnd)
    {
        // Call Mac-specific main loop
        return mac_main(argc, argv);
    }
    else
    {
        delete g_installer;
        return 1;
    }
}

