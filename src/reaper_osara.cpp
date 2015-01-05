/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2014 James Teh
 * License: GNU General Public License version 2.0
 */

#include <windows.h>
#include <initguid.h>
#include <oleacc.h>
#include <string>
#include <sstream>
#define REAPERAPI_MINIMAL
#define REAPERAPI_IMPLEMENT
#define REAPERAPI_WANT_GetLastTouchedTrack
#define REAPERAPI_WANT_GetSetMediaTrackInfo
#define REAPERAPI_WANT_TimeMap2_timeToBeats
#define REAPERAPI_WANT_GetCursorPosition
#define REAPERAPI_WANT_GetContextMenu
#define REAPERAPI_WANT_GetSelectedMediaItem
#define REAPERAPI_WANT_GetSetMediaItemInfo
#define REAPERAPI_WANT_GetActiveTake
#define REAPERAPI_WANT_GetTakeName
#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>

using namespace std;

HWND mainHwnd;
DWORD guiThread;
IAccPropServices* accPropServices = NULL;

// We cache the last reported time so we can report just the components which have changed.
int oldMeasure = 0;
int oldBeat = 0;
int oldBeatPercent = 0;
// We maintain our own idea of focus for context sensitivity.
enum {
	FOCUS_NONE = 0,
	FOCUS_TRACK,
	FOCUS_ITEM,
	FOCUS_RULER,
} fakeFocus = FOCUS_NONE;
MediaTrack* currentTrack = NULL;

void outputMessage(const wchar_t* message) {
	// Tweak the MSAA accName for the current focus.
	GUITHREADINFO* guiThreadInfo = new GUITHREADINFO;
	guiThreadInfo->cbSize = sizeof(GUITHREADINFO);
	GetGUIThreadInfo(guiThread, guiThreadInfo);
	if (guiThreadInfo->hwndFocus) {
		accPropServices->SetHwndPropStr(guiThreadInfo->hwndFocus, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, message);
		// Fire a nameChange event so ATs will report this text.
		NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, guiThreadInfo->hwndFocus, OBJID_CLIENT, CHILDID_SELF);
	}
	delete guiThreadInfo;
}

void outputMessage(wstringstream& message) {
	outputMessage(message.str().c_str());
}

wstring formatCursorPosition() {
	wostringstream s;
	int measure;
	double beat = TimeMap2_timeToBeats(NULL, GetCursorPosition(), &measure, NULL, NULL, NULL);
	measure += 1;
	int wholeBeat = (int)beat + 1;
	int beatPercent = (int)(beat * 100) % 100;
	if (measure != oldMeasure) {
		s << L"bar " << measure << L" ";
		oldMeasure = measure;
	}
	if (wholeBeat != oldBeat) {
		s << L"beat " << wholeBeat << L" ";
		oldBeat = wholeBeat;
	}
	if (beatPercent != oldBeatPercent) {
		s << beatPercent << L"%";
		oldBeatPercent = beatPercent;
	}
	return s.str();
}

/* Report messages, etc. after actions are executed.
 * This is where the majority of the work is done.
 */
void postCommand(int command, int flag) {
	MediaTrack* track;
	MediaItem* item;
	MediaItem_Take* take;
	char* stringVal;
	wstringstream s;

	switch (command) {
		case 40285: case 40286: case 40001:
			// Go to track
			fakeFocus = FOCUS_TRACK;
			if (!(track = currentTrack = GetLastTouchedTrack()))
				return;
			s << (int)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", NULL);
			if (stringVal = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL))
				s << L" " << stringVal;
			outputMessage(s);
			break;

		case 40280:
			// Mute/unmute tracks
			if (!(track = GetLastTouchedTrack()))
				return;
			outputMessage(*(bool*)GetSetMediaTrackInfo(track, "B_MUTE", NULL) ? L"muted" : L"unmuted");
			break;

		case 40281:
			// Solo/unsolo tracks
			if (!(track = GetLastTouchedTrack()))
				return;
			outputMessage(*(int*)GetSetMediaTrackInfo(track, "I_SOLO", NULL) ? L"soloed" : L"unsoloed");
			break;

		case 40294:
			// Arm/unarm tracks
			if (!(track = GetLastTouchedTrack()))
				return;
			outputMessage(*(int*)GetSetMediaTrackInfo(track, "I_RECARM", NULL) ? L"armed" : L"unarmed");
			break;

		case 40495:
			// Cycle track record monitor
			if (!(track = GetLastTouchedTrack()))
				return;
			switch (*(int*)GetSetMediaTrackInfo(track, "I_RECMON", NULL)) {
				case 0:
					outputMessage(L"record monitor off");
					break;
				case 1:
					outputMessage(L"normal");
					break;
				case 2:
					outputMessage(L"not when playing");
			}
			break;

		case 40104: case 40105: case 41042: case 41043: case 41044: case 41045:
		case 40042: case 40043:
			// Cursor movement
			fakeFocus = FOCUS_RULER;
			outputMessage(formatCursorPosition().c_str());
			break;

		case 40416: case 40417:
			// Select and move to next/previous item
			fakeFocus = FOCUS_ITEM;
			if (!(item = GetSelectedMediaItem(0, 0)))
				return;
			s << L"item " << (int)GetSetMediaItemInfo(item, "IP_ITEMNUMBER", NULL) + 1;
			if (take = GetActiveTake(item))
				s << L" " << GetTakeName(take);
			s << L" " << formatCursorPosition();
			outputMessage(s);
			break;
	}
}

// A capturing lambda can't be passed as a Windows callback, hence the struct.
typedef struct {
	int index;
	int foundCount;
	HWND retHwnd;
} GetCurrentTrackVuData;
// Get the track VU window for the current track.
HWND getCurrentTrackVu() {
	GetCurrentTrackVuData data;
	data.index = (int)GetSetMediaTrackInfo(currentTrack, "IP_TRACKNUMBER", NULL);
	data.retHwnd = NULL;
	data.foundCount = 0;
	WNDENUMPROC callback = [] (HWND testHwnd, LPARAM lParam) -> BOOL {
		GetCurrentTrackVuData* data = (GetCurrentTrackVuData*)lParam;
		wchar_t className[14];
		if (GetClassNameW(testHwnd, className, 14) != 0
			&& wcscmp(className, L"REAPERtrackvu") == 0
			&& ++data->foundCount == data->index
		)  {
			data->retHwnd = testHwnd;
			return false;
		}
		return true;
	};
	EnumChildWindows(mainHwnd, callback, (LPARAM)&data);
	return data.retHwnd;
}

// Handle keyboard keys which can't be bound to actions.
int handleAccel(MSG* msg, accelerator_register_t* ctx) {
	if (msg->message == WM_KEYUP && msg->wParam == VK_APPS) {
		// Reaper doesn't usually handle the applications key.
		// Unfortunately, binding an action to this key succeeds but doesn't work.
		// Display the appropriate context menu depending no fake focus.
		switch (fakeFocus) {
			// todo: Fix positioning when TrackPopupContextMenu is used.
			case FOCUS_TRACK:
				if (GetKeyState(VK_CONTROL) & 0x8000) // Secondary
					TrackPopupMenu(GetContextMenu(0), 0, 0, 0, 0, mainHwnd, NULL);
				else {
					// This menu can't be retrieved with GetContextMenu.
					HWND hwnd = getCurrentTrackVu();
					if (hwnd)
						PostMessage(hwnd, WM_CONTEXTMENU, NULL, NULL);
				}
				break;
			case FOCUS_ITEM:
				TrackPopupMenu(GetContextMenu(1), 0, 0, 0, 0, mainHwnd, NULL);
				break;
			case FOCUS_RULER:
				TrackPopupMenu(GetContextMenu(2), 0, 0, 0, 0, mainHwnd, NULL);
				break;
		}
		return 1;
	}
	return 0;
}

accelerator_register_t accelReg = {
	handleAccel,
	true,
};

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec) {
	if (rec) {
		// Load.
		if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc || REAPERAPI_LoadAPI(rec->GetFunc) != 0)
			return 0; // Incompatible.

		if (CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER, IID_IAccPropServices, (void**)&accPropServices) != S_OK)
			return 0;
		mainHwnd = rec->hwnd_main;
		guiThread = GetWindowThreadProcessId(mainHwnd, NULL);
		rec->Register("hookpostcommand", postCommand);
		rec->Register("accelerator", &accelReg);
		return 1;

	} else {
		// Unload.
		accPropServices->Release();
		return 0;
	}
}

}
