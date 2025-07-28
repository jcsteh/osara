#include "installer_app.h"

#ifndef _WIN32
#include "swell.h"
#import <Cocoa/Cocoa.h>

// External global variables defined in main.cpp
extern HWND g_hwnd;
extern HINSTANCE g_hInst;
extern InstallerApp* g_installer;

// Mac-specific SWELL initialization
void mac_initialize_swell()
{
    // Initialize NSApplication first
    NSApplication* app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    
    // Initialize SWELL
    SWELL_EnsureMultithreadedCocoa();
}

// Mac-specific main loop
int mac_main(int argc, char* argv[])
{
    NSApplication* app = [NSApplication sharedApplication];
    
    if (g_hwnd)
    {
        // Make sure the app appears in the dock and can receive focus
        [app activateIgnoringOtherApps:YES];
        
        // Run Cocoa event loop
        [app run];
        
        delete g_installer;
        return 0;
    }
    else
    {
        NSLog(@"Failed to create welcome dialog");
        delete g_installer;
        return 1;
    }
}

#endif // _WIN32
