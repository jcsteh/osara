#include "installer_app.h"
#include "resource.h"
#include "../../src/translation.h"
#include <string>

#include "swell.h"

// Forward declarations for Mac-specific functions
extern void mac_show_error(HWND hwnd, const char* message);
extern bool mac_confirm_cancel(HWND hwnd);
extern void mac_terminate_app();
extern bool mac_browse_for_folder(HWND hwnd, std::string& selectedPath);

// Helper function to show error messages
void ShowError(HWND hwnd, const char* message)
{
    mac_show_error(hwnd, message);
}

// Helper function to confirm cancellation
bool ConfirmCancel(HWND hwnd)
{
    return mac_confirm_cancel(hwnd);
}

static bool handleCancelButton(HWND hwnd) {
    if (ConfirmCancel(hwnd))
    {
        mac_terminate_app();
    }
    return TRUE;
}

// Welcome Dialog Procedure
INT_PTR CALLBACK WelcomeDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            g_hwnd = hwnd;
            
            // Localize dialog using main OSARA approach
            translateDialog(hwnd);
            
            // Center the dialog
            RECT rect;
            GetWindowRect(hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
            int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
            SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            
            // Set focus to Continue button
            SetFocus(GetDlgItem(hwnd, IDC_CONTINUE));
            return FALSE; // We set focus manually
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_CONTINUE:
                    if (g_installer)
                    {
                        g_installer->ShowScreen(IDD_MODE_SELECTION);
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    return handleCancelButton(hwnd);
            }
            break;
            
        case WM_DESTROY:
            // Don't automatically terminate app during dialog transitions
            // Only clean up if this was the current dialog
            printf("DEBUG: WM_DESTROY received for hwnd=%p, g_hwnd=%p\n", hwnd, g_hwnd);
            if (hwnd == g_hwnd) {
                printf("DEBUG: Clearing g_hwnd - dialog being destroyed is current dialog\n");
                g_hwnd = NULL;
            } else {
                printf("DEBUG: Dialog being destroyed is not current dialog\n");
            }
            return TRUE;
    }
    return FALSE;
}

// No OSARA Found Dialog Procedure
INT_PTR CALLBACK NoOSARAFoundDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            g_hwnd = hwnd;
            
            // Localize dialog using main OSARA approach
            translateDialog(hwnd);
            
            // Set focus to Back button
            SetFocus(GetDlgItem(hwnd, IDC_BACK));
            return FALSE; // We set focus manually
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_BACK:
                    if (g_installer)
                    {
                        // Go back to the Install Type dialog so user can try different location
                        g_installer->ShowPreviousScreen();
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    // Close button from "No OSARA Found" dialog - terminate directly without confirmation
                    mac_terminate_app();
                    return TRUE;
            }
            break;
            
        case WM_DESTROY:
            if (hwnd == g_hwnd) {
                g_hwnd = NULL;
            }
            return TRUE;
    }
    return FALSE;
}

// Mode Selection Dialog Procedure
INT_PTR CALLBACK ModeSelectionDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            g_hwnd = hwnd;
            
            // Localize dialog using main OSARA approach
            translateDialog(hwnd);
            
            // Set default to install mode
            CheckDlgButton(hwnd, IDC_MODE_INSTALL, BST_CHECKED);
            
            // Always enable the Continue button - no detection logic
            EnableWindow(GetDlgItem(hwnd, IDC_CONTINUE), TRUE);
            
            if (g_installer)
            {
                g_installer->GetState().operationMode = MODE_INSTALL;
            }
            
            // Set focus to install radio button
            SetFocus(GetDlgItem(hwnd, IDC_MODE_INSTALL));
            return FALSE; // We set focus manually
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_MODE_INSTALL:
                    if (HIWORD(wParam) == BN_CLICKED && g_installer)
                    {
                        CheckDlgButton(hwnd, IDC_MODE_INSTALL, BST_CHECKED);
                        CheckDlgButton(hwnd, IDC_MODE_UNINSTALL, BST_UNCHECKED);
                        SetFocus(GetDlgItem(hwnd, IDC_MODE_INSTALL));
                        g_installer->GetState().operationMode = MODE_INSTALL;
                    }
                    return TRUE;
                    
                case IDC_MODE_UNINSTALL:
                    if (HIWORD(wParam) == BN_CLICKED && g_installer)
                    {
                        CheckDlgButton(hwnd, IDC_MODE_UNINSTALL, BST_CHECKED);
                        CheckDlgButton(hwnd, IDC_MODE_INSTALL, BST_UNCHECKED);
                        SetFocus(GetDlgItem(hwnd, IDC_MODE_UNINSTALL));
                        g_installer->GetState().operationMode = MODE_UNINSTALL;
                        printf("DEBUG: Uninstall mode selected, Continue button should be enabled\n");
                        
                        // Ensure Continue button is enabled
                        EnableWindow(GetDlgItem(hwnd, IDC_CONTINUE), TRUE);
                    }
                    return TRUE;
                    
                case IDC_CONTINUE:
                    if (g_installer)
                    {
                        if (g_installer->GetState().operationMode == MODE_INSTALL)
                        {
                            g_installer->ShowScreen(IDD_LICENSE);
                        }
                        else
                        {
                            // For uninstall, skip license and go to install type (location selection)
                            g_installer->ShowScreen(IDD_INSTALL_TYPE);
                        }
                    }
                    return TRUE;
                    
                case IDC_BACK:
                    if (g_installer)
                    {
                        g_installer->ShowPreviousScreen();
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    return handleCancelButton(hwnd);
            }
            break;
            
        case WM_DESTROY:
            if (hwnd == g_hwnd) {
                g_hwnd = NULL;
            }
            return TRUE;
    }
    return FALSE;
}

// License Dialog Procedure
INT_PTR CALLBACK LicenseDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            g_hwnd = hwnd;
            
            // Localize dialog using main OSARA approach
            translateDialog(hwnd);
            
            // Load and display license text
            if (g_installer)
            {
                std::string licenseText = g_installer->LoadLicenseText();
                SetDlgItemText(hwnd, IDC_LICENSE_TEXT, licenseText.c_str());
            }
            
            // Ensure Continue button is disabled initially
            EnableWindow(GetDlgItem(hwnd, IDC_CONTINUE), FALSE);
            
            // Set initial focus to accept checkbox
            SetFocus(GetDlgItem(hwnd, IDC_ACCEPT_LICENSE));
            return FALSE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_ACCEPT_LICENSE:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        bool checked = (IsDlgButtonChecked(hwnd, IDC_ACCEPT_LICENSE) == BST_CHECKED);
                        EnableWindow(GetDlgItem(hwnd, IDC_CONTINUE), checked);
                        if (g_installer)
                        {
                            g_installer->GetState().licenseAccepted = checked;
                        }
                    }
                    return TRUE;
                    
                case IDC_CONTINUE:
                    if (g_installer && g_installer->GetState().licenseAccepted)
                    {
                        g_installer->ShowScreen(IDD_INSTALL_TYPE);
                    }
                    return TRUE;
                    
                case IDC_BACK:
                    if (g_installer)
                    {
                        g_installer->ShowPreviousScreen();
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    handleCancelButton(hwnd);
            }
            break;
            
        case WM_DESTROY:
            // Don't automatically terminate app during dialog transitions
            // Only clean up if this was the current dialog
            if (hwnd == g_hwnd) {
                g_hwnd = NULL;
            }
            return TRUE;
    }
    return FALSE;
}

// Installation Type Dialog Procedure
INT_PTR CALLBACK InstallTypeDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            g_hwnd = hwnd;
            
            // Localize dialog using main OSARA approach
            translateDialog(hwnd);
            
            // Set default to standard installation
            CheckDlgButton(hwnd, IDC_STANDARD_INSTALL, BST_CHECKED);
            
            // Disable portable path controls initially
            EnableWindow(GetDlgItem(hwnd, IDC_PORTABLE_PATH), FALSE);
            EnableWindow(GetDlgItem(hwnd, IDC_BROWSE_PATH), FALSE);
            
            if (g_installer)
            {
                // Set default install path
                std::string defaultPath = g_installer->GetDefaultInstallPath();
                g_installer->GetState().installPath = defaultPath;
                g_installer->GetState().installType = INSTALL_STANDARD;
            }
            
            // Set focus to the currently selected radio button
            SetFocus(GetDlgItem(hwnd, IDC_STANDARD_INSTALL));
            return FALSE; // We set focus manually
        }
        
        
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_STANDARD_INSTALL:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        // Ensure mutual exclusion with portable install radio button
                        CheckDlgButton(hwnd, IDC_STANDARD_INSTALL, BST_CHECKED);
                        CheckDlgButton(hwnd, IDC_PORTABLE_INSTALL, BST_UNCHECKED);
                        
                        EnableWindow(GetDlgItem(hwnd, IDC_PORTABLE_PATH), FALSE);
                        EnableWindow(GetDlgItem(hwnd, IDC_BROWSE_PATH), FALSE);
                        
                        // Clear the portable path edit box when switching to standard
                        SetDlgItemText(hwnd, IDC_PORTABLE_PATH, "");
                        
                        SetFocus(GetDlgItem(hwnd, IDC_STANDARD_INSTALL));
                        if (g_installer)
                        {
                            g_installer->GetState().installType = INSTALL_STANDARD;
                            g_installer->GetState().installPath = g_installer->GetDefaultInstallPath();
                        }
                    }
                    return TRUE;
                    
                case IDC_PORTABLE_INSTALL:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        // Ensure mutual exclusion with standard install radio button
                        CheckDlgButton(hwnd, IDC_PORTABLE_INSTALL, BST_CHECKED);
                        CheckDlgButton(hwnd, IDC_STANDARD_INSTALL, BST_UNCHECKED);
                        
                        EnableWindow(GetDlgItem(hwnd, IDC_PORTABLE_PATH), TRUE);
                        EnableWindow(GetDlgItem(hwnd, IDC_BROWSE_PATH), TRUE);
                        SetFocus(GetDlgItem(hwnd, IDC_PORTABLE_INSTALL));
                        if (g_installer)
                        {
                            g_installer->GetState().installType = INSTALL_PORTABLE;
                        }
                    }
                    return TRUE;
                    
                case IDC_BROWSE_PATH:
                {
                    std::string selectedPath;
                    bool success = false;
                    
                    // Use Mac-specific folder browser
                    success = mac_browse_for_folder(hwnd, selectedPath);
                    
                    if (success)
                    {
                        SetDlgItemText(hwnd, IDC_PORTABLE_PATH, selectedPath.c_str());
                        if (g_installer)
                        {
                            g_installer->GetState().installPath = selectedPath;
                        }
                    }
                    return TRUE;
                }
                
                case IDC_PORTABLE_PATH:
                    if (HIWORD(wParam) == EN_CHANGE && g_installer)
                    {
                        char path[MAX_PATH];
                        GetDlgItemText(hwnd, IDC_PORTABLE_PATH, path, sizeof(path));
                        g_installer->GetState().installPath = path;
                    }
                    return TRUE;
                    
                case IDC_CONTINUE:
                    if (g_installer)
                    {
                        if (g_installer->GetState().operationMode == MODE_INSTALL)
                        {
                            // Validate installation path for install mode
                            if (g_installer->GetState().installType == INSTALL_PORTABLE)
                            {
                                if (g_installer->GetState().installPath.empty())
                                {
                                    ShowError(hwnd, translate("Please specify a REAPER folder path for portable installation."));
                                    SetFocus(GetDlgItem(hwnd, IDC_PORTABLE_PATH));
                                    return TRUE;
                                }
                                if (!g_installer->ValidateInstallPath(g_installer->GetState().installPath))
                                {
                                    ShowError(hwnd, translate("The specified path does not appear to be a valid REAPER folder."));
                                    SetFocus(GetDlgItem(hwnd, IDC_PORTABLE_PATH));
                                    return TRUE;
                                }
                            }
                            g_installer->ShowScreen(IDD_KEYMAP);
                        }
                        else
                        {
                            // For uninstall mode, validate portable path and check for OSARA installation
                            if (g_installer->GetState().installType == INSTALL_PORTABLE)
                            {
                                if (g_installer->GetState().installPath.empty())
                                {
                                    ShowError(hwnd, translate("Please specify a REAPER folder path for portable uninstallation."));
                                    SetFocus(GetDlgItem(hwnd, IDC_PORTABLE_PATH));
                                    return TRUE;
                                }
                            }
                            
                            // Check for OSARA installation BEFORE showing uninstall confirmation dialog
                            std::string basePath;
                            if (g_installer->GetState().installType == INSTALL_STANDARD)
                            {
                                basePath = g_installer->GetDefaultInstallPath();
                            }
                            else
                            {
                                basePath = g_installer->GetState().installPath;
                            }
                            
                            // Check if OSARA is installed at this location
                            if (!g_installer->IsOSARAInstalledAt(basePath))
                            {
                                // OSARA not found - show the "No OSARA Found" dialog directly
                                g_installer->ShowScreen(IDD_NO_OSARA_FOUND);
                            }
                            else
                            {
                                // OSARA found - proceed to uninstall confirmation
                                g_installer->ShowScreen(IDD_UNINSTALL_CONFIRM);
                            }
                        }
                    }
                    return TRUE;
                    
                case IDC_BACK:
                    if (g_installer)
                    {
                        g_installer->ShowPreviousScreen();
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    handleCancelButton(hwnd);
            }
            break;
            
        case WM_DESTROY:
            // Don't automatically terminate app during dialog transitions
            // Only clean up if this was the current dialog
            if (hwnd == g_hwnd) {
                g_hwnd = NULL;
            }
            return TRUE;
    }
    return FALSE;
}

// Keymap Dialog Procedure
INT_PTR CALLBACK KeymapDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            g_hwnd = hwnd;
            
            // Localize dialog using main OSARA approach
            translateDialog(hwnd);
            
            // Set default to keep existing keymap (most conservative option)
            CheckDlgButton(hwnd, IDC_KEEP_KEYMAP, BST_CHECKED);
            
            if (g_installer)
            {
                g_installer->GetState().keymapOption = KEYMAP_KEEP;
            }
            
            // Set focus to the currently selected radio button
            SetFocus(GetDlgItem(hwnd, IDC_KEEP_KEYMAP));
            return FALSE; // We set focus manually
        }
        
        
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_INSTALL_KEYMAP:
                    if (HIWORD(wParam) == BN_CLICKED && g_installer)
                    {
                        // Ensure mutual exclusion with other keymap radio buttons
                        CheckDlgButton(hwnd, IDC_INSTALL_KEYMAP, BST_CHECKED);
                        CheckDlgButton(hwnd, IDC_KEEP_KEYMAP, BST_UNCHECKED);
                        
                        SetFocus(GetDlgItem(hwnd, IDC_INSTALL_KEYMAP));
                        g_installer->GetState().keymapOption = KEYMAP_INSTALL;
                    }
                    return TRUE;
                    
                case IDC_KEEP_KEYMAP:
                    if (HIWORD(wParam) == BN_CLICKED && g_installer)
                    {
                        // Ensure mutual exclusion with other keymap radio buttons
                        CheckDlgButton(hwnd, IDC_KEEP_KEYMAP, BST_CHECKED);
                        CheckDlgButton(hwnd, IDC_INSTALL_KEYMAP, BST_UNCHECKED);
                        
                        SetFocus(GetDlgItem(hwnd, IDC_KEEP_KEYMAP));
                        g_installer->GetState().keymapOption = KEYMAP_KEEP;
                    }
                    return TRUE;
                    
                case IDC_CONTINUE:
                    if (g_installer)
                    {
                        // Perform installation directly instead of showing progress dialog
                        bool success = g_installer->PerformInstallation();
                        g_installer->ShowScreen(IDD_COMPLETION);
                    }
                    return TRUE;
                    
                case IDC_BACK:
                    if (g_installer)
                    {
                        g_installer->ShowPreviousScreen();
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    return handleCancelButton(hwnd);
            }
            break;
            
        case WM_DESTROY:
            // Don't automatically terminate app during dialog transitions
            // Only clean up if this was the current dialog
            if (hwnd == g_hwnd) {
                g_hwnd = NULL;
            }
            return TRUE;
    }
    return FALSE;
}

// Uninstall Confirmation Dialog Procedure
INT_PTR CALLBACK UninstallConfirmDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            // OSARA detection now happens in InstallTypeDlgProc before this dialog is shown
            // So we can assume OSARA is installed and proceed with UI setup
            g_hwnd = hwnd;
            
            // Localize dialog using main OSARA approach
            translateDialog(hwnd);
            
            // Disable Continue button initially
            EnableWindow(GetDlgItem(hwnd, IDC_CONTINUE), FALSE);
            
            if (g_installer)
            {
                // Get the base path for uninstallation (we know OSARA is installed here)
                std::string basePath;
                if (g_installer->GetState().installType == INSTALL_STANDARD)
                {
                    basePath = g_installer->GetDefaultInstallPath();
                }
                else
                {
                    basePath = g_installer->GetState().installPath;
                }
                
                // Get list of files that will be removed
                g_installer->GetState().filesToRemove = g_installer->GetInstalledFilesAt(basePath);
                
                // Build list of files to be removed
                std::string fileList = translate("The following OSARA files will be removed:\n\n");
                fileList += translate("Location: ") + basePath + "\n\n";
                
                fileList += translate("Files to be removed:\n");
                for (const std::string& file : g_installer->GetState().filesToRemove)
                {
                    // Show relative path for readability
                    std::string relativePath = file;
                    if (relativePath.find(basePath) == 0)
                    {
                        relativePath = relativePath.substr(basePath.length());
                        if (relativePath[0] == '/' || relativePath[0] == '\\')
                        {
                            relativePath = relativePath.substr(1);
                        }
                    }
                    fileList += "- " + relativePath + "\n";
                }
                
                fileList += "\n" + std::string(translate("Your keymap configuration (reaper-kb.ini) will NOT be modified."));
                fileList += "\n\n" + std::string(translate("This action cannot be undone."));
                
                SetDlgItemText(hwnd, IDC_UNINSTALL_FILE_LIST, fileList.c_str());
            }
            
            // Set focus to confirmation checkbox
            SetFocus(GetDlgItem(hwnd, IDC_CONFIRM_UNINSTALL));
            return FALSE; // We set focus manually
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_CONFIRM_UNINSTALL:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        bool checked = (IsDlgButtonChecked(hwnd, IDC_CONFIRM_UNINSTALL) == BST_CHECKED);
                        EnableWindow(GetDlgItem(hwnd, IDC_CONTINUE), checked);
                    }
                    return TRUE;
                    
                case IDC_CONTINUE:
                    if (g_installer && IsDlgButtonChecked(hwnd, IDC_CONFIRM_UNINSTALL) == BST_CHECKED)
                    {
                        // Perform uninstallation directly instead of showing progress dialog
                        bool success = g_installer->PerformUninstallation();
                        if (success)
                        {
                            g_installer->ShowScreen(IDD_COMPLETION);
                        }
                        // If failed, the error handling is done in PerformUninstallation
                    }
                    return TRUE;
                    
                case IDC_BACK:
                    if (g_installer)
                    {
                        g_installer->ShowPreviousScreen();
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    return handleCancelButton(hwnd);
            }
            break;
            
        case WM_DESTROY:
            if (hwnd == g_hwnd) {
                g_hwnd = NULL;
            }
            return TRUE;
    }
    return FALSE;
}


// Completion Dialog Procedure
INT_PTR CALLBACK CompletionDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            g_hwnd = hwnd;
            
            // Localize dialog using main OSARA approach
            translateDialog(hwnd);
            
            // Display results based on operation mode and success/failure
            if (g_installer)
            {
                if (g_installer->GetState().operationMode == MODE_INSTALL)
                {
                    if (g_installer->GetState().installationSucceeded)
                    {
                        // Set success title and message
                        SetWindowText(hwnd, translate("Installation Complete"));
                        SetDlgItemText(hwnd, IDC_SUCCESS_TEXT, translate("OSARA has been successfully installed!"));
                        
                        std::string successMessage = std::string(translate("Installation completed successfully!")) + "\n\n";
                        
                        if (g_installer->GetState().installType == INSTALL_STANDARD)
                        {
                            successMessage += std::string(translate("OSARA has been installed to the standard REAPER location:")) + "\n";
                            successMessage += g_installer->GetDefaultInstallPath() + "\n\n";
                        }
                        else
                        {
                            successMessage += std::string(translate("OSARA has been installed to the portable REAPER location:")) + "\n";
                            successMessage += g_installer->GetState().installPath + "\n\n";
                        }
                        
                        successMessage += std::string(translate("Files installed:")) + "\n";
                        successMessage += std::string(translate("- OSARA plugin (reaper_osara.dylib)")) + "\n";
                        
                        if (g_installer->GetState().keymapOption == KEYMAP_INSTALL)
                        {
                            successMessage += std::string(translate("- OSARA keymap configuration")) + "\n";
                            if (!g_installer->GetState().keymapBackupPath.empty())
                            {
                                successMessage += std::string(translate("- Previous keymap backed up to: ")) + g_installer->GetState().keymapBackupPath + "\n";
                            }
                        }
                        
                        SetDlgItemText(hwnd, IDC_INSTALL_LOG, successMessage.c_str());
                    }
                    else
                    {
                        // Set failure title and message
                        SetWindowText(hwnd, translate("Installation Failed"));
                        SetDlgItemText(hwnd, IDC_SUCCESS_TEXT, translate("Installation could not be completed."));
                        
                        std::string failureMessage = std::string(translate("Installation Failed")) + "\n\n";
                        failureMessage += std::string(translate("The OSARA installation could not be completed.")) + "\n\n";
                        failureMessage += std::string(translate("Error details:")) + "\n";
                        failureMessage += g_installer->GetState().lastError;
                        failureMessage += "\n\n" + std::string(translate("Please check the following:")) + "\n";
                        failureMessage += std::string(translate("- You have write permissions to the installation directory")) + "\n";
                        failureMessage += std::string(translate("- The selected path is a valid REAPER folder (for portable installation)")) + "\n";
                        failureMessage += std::string(translate("- There is sufficient disk space")) + "\n";
                        failureMessage += std::string(translate("- No antivirus software is blocking the installation")) + "\n\n";
                        failureMessage += std::string(translate("You may try running the installer again or contact support for assistance."));
                        
                        SetDlgItemText(hwnd, IDC_INSTALL_LOG, failureMessage.c_str());
                    }
                }
                else // MODE_UNINSTALL
                {
                    if (g_installer->GetState().uninstallationSucceeded)
                    {
                        // Set success title and message
                        SetWindowText(hwnd, translate("Uninstallation Complete"));
                        SetDlgItemText(hwnd, IDC_SUCCESS_TEXT, translate("OSARA has been successfully uninstalled!"));
                        
                        std::string successMessage = std::string(translate("Uninstallation completed successfully!")) + "\n\n";
                        
                        std::string uninstallPath;
                        if (g_installer->GetState().installType == INSTALL_STANDARD)
                        {
                            uninstallPath = g_installer->GetDefaultInstallPath();
                            successMessage += std::string(translate("OSARA has been removed from the standard REAPER location:")) + "\n";
                        }
                        else
                        {
                            uninstallPath = g_installer->GetState().installPath;
                            successMessage += std::string(translate("OSARA has been removed from the portable REAPER location:")) + "\n";
                        }
                        successMessage += uninstallPath + "\n\n";
                        
                        successMessage += std::string(translate("Files removed:")) + "\n";
                        for (const std::string& file : g_installer->GetState().filesToRemove)
                        {
                            std::string relativePath = file;
                            if (relativePath.find(uninstallPath) == 0)
                            {
                                relativePath = relativePath.substr(uninstallPath.length());
                                if (relativePath[0] == '/' || relativePath[0] == '\\')
                                {
                                    relativePath = relativePath.substr(1);
                                }
                            }
                            successMessage += "- " + relativePath + "\n";
                        }
                        
                        successMessage += "\n" + std::string(translate("Your keymap configuration (reaper-kb.ini) was preserved."));
                        successMessage += "\n" + std::string(translate("OSARA has been completely removed from your system."));
                        
                        SetDlgItemText(hwnd, IDC_INSTALL_LOG, successMessage.c_str());
                    }
                    else
                    {
                        // Set failure title and message
                        SetWindowText(hwnd, translate("Uninstallation Failed"));
                        SetDlgItemText(hwnd, IDC_SUCCESS_TEXT, translate("Uninstallation could not be completed."));
                        
                        std::string failureMessage = std::string(translate("Uninstallation Failed")) + "\n\n";
                        failureMessage += std::string(translate("The OSARA uninstallation could not be completed.")) + "\n\n";
                        failureMessage += std::string(translate("Error details:")) + "\n";
                        failureMessage += g_installer->GetState().lastError;
                        failureMessage += "\n\n" + std::string(translate("Please check the following:")) + "\n";
                        failureMessage += std::string(translate("- You have write permissions to the REAPER directory")) + "\n";
                        failureMessage += std::string(translate("- REAPER is not currently running")) + "\n";
                        failureMessage += std::string(translate("- No antivirus software is blocking file removal")) + "\n\n";
                        failureMessage += std::string(translate("You may try running the uninstaller again or manually remove the remaining files."));
                        
                        SetDlgItemText(hwnd, IDC_INSTALL_LOG, failureMessage.c_str());
                    }
                }
            }
            else
            {
                // Set unknown error title and message
                SetWindowText(hwnd, translate("Installation Error"));
                SetDlgItemText(hwnd, IDC_SUCCESS_TEXT, translate("An unexpected error occurred."));
                SetDlgItemText(hwnd, IDC_INSTALL_LOG, 
                              translate("Installation status unknown - installer error occurred."));
            }
            
            return TRUE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDOK:
                    mac_terminate_app();
                    return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // Only terminate app if this dialog being destroyed is the current dialog
            // During transitions, g_hwnd points to the new dialog, so old dialog destruction won't terminate app
            if (hwnd == g_hwnd) {
                g_hwnd = NULL;
                mac_terminate_app();
            }
            return TRUE;
    }
    return FALSE;
}
