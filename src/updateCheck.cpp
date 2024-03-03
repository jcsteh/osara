/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Update check code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2024 James Teh
 * License: GNU General Public License version 2.0
 */

#include <ctime>
#include <string>
#include <sstream>
#include <simpleson/json.h>
// osara.h includes windows.h, which must be included before other Windows
// headers.
#include "osara.h"
// Disable warnings for WDL, since we don't have any control over those.
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#include <WDL/jnetlib/httpGet.h>
#include <WDL/jnetlib/util.h>
# pragma clang diagnostic pop
#include "buildVersion.h"
#include "config.h"
#include "resource.h"
#include "translation.h"

const char UPDATE_URL[] = "http://osara.reaperaccessibility.com/snapshots/update.json";
const char DOWNLOAD_URL[] = "https://osara.reaperaccessibility.com/snapshots/";

class UpdateChecker {
	private:
	UpdateChecker(bool manual);
	~UpdateChecker();
	void tick();
	void error();

	void killTimer() {
		if (this->timer) {
			KillTimer(nullptr, this->timer);
			this->timer = 0;
		}
	}

	static void CALLBACK timerCb(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
		UpdateChecker::instance->tick();
	}

	static INT_PTR CALLBACK dialogProc(HWND dialog, UINT msg,
		WPARAM wParam, LPARAM lParam);

	bool manual;
	JNL_HTTPGet connection;
	UINT_PTR timer = 0;
	// The singleton instance.
	static UpdateChecker* instance;

	friend void startUpdateCheck(bool);
	friend void cancelUpdateCheck();
};

UpdateChecker* UpdateChecker::instance = nullptr;

void startUpdateCheck(bool manual) {
	if (UpdateChecker::instance) {
		// An update check is already running.
		return;
	}
	if (std::string(OSARA_VERSION).find(",") == std::string::npos) {
		// No commit in the version string. This is a local build.
		return;
	}
	auto curTime = time(nullptr);
	const char LAST_CHECK_KEY[] = "lastUpdateCheck";
	if (!manual) {
		// If REAPER update checks are disabled, disable ours too.
		char verCheck[2];
		GetPrivateProfileString("REAPER", "verchk", "", verCheck,
			sizeof(verCheck), get_ini_file());
		if (verCheck[0] == '0') {
			return;
		}
		// If we've checked for an update within the last day, don't check again.
		const char* lastCheckStr = GetExtState(CONFIG_SECTION, LAST_CHECK_KEY);
		uint64_t lastCheck = atoll(lastCheckStr);
		constexpr uint64_t ONE_DAY = 24 * 60 * 60;
		if (lastCheck + ONE_DAY > curTime) {
			return;
		}
	}
	// Keep track of the last time we checked for an update. We do this even for
	// a manual check because the user probably doesn't want auto update checks
	// soon if they've just done a manual check.
	SetExtState(CONFIG_SECTION, LAST_CHECK_KEY, format("{}", curTime).c_str(),
		true);
	UpdateChecker::instance = new UpdateChecker(manual);
}

void cancelUpdateCheck() {
	delete UpdateChecker::instance;
}

UpdateChecker::UpdateChecker(bool manual): manual(manual) {
	JNL::open_socketlib();
	this->connection.connect(
		UPDATE_URL);
	this->timer = SetTimer(nullptr, 0, 500, UpdateChecker::timerCb);
}

UpdateChecker::~UpdateChecker() {
	this->killTimer();
	UpdateChecker::instance = nullptr;
	JNL::close_socketlib();
}

void UpdateChecker::tick() {
	int res = this->connection.run();
	if (res == -1) {
		killTimer();
		this->error();
		cancelUpdateCheck();
		return;
	}
	if (res == 0) {
		// Still waiting for the connection to close.
		return;
	}
	// The connection has closed, so we've received all the data.
	this->killTimer();
	char data[16384];
	this->connection.get_bytes(data, sizeof(data));
	std::string curVersion = OSARA_VERSION;
	std::ostringstream s;
	try {
		json::jobject obj = json::jobject::parse(data);
		if ((std::string)obj["version"] == curVersion) {
			// We're running the latest version.
			if (this->manual) {
				MessageBox(GetForegroundWindow(), translate("No OSARA update available."),
					translate_ctxt("OSARA Update", "OSARA Update"),
					MB_ICONINFORMATION | MB_OK);
			}
			cancelUpdateCheck();
			return;
		}
		const char SEPARATOR[] = "\r\n\r\n";
		s << format(translate("OSARA version {} is available. Changes:"),
			(std::string)obj["version"]) << SEPARATOR;
		auto pos = curVersion.find(",");
		std::string curCommit = curVersion.substr(pos + 1);
		auto commits = obj["commits"].as_object();
		for (int c = 0; c < commits.size(); ++c) {
			auto commit = commits.array(c);
			std::string hash = commit.array(0);
			if (hash == curCommit) {
				// We're running this commit. The user doesn't need to know about changes
				// from here.
				break;
			}
			std::string message = commit.array(1);
			s << message << SEPARATOR;
		}
	} catch (...) {
		// JSON error.
		this->error();
		cancelUpdateCheck();
		return;
	}
	// Tell the user about the update!
	HWND dialog = CreateDialog(pluginHInstance, 
		MAKEINTRESOURCE(ID_UPDATE_DLG), GetForegroundWindow(),
		UpdateChecker::dialogProc);
	translateDialog(dialog);
	SetDlgItemText(dialog, ID_UPDATE_TEXT, s.str().c_str());
	ShowWindow(dialog, SW_SHOWNORMAL);
	cancelUpdateCheck();
}

void UpdateChecker::error() {
	if (this->manual) {
		MessageBox(GetForegroundWindow(),
			translate("Error checking for OSARA update."),
			nullptr, MB_OK | MB_ICONERROR);
	}
}

INT_PTR CALLBACK UpdateChecker::dialogProc(HWND dialog, UINT msg, WPARAM wParam,
	LPARAM lParam
) {
	switch (msg) {
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL) {
				DestroyWindow(dialog);
				return TRUE;
			}
			if (LOWORD(wParam) == IDOK) {
				ShellExecute(nullptr, "open", DOWNLOAD_URL, nullptr, nullptr,
					SW_SHOWNORMAL);
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

void cmdCheckForUpdate(Command* command) {
	startUpdateCheck(true);
}
