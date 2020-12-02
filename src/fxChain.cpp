/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Code related to FX chain windows
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2020 James Teh
 * License: GNU General Public License version 2.0
 */

#include <windows.h>
#ifdef _WIN32
# include <windowsx.h>
# include <CommCtrl.h>
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <WDL/win32_utf8.h>
#include "osara.h"
#include "resource.h"

using namespace std;

#ifdef _WIN32

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
				if (LOWORD(wParam) == ID_FXPRE_FILTER &&
						HIWORD(wParam) == EN_KILLFOCUS) {
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
		}
	}

	public:

	PresetDialog(HWND presetCombo): combo(presetCombo) {
		this->dialog = CreateDialog(pluginHInstance,
			MAKEINTRESOURCE(ID_FX_PRESET_DLG), mainHwnd, PresetDialog::dialogProc);
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
	if (GetWindowLong(hwnd, GWL_ID) != 1000) {
		// Not the FX preset combo box.
		return false;
	}
	new PresetDialog(hwnd);
	return true;
}

#endif // _WIN32
