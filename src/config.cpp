/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Configuration code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2022-2023 James Teh
 * License: GNU General Public License version 2.0
 */

#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include "config.h"
#include "midiEditorCommands.h"
#include "resource.h"
#include "translation.h"

using namespace std;

namespace settings {
// Define the variable for each setting.
#define BoolSetting(name, sectionId, displayName, default) bool name = default;
#include "settings.h"
#undef BoolSetting
}

void loadConfig() {
	// GetExtState returns an empty string (not NULL) if the key doesn't exist.
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
// vector to store the SettingCommand structs. We then map ids to SettingCommand
// pointers.
vector<SettingCommand> settingCommands;
map<int, SettingCommand*> settingCommandsMap;

bool handleSettingCommand(int command) {
	auto it = settingCommandsMap.find(command);
	if (it == settingCommandsMap.end()) {
		return false;
	}
	isHandlingCommand = true;
	SettingCommand& sc = *it->second;
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
	if (it == settingCommandsMap.end() || it->second->toggleCommand != command) {
		return -1;
	}
	return *it->second->setting;
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
			settingCommandsMap.insert({cmd, &sc}); \
		} \
	}
#include "settings.h"
#undef BoolSetting
	plugin_register("toggleaction", (void*)handleToggleState);
}
