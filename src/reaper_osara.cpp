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

/*** Utilities */

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

void outputMessage(wostringstream& message) {
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

/*** Code to execute after existing actions.
 * This is used to report messages regarding the effect of the command, etc.
 */

void postGoToTrack(int command) {
	fakeFocus = FOCUS_TRACK;
	MediaTrack* track = currentTrack = GetLastTouchedTrack();
	if (!track)
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

	wostringstream s;
	s << trackNum;
	int folderDepth = *(int*)GetSetMediaTrackInfo(track, "I_FOLDERDEPTH", NULL);
	if (folderDepth == 1) // Folder
		s << L" " << getFolderCompacting(track);
	const wchar_t* message = formatFolderState(folderDepth, false);
	if (message)
		s << L" " << message;
	char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);
	if (trackName)
		s << L" " << trackName;
	if (*(bool*)GetSetMediaTrackInfo(track, "B_MUTE", NULL))
		s << L" muted";
	if (*(int*)GetSetMediaTrackInfo(track, "I_SOLO", NULL))
		s << L" soloed";
	if (*(int*)GetSetMediaTrackInfo(track, "I_RECARM", NULL))
		s << L" armed";
	if (*(bool*)GetSetMediaTrackInfo(track, "B_PHASE", NULL))
		s << L" phase inverted";
	outputMessage(s);
}

void postToggleTrackMute(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(*(bool*)GetSetMediaTrackInfo(track, "B_MUTE", NULL) ? L"muted" : L"unmuted");
}

void postToggleTrackSolo(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(*(int*)GetSetMediaTrackInfo(track, "I_SOLO", NULL) ? L"soloed" : L"unsoloed");
}

void postToggleTrackArm(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(*(int*)GetSetMediaTrackInfo(track, "I_RECARM", NULL) ? L"armed" : L"unarmed");
}

void postCycleTrackMonitor(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
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
}

void postInvertTrackPhase(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(*(bool*)GetSetMediaTrackInfo(track, "B_PHASE", NULL) ? L"phase inverted" : L"phase normal");
}

void postCursorMovement(int command) {
	fakeFocus = FOCUS_RULER;
	outputMessage(formatCursorPosition().c_str());
}

void postMoveToItem(int command) {
	fakeFocus = FOCUS_ITEM;
	MediaItem* item = GetSelectedMediaItem(0, 0);
	if (!item)
		return;
	wostringstream s;
	s << L"item " << (int)GetSetMediaItemInfo(item, "IP_ITEMNUMBER", NULL) + 1;
	MediaItem_Take* take = GetActiveTake(item);
	if (take)
		s << L" " << GetTakeName(take);
	s << L" " << formatCursorPosition();
	outputMessage(s);
}

void postCycleTrackFolderState(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(formatFolderState(*(int*)GetSetMediaTrackInfo(track, "I_FOLDERDEPTH", NULL)));
}

void postCycleTrackFolderCollapsed(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(getFolderCompacting(track));
}

typedef void (*PostCommandExecute)(int);
typedef struct PostCommand {
	int cmd;
	PostCommandExecute execute;
} PostCommand;

PostCommand POST_COMMANDS[] = {
	{40285, postGoToTrack}, // Track: Go to next track
	{40286, postGoToTrack}, // Track: Go to previous track
	{40001, postGoToTrack}, // Track: Insert new track
	{40280, postToggleTrackMute}, // Track: Mute/unmute tracks
	{40281, postToggleTrackSolo}, // Track: Solo/unsolo tracks
	{40294, postToggleTrackArm}, // Toggle record arming for current (last touched) track
	{40495, postCycleTrackMonitor}, // Track: Cycle track record monitor
	{40282, postInvertTrackPhase}, // Track: Invert track phase
	{40104, postCursorMovement}, // View: Move cursor left one pixel
	{40105, postCursorMovement}, // View: Move cursor right one pixel
	{40042, postCursorMovement}, // Transport: Go to start of project
	{40043, postCursorMovement}, // Transport: Go to end of project
	{41042, postCursorMovement}, // Go forward one measure
	{41043, postCursorMovement}, // Go back one measure
	{41044, postCursorMovement}, // Go forward one beat
	{41045, postCursorMovement}, // Go back one beat
	{40416, postMoveToItem}, // Item navigation: Select and move to previous item
	{40417, postMoveToItem}, // Item navigation: Select and move to next item
	{1041, postCycleTrackFolderState}, // Track: Cycle track folder state
	{1042, postCycleTrackFolderCollapsed}, // Track: Cycle track folder collapsed state
	{0},
};
map<int, PostCommandExecute> postCommandsMap;

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

void postCommand(int command, int flag) {
	const auto it = postCommandsMap.find(command);
	if (it != postCommandsMap.end())
		it->second(command);
}

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

		for (int i = 0; POST_COMMANDS[i].cmd; ++i)
			postCommandsMap.insert(make_pair(POST_COMMANDS[i].cmd, POST_COMMANDS[i].execute));
		rec->Register("hookpostcommand", postCommand);

		for (int i = 0; COMMANDS[i].id; ++i) {
			int cmd = rec->Register("command_id", (void*)COMMANDS[i].id);
			COMMANDS[i].gaccel.accel.cmd = cmd;
			commandsMap.insert(make_pair(cmd, &COMMANDS[i]));
			rec->Register("gaccel", &COMMANDS[i].gaccel);
		}
		rec->Register("hookcommand", handleCommand);

		rec->Register("accelerator", &accelReg);
		return 1;

	} else {
		// Unload.
		accPropServices->Release();
		return 0;
	}
}

}
