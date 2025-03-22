/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Configuration code
 * Copyright 2022-2024 James Teh
 * License: GNU General Public License version 2.0
 */

#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include "config.h"
#include <WDL/win32_utf8.h>
#include "midiEditorCommands.h"
#include "resource.h"
#include "translation.h"

#ifdef _WIN32
#include <commctrl.h>
#endif

using namespace std;

namespace settings {
// Define the variable for each setting.
#define BoolSetting(name, sectionId, displayName, default) bool name = default;
#include "settings.h"
#undef BoolSetting
}

void loadConfig() {
	// GetExtState returns an empty string (not null) if the key doesn't exist.
	char v = '\0';
#define BoolSetting(name, sectionId, displayName, default) \
	v = GetExtState(CONFIG_SECTION, #name)[0]; \
	settings::name = default ? v != '0' : v == '1';
#include "settings.h"
#undef BoolSetting
}

void config_onOk(HWND dialog) {
	int id = ID_CONFIG_DLG;
#define BoolSetting(name, sectionId, displayName, default) \
	settings::name = IsDlgButtonChecked(dialog, ++id) == BST_CHECKED; \
	SetExtState(CONFIG_SECTION, #name, settings::name ? "1" : "0", true);
#include "settings.h"
#undef BoolSetting
}

INT_PTR CALLBACK config_dialogProc(HWND dialog, UINT msg, WPARAM wParam,
	LPARAM lParam
) {
	switch (msg) {
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK) {
				config_onOk(dialog);
				DestroyWindow(dialog);
				return TRUE;
			} else if (LOWORD(wParam) == IDCANCEL) {
				DestroyWindow(dialog);
				return TRUE;
			}
			break;
		case WM_CLOSE:
			DestroyWindow(dialog);
			return TRUE;
	}
	return FALSE;
}

void cmdConfig(Command* command) {
	HWND dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_CONFIG_DLG),
		GetForegroundWindow(), config_dialogProc);
	translateDialog(dialog);
	int id = ID_CONFIG_DLG;
#define BoolSetting(name, sectionId, displayName, default) \
	CheckDlgButton(dialog, ++id, \
		settings::name ? BST_CHECKED : BST_UNCHECKED);
#include "settings.h"
#undef BoolSetting
	ShowWindow(dialog, SW_SHOWNORMAL);
}

// Information about commands to toggle an OSARA setting.
struct SettingCommand {
	// We add three actions: one to toggle the setting, one to enable it and one to
	// disable it.
	int toggleCommand;
	int enableCommand;
	int disableCommand;
	// The descriptions of the actions. We must store these because REAPER doesn't
	// copy the strings we pass it.
	string toggleDesc;
	string enableDesc;
	string disableDesc;
	bool* setting;
	string settingName;
	string settingDisp;
};
// We need to map the three action ids to a single SettingCommand. We use a
// vector to store the SettingCommand structs. We then map ids to indexes
// in the vector.
vector<SettingCommand> settingCommands;
map<int, decltype(settingCommands)::size_type> settingCommandsMap;

bool handleSettingCommand(int command) {
	auto it = settingCommandsMap.find(command);
	if (it == settingCommandsMap.end()) {
		return false;
	}
	isHandlingCommand = true;
	SettingCommand& sc = settingCommands[it->second];
	if (command == sc.toggleCommand) {
		*sc.setting = !*sc.setting;
	} else if (command == sc.enableCommand) {
		*sc.setting = true;
	} else {
		assert(command == sc.disableCommand);
		*sc.setting = false;
	}
	SetExtState(CONFIG_SECTION, sc.settingName.c_str(), *sc.setting ? "1" : "0",
		true);
	ostringstream s;
	s << (*sc.setting ? translate("enabled") : translate("disabled")) <<
		" " << sc.settingDisp;
	outputMessage(s);
	isHandlingCommand = false;
	return true;
}

int handleToggleState(int command) {
	auto it = settingCommandsMap.find(command);
	if (it == settingCommandsMap.end()) {
		return -1;
	}
	SettingCommand& sc = settingCommands[it->second];
	if (sc.toggleCommand != command) {
		return -1;
	}
	return *sc.setting;
}

void registerSettingCommands() {
#define BoolSetting(cmdName, sectionId, displayName, default) \
	{ \
		settingCommands.push_back({}); \
		SettingCommand& sc = settingCommands.back(); \
		ostringstream s; \
		s << "OSARA_CONFIG_" << #cmdName; \
		string toggleIdStr = s.str(); \
		string enableIdStr = toggleIdStr + "_ENABLE"; \
		string disableIdStr = toggleIdStr + "_DISABLE"; \
		sc.settingDisp = translate_ctxt("OSARA Configuration", displayName); \
		/* Strip the '&' character indicating the access key. */ \
		sc.settingDisp.erase(remove(sc.settingDisp.begin(), sc.settingDisp.end(), \
			'&'), sc.settingDisp.end()); \
		s.str(""); \
		s << translate("OSARA: Toggle") << " " << sc.settingDisp; \
		sc.toggleDesc = s.str(); \
		s.str(""); \
		s << translate("OSARA: Enable") << " " << sc.settingDisp; \
		sc.enableDesc = s.str(); \
		s.str(""); \
		s << translate("OSARA: Disable") << " " << sc.settingDisp; \
		sc.disableDesc = s.str(); \
		if (sectionId == MAIN_SECTION) { \
			gaccel_register_t gaccel; \
			gaccel.accel = {0}; \
			sc.toggleCommand = plugin_register("command_id", \
				(void*)toggleIdStr.c_str()); \
			gaccel.accel.cmd = sc.toggleCommand; \
			gaccel.desc = sc.toggleDesc.c_str(); \
			plugin_register("gaccel", &gaccel); \
			sc.enableCommand = plugin_register("command_id", \
				(void*)enableIdStr.c_str()); \
			gaccel.accel.cmd = sc.enableCommand; \
			gaccel.desc = sc.enableDesc.c_str(); \
			plugin_register("gaccel", &gaccel); \
			sc.disableCommand = plugin_register("command_id", \
				(void*)disableIdStr.c_str()); \
			gaccel.accel.cmd = sc.disableCommand; \
			gaccel.desc = sc.disableDesc.c_str(); \
			plugin_register("gaccel", &gaccel); \
		}  else { \
			custom_action_register_t action; \
			action.uniqueSectionId = sectionId; \
			action.idStr = toggleIdStr.c_str(); \
			action.name = sc.toggleDesc.c_str(); \
			sc.toggleCommand = plugin_register("custom_action", &action); \
			action.idStr = enableIdStr.c_str(); \
			action.name = sc.enableDesc.c_str(); \
			sc.enableCommand = plugin_register("custom_action", &action); \
			action.idStr = disableIdStr.c_str(); \
			action.name = sc.disableDesc.c_str(); \
			sc.disableCommand = plugin_register("custom_action", &action); \
		} \
		sc.setting = &settings::cmdName; \
		sc.settingName = #cmdName; \
		for (int cmd: {sc.toggleCommand, sc.enableCommand, sc.disableCommand}) { \
			settingCommandsMap.insert({cmd, settingCommands.size() - 1}); \
		} \
	}
#include "settings.h"
#undef BoolSetting
	plugin_register("toggleaction", (void*)handleToggleState);
}

// We only support tweaking settings in reaper.ini for now.
struct ReaperSetting {
	const char* section;
	const char* key;
	// If flag > 0, we will add this bit flag to the existing setting. If it
	// doesn't exist, we will use value below.
	int flag;
	// If flag == 0, we will always use this value.
	int value;
};
// If any settings are added, changed or removed below, this number should be
// increased.
constexpr int REAPER_OPTIMAL_CONFIG_VERSION = 3;
const char KEY_REAPER_OPTIMAL_CONFIG_VERSION[] = "reaperOptimalConfigVersion";

#ifdef _WIN32
// Prevent an Edit control from selecting all its text when it gets focus. See:
// https://devblogs.microsoft.com/oldnewthing/20031114-00/?p=41823
// Swell doesn't support this. Hopefully, it isn't needed there.
LRESULT CALLBACK removeHasSetSelSubclassProc(
	HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subclass,
	DWORD_PTR data
) {
	switch (msg) {
		case WM_NCDESTROY:
			RemoveWindowSubclass(hwnd, removeHasSetSelSubclassProc, subclass);
			break;
		case WM_GETDLGCODE:
			return DefSubclassProc(hwnd, msg, wParam, lParam) & ~DLGC_HASSETSEL;
	}
	return DefSubclassProc(hwnd, msg, wParam, lParam);
}
#endif // _WIN32

INT_PTR CALLBACK configReaperOptimal_dialogProc(HWND dialog, UINT msg,
	WPARAM wParam, LPARAM lParam
) {
	switch (msg) {
		case WM_INITDIALOG: {
			translateDialog(dialog);
			ostringstream s;
			const char nl[] = "\r\n";
			s <<
				translate_ctxt("optimal REAPER configuration", "Would you like to adjust REAPER preferences for optimal compatibility with screen readers? Choosing yes will make the following changes:")
				<< nl << translate_ctxt("optimal REAPER configuration", "1. Undock the Media Explorer so that it gets focus when opened.")
				<< nl << translate_ctxt("optimal REAPER configuration", "2. Enable closing Media Explorer using the escape key.")
				<< nl << translate_ctxt("optimal REAPER configuration", "3. Enable legacy file browse dialogs, so that REAPER specific options in the Open and Save As dialogs can be reached with the tab key.")
				<< nl << translate_ctxt("optimal REAPER configuration", "4. Enable the space key to be used for check boxes and buttons in various windows, wherever that's more convenient than space playing the project.")
				<< nl << translate_ctxt("optimal REAPER configuration", "5. Show text labels to indicate parallel, offline and bypassed in the FX list.")
				<< nl << translate_ctxt("optimal REAPER configuration", "6. Use a standard, accessible edit control for the video code editor.")
				<< nl << translate_ctxt("optimal REAPER configuration", "7. Hide type prefixes in the FX browser so that browsing through FX is more efficient.")
				<< nl << translate_ctxt("optimal REAPER configuration", "Note: if now isn't a good time to tweak REAPER, you can apply these adjustments later by going to the Extensions menu in the menu bar and then the OSARA submenu.")
				<< nl;
			HWND text = GetDlgItem(dialog, ID_CFGOPT_TEXT);
			SetWindowText(text, s.str().c_str());
#ifdef _WIN32
			SetWindowSubclass(text, removeHasSetSelSubclassProc, 0, 0);
#endif
			return TRUE;
		}
		case WM_COMMAND: {
			const WORD cid = LOWORD(wParam);
			if (cid == IDYES || cid == IDNO) {
				EndDialog(dialog, cid);
				return TRUE;
			}
			break;
		}
		case WM_CLOSE:
			EndDialog(dialog, IDNO);
			return TRUE;
	}
	return FALSE;
}

void cmdConfigReaperOptimal(Command* command) {
	// Even if the user chooses not to apply the configuration, we don't want to
	// ask them again at startup until the optimal settings are updated.
	string version = format("{}", REAPER_OPTIMAL_CONFIG_VERSION);
	SetExtState(CONFIG_SECTION, KEY_REAPER_OPTIMAL_CONFIG_VERSION, version.c_str(),
		true);
	if (DialogBox(
		pluginHInstance, MAKEINTRESOURCE(ID_CONFIG_REAPER_OPTIMAL_DLG),
		GetForegroundWindow(), configReaperOptimal_dialogProc
	) != IDYES) {
		return;
	}
	const ReaperSetting settings[] = {
		// Undock Media Explorer
		{"reaper_explorer", "docked", 0, 0},
		// Enable "Close window on escape key" option in Media Explorer menu
		{"reaper_explorer", "autoplay", 1 << 4, 17},
		// Enable legacy file browse dialogs for more accessible check boxes
		{"REAPER", "legacy_filebrowse", 0, 1},
		// Prefs -> Keyboard/Multi-touch: Allow space key to be used for navigation in various windows
		{"REAPER", "mousewheelmode", 1 << 7, 130},
		// Prefs -> Plug-ins: Show FX state as accessible text in name
		{"REAPER", "fxfloat_focus", 1 << 20, 1048579},
		// Prefs -> Video: Use standard edit control for video code editor (for accessibility, lacks many features)
		{"REAPER", "video_colorspace", 1 << 11, 789507},
		// Disable FX browser -> Options menu -> Show in FX list -> Plug-in type prefixes
		{"REAPER-fxadd", "uiflags", 1 << 24, 16777216},
	};
	for (const auto& setting: settings) {
		int newVal = setting.value;
		if (setting.flag) {
			char existingVal[50];
			GetPrivateProfileString(setting.section, setting.key, "", existingVal,
				sizeof(existingVal), get_ini_file());
			if (existingVal[0]) {
				// There is an existing value. Add the flag to it rather than overwriting it
				// completely.
				newVal = atoi(existingVal);
				newVal |= setting.flag;
			}
		}
		string writeVal = format("{}", newVal);
		if (!WritePrivateProfileString(setting.section, setting.key, writeVal.c_str(),
				get_ini_file())) {
			MessageBox(
				GetForegroundWindow(),
				translate("Error writing configuration changes."),
				nullptr,
				MB_OK | MB_ICONERROR
			);
			break;
		}
	}
	MessageBox(
		GetForegroundWindow(),
		translate("REAPER will now exit. Please restart REAPER  to apply the changes."),
		translate("Restart REAPER"),
		MB_ICONINFORMATION
	);
	Main_OnCommand(40004, 0); // File: Quit REAPER
}

void maybeAutoConfigReaperOptimal() {
	const char* raw = GetExtState(CONFIG_SECTION, KEY_REAPER_OPTIMAL_CONFIG_VERSION);
	int value = atoi(raw);
	if (value < REAPER_OPTIMAL_CONFIG_VERSION) {
		cmdConfigReaperOptimal(nullptr);
	}
}
