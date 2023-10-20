/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Code related to FX chain windows
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2016-2023 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

// osara.h includes windows.h, which must be included before other Windows
// headers.
#include "osara.h"
#ifdef _WIN32
#include <windowsx.h>
#include <CommCtrl.h>
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <regex>
#include <WDL/win32_utf8.h>
#include "fxChain.h"
#include "resource.h"
#include "translation.h"

using namespace std;

bool getFocusedFx(MediaTrack** track, MediaItem_Take** take, int* fx) {
	// GetTouchedOrFocusedFX is only available in REAPER 7. To ease the
	// transition, we don't use REAPERAPI_WANT for this and we use the older
	// function if this is unavailable. This hack should be removed in a few
	// months once we can reasonably bump our minimum REAPER version to 7+.
	static bool (*GetTouchedOrFocusedFX)(
			int mode, int* trackidxOut, int* itemidxOut, int* takeidxOut, int* fxidxOut, int* parmOut
	) = [] { return (decltype(GetTouchedOrFocusedFX))plugin_getapi("GetTouchedOrFocusedFX"); }();
	int trackIdx, itemIdx, takeIdx;
	if (GetTouchedOrFocusedFX) {
		int param;
		if (!GetTouchedOrFocusedFX(1, &trackIdx, &itemIdx, &takeIdx, fx, &param)) {
			return false;
		}
		if (param & 1) {
			return false; // Open, but no longer focused.
		}
	} else {
		// Temporary REAPER 6 compatibility.
		int type = GetFocusedFX2(&trackIdx, &itemIdx, fx);
		if (!type || type & 4) {
			return false;
		}
		--trackIdx;
		if (type == 2 && fx) { // Take
			takeIdx = HIWORD(*fx);
			*fx = LOWORD(*fx);
		} else { // Track
			takeIdx = -1;
		}
	}
	if (!track) {
		return true;
	}
	*track = trackIdx == -1 ? GetMasterTrack(nullptr) : GetTrack(nullptr, trackIdx);
	if (!take) {
		return true;
	}
	if (takeIdx != -1) {
		MediaItem* item = GetTrackMediaItem(*track, itemIdx);
		*take = GetTake(item, takeIdx);
	} else {
		*take = nullptr;
	}
	return true;
}

bool isFxListFocused() {
	return GetWindowLong(GetFocus(), GWL_ID) == 1076 && getFocusedFx();
}

void shortenFxName(const char* name, ostringstream& s) {
	const regex RE_FX_NAME("^(\\w+): (.+?)( \\(.*?\\))?$");
	cmatch m;
	regex_search(name, m, RE_FX_NAME);
	if (m.empty()) {
		s << name;
	} else {
		// Group 1 is the prefix, group 2 is the FX name, group 3 is the
		// parenthesised suffix.
		s << m.str(2);
		if (m.str(1) == "JS") {
			// For JS, not all effects have a vendor name. Therefore, we always
			// include the parenthesised suffix to avoid stripping potentially
			// useful info.
			s << m.str(3);
		}
	}
}

#ifdef _WIN32

bool maybeSwitchToFxPluginWindow() {
	HWND window = GetForegroundWindow();
	if (!getFocusedFx()) {
		return false;
	}
	// Descend. Observed as the first or as the last.
	if (!(window = FindWindowExA(window, nullptr, "#32770", nullptr))) {
		return false;
	}
	// Descend. Observed as the first or as the last.
	// Can not just search, we do not know the class nor name.
	if (!(window = GetWindow(window, GW_CHILD)))
		return false;
	char classname[16];
	if (!GetClassName(window, classname, sizeof(classname))) {
		return false;
	}
	if (!strcmp(classname, "ComboBox")) {
		// Plugin window should be the last.
		if (!(window = GetWindow(window, GW_HWNDLAST))) {
			return false;
		}
	} // else it is the first
	// We have found plug-in window or its container
	HWND plugin = window;
	// if focus is already inside plug-in window, let F6 work as usual
	HWND focus = GetFocus();
	if ((focus == plugin) || (IsChild(plugin, focus))) {
		return false;
	}
	// Try to focus the first child in Z order
	HWND child;
	while ((child = GetWindow(window, GW_CHILD))) {
		window = child;
	}
	while (window) {
		SetFocus(window);
		if ((window == plugin) || (GetFocus() == window)) {
			break; // success or the last possible attempt
		}
		window = GetParent(window);
	}
	return true;
}

// If the FX list in an FX chain dialog is focused, report active/bypassed for
// the selected effect.
bool maybeReportFxChainBypass(bool aboutToToggle) {
	if (!isFxListFocused()) {
		return false;
	}
	MediaTrack* track;
	MediaItem_Take* take;
	int fx;
	if (!getFocusedFx(&track, &take, &fx)) {
		return false; // No FX chain focused.
	}
	bool enabled;
	if (take) {
		enabled = TakeFX_GetEnabled(take, fx);
	} else {
		enabled = TrackFX_GetEnabled(track, fx);
	}
	if (aboutToToggle) {
		enabled = !enabled;
	}
	outputMessage(
			enabled ? translate("active") : translate("bypassed"),
			/* interrupt */ false
	);
	return true;
}

// When focusing a new effect, we delay reporting of bypass for three reasons:
// 1. The value returned by GetFocusedFX might not be updated immediately.
// 2. We want the bypass state to be consistently reported after the effect.
// 3. We want to give braille users a chance to read the effect name before
// the message with the bypass state clobbers it.
UINT_PTR reportFxChainBypassTimer = 0;

bool maybeReportFxChainBypassDelayed() {
	if (reportFxChainBypassTimer) {
		KillTimer(nullptr, reportFxChainBypassTimer);
	}
	if (!isFxListFocused()) {
		return false;
	}
	auto callback = [](HWND hwnd, UINT msg, UINT_PTR event, DWORD time) -> void {
		KillTimer(nullptr, event);
		reportFxChainBypassTimer = 0;
		maybeReportFxChainBypass(/* aboutToToggle */ false);
	};
	reportFxChainBypassTimer = SetTimer(nullptr, 0, 1000, callback);
	return true;
}

class PresetDialog {
	private:
	HWND combo; // REAPER's FX preset combo box.
	HWND dialog;
	HWND list; // Our preset ListView.
	string filter;

	void close() {
		DestroyWindow(this->dialog);
		SetFocus(this->combo);
		delete this;
	}

	static INT_PTR CALLBACK dialogProc(HWND dialogHwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto dialog = (PresetDialog*)GetWindowLongPtr(dialogHwnd, GWLP_USERDATA);
		switch (msg) {
			case WM_COMMAND:
				if (LOWORD(wParam) == ID_FXPRE_FILTER && HIWORD(wParam) == EN_KILLFOCUS) {
					dialog->onFilterChange();
					return TRUE;
				} else if (LOWORD(wParam) == IDOK) {
					dialog->applyPreset();
					dialog->close();
					return TRUE;
				} else if (LOWORD(wParam) == IDCANCEL) {
					dialog->close();
					return TRUE;
				}
				break;
			case WM_CLOSE:
				dialog->close();
				return TRUE;
		}
		return FALSE;
	}

	void onFilterChange() {
		char rawText[100];
		GetDlgItemText(this->dialog, ID_FXPRE_FILTER, rawText, sizeof(rawText));
		string text = rawText;
		// We want to match case insensitively, so convert to lower case.
		transform(text.begin(), text.end(), text.begin(), ::tolower);
		if (this->filter.compare(text) == 0) {
			return; // No change.
		}
		this->filter = text;
		this->updateList();
	}

	bool shouldIncludePreset(string name) {
		if (this->filter.empty()) {
			return true;
		}
		// Convert preset name to lower case for match.
		transform(name.begin(), name.end(), name.begin(), ::tolower);
		return name.find(this->filter) != string::npos;
	}

	void updateList() {
		int oldSel = this->getSelectedPreset();
		if (oldSel == -1) {
			oldSel = ComboBox_GetCurSel(this->combo);
		}
		ListView_DeleteAllItems(this->list);
		LRESULT count = SendMessage(this->combo, CB_GETCOUNT, 0, 0);
		int listIndex = 0;
		for (LRESULT comboIndex = 0; comboIndex < count; ++comboIndex) {
			LRESULT len = SendMessage(this->combo, CB_GETLBTEXTLEN, comboIndex, 0);
			if (len == CB_ERR) {
				break;
			}
			// len doesn't inclue null terminator.
			auto text = make_unique<char[]>(len + 1);
			SendMessage(this->combo, CB_GETLBTEXT, comboIndex, (LPARAM)text.get());
			if (!this->shouldIncludePreset(text.get())) {
				continue;
			}
			LVITEM item = {0};
			item.mask = LVIF_TEXT | LVIF_PARAM;
			item.iItem = listIndex++;
			item.pszText = text.get();
			item.lParam = comboIndex;
			if (comboIndex == oldSel) {
				// Preserve the previous selection when filtering.
				item.mask |= LVIF_STATE;
				item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
				item.state = LVIS_SELECTED | LVIS_FOCUSED;
			}
			ListView_InsertItem(this->list, &item);
		}
	}

	int getSelectedPreset() {
		LVITEM item = {0};
		item.mask = LVIF_PARAM;
		item.iItem = ListView_GetNextItem(this->list, -1, LVNI_FOCUSED);
		if (item.iItem == -1) {
			return -1;
		}
		ListView_GetItem(this->list, &item);
		return (int)item.lParam;
	}

	void applyPreset() {
		int preset = this->getSelectedPreset();
		if (preset != -1) {
			ComboBox_SetCurSel(this->combo, preset);
			LONG controlId = GetWindowLong(this->combo, GWL_ID);
			SendMessage(GetParent(this->combo), WM_COMMAND, MAKEWPARAM(controlId, CBN_SELCHANGE), (LPARAM)this->combo);
		}
	}

	public:
	PresetDialog(HWND presetCombo) : combo(presetCombo) {
		this->dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_FX_PRESET_DLG), mainHwnd, PresetDialog::dialogProc);
		translateDialog(this->dialog);
		SetWindowLongPtr(this->dialog, GWLP_USERDATA, (LONG_PTR)this);
		this->list = GetDlgItem(this->dialog, ID_FXPRE_PRESET);
		WDL_UTF8_HookListView(this->list);
		LVCOLUMN col = {0};
		col.mask = LVCF_WIDTH;
		col.cx = 150;
		ListView_InsertColumn(this->list, 0, &col);
		this->updateList();
		ShowWindow(this->dialog, SW_SHOWNORMAL);
	}
};

bool maybeOpenFxPresetDialog() {
	HWND hwnd = GetFocus();
	if (GetWindowLong(hwnd, GWL_ID) != 1000 || !isClassName(hwnd, "ComboBox") || !getFocusedFx()) {
		// Not the FX preset combo box.
		return false;
	}
	new PresetDialog(hwnd);
	return true;
}

void CALLBACK fireValueChangeOnFocus(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	KillTimer(nullptr, event);
	NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, GetFocus(), OBJID_CLIENT, CHILDID_SELF);
}

bool maybeSwitchFxTab(bool previous) {
	if (!getFocusedFx()) {
		// No FX focused.
		return false;
	}

	HWND tabCtrl = nullptr;
	auto findTabCtrl = [](HWND hwnd, LPARAM lParam) -> BOOL {
		if (isClassName(hwnd, "SysTabControl32")) {
			auto tabCtrl = (HWND*)lParam;
			*tabCtrl = hwnd;
			return FALSE; // Stop enumeration.
		}
		return TRUE; // Continue enumeration.
	};
	EnumChildWindows(GetForegroundWindow(), findTabCtrl, (LPARAM)&tabCtrl);
	if (!tabCtrl) {
		return false;
	}

	int selected = TabCtrl_GetCurSel(tabCtrl);
	if (selected == -1) {
		return false;
	}
	int count = TabCtrl_GetItemCount(tabCtrl);
	if (previous) {
		selected = selected > 0 ? selected - 1 : count - 1;
	} else {
		selected = selected < count - 1 ? selected + 1 : 0;
	}
	// We use SetCurFocus instead of SetCurSel because SetCurFocus sends
	// notifications, but SetCurSel doesn't.
	TabCtrl_SetCurFocus(tabCtrl, selected);
	TCITEM item = {0};
	item.mask = TCIF_TEXT;
	char text[50] = "\0";
	item.pszText = text;
	item.cchTextMax = sizeof(text);
	TabCtrl_GetItem(tabCtrl, selected, &item);
	if (text[0]) {
		// Translators: Reported when switching tabs in an effect such as ReaEQ.
		// {} will be replaced with the name of the tab; e.g. "1 tab".
		outputMessage(format(translate("{} tab"), text));
	}
	// The focused control doesn't change and it may not fire its own value
	// change event, so fire one ourselves. However, we have to delay this
	// because these ComboBox controls take a while to update.
	SetTimer(nullptr, 0, 30, fireValueChangeOnFocus);
	return true;
}

#endif // _WIN32
