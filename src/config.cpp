/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Configuration code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2022-2023 James Teh
 * License: GNU General Public License version 2.0
 */

#include <algorithm>
#include <map>
#include <string>
#include <sstream>
#include "config.h"
#include "resource.h"
#include "translation.h"

using namespace std;

namespace settings {
// Define the variable for each setting.
#define BoolSetting(name, displayName, default) bool name = default;
#include "settings.h"
#undef BoolSetting
}

void loadConfig() {
	// GetExtState returns an empty string (not NULL) if the key doesn't exist.
	char v = '\0';
#define BoolSetting(name, displayName, default) \
	v = GetExtState(CONFIG_SECTION, #name)[0]; \
	settings::name = default ? v != '0' : v == '1';
#include "settings.h"
#undef BoolSetting
}

void config_onOk(HWND dialog) {
	int id = ID_CONFIG_DLG;
#define BoolSetting(name, displayName, default) \
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
#define BoolSetting(name, displayName, default) \
	CheckDlgButton(dialog, ++id, \
		settings::name ? BST_CHECKED : BST_UNCHECKED);
#include "settings.h"
#undef BoolSetting
	ShowWindow(dialog, SW_SHOWNORMAL);
}

// Information about a command to toggle an OSARA setting.
struct ToggleCommand {
	// The description of the action. We must store this because REAPER doesn't
	// copy the string we pass it.
	string desc;
	bool* setting;
	string settingName;
	string settingDisp;
};
map<int, ToggleCommand> toggleCommands;

bool handleSettingCommand(int command) {
	auto it = toggleCommands.find(command);
	if (it == toggleCommands.end()) {
		return false;
	}
	isHandlingCommand = true;
	ToggleCommand& tc = it->second;
	*tc.setting = !*tc.setting;
	SetExtState(CONFIG_SECTION, tc.settingName.c_str(), *tc.setting ? "1" : "0",
		true);
	ostringstream s;
	s << (*tc.setting ? translate("enabled") : translate("disabled")) <<
		" " << tc.settingDisp;
	outputMessage(s);
	isHandlingCommand = false;
	return true;
}

int handleToggleState(int command) {
	auto it = toggleCommands.find(command);
	if (it == toggleCommands.end()) {
		return -1;
	}
	return *it->second.setting;
}

void registerSettingCommands() {
#define BoolSetting(name, displayName, default) \
	{ \
		ostringstream s; \
		s << "OSARA_CONFIG_" << #name; \
		int cmd = plugin_register("command_id", (void*)s.str().c_str()); \
		auto [iter, ignore] = toggleCommands.insert({cmd, {}}); \
		ToggleCommand& tc = iter->second; \
		gaccel_register_t gaccel; \
		gaccel.accel = {0}; \
		gaccel.accel.cmd = cmd; \
		tc.settingDisp = translate_ctxt("OSARA Configuration", displayName); \
		/* Strip the '&' character indicating the access key. */ \
		tc.settingDisp.erase(remove(tc.settingDisp.begin(), tc.settingDisp.end(), \
			'&'), tc.settingDisp.end()); \
		s.str(""); \
		s << translate("OSARA: Toggle") << " " << tc.settingDisp; \
		tc.desc = s.str(); \
		gaccel.desc = tc.desc.c_str(); \
		plugin_register("gaccel", &gaccel); \
		tc.setting = &settings::name; \
		tc.settingName = #name; \
	}
#include "settings.h"
#undef BoolSetting
	plugin_register("toggleaction", (void*)handleToggleState);
}
