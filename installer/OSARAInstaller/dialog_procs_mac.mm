#include "installer_app.h"
#include "resource.h"
#include "../../src/translation.h"
#include <string>

#include "swell.h"
#import <Cocoa/Cocoa.h>

// Mac-specific error dialog using native Cocoa
void mac_show_error(HWND hwnd, const char* message)
{
    // Use SWELL's MessageBox for consistency with the rest of the UI
    MessageBox(hwnd, message, translate("OSARA Installer Error"), MB_OK | MB_ICONERROR);
}

// Mac-specific confirmation dialog using native Cocoa
bool mac_confirm_cancel(HWND hwnd)
{
    bool isUninstall = g_installer &&
        g_installer->GetState().operationMode == MODE_UNINSTALL;
    const char* message = isUninstall
        ? translate("Are you sure you want to cancel the uninstallation?")
        : translate("Are you sure you want to cancel the installation?");
    const char* title = isUninstall
        ? translate("Cancel Uninstallation")
        : translate("Cancel Installation");
    int result = MessageBox(hwnd, message, title, MB_YESNO | MB_ICONQUESTION);
    return (result == IDYES);
}

// Check whether REAPER is currently running using the Cocoa workspace API.
// This avoids forking a shell process and doesn't depend on pgrep being in PATH.
bool mac_is_reaper_running()
{
    NSArray<NSRunningApplication*>* apps =
        [[NSWorkspace sharedWorkspace] runningApplications];
    for (NSRunningApplication* app in apps)
    {
        if ([app.bundleIdentifier isEqualToString:@"com.cockos.reaper"])
            return true;
    }
    return false;
}

// Mac-specific app termination
void mac_terminate_app()
{
    // On macOS, properly terminate the app when user chooses to quit
    [NSApp terminate:nil];
}

// Mac-specific folder browser using native NSOpenPanel
bool mac_browse_for_folder(HWND hwnd, std::string& selectedPath)
{
    // Use native macOS folder selection dialog
    NSOpenPanel* openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseFiles:NO];
    [openPanel setCanChooseDirectories:YES];
    [openPanel setAllowsMultipleSelection:NO];
    [openPanel setTitle:@"Select REAPER Folder"];
    [openPanel setMessage:@"Choose the REAPER folder for portable installation"];
    
    // Default to the user's home directory.  Portable REAPER installations
    // live wherever the user chose to put them, so starting at the home
    // directory is a better neutral starting point than /Applications (which
    // is where REAPER.app lives, not its data folder).
    [openPanel setDirectoryURL:[NSURL fileURLWithPath:NSHomeDirectory()]];
    
    NSModalResponse result = [openPanel runModal];
    if (result == NSModalResponseOK)
    {
        NSURL* selectedURL = [[openPanel URLs] objectAtIndex:0];
        NSString* selectedPathNS = [selectedURL path];
        
        // Convert NSString to C++ string
        const char* pathCStr = [selectedPathNS UTF8String];
        selectedPath = pathCStr;
        return true;
    }
    
    return false;
}

