#include "installer_app.h"
#include "resource.h"
#include "../../src/translation.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>
#include <cerrno>
#include <cstdio>

#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <mach-o/dyld.h>
#define PATH_SEPARATOR "/"

// Single definition of kLocaleFiles (declared extern in installer_app.h).
// CopyLocaleFiles() uses this list during install; the uninstaller scans the
// on-disk locale directory so that future translations are also removed.
const std::vector<std::string> kLocaleFiles = {
    "de_DE.po", "es_ES.po", "es_MX.po", "fr_CA.po", "fr_FR.po",
    "nb_NO.po", "pt_BR.po", "ru_RU.po", "tr_TR.po", "zh_CN.po", "zh_TW.po"
};

// Mac-specific functions (defined in dialog_procs_mac.mm).
extern bool mac_is_reaper_running();
extern void mac_terminate_app();

InstallerApp::InstallerApp()
{
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
        // Destroy old dialog first to prevent handle reuse issues
        HWND oldHwnd = g_hwnd;
        if (oldHwnd)
        {
            g_hwnd = NULL; // Clear g_hwnd before destroying to prevent WM_DESTROY issues
            DestroyWindow(oldHwnd);
        }

        HWND newHwnd = CreateDialog(g_hInst, MAKEINTRESOURCE(dialogId), NULL, dlgProc);
        if (newHwnd)
        {
            g_hwnd = newHwnd;

            // Center on screen before making the window visible to avoid flicker.
            RECT rect;
            GetWindowRect(g_hwnd, &rect);
            int width  = rect.right  - rect.left;
            int height = rect.bottom - rect.top;
            int x = (GetSystemMetrics(SM_CXSCREEN) - width)  / 2;
            int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
            SetWindowPos(g_hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

            ShowWindow(g_hwnd, SW_SHOW);
            SetFocus(g_hwnd);
        }
        else
        {
            // If dialog creation failed, remove it from history
            if (!m_screenHistory.empty())
            {
                m_screenHistory.pop_back();
            }
            // No visible window remains — the app is in an unrecoverable state.
            fprintf(stderr, "Fatal: CreateDialog failed for dialog %d\n", dialogId);
            mac_terminate_app();
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
    if (path.empty() || !DirectoryExists(path))
        return false;

    // A valid REAPER data folder contains at least one of these markers.
    // UserPlugins may be absent on a fresh installation that has never had a
    // plugin installed.  KeyMaps is created by REAPER on first launch.
    // reaper.ini is always present once REAPER has run at least once.
    struct stat st;
    return DirectoryExists(path + PATH_SEPARATOR + "UserPlugins")
        || DirectoryExists(path + PATH_SEPARATOR + "KeyMaps")
        || stat((path + PATH_SEPARATOR + "reaper.ini").c_str(), &st) == 0;
}

bool InstallerApp::PerformInstallation()
{
    m_state.lastError = "";

    // Track files written so far; removed if an unexpected exception fires
    // after a partial write.
    std::vector<std::string> installedFiles;

    try
    {
        if (mac_is_reaper_running())
        {
            m_state.lastError = translate("Please quit REAPER before installing OSARA.");
            m_state.installationSucceeded = false;
            return false;
        }

        // Create necessary directories
        if (!CreateDirectories())
        {
            m_state.lastError = translate("Failed to create installation directories. Check that you have write permissions to the selected location.");
            m_state.installationSucceeded = false;
            return false;
        }

        // Backup existing files before overwriting anything
        if (!BackupExistingFiles())
        {
            m_state.lastError = translate("Failed to backup existing files");
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
        // Track the installed plugin so an unexpected exception can roll it back.
        {
            std::string base = GetBasePath();
            installedFiles.push_back(
                base + PATH_SEPARATOR + "UserPlugins" + PATH_SEPARATOR + "reaper_osara.dylib");
        }

        // Copy locale files (non-critical; continue on failure)
        CopyLocaleFiles();

        // Pre-track the keymap files so an exception thrown inside InstallKeymap()
        // triggers rollback of any partial writes.  InstallKeymap() handles its
        // own cleanup for normal (non-exception) failures, so these entries are
        // only exercised by the catch handlers below.
        {
            std::string base = GetBasePath();
            installedFiles.push_back(
                base + PATH_SEPARATOR + "KeyMaps" + PATH_SEPARATOR + "OSARA.ReaperKeyMap");
            if (m_state.keymapOption == KEYMAP_INSTALL)
                installedFiles.push_back(base + PATH_SEPARATOR + "reaper-kb.ini");
        }

        // Install keymap.
        if (!InstallKeymap())
        {
            // Error message already set by InstallKeymap/CopyFile.
            m_state.installationSucceeded = false;
            return false;
        }

        m_state.installationSucceeded = true;
        m_state.lastError = "";
        return true;
    }
    catch (const std::exception& e)
    {
        for (const auto& f : installedFiles) remove(f.c_str());
        m_state.lastError = std::string("Installation failed with exception: ") + e.what();
        m_state.installationSucceeded = false;
        return false;
    }
    catch (...)
    {
        for (const auto& f : installedFiles) remove(f.c_str());
        m_state.lastError = "Installation failed with unknown error";
        m_state.installationSucceeded = false;
        return false;
    }
}


std::string InstallerApp::GetResourcePath()
{
    // Try with a stack buffer first; resize to the required length if the path
    // is longer (e.g. deeply-nested app bundles or long volume names).
    uint32_t size = 1024;
    std::vector<char> buf(size);
    if (_NSGetExecutablePath(buf.data(), &size) != 0)
    {
        // Buffer was too small; size now holds the required number of bytes.
        buf.resize(size);
        if (_NSGetExecutablePath(buf.data(), &size) != 0)
            buf[0] = '\0'; // Give up; fall through to fallback.
    }

    if (buf[0])
    {
        std::string exePath(buf.data());
        size_t pos = exePath.find("/Contents/MacOS/");
        if (pos != std::string::npos)
            return exePath.substr(0, pos) + "/Contents/Resources";
    }

    return "../OSARAInstaller/Resources"; // Fallback for command-line use
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
    
    pluginDest = GetBasePath() + PATH_SEPARATOR + "UserPlugins"
        + PATH_SEPARATOR + "reaper_osara.dylib";
    
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
    
    localeDestDir = GetBasePath() + PATH_SEPARATOR + "osara" + PATH_SEPARATOR + "locale";
    
    for (const std::string& filename : kLocaleFiles)
    {
        std::string sourcePath = localeSourceDir + PATH_SEPARATOR + filename;
        std::string destPath = localeDestDir + PATH_SEPARATOR + filename;
        
        if (!CopyFile(sourcePath, destPath))
        {
            // Non-critical: locale files are best-effort; continue without them.
        }
    }
    
    return true; // Don't fail installation if some locale files are missing
}

bool InstallerApp::InstallKeymap()
{
    std::string resourcePath = GetResourcePath();
    std::string keymapSource;
    
    if (!resourcePath.empty())
    {
        keymapSource = resourcePath + PATH_SEPARATOR + "OSARA.ReaperKeyMap";
    }
    else
    {
        keymapSource = "OSARA.ReaperKeyMap";
    }
    
    std::string basePath = GetBasePath();

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
        if (!CopyFile(keymapSource, keymapDest))
        {
            // Roll back the KeyMaps file we already installed so the caller
            // sees a clean failure with no partial state left behind.
            remove(keyMapsDest.c_str());
            return false;
        }
    }

    return true;
}

bool InstallerApp::PerformUninstallation()
{
    try
    {
        if (mac_is_reaper_running())
        {
            m_state.lastError = translate("Please quit REAPER before uninstalling OSARA.");
            m_state.uninstallationSucceeded = false;
            return false;
        }

        std::string basePath = GetBasePath();
        
        // OSARA detection already done in confirmation dialog, proceed with removal.
        // filesToRemove was populated by UninstallConfirmDlgProc (WM_INITDIALOG)
        // before this method was called, ensuring the confirmation UI and the
        // actual removal operate on exactly the same set of files.

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
    
    // Scan the locale directory rather than consulting the hardcoded list so
    // that any .po files added after installation are also removed.
    std::string localePath = path + PATH_SEPARATOR + "osara" + PATH_SEPARATOR + "locale";
    if (DirectoryExists(localePath))
    {
        DIR* dir = opendir(localePath.c_str());
        if (dir)
        {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                std::string name = entry->d_name;
                if (name.size() > 3 && name.compare(name.size() - 3, 3, ".po") == 0)
                {
                    std::string filePath = localePath + PATH_SEPARATOR + name;
                    struct stat fstatbuf;
                    if (stat(filePath.c_str(), &fstatbuf) == 0 && S_ISREG(fstatbuf.st_mode))
                        files.push_back(filePath);
                }
            }
            closedir(dir);
        }
    }

    return files;
}


bool InstallerApp::RemoveOSARAFiles()
{
    // Remove the files that were detected when the user confirmed uninstallation.
    // Using the pre-built list (rather than re-deriving paths) ensures the UI
    // confirmation and the actual removal operate on exactly the same set.
    std::vector<std::string> failedRemovals;
    for (const std::string& filePath : m_state.filesToRemove)
    {
        if (remove(filePath.c_str()) != 0)
            failedRemovals.push_back(filePath);
    }

    // Independently verify the plugin is gone — it is the only critical file.
    // We do this via stat() rather than relying solely on remove()'s return value
    // (remove() can succeed on some file systems even when the file is still
    // accessible via a hard link or the removal is asynchronous).
    // If stat() shows the plugin still present and it is not already in
    // failedRemovals, add it so the error report is always complete.
    std::string basePath = GetBasePath();
    std::string pluginPath = basePath + PATH_SEPARATOR + "UserPlugins"
        + PATH_SEPARATOR + "reaper_osara.dylib";
    struct stat statbuf;
    if (stat(pluginPath.c_str(), &statbuf) == 0)
    {
        if (std::find(failedRemovals.begin(), failedRemovals.end(), pluginPath)
                == failedRemovals.end())
            failedRemovals.insert(failedRemovals.begin(), pluginPath);
    }

    if (!failedRemovals.empty())
    {
        m_state.lastError = translate("Failed to remove the following files:");
        for (const auto& f : failedRemovals)
            m_state.lastError += "\n  " + f;
        return false;
    }

    // Remove locale directories if now empty; leave them if the user has
    // placed other files there.
    std::string localePath = basePath + PATH_SEPARATOR + "osara" + PATH_SEPARATOR + "locale";
    std::string osaraPath  = basePath + PATH_SEPARATOR + "osara";
    rmdir(localePath.c_str());
    rmdir(osaraPath.c_str());

    return true;
}

bool InstallerApp::CreateDirectories()
{
    std::string basePath = GetBasePath();
    
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
    std::string basePath = GetBasePath();

    // Generate a shared timestamp for all backup filenames in this run.
    time_t now = time(nullptr);
    struct tm tmbuf;
    localtime_r(&now, &tmbuf);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tmbuf);

    // Always back up the existing plugin so the user can revert if the new
    // build turns out to be broken.
    std::string pluginPath = basePath + PATH_SEPARATOR + "UserPlugins"
        + PATH_SEPARATOR + "reaper_osara.dylib";
    struct stat statbuf;
    if (stat(pluginPath.c_str(), &statbuf) == 0)
    {
        std::string backupPath = pluginPath + ".backup." + timestamp;
        if (!CopyFile(pluginPath, backupPath))
        {
            m_state.lastError = "Failed to backup existing plugin: " + pluginPath;
            return false;
        }
        m_state.pluginBackupPath = backupPath;
    }

    // Back up the active keymap only when we are going to replace it.
    if (m_state.keymapOption == KEYMAP_INSTALL)
    {
        std::string keymapPath = basePath + PATH_SEPARATOR + "reaper-kb.ini";
        if (stat(keymapPath.c_str(), &statbuf) == 0)
        {
            std::string backupPath = keymapPath + ".backup." + timestamp;
            if (!CopyFile(keymapPath, backupPath))
            {
                m_state.lastError = "Failed to backup existing keymap: " + keymapPath;
                // Clean up the plugin backup already written so we don't leave
                // stale .backup files in the user's REAPER folder.
                if (!m_state.pluginBackupPath.empty())
                {
                    remove(m_state.pluginBackupPath.c_str());
                    m_state.pluginBackupPath = "";
                }
                return false;
            }
            m_state.keymapBackupPath = backupPath;
        }
    }

    return true;
}

std::string InstallerApp::GetBasePath()
{
    return (m_state.installType == INSTALL_STANDARD)
        ? GetDefaultInstallPath() : m_state.installPath;
}

std::string InstallerApp::GetUserLibraryPath()
{
    const char* home = getenv("HOME");
    if (home)
    {
        return std::string(home) + "/Library";
    }
    
    // Fallback: get from passwd database.
    struct passwd* pw = getpwuid(getuid());
    std::string result;
    if (pw && pw->pw_dir)
        result = std::string(pw->pw_dir) + "/Library";
    endpwent();
    return result;
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
    
    if (mkdir(path.c_str(), 0755) == 0)
        return true;
    // Another process may have created the directory between the DirectoryExists
    // check above and this mkdir call.  Treat EEXIST as success only when the
    // path is actually a directory (not a file that happens to have the same name).
    if (errno == EEXIST)
        return DirectoryExists(path);
    return false;
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

    // Lambda that removes the (possibly partial) dest file and sets an error,
    // called on every failure path after the destination has been created.
    auto failAndClean = [&](const std::string& msg) -> bool {
        dst.close();
        remove(dest.c_str());
        m_state.lastError = msg;
        return false;
    };

    dst << src.rdbuf();

    // Explicitly flush before close so we can detect write-back failures.
    // ofstream::close() calls flush() internally but does not update failbit
    // on flush failure, so a full-disk error at close time would be invisible
    // without this explicit check.
    dst.flush();
    if (!dst.good())
        return failAndClean("Error writing to destination file: " + dest);

    src.close();
    dst.close();

    // Preserve the source file's permission bits on the destination.
    // ofstream creates files with the process umask applied (typically 0644),
    // which may differ from the source (e.g. 0755 for an executable dylib).
    chmod(dest.c_str(), sourceStat.st_mode & 07777);

    // Final size sanity check.
    struct stat destStat;
    if (stat(dest.c_str(), &destStat) != 0)
    {
        remove(dest.c_str());
        m_state.lastError = "Destination file was not created: " + dest;
        return false;
    }
    if (sourceStat.st_size != destStat.st_size)
    {
        remove(dest.c_str());
        m_state.lastError = "File size mismatch after copy. Source: " +
            std::to_string(sourceStat.st_size) + ", Dest: " +
            std::to_string(destStat.st_size);
        return false;
    }
    return true;
}
