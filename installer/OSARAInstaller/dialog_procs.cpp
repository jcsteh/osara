#include "installer_app.h"
#include "resource.h"
#include "../../src/translation.h"
#include <string>

#ifndef _WIN32
#include "swell.h"
// Forward declarations for Mac-specific functions
extern void mac_show_error(HWND hwnd, const char* message);
extern bool mac_confirm_cancel(HWND hwnd);
extern void mac_terminate_app();
extern bool mac_browse_for_folder(HWND hwnd, std::string& selectedPath);
#else
#include <shlobj.h>  // For SHBrowseForFolder
#endif

// Helper function to show error messages
void ShowError(HWND hwnd, const char* message)
{
#ifdef _WIN32
    MessageBox(hwnd, message, translate("OSARA Installer Error"), MB_OK | MB_ICONERROR);
#else
    mac_show_error(hwnd, message);
#endif
}

// Helper function to confirm cancellation
bool ConfirmCancel(HWND hwnd)
{
#ifdef _WIN32
    int result = MessageBox(hwnd, translate("Are you sure you want to cancel the installation?"), 
                           translate("Cancel Installation"), MB_YESNO | MB_ICONQUESTION);
    return (result == IDYES);
#else
    return mac_confirm_cancel(hwnd);
#endif
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
                        g_installer->ShowScreen(IDD_LICENSE);
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    if (ConfirmCancel(hwnd))
                    {
#ifdef _WIN32
                        PostQuitMessage(0);
#else
                        mac_terminate_app();
#endif
                    }
                    return TRUE;
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
                    if (ConfirmCancel(hwnd))
                    {
#ifdef _WIN32
                        PostQuitMessage(0);
#else
                        mac_terminate_app();
#endif
                    }
                    return TRUE;
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
                    
#ifdef _WIN32
                    // Windows implementation using SHBrowseForFolder
                    BROWSEINFO bi = {0};
                    bi.hwndOwner = hwnd;
                    bi.lpszTitle = "Select REAPER Folder";
                    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                    
                    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
                    if (pidl != NULL)
                    {
                        char path[MAX_PATH];
                        if (SHGetPathFromIDList(pidl, path))
                        {
                            selectedPath = path;
                            success = true;
                        }
                        CoTaskMemFree(pidl);
                    }
#else
                    // Use Mac-specific folder browser
                    success = mac_browse_for_folder(hwnd, selectedPath);
#endif
                    
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
                        // Validate installation path
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
                    return TRUE;
                    
                case IDC_BACK:
                    if (g_installer)
                    {
                        g_installer->ShowPreviousScreen();
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    if (ConfirmCancel(hwnd))
                    {
#ifdef _WIN32
                        PostQuitMessage(0);
#else
                        mac_terminate_app();
#endif
                    }
                    return TRUE;
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
                        g_installer->ShowScreen(IDD_PROGRESS);
                    }
                    return TRUE;
                    
                case IDC_BACK:
                    if (g_installer)
                    {
                        g_installer->ShowPreviousScreen();
                    }
                    return TRUE;
                    
                case IDCANCEL:
                    if (ConfirmCancel(hwnd))
                    {
#ifdef _WIN32
                        PostQuitMessage(0);
#else
                        mac_terminate_app();
#endif
                    }
                    return TRUE;
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

// Progress callback function
void ProgressCallback(int percent, const char* status)
{
    if (g_hwnd)
    {
        // Update progress bar
        SendDlgItemMessage(g_hwnd, IDC_PROGRESS_BAR, PBM_SETPOS, percent, 0);
        
        // Update status text
        SetDlgItemText(g_hwnd, IDC_STATUS_TEXT, status);
        
#ifdef _WIN32
        // Process messages to keep UI responsive (Windows only)
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (!IsDialogMessage(g_hwnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
#else
        // On macOS with SWELL, just update the display
        // SWELL handles message processing differently
#endif
    }
}

// Progress Dialog Procedure
INT_PTR CALLBACK ProgressDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
        {
            g_hwnd = hwnd;
            
            // Localize dialog using main OSARA approach
            translateDialog(hwnd);
            
            // Initialize progress bar
            SendDlgItemMessage(hwnd, IDC_PROGRESS_BAR, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendDlgItemMessage(hwnd, IDC_PROGRESS_BAR, PBM_SETPOS, 0, 0);
            
            if (g_installer)
            {
                // Set progress callback
                g_installer->SetProgressCallback(ProgressCallback);
                
                // Start installation in a separate thread (simplified for now)
                // In a real implementation, you'd use a worker thread
                SetTimer(hwnd, 1, 100, NULL); // Start installation after a brief delay
            }
            
            return TRUE;
        }
        
        case WM_TIMER:
            if (wParam == 1)
            {
                KillTimer(hwnd, 1);
                if (g_installer)
                {
                    bool success = g_installer->PerformInstallation();
                    // Always show completion screen - it will display success or failure
                    g_installer->ShowScreen(IDD_COMPLETION);
                }
            }
            return TRUE;
        
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDCANCEL:
                    // TODO: Implement installation cancellation
                    if (MessageBox(hwnd, translate("Are you sure you want to cancel the installation?\n\nThis may leave your system in an inconsistent state."),
                                  translate("Cancel Installation"), MB_YESNO | MB_ICONWARNING) == IDYES)
                    {
                        DestroyWindow(hwnd);
                    }
                    return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // Only terminate app if this dialog being destroyed is the current dialog
            // During transitions, g_hwnd points to the new dialog, so old dialog destruction won't terminate app
            if (hwnd == g_hwnd) {
                g_hwnd = NULL;
#ifdef _WIN32
                PostQuitMessage(0);
#else
                mac_terminate_app();
#endif
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
            
            // Display results based on installation success/failure
            if (g_installer)
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
                    
                    successMessage += "\n" + std::string(translate("You can now start REAPER to use OSARA's accessibility features."));
                    
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
#ifdef _WIN32
                    DestroyWindow(hwnd);
#else
                    mac_terminate_app();
#endif
                    return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // Only terminate app if this dialog being destroyed is the current dialog
            // During transitions, g_hwnd points to the new dialog, so old dialog destruction won't terminate app
            if (hwnd == g_hwnd) {
                g_hwnd = NULL;
#ifdef _WIN32
                PostQuitMessage(0);
#else
                mac_terminate_app();
#endif
            }
            return TRUE;
    }
    return FALSE;
}
