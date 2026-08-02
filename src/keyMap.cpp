/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Key map merge code
 * Copyright 2026 James Teh
 * License: GNU General Public License version 2.0
 */

#include "osara.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <regex>
#include <vector>
#include <WDL/win32_utf8.h>
#include "keyMap.h"
#include "resource.h"
#include "translation.h"

#ifdef _WIN32
#include <CommCtrl.h>
#else
// SWELL does not define these extended ListView styles. It still supports the
// state-image values used below to retain each conflict's checked state.
#define LVS_EX_CHECKBOXES 0
#define LVS_EX_FULLROWSELECT 0
#endif

using namespace std;

namespace {

string joinPath(const string& directory, const string& file) {
	if (directory.empty()) {
		return file;
	}
	const char last = directory.back();
	if (last == '/' || last == '\\') {
		return directory + file;
	}
#ifdef _WIN32
	return directory + "\\" + file;
#else
	return directory + "/" + file;
#endif
}

bool readLines(const string& path, vector<string>& lines) {
#ifdef _WIN32
	ifstream input(widen(path));
#else
	ifstream input(path);
#endif
	if (!input) {
		return false;
	}
	string line;
	while (getline(input, line)) {
		lines.push_back(line);
	}
	return !input.bad();
}

bool writeLines(const string& path, const vector<string>& lines) {
#ifdef _WIN32
	ofstream output(widen(path), ios::binary | ios::trunc);
#else
	ofstream output(path, ios::binary | ios::trunc);
#endif
	if (!output) {
		return false;
	}
	for (const string& line: lines) {
		output << line << '\n';
	}
	return static_cast<bool>(output);
}

#ifndef _WIN32
bool copyFile(const string& source, const string& destination) {
	ifstream input(source, ios::binary);
	ofstream output(destination, ios::binary | ios::trunc);
	if (!input || !output) {
		return false;
	}
	output << input.rdbuf();
	return static_cast<bool>(output);
}
#endif

string trim(string text) {
	const auto first = text.find_first_not_of(" \t");
	if (first == string::npos) {
		return "";
	}
	const auto last = text.find_last_not_of(" \t");
	return text.substr(first, last - first + 1);
}

struct KeyRecord {
	string identity;
	string command;
	string line;
	size_t lineIndex;
};

struct OtherRecord {
	string identity;
	string line;
	size_t lineIndex;
};

struct Conflict {
	KeyRecord osara;
	KeyRecord user;
	bool accepted = false;
};

class KeyMapMerge {
	vector<string> userLines;
	vector<string> osaraLines;
	map<string, KeyRecord> userKeys;
	map<string, KeyRecord> osaraKeys;
	map<string, OtherRecord> userOther;
	map<string, OtherRecord> osaraOther;
	vector<Conflict> conflicts;
	vector<KeyRecord> keysToAdd;
	vector<OtherRecord> otherToAdd;
	map<size_t, string> replacements;
	string error;

	static bool parseKey(const string& line, size_t lineIndex, KeyRecord& record) {
		static const regex pattern(R"(^KEY\s+(\d+)\s+(\d+)\s+(\S+)\s+(\d+)(.*)$)");
		smatch match;
		if (!regex_match(line, match, pattern)) {
			return false;
		}
		record.identity = match.str(1) + " " + match.str(2) + " " + match.str(4);
		record.command = match.str(3);
		record.line = line;
		record.lineIndex = lineIndex;
		return true;
	}

	static bool parseOther(const string& line, size_t lineIndex, OtherRecord& record) {
		static const regex actionPattern(R"ACT(^ACT\s+\S+\s+\S+\s+"([^"]+)")ACT");
		static const regex scriptPattern(R"(^SCR\s+\S+\s+\S+\s+(\S+))");
		smatch match;
		if (regex_search(line, match, actionPattern)) {
			record.identity = "ACT " + match.str(1);
		} else if (regex_search(line, match, scriptPattern)) {
			record.identity = "SCR " + match.str(1);
		} else {
			return false;
		}
		record.line = line;
		record.lineIndex = lineIndex;
		return true;
	}

	static void indexLines(
		const vector<string>& lines, map<string, KeyRecord>& keys,
		map<string, OtherRecord>& other
	) {
		for (size_t index = 0; index < lines.size(); ++index) {
			KeyRecord key;
			if (parseKey(lines[index], index, key)) {
				keys[key.identity] = key;
				continue;
			}
			OtherRecord record;
			if (parseOther(lines[index], index, record)) {
				other[record.identity] = record;
			}
		}
	}

	void analyse() {
		for (const auto& [identity, osara]: osaraKeys) {
			auto user = userKeys.find(identity);
			if (user == userKeys.end()) {
				keysToAdd.push_back(osara);
			} else if (user->second.command != osara.command) {
				conflicts.push_back({osara, user->second});
			}
		}
		for (const auto& [identity, osara]: osaraOther) {
			auto user = userOther.find(identity);
			if (user == userOther.end()) {
				otherToAdd.push_back(osara);
			} else if (user->second.line != osara.line) {
				replacements[user->second.lineIndex] = osara.line;
			}
		}
	}

	public:
	bool load() {
		const string resourcePath = GetResourcePath();
		const string userPath = joinPath(resourcePath, "reaper-kb.ini");
		const string osaraPath = joinPath(resourcePath, "KeyMaps/OSARA.ReaperKeyMap");
		if (!readLines(userPath, userLines)) {
			error = translate("Unable to read your REAPER key map.");
			return false;
		}
		if (!readLines(osaraPath, osaraLines)) {
			error = translate("Unable to read the OSARA key map");
			return false;
		}
		indexLines(userLines, userKeys, userOther);
		indexLines(osaraLines, osaraKeys, osaraOther);
		analyse();
		return true;
	}

	const string& getError() const { return error; }
	const vector<Conflict>& getConflicts() const { return conflicts; }
	void setConflictAccepted(size_t index, bool accepted) { conflicts[index].accepted = accepted; }
	string getKeyText(const Conflict& conflict) const {
		static const regex pattern(R"(#\s*([^:]+)\s*:\s*([^:]+)\s*:)");
		smatch match;
		if (regex_search(conflict.osara.line, match, pattern)) {
			return trim(match.str(1)) + ": " + trim(match.str(2));
		}
		return conflict.osara.identity;
	}

	string getActionText(const KeyRecord& record) const {
		if (record.command == "0") {
			// Translators: Shown for a key in the Merge OSARA Key map dialog when
			// REAPER's default mapping for that key has been disabled.
			return translate("Disabled default");
		}
		static const regex pattern(
			R"(#\s*[^:]+\s*:\s*[^:]+\s*:\s*(?:OVERRIDE DEFAULT\s*:\s*)?(.*)$)");
		smatch match;
		if (regex_search(record.line, match, pattern)) {
			return trim(match.str(1));
		}
		return record.command;
	}

	int getAddedKeyCount() const { return static_cast<int>(keysToAdd.size()); }
	int getAcceptedConflictCount() const {
		return static_cast<int>(count_if(conflicts.begin(), conflicts.end(),
			[](const Conflict& conflict) { return conflict.accepted; }));
	}
	bool hasChanges() const {
		return !keysToAdd.empty() || !otherToAdd.empty() || !replacements.empty() ||
			getAcceptedConflictCount() != 0;
	}

	bool write() {
		map<size_t, string> outputReplacements = replacements;
		for (const Conflict& conflict: conflicts) {
			if (conflict.accepted) {
				outputReplacements[conflict.user.lineIndex] = conflict.osara.line;
			}
		}
		vector<string> output;
		output.reserve(userLines.size() + keysToAdd.size() + otherToAdd.size());
		for (size_t index = 0; index < userLines.size(); ++index) {
			auto replacement = outputReplacements.find(index);
			output.push_back(replacement == outputReplacements.end() ?
				userLines[index] : replacement->second);
		}
		for (const OtherRecord& record: otherToAdd) {
			output.push_back(record.line);
		}
		for (const KeyRecord& record: keysToAdd) {
			output.push_back(record.line);
		}

		const string userPath = joinPath(GetResourcePath(), "reaper-kb.ini");
		const string tempPath = userPath + ".osara-merge.tmp";
		const string backupPath = userPath + ".osara-merge-backup";
		if (!writeLines(tempPath, output)) {
			error = translate("Unable to write the merged key map.");
			return false;
		}
#ifdef _WIN32
		if (!CopyFileW(widen(userPath).c_str(), widen(backupPath).c_str(), TRUE) ||
			!MoveFileExW(widen(tempPath).c_str(), widen(userPath).c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			DeleteFileW(widen(tempPath).c_str());
			error = translate("Unable to replace your REAPER key map.");
			return false;
		}
#else
		if (!copyFile(userPath, backupPath) || rename(tempPath.c_str(), userPath.c_str()) != 0) {
			remove(tempPath.c_str());
			error = translate("Unable to replace your REAPER key map.");
			return false;
		}
#endif
		return true;
	}
};

void showMergeCompleteAndExit(int added, int overridden) {
	MessageBox(GetForegroundWindow(),
		format(translate("{} key bindings added and {} conflicting bindings overridden. REAPER will now exit. Please restart REAPER to apply the changes."),
			added, overridden).c_str(),
		translate("Restart REAPER"), MB_OK | MB_ICONINFORMATION);
	Main_OnCommand(40004, 0); // File: Quit REAPER
}

class KeyMapMergeDialog {
	unique_ptr<KeyMapMerge> merge;
	HWND dialog;
	HWND list;

	void close() {
		DestroyWindow(dialog);
		delete this;
	}

	void updateChoices() {
		for (int index = 0; index < ListView_GetItemCount(list); ++index) {
			merge->setConflictAccepted(index, isChecked(index));
		}
	}

	bool isChecked(int index) const {
		LVITEM item = {};
		item.mask = LVIF_STATE;
		item.iItem = index;
		item.stateMask = LVIS_STATEIMAGEMASK;
		ListView_GetItem(list, &item);
		return (item.state & LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(2);
	}

	void setChecked(int index, bool checked) {
		LVITEM item = {};
		item.mask = LVIF_STATE;
		item.iItem = index;
		item.stateMask = LVIS_STATEIMAGEMASK;
		item.state = INDEXTOSTATEIMAGEMASK(checked ? 2 : 1);
		ListView_SetItem(list, &item);
	}

	void setAllChoices(bool accepted) {
		for (int index = 0; index < ListView_GetItemCount(list); ++index) {
			setChecked(index, accepted);
		}
	}

	void mergeAndExit() {
		updateChoices();
		if (!merge->hasChanges()) {
			MessageBox(dialog, translate("No changes were selected."), nullptr,
				MB_OK | MB_ICONINFORMATION);
			return;
		}
		if (!merge->write()) {
			MessageBox(dialog, merge->getError().c_str(), nullptr, MB_OK | MB_ICONERROR);
			return;
		}
		const int added = merge->getAddedKeyCount();
		const int overridden = merge->getAcceptedConflictCount();
		close();
		showMergeCompleteAndExit(added, overridden);
	}

	static INT_PTR CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto* self = reinterpret_cast<KeyMapMergeDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		switch (msg) {
			case WM_COMMAND:
				if (LOWORD(wParam) == ID_KEYMAP_ACCEPT_ALL) {
					self->setAllChoices(true);
					return TRUE;
				}
				if (LOWORD(wParam) == ID_KEYMAP_ACCEPT_NONE) {
					self->setAllChoices(false);
					return TRUE;
				}
				if (LOWORD(wParam) == IDOK) {
					self->mergeAndExit();
					return TRUE;
				}
				if (LOWORD(wParam) == IDCANCEL) {
					self->close();
					return TRUE;
				}
				break;
			case WM_CLOSE:
				self->close();
				return TRUE;
		}
		return FALSE;
	}

	public:
	KeyMapMergeDialog(unique_ptr<KeyMapMerge> merge): merge(std::move(merge)) {
		dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_KEYMAP_DLG),
			mainHwnd, dialogProc);
		translateDialog(dialog);
		SetWindowLongPtr(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		list = GetDlgItem(dialog, ID_KEYMAP_LIST);
		WDL_UTF8_HookListView(list);
		ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
		const vector<string> headings = {translate("Key"), translate("OSARA"), translate("Yours")};
		for (int index = 0; index < static_cast<int>(headings.size()); ++index) {
			LVCOLUMN column = {};
			column.mask = LVCF_TEXT | LVCF_WIDTH;
			column.pszText = const_cast<char*>(headings[index].c_str());
			column.cx = index == 0 ? 100 : 200;
			ListView_InsertColumn(list, index, &column);
		}
		const vector<Conflict>& conflicts = this->merge->getConflicts();
		for (int index = 0; index < static_cast<int>(conflicts.size()); ++index) {
			LVITEM item = {};
			item.mask = LVIF_TEXT;
			item.iItem = index;
			string key = this->merge->getKeyText(conflicts[index]);
			item.pszText = key.data();
			ListView_InsertItem(list, &item);
			string osara = this->merge->getActionText(conflicts[index].osara);
			string user = this->merge->getActionText(conflicts[index].user);
			ListView_SetItemText(list, index, 1, osara.data());
			ListView_SetItemText(list, index, 2, user.data());
		}
		ShowWindow(dialog, SW_SHOWNORMAL);
	}
};

void finishMerge(unique_ptr<KeyMapMerge> merge) {
	if (!merge->write()) {
		MessageBox(GetForegroundWindow(), merge->getError().c_str(), nullptr, MB_OK | MB_ICONERROR);
		return;
	}
	showMergeCompleteAndExit(merge->getAddedKeyCount(), merge->getAcceptedConflictCount());
}

bool confirmAutomaticMerge(const KeyMapMerge& merge) {
	return MessageBox(GetForegroundWindow(),
		format(translate("No key binding conflicts were found. {} key bindings will be added. Do you want to merge the OSARA key map?"),
			merge.getAddedKeyCount()).c_str(),
		translate("Merge OSARA Key Map"), MB_YESNO | MB_ICONQUESTION) == IDYES;
}

} // namespace

void cmdMergeOsaraKeyMap(int command) {
	auto merge = make_unique<KeyMapMerge>();
	if (!merge->load()) {
		MessageBox(GetForegroundWindow(), merge->getError().c_str(), nullptr, MB_OK | MB_ICONERROR);
		return;
	}
	if (merge->getConflicts().empty()) {
		if (!merge->hasChanges()) {
			MessageBox(GetForegroundWindow(),
				translate("Your key map already includes all OSARA key map changes."),
				translate("Merge OSARA Key Map"), MB_OK | MB_ICONINFORMATION);
			return;
		}
		if (!confirmAutomaticMerge(*merge)) {
			return;
		}
		finishMerge(std::move(merge));
		return;
	}
	new KeyMapMergeDialog(std::move(merge));
}
