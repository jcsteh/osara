/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Main plug-in code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2014-2015 James Teh
 * License: GNU General Public License version 2.0
 */

#include <windows.h>
#include <initguid.h>
#include <oleacc.h>
#include <string>
#include <sstream>
#include <map>
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
#define REAPERAPI_WANT_Main_OnCommand
#define REAPERAPI_WANT_CountTracks
#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>

using namespace std;

HINSTANCE pluginHInstance;
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

const wchar_t* formatFolderState(int state, bool reportTrack=true) {
	if (state == 0)
		return reportTrack ? L"track" : NULL;
	else if (state == 1)
		return L"folder";
	return L"end of folder";
}

const wchar_t* getFolderCompacting(MediaTrack* track) {
	switch (*(int*)GetSetMediaTrackInfo(track, "I_FOLDERCOMPACT", NULL)) {
		case 0:
			return L"open";
		case 1:
			return L"small";
		case 2:
			return L"closed";
	}
	return L""; // Should never happen.
}

/* Report messages, etc. after actions are executed.
 * This is where the majority of the work is done.
 */
void postCommand(int command, int flag) {
	MediaTrack* track;
	MediaItem* item;
	MediaItem_Take* take;
	char* stringVal;
	int intVal;
	const wchar_t* message;
	wstringstream s;

	switch (command) {
		case 40285: case 40286: case 40001: {
			// Go to track
			fakeFocus = FOCUS_TRACK;
			if (!(track = currentTrack = GetLastTouchedTrack()))
				return;
			int trackNum = (int)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", NULL);
			MediaTrack* parentTrack = (MediaTrack*)GetSetMediaTrackInfo(track, "P_PARTRACK", NULL);
			if (parentTrack
				&& *(int*)GetSetMediaTrackInfo(parentTrack, "I_FOLDERDEPTH", NULL) == 1
				&& *(int*)GetSetMediaTrackInfo(parentTrack, "I_FOLDERCOMPACT", NULL) == 2
			) {
				// This track is inside a closed folder, so skip it.
				if (command != 40286 && trackNum == CountTracks(0)) {
					// We're moving forward and we're on the last track.
					// Therefore, go backward.
					// Note that this can't happen when the user moves backward
					// because the first track can never be inside a folder.
					command = 40286;
				}
				if (command == 40001) // Inserting a track
					command = 40285; // Skip by moving forward.
				Main_OnCommand(command, 0);
				return;
			}
			s << trackNum;
			intVal = *(int*)GetSetMediaTrackInfo(track, "I_FOLDERDEPTH", NULL);
			if (intVal == 1) // Folder
				s << L" " << getFolderCompacting(track);
			if (message = formatFolderState(intVal, false))
				s << L" " << message;
			if (stringVal = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL))
				s << L" " << stringVal;
			if (*(bool*)GetSetMediaTrackInfo(track, "B_MUTE", NULL))
				s << L" muted";
			if (*(int*)GetSetMediaTrackInfo(track, "I_SOLO", NULL))
				s << L" soloed";
			if (*(int*)GetSetMediaTrackInfo(track, "I_RECARM", NULL))
				s << L" armed";
			if (*(bool*)GetSetMediaTrackInfo(track, "B_PHASE", NULL))
				s << L" phase inverted";
			outputMessage(s);
			break;
		}

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

		case 40282:
			// Invert track phase
			if (!(track = GetLastTouchedTrack()))
				return;
			outputMessage(*(bool*)GetSetMediaTrackInfo(track, "B_PHASE", NULL) ? L"phase inverted" : L"phase normal");
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

		case 1041:
			// Cycle track folder state
			if (!(track = GetLastTouchedTrack()))
				return;
			outputMessage(formatFolderState(*(int*)GetSetMediaTrackInfo(track, "I_FOLDERDEPTH", NULL)));
			break;

		case 1042:
			// Cycle folder collapsed state
			if (!(track = GetLastTouchedTrack()))
				return;
			outputMessage(getFolderCompacting(track));
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

/*** Our commands/commands we want to intercept.
 * Each command should have a function and should be added to the COMMANDS array below.
 */

#define DEFACCEL {0, 0, 0}
typedef struct Command {
	gaccel_register_t gaccel;
	const char* id;
	void (*execute)(Command*);
} Command;

Command COMMANDS[] = {
	{{}, NULL},
};
map<int, Command*> commandsMap;

/*** Initialisation, termination and inner workings. */

bool handleCommand(int command, int flag) {
	const auto it = commandsMap.find(command);
	if (it != commandsMap.end()) {
		it->second->execute(it->second);
		return true;
	}
	return false;
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

		pluginHInstance = hInstance;
		if (CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER, IID_IAccPropServices, (void**)&accPropServices) != S_OK)
			return 0;
		mainHwnd = rec->hwnd_main;
		guiThread = GetWindowThreadProcessId(mainHwnd, NULL);
		rec->Register("hookpostcommand", postCommand);
		rec->Register("accelerator", &accelReg);

		for (int i = 0; COMMANDS[i].id; ++i) {
			int cmd = rec->Register("command_id", (void*)COMMANDS[i].id);
			COMMANDS[i].gaccel.accel.cmd = cmd;
			commandsMap.insert(make_pair(cmd, &COMMANDS[i]));
			rec->Register("gaccel", &COMMANDS[i].gaccel);
		}
		rec->Register("hookcommand", handleCommand);

		return 1;

	} else {
		// Unload.
		accPropServices->Release();
		return 0;
	}
}

}
