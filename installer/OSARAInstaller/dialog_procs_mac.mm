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
    int result = MessageBox(hwnd, translate("Are you sure you want to cancel the installation?"), 
                           translate("Cancel Installation"), MB_YESNO | MB_ICONQUESTION);
    return (result == IDYES);
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
    
    // Set initial directory to Applications if it exists
    NSString* applicationsPath = @"/Applications";
    if ([[NSFileManager defaultManager] fileExistsAtPath:applicationsPath])
    {
        [openPanel setDirectoryURL:[NSURL fileURLWithPath:applicationsPath]];
    }
    
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

