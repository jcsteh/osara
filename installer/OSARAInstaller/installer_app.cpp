#include "installer_app.h"
#include "resource.h"
#include "../../src/translation.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>

#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <mach-o/dyld.h>
#define PATH_SEPARATOR "/"

InstallerApp::InstallerApp()
{
    // Initialize translation system
    initTranslation();
}

InstallerApp::~InstallerApp()
{
}

void InstallerApp::ShowScreen(int dialogId)
{
    // Add current screen to history (before destroying it)
    m_screenHistory.push_back(dialogId);
    
    // Create new dialog first
    DLGPROC dlgProc = nullptr;
    switch (dialogId)
    {
        case IDD_WELCOME:
        dlgProc = WelcomeDlgProc;
        break;
        case IDD_MODE_SELECTION:
        dlgProc = ModeSelectionDlgProc;
        break;
        case IDD_LICENSE:
        dlgProc = LicenseDlgProc;
        break;
        case IDD_INSTALL_TYPE:
        dlgProc = InstallTypeDlgProc;
        break;
        case IDD_KEYMAP:
        dlgProc = KeymapDlgProc;
        break;
        case IDD_UNINSTALL_CONFIRM:
        dlgProc = UninstallConfirmDlgProc;
        break;
        case IDD_NO_OSARA_FOUND:
        dlgProc = NoOSARAFoundDlgProc;
        break;
        case IDD_COMPLETION:
        dlgProc = CompletionDlgProc;
        break;
        default:
        return;
    }
    
    if (dlgProc)
    {
        printf("DEBUG: Creating dialog %d\n", dialogId);
        
        // Destroy old dialog first to prevent handle reuse issues
        HWND oldHwnd = g_hwnd;
        if (oldHwnd)
        {
            printf("DEBUG: Destroying old dialog %p first\n", oldHwnd);
            g_hwnd = NULL; // Clear g_hwnd before destroying to prevent WM_DESTROY issues
            DestroyWindow(oldHwnd);
            printf("DEBUG: Old dialog destroyed\n");
        }
        
        HWND newHwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(dialogId), NULL, dlgProc);
        if (newHwnd)
        {
            printf("DEBUG: Dialog created successfully, newHwnd=%p\n", newHwnd);
            g_hwnd = newHwnd;
            ShowWindow(g_hwnd, SW_SHOW);
            SetFocus(g_hwnd);
            printf("DEBUG: New dialog shown and focused\n");
        }
        else
        {
            printf("DEBUG: Dialog creation FAILED for dialog %d\n", dialogId);
            // If dialog creation failed, remove it from history
            if (!m_screenHistory.empty())
            {
                m_screenHistory.pop_back();
            }
        }
    }
}

void InstallerApp::ShowPreviousScreen()
{
    if (m_screenHistory.size() >= 2)
    {
        // Remove current screen
        m_screenHistory.pop_back();
        // Get previous screen
        int prevScreen = m_screenHistory.back();
        m_screenHistory.pop_back(); // Remove it so ShowScreen can add it back
        ShowScreen(prevScreen);
    }
}

std::string InstallerApp::GetDefaultInstallPath()
{
    // macOS: ~/Library/Application Support/REAPER
    std::string userLib = GetUserLibraryPath();
    if (!userLib.empty())
    {
        return userLib + "/Application Support/REAPER";
    }
    return "";
}

bool InstallerApp::ValidateInstallPath(const std::string& path)
{
    if (path.empty())
    return false;
    
    // Check if directory exists
    if (!DirectoryExists(path))
    return false;
    
    // For portable installation, check if it looks like a REAPER folder
    // Look for REAPER executable or other REAPER-specific files
    std::string reaperExe = path + PATH_SEPARATOR + "REAPER.exe";
    std::string reaperApp = path + PATH_SEPARATOR + "REAPER.app";
    std::string reaperBin = path + PATH_SEPARATOR + "reaper";
    
    // Check for REAPER executable (Windows, macOS app bundle, or Linux binary)
    struct stat statbuf;
    if (stat(reaperExe.c_str(), &statbuf) == 0 ||
    stat(reaperApp.c_str(), &statbuf) == 0 ||
    stat(reaperBin.c_str(), &statbuf) == 0)
    {
        return true;
    }
    
    // Also check for UserPlugins directory (common in REAPER installations)
    std::string userPlugins = path + PATH_SEPARATOR + "UserPlugins";
    if (DirectoryExists(userPlugins))
    {
        return true;
    }
    
    return false;
}

bool InstallerApp::PerformInstallation()
{
    try
    {
        // Create necessary directories
        if (!CreateDirectories())
        {
            m_state.lastError = translate("Failed to create installation directories. Check that you have write permissions to the selected location.");
            m_state.installationSucceeded = false;
            return false;
        }
        
        // Copy plugin files
        if (!CopyPluginFiles())
        {
            // Error message is already set by CopyFile method
            m_state.installationSucceeded = false;
            return false;
        }
        
        // Copy locale files
        if (!CopyLocaleFiles())
        {
            // Locale files are not critical, continue installation
        }
        
        // Backup existing files if needed
        if (!BackupExistingFiles())
        {
            m_state.lastError = translate("Failed to backup existing files");
            m_state.installationSucceeded = false;
            return false;
        }
        
        // Install keymap (always copy OSARA.ReaperKeyMap to KeyMaps, optionally replace reaper-kb.ini)
        if (!InstallKeymap())
        {
            // Don't fail installation if keymap fails, just warn
        }
        
        m_state.installationSucceeded = true;
        m_state.lastError = "";
        return true;
    }
    catch (const std::exception& e)
    {
        m_state.lastError = std::string("Installation failed with exception: ") + e.what();
        m_state.installationSucceeded = false;
        return false;
    }
    catch (...)
    {
        m_state.lastError = "Installation failed with unknown error";
        m_state.installationSucceeded = false;
        return false;
    }
}


std::string InstallerApp::GetResourcePath()
{
    // On macOS, resources are in the app bundle
    // Get the path to the current executable
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0)
    {
        std::string exePath(path);
        
        // Navigate from Contents/MacOS/OSARAInstaller to Contents/Resources
        size_t pos = exePath.find("/Contents/MacOS/");
        if (pos != std::string::npos)
        {
            std::string resourcePath = exePath.substr(0, pos) + "/Contents/Resources";
            return resourcePath;
        }
    }
    
    return "../OSARAInstaller/Resources"; // Fallback for command line execution
}

std::string InstallerApp::LoadLicenseText()
{
    std::string resourcePath = GetResourcePath();
    std::string licensePath;
    
    if (!resourcePath.empty())
    {
        licensePath = resourcePath + PATH_SEPARATOR + "copying.txt";
    }
    else
    {
        licensePath = "copying.txt";
    }
    
    std::ifstream file(licensePath);
    if (file.is_open())
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    return "";
}

bool InstallerApp::IsReaperInstalled()
{
    std::string reaperPath = FindReaperPath();
    return !reaperPath.empty();
}

std::string InstallerApp::FindReaperPath()
{
    // Try common REAPER installation locations
    std::vector<std::string> searchPaths;
    
    // macOS
    searchPaths.push_back("/Applications/REAPER.app");
    searchPaths.push_back("/Applications/REAPER64.app");
    
    // User's Applications folder
    std::string userLib = GetUserLibraryPath();
    if (!userLib.empty())
    {
        std::string userHome = userLib.substr(0, userLib.find("/Library"));
        searchPaths.push_back(userHome + "/Applications/REAPER.app");
        searchPaths.push_back(userHome + "/Applications/REAPER64.app");
    }
    
    for (const std::string& path : searchPaths)
    {
        if (DirectoryExists(path))
        {
            return path;
        }
    }
    
    return "";
}

bool InstallerApp::CopyPluginFiles()
{
    std::string resourcePath = GetResourcePath();
    std::string pluginSource;
    std::string pluginDest;
    
    if (!resourcePath.empty())
    {
        pluginSource = resourcePath + PATH_SEPARATOR + "reaper_osara.dylib";
    }
    else
    {
        pluginSource = "reaper_osara.dylib";
    }
    
    // Determine destination based on installation type
    if (m_state.installType == INSTALL_STANDARD)
    {
        std::string userPluginsPath = GetDefaultInstallPath() + PATH_SEPARATOR + "UserPlugins";
        pluginDest = userPluginsPath + PATH_SEPARATOR + "reaper_osara.dylib";
    }
    else
    {
        std::string userPluginsPath = m_state.installPath + PATH_SEPARATOR + "UserPlugins";
        pluginDest = userPluginsPath + PATH_SEPARATOR + "reaper_osara.dylib";
    }
    
    bool result = CopyFile(pluginSource, pluginDest);
    return result;
}

bool InstallerApp::CopyLocaleFiles()
{
    std::string resourcePath = GetResourcePath();
    std::string localeSourceDir;
    std::string localeDestDir;
    
    if (!resourcePath.empty())
    {
        localeSourceDir = resourcePath + PATH_SEPARATOR + "locale";
    }
    else
    {
        localeSourceDir = "locale";
    }
    
    // Determine destination based on installation type
    if (m_state.installType == INSTALL_STANDARD)
    {
        localeDestDir = GetDefaultInstallPath() + PATH_SEPARATOR + "osara" + PATH_SEPARATOR + "locale";
    }
    else
    {
        localeDestDir = m_state.installPath + PATH_SEPARATOR + "osara" + PATH_SEPARATOR + "locale";
    }
    
    // Copy all .po files from the locale directory
    std::vector<std::string> localeFiles = {
        "de_DE.po", "es_ES.po", "es_MX.po", "fr_CA.po", "fr_FR.po",
        "nb_NO.po", "pt_BR.po", "ru_RU.po", "tr_TR.po", "zh_CN.po", "zh_TW.po"
    };
    
    for (const std::string& filename : localeFiles)
    {
        std::string sourcePath = localeSourceDir + PATH_SEPARATOR + filename;
        std::string destPath = localeDestDir + PATH_SEPARATOR + filename;
        
        if (!CopyFile(sourcePath, destPath))
        {
            // If a specific locale file fails, continue with others but note the error
            m_state.lastError += "Failed to copy locale file: " + filename + "; ";
        }
    }
    
    return true; // Don't fail installation if some locale files are missing
}

bool InstallerApp::InstallKeymap()
{
    std::string resourcePath = GetResourcePath();
    std::string keymapSource;
    std::string basePath;
    
    if (!resourcePath.empty())
    {
        keymapSource = resourcePath + PATH_SEPARATOR + "OSARA.ReaperKeyMap";
    }
    else
    {
        keymapSource = "OSARA.ReaperKeyMap";
    }
    
    // Determine base path
    if (m_state.installType == INSTALL_STANDARD)
    {
        basePath = GetDefaultInstallPath();
    }
    else
    {
        basePath = m_state.installPath;
    }
    
    // First, always copy OSARA.ReaperKeyMap to KeyMaps directory
    std::string keyMapsDest = basePath + PATH_SEPARATOR + "KeyMaps" + PATH_SEPARATOR + "OSARA.ReaperKeyMap";
    if (!CopyFile(keymapSource, keyMapsDest))
    {
        return false; // This is a critical failure
    }
    
    // Then, if user chose to install keymap, copy it to reaper-kb.ini
    if (m_state.keymapOption == KEYMAP_INSTALL)
    {
        std::string keymapDest = basePath + PATH_SEPARATOR + "reaper-kb.ini";
        return CopyFile(keymapSource, keymapDest);
    }
    
    return true;
}

bool InstallerApp::PerformUninstallation()
{
    try
    {
        // Get the base path for uninstallation
        std::string basePath;
        if (m_state.installType == INSTALL_STANDARD)
        {
            basePath = GetDefaultInstallPath();
        }
        else
        {
            basePath = m_state.installPath;
        }
        
        // OSARA detection already done in confirmation dialog, proceed with removal
        // Get list of files to remove
        m_state.filesToRemove = GetInstalledFilesAt(basePath);
        
        // Remove OSARA files
        if (!RemoveOSARAFiles())
        {
            m_state.uninstallationSucceeded = false;
            return false;
        }
        
        m_state.uninstallationSucceeded = true;
        m_state.lastError = "";
        return true;
    }
    catch (const std::exception& e)
    {
        m_state.lastError = std::string("Uninstallation failed with exception: ") + e.what();
        m_state.uninstallationSucceeded = false;
        return false;
    }
    catch (...)
    {
        m_state.lastError = "Uninstallation failed with unknown error";
        m_state.uninstallationSucceeded = false;
        return false;
    }
}

bool InstallerApp::IsOSARAInstalledAt(const std::string& path)
{
    if (path.empty() || !DirectoryExists(path))
        return false;
    
    // Check for OSARA plugin (required)
    std::string pluginPath = path + PATH_SEPARATOR + "UserPlugins" + PATH_SEPARATOR + "reaper_osara.dylib";
    struct stat statbuf;
    if (stat(pluginPath.c_str(), &statbuf) != 0)
        return false;
    
    return true; // Plugin exists, so OSARA is installed
}

std::vector<std::string> InstallerApp::GetInstalledFilesAt(const std::string& path)
{
    std::vector<std::string> files;
    struct stat statbuf;
    
    // Check for plugin file
    std::string pluginPath = path + PATH_SEPARATOR + "UserPlugins" + PATH_SEPARATOR + "reaper_osara.dylib";
    if (stat(pluginPath.c_str(), &statbuf) == 0)
    {
        files.push_back(pluginPath);
    }
    
    // Check for keymap file
    std::string keymapPath = path + PATH_SEPARATOR + "KeyMaps" + PATH_SEPARATOR + "OSARA.ReaperKeyMap";
    if (stat(keymapPath.c_str(), &statbuf) == 0)
    {
        files.push_back(keymapPath);
    }
    
    // Check for locale files
    std::string localePath = path + PATH_SEPARATOR + "osara" + PATH_SEPARATOR + "locale";
    if (DirectoryExists(localePath))
    {
        std::vector<std::string> localeFiles = {
            "de_DE.po", "es_ES.po", "es_MX.po", "fr_CA.po", "fr_FR.po",
            "nb_NO.po", "pt_BR.po", "ru_RU.po", "tr_TR.po", "zh_CN.po", "zh_TW.po"
        };
        
        for (const std::string& filename : localeFiles)
        {
            std::string filePath = localePath + PATH_SEPARATOR + filename;
            if (stat(filePath.c_str(), &statbuf) == 0)
            {
                files.push_back(filePath);
            }
        }
    }
    
    return files;
}

bool InstallerApp::RemoveOSARAFiles()
{
    std::string basePath;
    if (m_state.installType == INSTALL_STANDARD)
    {
        basePath = GetDefaultInstallPath();
    }
    else
    {
        basePath = m_state.installPath;
    }
    
    bool success = true;
    std::string errors;
    
    // Remove plugin file
    std::string pluginPath = basePath + PATH_SEPARATOR + "UserPlugins" + PATH_SEPARATOR + "reaper_osara.dylib";
    if (remove(pluginPath.c_str()) != 0)
    {
        struct stat statbuf;
        if (stat(pluginPath.c_str(), &statbuf) == 0) // File still exists
        {
            errors += "Failed to remove plugin: " + pluginPath + "; ";
            success = false;
        }
    }
    
    // Remove keymap file
    std::string keymapPath = basePath + PATH_SEPARATOR + "KeyMaps" + PATH_SEPARATOR + "OSARA.ReaperKeyMap";
    if (remove(keymapPath.c_str()) != 0)
    {
        struct stat statbuf;
        if (stat(keymapPath.c_str(), &statbuf) == 0) // File still exists
        {
            errors += "Failed to remove keymap: " + keymapPath + "; ";
            // Don't fail uninstallation for keymap removal failure
        }
    }
    
    // Remove locale files
    std::string localePath = basePath + PATH_SEPARATOR + "osara" + PATH_SEPARATOR + "locale";
    if (DirectoryExists(localePath))
    {
        std::vector<std::string> localeFiles = {
            "de_DE.po", "es_ES.po", "es_MX.po", "fr_CA.po", "fr_FR.po",
            "nb_NO.po", "pt_BR.po", "ru_RU.po", "tr_TR.po", "zh_CN.po", "zh_TW.po"
        };
        
        for (const std::string& filename : localeFiles)
        {
            std::string filePath = localePath + PATH_SEPARATOR + filename;
            remove(filePath.c_str()); // Don't check result for locale files
        }
        
        // Try to remove locale directory if empty
        rmdir(localePath.c_str());
        
        // Try to remove osara directory if empty
        std::string osaraPath = basePath + PATH_SEPARATOR + "osara";
        rmdir(osaraPath.c_str());
    }
    
    if (!success)
    {
        m_state.lastError = errors;
    }
    
    return success;
}

bool InstallerApp::CreateDirectories()
{
    std::string basePath;
    
    if (m_state.installType == INSTALL_STANDARD)
    {
        basePath = GetDefaultInstallPath();
    }
    else
    {
        basePath = m_state.installPath;
    }
    
    if (basePath.empty())
    {
        m_state.lastError = "Installation path is empty";
        return false;
    }
    
    // Create base directory
    if (!CreateDirectoryPath(basePath))
    {
        m_state.lastError = "Failed to create base directory: " + basePath;
        return false;
    }
    
    // Create UserPlugins directory
    std::string userPluginsPath = basePath + PATH_SEPARATOR + "UserPlugins";
    if (!CreateDirectoryPath(userPluginsPath))
    {
        m_state.lastError = "Failed to create UserPlugins directory: " + userPluginsPath;
        return false;
    }
    
    // Create KeyMaps directory
    std::string keyMapsPath = basePath + PATH_SEPARATOR + "KeyMaps";
    if (!CreateDirectoryPath(keyMapsPath))
    {
        m_state.lastError = "Failed to create KeyMaps directory: " + keyMapsPath;
        return false;
    }
    
    // Create osara/locale directory
    std::string osaraPath = basePath + PATH_SEPARATOR + "osara";
    if (!CreateDirectoryPath(osaraPath))
    {
        m_state.lastError = "Failed to create osara directory: " + osaraPath;
        return false;
    }
    
    std::string localePath = osaraPath + PATH_SEPARATOR + "locale";
    if (!CreateDirectoryPath(localePath))
    {
        m_state.lastError = "Failed to create locale directory: " + localePath;
        return false;
    }
    
    return true;
}

bool InstallerApp::BackupExistingFiles()
{
    // Only backup if we're going to install the keymap
    if (m_state.keymapOption != KEYMAP_INSTALL)
    {
        return true; // No backup needed
    }
    
    std::string basePath;
    if (m_state.installType == INSTALL_STANDARD)
    {
        basePath = GetDefaultInstallPath();
    }
    else
    {
        basePath = m_state.installPath;
    }
    
    std::string keymapPath = basePath + PATH_SEPARATOR + "reaper-kb.ini";
    
    // Check if keymap file exists
    struct stat statbuf;
    if (stat(keymapPath.c_str(), &statbuf) != 0)
    {
        // No existing keymap to backup
        return true;
    }
    
    // Create backup filename with timestamp
    time_t now = time(0);
    struct tm* timeinfo = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);
    
    std::string backupPath = basePath + PATH_SEPARATOR + "reaper-kb.ini.backup." + timestamp;
    
    // Copy the existing keymap to backup location
    if (!CopyFile(keymapPath, backupPath))
    {
        m_state.lastError = "Failed to backup existing keymap: " + keymapPath;
        return false;
    }
    
    return true;
}

std::string InstallerApp::GetApplicationSupportPath()
{
    std::string userLib = GetUserLibraryPath();
    if (!userLib.empty())
    {
        return userLib + "/Application Support";
    }
    return "";
}

std::string InstallerApp::GetUserLibraryPath()
{
    const char* home = getenv("HOME");
    if (home)
    {
        return std::string(home) + "/Library";
    }
    
    // Fallback: get from passwd
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
    {
        return std::string(pw->pw_dir) + "/Library";
    }
    return "";
}

bool InstallerApp::DirectoryExists(const std::string& path)
{
    struct stat statbuf;
    return (stat(path.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
}

bool InstallerApp::CreateDirectoryPath(const std::string& path)
{
    if (DirectoryExists(path))
    return true;
    
    // Create parent directories recursively
    size_t pos = path.find_last_of(PATH_SEPARATOR);
    if (pos != std::string::npos)
    {
        std::string parentPath = path.substr(0, pos);
        if (!parentPath.empty() && !DirectoryExists(parentPath))
        {
            if (!CreateDirectoryPath(parentPath))
            return false;
        }
    }
    
    return (mkdir(path.c_str(), 0755) == 0);
}

bool InstallerApp::CopyFile(const std::string& source, const std::string& dest)
{
    // Check if source file exists
    struct stat sourceStat;
    if (stat(source.c_str(), &sourceStat) != 0)
    {
        m_state.lastError = "Source file does not exist: " + source;
        return false;
    }
    std::ifstream src(source, std::ios::binary);
    if (!src.is_open())
    {
        m_state.lastError = "Failed to open source file: " + source;
        return false;
    }
    std::ofstream dst(dest, std::ios::binary);
    if (!dst.is_open())
    {
        m_state.lastError = "Failed to open destination file: " + dest;
        return false;
    }
    dst << src.rdbuf();
    
    if (!src.good())
    {
        m_state.lastError = "Error reading from source file: " + source;
        return false;
    }
    
    if (!dst.good())
    {
        m_state.lastError = "Error writing to destination file: " + dest;
        return false;
    }
    
    // Close the streams to ensure all data is flushed
    src.close();
    dst.close();
    
    // Verify the file was actually copied
    struct stat destStat;
    if (stat(dest.c_str(), &destStat) != 0)
    {
        m_state.lastError = "Destination file was not created: " + dest;
        return false;
    }
    if (sourceStat.st_size != destStat.st_size)
    {
        m_state.lastError = "File size mismatch after copy. Source: " + std::to_string(sourceStat.st_size) + ", Dest: " + std::to_string(destStat.st_size);
        return false;
    }
    return true;
}
