#ifndef INSTALLER_APP_H
#define INSTALLER_APP_H

#include "swell.h"

#include <string>
#include <vector>

// Single authoritative list of OSARA locale files used during install.
// Update here when a new translation is added; CopyLocaleFiles() consumes
// this list.  The uninstaller scans the on-disk locale directory instead so
// that translations added after installation are also removed.
extern const std::vector<std::string> kLocaleFiles;

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
    bool licenseAccepted;
    bool installationSucceeded;
    bool uninstallationSucceeded;
    std::string lastError;
    std::string keymapBackupPath;
    std::string pluginBackupPath;
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
    std::string GetBasePath();
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
