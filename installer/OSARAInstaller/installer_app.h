#ifndef INSTALLER_APP_H
#define INSTALLER_APP_H

#include "swell.h"

#include <string>
#include <vector>

// Operation modes
enum OperationMode
{
    MODE_INSTALL,
    MODE_UNINSTALL
};

// Installation types
enum InstallationType
{
    INSTALL_STANDARD,
    INSTALL_PORTABLE
};

// Keymap options
enum KeymapOption
{
    KEYMAP_INSTALL,
    KEYMAP_KEEP
};

// Installation state
struct InstallationState
{
    OperationMode operationMode;
    InstallationType installType;
    KeymapOption keymapOption;
    std::string installPath;
    std::string reaperPath;
    bool licenseAccepted;
    bool installationSucceeded;
    bool uninstallationSucceeded;
    std::string lastError;
    std::string keymapBackupPath;
    std::vector<std::string> filesToRemove;
    
    InstallationState() 
        : operationMode(MODE_INSTALL)
        , installType(INSTALL_STANDARD)
        , keymapOption(KEYMAP_KEEP)
        , licenseAccepted(false)
        , installationSucceeded(false)
        , uninstallationSucceeded(false)
    {
    }
};

class InstallerApp
{
public:
    InstallerApp();
    ~InstallerApp();
    
    // State management
    InstallationState& GetState() { return m_state; }
    
    // Navigation
    void ShowScreen(int dialogId);
    void ShowPreviousScreen();

    // Installation paths
    std::string GetDefaultInstallPath();
    bool ValidateInstallPath(const std::string& path);
    
    // Installation process
    bool PerformInstallation();
    bool PerformUninstallation();
    
    // Utility functions
    std::string GetResourcePath();
    std::string LoadLicenseText();
    bool IsReaperInstalled();
    std::string FindReaperPath();
    
    // Uninstall functions
    bool IsOSARAInstalledAt(const std::string& path);
    std::vector<std::string> GetInstalledFilesAt(const std::string& path);
    bool RemoveOSARAFiles();
    
private:
    InstallationState m_state;
    std::vector<int> m_screenHistory;
    
    // Internal installation methods
    bool CopyPluginFiles();
    bool CopyLocaleFiles();
    bool InstallKeymap();
    bool CreateDirectories();
    bool BackupExistingFiles();

    // Path utilities
    std::string GetApplicationSupportPath();
    std::string GetUserLibraryPath();
    bool DirectoryExists(const std::string& path);
    bool CreateDirectoryPath(const std::string& path);
    bool CopyFile(const std::string& source, const std::string& dest);
};

// Global variables (defined in main.cpp)
extern HWND g_hwnd;
extern HINSTANCE g_hInst;
extern InstallerApp* g_installer;

// Dialog procedures (defined in dialog_procs.cpp)
INT_PTR CALLBACK WelcomeDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ModeSelectionDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK LicenseDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK InstallTypeDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK KeymapDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK UninstallConfirmDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK NoOSARAFoundDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK CompletionDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // INSTALLER_APP_H
