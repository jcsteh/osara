/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Peak Watcher code
 * Author: James Teh <jamie@nvaccess.org>
 * Copyright 2015-2016 NV Access Limited
 * License: GNU General Public License version 2.0
 */

#define UNICODE
#include <windows.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <Commctrl.h>
#include <Windowsx.h>
#include <WDL/db2val.h>
#include "osara.h"
#include "resource.h"

using namespace std;

const int PW_NUM_TRACKS = 2;
const int PW_NUM_CHANNELS = 2;
struct {
	MediaTrack* track;
	bool follow;
	struct {
		double peak;
		DWORD time;
	} channels[PW_NUM_CHANNELS];
} pw_tracks[PW_NUM_TRACKS] = {};
// What the user can choose to watch.
enum {
	PWT_DISABLED,
	PWT_FOLLOW,
	PWT_MASTER,
	PWT_TRACKS_START,
};
bool pw_notifyChannels[PW_NUM_CHANNELS] = {true, true};
double pw_level = 0;
int pw_hold = 0;
int pw_numTracksEnabled = 0;
UINT_PTR pw_timer = 0;

void pw_resetTrack(int trackIndex, bool report=false) {
	for (int c = 0; c < PW_NUM_CHANNELS; ++c)
		pw_tracks[trackIndex].channels[c].peak = -150;
	if (report)
		outputMessage("reset");
}

VOID CALLBACK pw_watcher(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	ostringstream s;
	s << fixed << setprecision(1);
	for (int t = 0; t < PW_NUM_TRACKS; ++t) {
		auto& pwTrack = pw_tracks[t];
		if (!pwTrack.track && !pwTrack.follow)
			continue; // Disabled.
		MediaTrack* currentTrack = GetLastTouchedTrack();
		if (pwTrack.follow && pwTrack.track != currentTrack) {
			// We're following the current track and it changed.
			pwTrack.track = currentTrack;
			pw_resetTrack(t);
			if (!currentTrack)
				continue; // No current track, so nothing to do.
		}

		bool trackReported = false;
		for (int c = 0; c < PW_NUM_CHANNELS; ++c) {
			auto& pwChan = pwTrack.channels[c];
			double newPeak = VAL2DB(Track_GetPeakInfo(pwTrack.track, c));
			if (pw_hold == -1 // Hold disabled
				|| newPeak > pwChan.peak
				|| (pw_hold != 0 && time > pwChan.time + pw_hold)
			) {
				pwChan.peak = newPeak;
				pwChan.time = time;
				if (pw_notifyChannels[c] && newPeak > pw_level) {
					if (pw_numTracksEnabled > 1 && !trackReported) {
						// Only report the track name if watching more than one track
						// and a channel actually changed for this track.
						int trackNum = (int)(size_t)GetSetMediaTrackInfo(pwTrack.track, "IP_TRACKNUMBER", NULL);
						if (trackNum <= 0)
							s << "master ";
						else
							s << "track " << trackNum << " ";
					}
					s << "chan " << c + 1 << ": " << newPeak;
					outputMessage(s);
				}
			}
		}
	}
}

void pw_onOk(HWND dialog) {
	// Retrieve the notification state for channels.
	for (int c = 0; c < PW_NUM_CHANNELS; ++c) {
		HWND channel = GetDlgItem(dialog, ID_PEAK_CHAN1 + c);
		pw_notifyChannels[c] = Button_GetCheck(channel) == BST_CHECKED;
	}

	WCHAR inText[7];
	// Retrieve the entered maximum level.
	if (GetDlgItemText(dialog, ID_PEAK_LEVEL, inText, ARRAYSIZE(inText)) > 0) {
		pw_level = _wtof(inText);
		// Restrict the range.
		pw_level = max(min(pw_level, 40), -40);
	}

	// Retrieve the hold choice/time.
	if (Button_GetCheck(GetDlgItem(dialog, ID_PEAK_HOLD_DISABLED)) == BST_CHECKED)
		pw_hold = -1;
	else if (Button_GetCheck(GetDlgItem(dialog, ID_PEAK_HOLD_FOREVER)) == BST_CHECKED)
		pw_hold = 0;
	else if (GetDlgItemText(dialog, ID_PEAK_HOLD_TIME, inText, ARRAYSIZE(inText)) > 0) {
		pw_hold = _wtoi(inText);
		// Restrict the range.
		pw_hold = max(min(pw_hold, 20000), 1);
	}

	// Set up according to what tracks the user chose to watch.
	// If there was a change, reset.
	pw_numTracksEnabled = 0;
	for (int t = 0; t < PW_NUM_TRACKS; ++t) {
		int id = ID_PEAK_TRACK1 + t;
		HWND trackSel = GetDlgItem(dialog, id);
		int sel = ComboBox_GetCurSel(trackSel);
		auto& pwTrack = pw_tracks[t];
		if (sel == PWT_DISABLED) {
			// Disabled.
			pwTrack.track = NULL;
			pwTrack.follow = false;
			continue;
		}
		++pw_numTracksEnabled;
		if (sel == PWT_FOLLOW) {
			// Follow current track.
			if (pwTrack.follow)
				continue; // Already following.
			pw_resetTrack(t);
			pwTrack.follow = true;
			pwTrack.track = NULL;
			continue;
		}
		MediaTrack* track;
		if (sel == PWT_MASTER)
			track = GetMasterTrack(0);
		else // sel >= PWT_TRACKS_START
			track = GetTrack(0, sel - PWT_TRACKS_START);
		if (pwTrack.track != track) {
			pw_resetTrack(t);
			pwTrack.track = track;
			pwTrack.follow = false;
		}
	}

	if (pw_numTracksEnabled == 0 && pw_timer) {
		// Peak watcher disabled completely.
		KillTimer(NULL, pw_timer);
		pw_timer = 0;
	} else if (!pw_timer) // Previously disabled.
		pw_timer = SetTimer(NULL, 0, 30, pw_watcher);
}

INT_PTR CALLBACK pw_dialogProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND: {
			int id = LOWORD(wParam);
			if (ID_PEAK_HOLD_DISABLED <= id && id <= ID_PEAK_HOLD_FOR) {
				EnableWindow(GetDlgItem(dialog, ID_PEAK_HOLD_TIME),
					id == ID_PEAK_HOLD_FOR ? BST_CHECKED : BST_UNCHECKED);
			} else if (id == ID_PEAK_RESET) {
				for (int t = 0; t < PW_NUM_TRACKS; ++t)
					pw_resetTrack(t);
				DestroyWindow(dialog);
				return TRUE;
			} else if (id == IDOK) {
				pw_onOk(dialog);
				DestroyWindow(dialog);
				return TRUE;
			} else if (id == IDCANCEL) {
				DestroyWindow(dialog);
				return TRUE;
			}
			break;
		}
		case WM_CLOSE:
			DestroyWindow(dialog);
			return TRUE;
	}
	return FALSE;
}

void cmdPeakWatcher(Command* command) {
	int trackCount = CountTracks(0);
	if (trackCount == 0)
		return;
	ostringstream s;
	HWND dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_PEAK_WATCHER_DLG), mainHwnd, pw_dialogProc);

	for (int pwt = 0; pwt < PW_NUM_TRACKS; ++pwt) {
		HWND trackSel = GetDlgItem(dialog, ID_PEAK_TRACK1 + pwt);
		auto& pwTrack = pw_tracks[pwt];
		// Populate the list of what to watch.
		ComboBox_AddString(trackSel, L"None");
		if (!pwTrack.follow && !pwTrack.track)
			ComboBox_SetCurSel(trackSel, PWT_DISABLED);
		ComboBox_AddString(trackSel, L"Follow current track");
		if (pwTrack.follow)
			ComboBox_SetCurSel(trackSel, PWT_FOLLOW);
		ComboBox_AddString(trackSel, L"Master");
		MediaTrack* track = GetMasterTrack(0);
		if (pwTrack.track == track)
			ComboBox_SetCurSel(trackSel, PWT_MASTER);
		MediaTrack* currentTrack = GetLastTouchedTrack();
		for (int t = 0; t < trackCount; ++t) {
			track = GetTrack(0, t);
			if (track == currentTrack)
				s << "current ";
			s << (int)(size_t)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", NULL);
			char* name;
			if (name = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL))
				s << ": " << name;
			ComboBox_AddString(trackSel, widen(s.str()).c_str());
			s.str("");
			if (!pwTrack.follow && pwTrack.track == track)
				ComboBox_SetCurSel(trackSel, PWT_TRACKS_START + t);
		}
	}

	for (int c = 0; c < PW_NUM_CHANNELS; ++c) {
		HWND channel = GetDlgItem(dialog, ID_PEAK_CHAN1 + c);
		Button_SetCheck(channel, pw_notifyChannels[c] ? BST_CHECKED : BST_UNCHECKED);
	}

	HWND level = GetDlgItem(dialog, ID_PEAK_LEVEL);
	SendMessage(level, EM_SETLIMITTEXT, 6, 0);
	s << fixed << setprecision(2);
	s << pw_level;
	SendMessage(level, WM_SETTEXT, 0, (LPARAM)widen(s.str()).c_str());
	s.str("");

	HWND holdTime = GetDlgItem(dialog, ID_PEAK_HOLD_TIME);
	SendMessage(holdTime, EM_SETLIMITTEXT, 5, 0);
	int id;
	if (pw_hold == -1)
		id = ID_PEAK_HOLD_DISABLED;
	else if (pw_hold == 0)
		id = ID_PEAK_HOLD_FOREVER;
	else {
		id = ID_PEAK_HOLD_FOR;
		s << pw_hold;
		SendMessage(holdTime, WM_SETTEXT, 0, (LPARAM)widen(s.str()).c_str());
	}
	HWND hold = GetDlgItem(dialog, id);
	Button_SetCheck(hold, BST_CHECKED);
	EnableWindow(holdTime, pw_hold > 0);

	ShowWindow(dialog, SW_SHOWNORMAL);
}

void pw_report(int trackIndex, int channel) {
	assert(trackIndex < PW_NUM_TRACKS);
	assert(channel < PW_NUM_CHANNELS);
	auto& pwTrack = pw_tracks[trackIndex];
	if (!pwTrack.track && !pwTrack.follow) {
		outputMessage("Peak Watcher track disabled");
		return;
	}
	ostringstream s;
	s << fixed << setprecision(1);
	s << pwTrack.channels[channel].peak;
	outputMessage(s);
}

void cmdReportPeakWatcherT1C1(Command* command) {
	pw_report(0, 0);
}

void cmdReportPeakWatcherT1C2(Command* command) {
	pw_report(0, 1);
}

void cmdReportPeakWatcherT2C1(Command* command) {
	pw_report(1, 0);
}

void cmdReportPeakWatcherT2C2(Command* command) {
	pw_report(1, 1);
}

void cmdResetPeakWatcherT1(Command* command) {
	pw_resetTrack(0, true);
}

void cmdResetPeakWatcherT2(Command* command) {
	pw_resetTrack(1, true);
}
