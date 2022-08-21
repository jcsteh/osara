/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Configuration code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2022 James Teh
 * License: GNU General Public License version 2.0
 */

#include "config.h"
#include "resource.h"
#include "translation.h"

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
