/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Main plug-in code
 * Author: James Teh <jamie@nvaccess.org>
 * Copyright 2014-2015 NV Access Limited
 * License: GNU General Public License version 2.0
 */

#define UNICODE
#include <windows.h>
#ifdef _WIN32
#include <initguid.h>
#include <oleacc.h>
#include <Windowsx.h>
#include <Commctrl.h>
#endif
#include <string>
#include <sstream>
#include <map>
#include <iomanip>
#include <math.h>
#ifdef _WIN32
// We only need this on Windows and it apparently causes compilation issues on Mac.
#include <codecvt>
#endif
#include "osxa11y_wrapper.h" // NSA11y wrapper for OS X accessibility API
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
#define REAPERAPI_WANT_GetTrack
#define REAPERAPI_WANT_TrackFX_GetNumParams
#define REAPERAPI_WANT_TrackFX_GetParamName
#define REAPERAPI_WANT_TrackFX_GetCount
#define REAPERAPI_WANT_TrackFX_GetFXName
#define REAPERAPI_WANT_TrackFX_GetParam
#define REAPERAPI_WANT_TrackFX_SetParam
#define REAPERAPI_WANT_TrackFX_FormatParamValue
#define REAPERAPI_WANT_GetLastMarkerAndCurRegion
#define REAPERAPI_WANT_EnumProjectMarkers
#define REAPERAPI_WANT_GetSelectedEnvelope
#define REAPERAPI_WANT_GetEnvelopeName
#define REAPERAPI_WANT_NamedCommandLookup
#define REAPERAPI_WANT_GetMasterTrack
#define REAPERAPI_WANT_Track_GetPeakInfo
#define REAPERAPI_WANT_GetHZoomLevel
#define REAPERAPI_WANT_GetToggleCommandState
#define REAPERAPI_WANT_Main_OnCommand
#define REAPERAPI_WANT_Undo_CanUndo2
#define REAPERAPI_WANT_Undo_CanRedo2
#define REAPERAPI_WANT_parse_timestr_pos
#define REAPERAPI_WANT_GetMasterTrackVisibility
#define REAPERAPI_WANT_SetMasterTrackVisibility
#define REAPERAPI_WANT_GetAppVersion
#define REAPERAPI_WANT_SetCursorContext
#define REAPERAPI_WANT_GetPlayPosition
#define REAPERAPI_WANT_SetEditCurPos
#define REAPERAPI_WANT_CountMediaItems
#define REAPERAPI_WANT_GetSet_LoopTimeRange
#define REAPERAPI_WANT_CountTrackMediaItems
#define REAPERAPI_WANT_GetSetMediaItemTakeInfo
#define REAPERAPI_WANT_kbd_getTextFromCmd
#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>
#include <WDL/db2val.h>
#include "resource.h"

using namespace std;

HINSTANCE pluginHInstance;
HWND mainHwnd;
#ifdef _WIN32
DWORD guiThread;
IAccPropServices* accPropServices = NULL;
#endif

// We cache the last reported time so we can report just the components which have changed.
int oldMeasure = 0;
int oldBeat = 0;
int oldBeatPercent = 0;
int oldMinute = 0;
// We maintain our own idea of focus for context sensitivity.
enum {
	FOCUS_NONE = 0,
	FOCUS_TRACK,
	FOCUS_ITEM,
	FOCUS_RULER,
} fakeFocus = FOCUS_NONE;

/*** Utilities */

#ifdef _WIN32

wstring_convert<codecvt_utf8_utf16<WCHAR>, WCHAR> utf8Utf16;
wstring widen(const string& text) {
	return utf8Utf16.from_bytes(text);
}
string narrow(const wstring& text) {
	return utf8Utf16.to_bytes(text);
}

string lastMessage;
HWND lastMessageHwnd = NULL;
void outputMessage(const string& message) {
	// Tweak the MSAA accName for the current focus.
	GUITHREADINFO guiThreadInfo;
	guiThreadInfo.cbSize = sizeof(GUITHREADINFO);
	GetGUIThreadInfo(guiThread, &guiThreadInfo);
	if (!guiThreadInfo.hwndFocus)
		return;
	if (lastMessage.compare(message) == 0) {
		// The last message was the same.
		// Clients may ignore a nameChange event if the name didn't change.
		// Append a space to make it different.
		string procMessage = message;
		procMessage += ' ';
		accPropServices->SetHwndPropStr(guiThreadInfo.hwndFocus, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, widen(procMessage).c_str());
		lastMessage = procMessage;
	} else {
		accPropServices->SetHwndPropStr(guiThreadInfo.hwndFocus, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, widen(message).c_str());
		lastMessage = message;
	}
	// Fire a nameChange event so ATs will report this text.
	NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, guiThreadInfo.hwndFocus, OBJID_CLIENT, CHILDID_SELF);
	lastMessageHwnd = guiThreadInfo.hwndFocus;
}

#else // _WIN32

void outputMessage(const string& message) {
  NSA11yWrapper::osxa11y_announce(message);
}

#endif // _WIN32

void outputMessage(ostringstream& message) {
	outputMessage(message.str());
}

string formatCursorPosition(bool useMeasure=false) {
	ostringstream s;
	if (useMeasure) {
		int measure;
		double beat = TimeMap2_timeToBeats(NULL, GetCursorPosition(), &measure, NULL, NULL, NULL);
		measure += 1;
		int wholeBeat = (int)beat + 1;
		int beatPercent = (int)(beat * 100) % 100;
		if (measure != oldMeasure) {
			s << "bar " << measure << " ";
			oldMeasure = measure;
		}
		if (wholeBeat != oldBeat) {
			s << "beat " << wholeBeat << " ";
			oldBeat = wholeBeat;
		}
		if (beatPercent != oldBeatPercent) {
			s << beatPercent << "%";
			oldBeatPercent = beatPercent;
		}
		// #31: Clear cache for other units to avoid confusion if they are used later.
		oldMinute = 0;
	} else if (GetToggleCommandState(40365)) {
		// Minutes:seconds
		double second = GetCursorPosition();
		int minute = second / 60;
		second = fmod(second, 60);
		if (oldMinute != minute) {
			s << minute << " min ";
			oldMinute = minute;
		}
		s << fixed << setprecision(3);
		s << second << " sec";
		// #31: Clear cache for other units to avoid confusion if they are used later.
		oldMeasure = oldBeat = oldBeatPercent = 0;
	} else
		return formatCursorPosition(true);
	return s.str();
}

const char* formatFolderState(int state, bool reportTrack=true) {
	if (state == 0)
		return reportTrack ? "track" : NULL;
	else if (state == 1)
		return "folder";
	return "end of folder";
}

const char* getFolderCompacting(MediaTrack* track) {
	switch (*(int*)GetSetMediaTrackInfo(track, "I_FOLDERCOMPACT", NULL)) {
		case 0:
			return "open";
		case 1:
			return "small";
		case 2:
			return "closed";
	}
	return ""; // Should never happen.
}

void reportActionName(int command) {
	const char* name = kbd_getTextFromCmd(command, NULL);
	const char* start;
	// Skip the category before the colon (if any).
	for (start = name; *start; ++start) {
		if (*start == ':') {
			name = start + 2;
			break;
		}
	}
	ostringstream s;
	s << name;
	outputMessage(s);
}

typedef bool(*TrackStateCheck)(MediaTrack* track);

bool isTrackMuted(MediaTrack* track) {
	return *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", NULL);
}

bool isTrackSoloed(MediaTrack* track) {
	return *(int*)GetSetMediaTrackInfo(track, "I_SOLO", NULL);
}

bool isTrackArmed(MediaTrack* track) {
	return *(int*)GetSetMediaTrackInfo(track, "I_RECARM", NULL);
}

bool isTrackMonitored(MediaTrack* track) {
	return *(int*)GetSetMediaTrackInfo(track, "I_RECMON", NULL);
}

bool isTrackPhaseInverted(MediaTrack* track) {
	return *(bool*)GetSetMediaTrackInfo(track, "B_PHASE", NULL);
}

/*** Code to execute after existing actions.
 * This is used to report messages regarding the effect of the command, etc.
 */

void postGoToTrack(int command) {
	fakeFocus = FOCUS_TRACK;
	SetCursorContext(0, NULL);
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	// Need to cast to size_t first to avoid "loses information" error with clang.
	int trackNum = (int)(size_t)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", NULL);
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

	ostringstream s;
	s << trackNum;
	int folderDepth = *(int*)GetSetMediaTrackInfo(track, "I_FOLDERDEPTH", NULL);
	if (folderDepth == 1) // Folder
		s << " " << getFolderCompacting(track);
	const char* message = formatFolderState(folderDepth, false);
	if (message)
		s << " " << message;
	char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);
	if (trackName)
		s << " " << trackName;
	if (isTrackMuted(track))
		s << " muted";
	if (isTrackSoloed(track))
		s << " soloed";
	if (isTrackArmed(track))
		s << " armed";
	if (isTrackPhaseInverted(track))
		s << " phase inverted";
	int itemCount = CountTrackMediaItems(track);
	s << " " << itemCount << (itemCount == 1 ? " item" : " items");
	outputMessage(s);
}

void postToggleTrackMute(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(isTrackMuted(track) ? "muted" : "unmuted");
}

void postToggleTrackSolo(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(isTrackSoloed(track) ? "soloed" : "unsoloed");
}

void postToggleTrackArm(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(isTrackArmed(track) ? "armed" : "unarmed");
}

void postCycleTrackMonitor(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	switch (*(int*)GetSetMediaTrackInfo(track, "I_RECMON", NULL)) {
		case 0:
			outputMessage("record monitor off");
			break;
		case 1:
			outputMessage("normal");
			break;
		case 2:
			outputMessage("not when playing");
	}
}

void postInvertTrackPhase(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(isTrackPhaseInverted(track) ? "phase inverted" : "phase normal");
}

void postCursorMovement(int command) {
	fakeFocus = FOCUS_RULER;
	outputMessage(formatCursorPosition().c_str());
}

void postCursorMovementMeasure(int command) {
	fakeFocus = FOCUS_RULER;
	outputMessage(formatCursorPosition(true).c_str());
}

void postMoveToItem(int command) {
	MediaItem* item = GetSelectedMediaItem(0, 0);
	if (!item || (MediaTrack*)GetSetMediaItemInfo(item, "P_TRACK", NULL) != GetLastTouchedTrack())
		return; // No item in this direction on this track.
	fakeFocus = FOCUS_ITEM;
	SetCursorContext(1, NULL);
	ostringstream s;
	// Need to cast to size_t first to avoid "loses information" error with clang.
	s << (int)(size_t)GetSetMediaItemInfo(item, "IP_ITEMNUMBER", NULL) + 1;
	MediaItem_Take* take = GetActiveTake(item);
	if (take)
		s << " " << GetTakeName(take);
	s << " " << formatCursorPosition();
	outputMessage(s);
	double cursorPos = GetCursorPosition();
	if (GetPlayPosition() != cursorPos)
		SetEditCurPos(cursorPos, true, true); // Seek playback.
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

void postGoToMarker(int command) {
	ostringstream s;
	int marker, region;
	double markerPos;
	double cursorPos = GetCursorPosition();
	GetLastMarkerAndCurRegion(0, cursorPos, &marker, &region);
	const char* name;
	int number;
	if (marker >= 0) {
		EnumProjectMarkers(marker, NULL, &markerPos, NULL, &name, &number);
		if (markerPos == cursorPos) {
			if (name[0])
				s << name << " marker" << " ";
			else
				s << "marker " << number << " ";
		}
	}
	if (region >= 0) {
		EnumProjectMarkers(region, NULL, NULL, NULL, &name, &number);
		if (name[0])
			s << name << " region ";
		else
			s << "region " << number << " ";
	}
	s << formatCursorPosition();
	if (s.tellp() > 0)
		outputMessage(s);
}

void postSelectEnvelope(int command) {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope)
		return;
	char name[50];
	GetEnvelopeName(envelope, name, sizeof(name));
	ostringstream s;
	s << name << " envelope selected";
	outputMessage(s);
}

void postChangeTrackVolume(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	ostringstream s;
	s << fixed << setprecision(2);
	s << VAL2DB(*(double*)GetSetMediaTrackInfo(track, "D_VOL", NULL));
	outputMessage(s);
}

void postChangeHorizontalZoom(int command) {
	ostringstream s;
	s << fixed << setprecision(3);
	s << GetHZoomLevel() << " pixels/second";
	outputMessage(s);
}

void postChangeTrackPan(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	double pan = *(double*)GetSetMediaTrackInfo(track, "D_PAN", NULL) * 100;
	ostringstream s;
	if (pan == 0)
		s << "center";
	else if (pan < 0)
		s << -pan << "% left";
	else
		s << pan << "% right";
	outputMessage(s);
}

void postCycleRippleMode(int command) {
	ostringstream s;
	s << "ripple ";
	if (GetToggleCommandState(40310))
		s << "per-track";
	else if (GetToggleCommandState(40311))
		s << "all tracks";
	else
		s << "off";
	outputMessage(s);
}

void postToggleRepeat(int command) {
	outputMessage(GetToggleCommandState(command) ? "repeat on" : "repeat off");
}

void postSwitchToTake(int command) {
	MediaItem* item = GetSelectedMediaItem(0, 0);
	if (!item)
		return;
	MediaItem_Take* take = GetActiveTake(item);
	if (!take)
		return;
	ostringstream s;
	// Need to cast to size_t first to avoid "loses information" error with clang.
	s << (int)(size_t)GetSetMediaItemTakeInfo(take, "IP_TAKENUMBER", NULL) + 1 << " "
		<< GetTakeName(take);
	outputMessage(s);
}

typedef void (*PostCommandExecute)(int);
typedef struct PostCommand {
	int cmd;
	PostCommandExecute execute;
} PostCommand;
// For commands registered by other plug-ins.
typedef struct {
	const char* id;
	PostCommandExecute execute;
} PostCustomCommand;

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
	{41042, postCursorMovementMeasure}, // Go forward one measure
	{41043, postCursorMovementMeasure}, // Go back one measure
	{41044, postCursorMovementMeasure}, // Go forward one beat
	{41045, postCursorMovementMeasure}, // Go back one beat
	{40416, postMoveToItem}, // Item navigation: Select and move to previous item
	{40417, postMoveToItem}, // Item navigation: Select and move to next item
	{1041, postCycleTrackFolderState}, // Track: Cycle track folder state
	{1042, postCycleTrackFolderCollapsed}, // Track: Cycle track folder collapsed state
	{40172, postGoToMarker}, // Markers: Go to previous marker/project start
	{40173, postGoToMarker}, // Markers: Go to next marker/project end
	{40161, postGoToMarker}, // Markers: Go to marker 01
	{40162, postGoToMarker}, // Markers: Go to marker 02
	{40163, postGoToMarker}, // Markers: Go to marker 03
	{40164, postGoToMarker}, // Markers: Go to marker 04
	{40165, postGoToMarker}, // Markers: Go to marker 05
	{40166, postGoToMarker}, // Markers: Go to marker 06
	{40167, postGoToMarker}, // Markers: Go to marker 07
	{40168, postGoToMarker}, // Markers: Go to marker 08
	{40169, postGoToMarker}, // Markers: Go to marker 09
	{40160, postGoToMarker}, // Markers: Go to marker 10
	{41761, postGoToMarker}, // Regions: Go to region 01 after current region finishes playing (smooth seek)
	{41762, postGoToMarker}, // Regions: Go to region 02 after current region finishes playing (smooth seek)
	{41763, postGoToMarker}, // Regions: Go to region 03 after current region finishes playing (smooth seek)
	{41764, postGoToMarker}, // Regions: Go to region 04 after current region finishes playing (smooth seek)
	{41765, postGoToMarker}, // Regions: Go to region 05 after current region finishes playing (smooth seek)
	{41766, postGoToMarker}, // Regions: Go to region 06 after current region finishes playing (smooth seek)
	{41767, postGoToMarker}, // Regions: Go to region 07 after current region finishes playing (smooth seek)
	{41768, postGoToMarker}, // Regions: Go to region 08 after current region finishes playing (smooth seek)
	{41769, postGoToMarker}, // Regions: Go to region 09 after current region finishes playing (smooth seek)
	{41760, postGoToMarker}, // Regions: Go to region 10 after current region finishes playing (smooth seek)
	{41863, postSelectEnvelope}, // Track: Select previous envelope
	{41864, postSelectEnvelope}, // Track: Select next envelope
	{40115, postChangeTrackVolume}, // Track: Nudge track volume up
	{40116, postChangeTrackVolume}, // Track: Nudge track volume down
	{1011, postChangeHorizontalZoom}, // Zoom out horizontal
	{1012, postChangeHorizontalZoom}, // Zoom in horizontal
	{40283, postChangeTrackPan}, // Track: Nudge track pan left
	{40284, postChangeTrackPan}, // Track: Nudge track pan right
	{1155, postCycleRippleMode}, // Options: Cycle ripple editing mode
	{1068, postToggleRepeat}, // Transport: Toggle repeat
	{40125, postSwitchToTake}, // Take: Switch items to next take
	{40126, postSwitchToTake}, // Take: Switch items to previous take
	{0},
};
PostCustomCommand POST_CUSTOM_COMMANDS[] = {
	{"_XENAKIOS_NUDGSELTKVOLUP", postChangeTrackVolume}, // Xenakios/SWS: Nudge volume of selected tracks up
	{"_XENAKIOS_NUDGSELTKVOLDOWN", postChangeTrackVolume}, // Xenakios/SWS: Nudge volume of selected tracks down
	{NULL},
};
map<int, PostCommandExecute> postCommandsMap;

#ifdef _WIN32

// A capturing lambda can't be passed as a Windows callback, hence the struct.
typedef struct {
	int index;
	int foundCount;
	HWND retHwnd;
} GetTrackVuData;
// Get the track VU window for the current track.
HWND getTrackVu(MediaTrack* track) {
	GetTrackVuData data;
	data.index = (int)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", NULL);
	if (data.index == -1) // Master
		data.index = 0;
	if (GetMasterTrackVisibility() & 1)
		data.index += 1;
	data.retHwnd = NULL;
	data.foundCount = 0;
	WNDENUMPROC callback = [] (HWND testHwnd, LPARAM lParam) -> BOOL {
		GetTrackVuData* data = (GetTrackVuData*)lParam;
		WCHAR className[14];
		if (GetClassName(testHwnd, className, 14) != 0
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

bool shouldOverrideContextMenu() {
	GUITHREADINFO info;
	info.cbSize = sizeof(GUITHREADINFO);
	GetGUIThreadInfo(guiThread, &info);
	if (!info.hwndFocus)
		return false;
	WCHAR className[22];
	ostringstream s;
	return GetClassName(info.hwndFocus, className,  ARRAYSIZE(className)) && (
			wcscmp(className, L"REAPERTrackListWindow") == 0
			|| wcscmp(className, L"REAPERtrackvu") == 0
	);
}

// Handle keyboard keys which can't be bound to actions.
int handleAccel(MSG* msg, accelerator_register_t* ctx) {
	if (msg->message == WM_KEYUP && msg->wParam == VK_APPS && shouldOverrideContextMenu()) {
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
					MediaTrack* track = GetLastTouchedTrack();
					if (!track)
						return 0;
					HWND hwnd = getTrackVu(track);
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

#endif // _WIN32

/*** Our commands/commands we want to intercept.
 * Each command should have a function and should be added to the COMMANDS array below.
 */

typedef struct Command {
	int section;
	gaccel_register_t gaccel;
	const char* id;
	void (*execute)(Command*);
} Command;

void cmdUndo(Command* command) {
	const char* text = Undo_CanUndo2(0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	if (!text)
		return;
	ostringstream s;
	s << "Undo " << text;
	outputMessage(s);
}

void cmdRedo(Command* command) {
	const char* text = Undo_CanRedo2(0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	if (!text)
		return;
	ostringstream s;
	s << "Redo " << text;
	outputMessage(s);
}

void cmdSplitItems(Command* command) {
	int oldCount = CountMediaItems(0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	int added = CountMediaItems(0) - oldCount;
	ostringstream s;
	s << added << (added == 1 ? " item" : " items") << " added";
	outputMessage(s);
}

void cmdRemoveTracks(Command* command) {
	int oldCount = CountTracks(0);
	Main_OnCommand(40005, 0); // Track: Remove tracks
	int removed = oldCount - CountTracks(0);
	ostringstream s;
	s << removed << (removed == 1 ? " track" : " tracks") << " removed";
	outputMessage(s);
}

void cmdRemoveItems(Command* command) {
	int oldCount = CountMediaItems(0);
	Main_OnCommand(40006, 0); // Item: Remove items
	int removed = oldCount - CountMediaItems(0);
	ostringstream s;
	s << removed << (removed == 1 ? " item" : " items") << " removed";
	outputMessage(s);
}

void cmdRemoveTimeSelection(Command* command) {
	double start, end;
	GetSet_LoopTimeRange(false, false, &start, &end, false);
	Main_OnCommand(40201, 0); // Time selection: Remove contents of time selection (moving later items)
	if (start != end)
		outputMessage("Contents of time selection removed");
}

void cmdMoveItems(Command* command) {
	MediaItem* item = GetSelectedMediaItem(0, 0);
	double oldPos, oldLen;
	if (item) {
		oldPos = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
		oldLen = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", NULL);
	}
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	if (!item)
		return;
	// Only report if something actually happened.
	double newPos = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
	double newLen = *(double*)GetSetMediaItemInfo(item, "D_LENGTH", NULL);
	if (newPos != oldPos || newLen != oldLen)
		reportActionName(command->gaccel.accel.cmd);
}

#ifdef _WIN32
const int FXPARAMS_SLIDER_RANGE = 1000;
MediaTrack* fxParams_track;
int fxParams_fx;
int fxParams_param;
double fxParams_val, fxParams_valMin, fxParams_valMax;
// The raw value adjustment for an adjustment of 1 on the slider.
double fxParams_valStep;
const int FXPARAMS_VAL_TEXT_SIZE = 50;
// We cache the value text for later comparison.
char fxParams_valText[FXPARAMS_VAL_TEXT_SIZE];

void fxParams_updateValueText(HWND slider) {
	if (!fxParams_valText[0]) {
		// No value text.
		accPropServices->ClearHwndProps(slider, OBJID_CLIENT, CHILDID_SELF, &PROPID_ACC_VALUE, 1);
		return;
	}

	// Convert to Unicode.
	ostringstream s;
	s << fxParams_valText;
	// Set the slider's accessible value to this text.
	accPropServices->SetHwndPropStr(slider, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_VALUE,
		widen(s.str()).c_str());
}

void fxParams_updateSlider(HWND slider) {
	SendMessage(slider, TBM_SETPOS, TRUE,
		(int)((fxParams_val - fxParams_valMin) / fxParams_valStep));
	if (!TrackFX_FormatParamValue(fxParams_track, fxParams_fx, fxParams_param, fxParams_val, fxParams_valText, FXPARAMS_VAL_TEXT_SIZE))
		fxParams_valText[0] = '\0';
	fxParams_updateValueText(slider);
}

void fxParams_onParamChange(HWND dialog, HWND params) {
	fxParams_param = ComboBox_GetCurSel(params);
	fxParams_val = TrackFX_GetParam(fxParams_track, fxParams_fx, fxParams_param, 
		&fxParams_valMin, &fxParams_valMax);
	fxParams_valStep = (fxParams_valMax - fxParams_valMin) / FXPARAMS_SLIDER_RANGE;
	HWND slider = GetDlgItem(dialog, ID_FX_PARAM_VAL_SLIDER);
	fxParams_updateSlider(slider);
}

void fxParams_onSliderChange(HWND slider) {
	int sliderVal = SendMessage(slider, TBM_GETPOS, 0, 0);
	double newVal = sliderVal * fxParams_valStep + fxParams_valMin;
	TrackFX_SetParam(fxParams_track, fxParams_fx, fxParams_param, newVal);
	if (newVal == fxParams_val)
		return; // This is due to our own snapping call (below).
	int step = (newVal > fxParams_val) ? 1 : -1;
	fxParams_val = newVal;

	// If the value text (if any) doesn't change, the value change is insignificant.
	// Snap to the next change in value text.
	// todo: Optimise; perhaps a binary search?
	for (; 0 <= sliderVal && sliderVal <= FXPARAMS_SLIDER_RANGE; sliderVal += step) {
		// Continually adding to a float accumulates inaccuracy,
		// so calculate the value from scratch each time.
		newVal = sliderVal * fxParams_valStep + fxParams_valMin;
		char testText[FXPARAMS_VAL_TEXT_SIZE];
		if (!TrackFX_FormatParamValue(fxParams_track, fxParams_fx, fxParams_param, newVal, testText, FXPARAMS_VAL_TEXT_SIZE))
			break; // Formatted values not supported.
		if (strncmp(testText, fxParams_valText, FXPARAMS_VAL_TEXT_SIZE) != 0) {
			// The value text is different, so this change is significant.
			// Snap to this value.
			fxParams_val = newVal;
			fxParams_updateSlider(slider);
			break;
		}
	}
}

INT_PTR CALLBACK fxParams_dialogProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND:
			if (LOWORD(wParam) == ID_FX_PARAM && HIWORD(wParam) == CBN_SELCHANGE) {
				fxParams_onParamChange((HWND)dialog, (HWND)lParam);
				return TRUE;
			} else if (LOWORD(wParam) == IDCANCEL) {
				DestroyWindow(dialog);
				return TRUE;
			}
			break;
		case WM_HSCROLL:
			if (GetWindowLong((HWND)lParam, GWL_ID) == ID_FX_PARAM_VAL_SLIDER) {
				fxParams_onSliderChange((HWND)lParam);
				return TRUE;
			}
			break;
		case WM_CLOSE:
			DestroyWindow(dialog);
			return TRUE;
	}
	return FALSE;
}

void fxParams_begin(MediaTrack* track) {
	fxParams_track = track;
	char name[256];

	int fxCount = TrackFX_GetCount(fxParams_track);
	if (fxCount == 0)
		return;
	else if (fxCount == 1)
		fxParams_fx = 0;
	else {
		// Present a menu of effects.
		HMENU effects = CreatePopupMenu();
		MENUITEMINFO itemInfo;
		itemInfo.cbSize = sizeof(MENUITEMINFO);
		for (int f = 0; f < fxCount; ++f) {
			TrackFX_GetFXName(fxParams_track, f, name, sizeof(name));
			itemInfo.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
			itemInfo.fType = MFT_STRING;
			itemInfo.wID = f + 1;
			itemInfo.dwTypeData = (wchar_t*)widen(name).c_str();
			itemInfo.cch = ARRAYSIZE(name);
			InsertMenuItem(effects, f, false, &itemInfo);
		}
		fxParams_fx = TrackPopupMenu(effects, TPM_NONOTIFY | TPM_RETURNCMD, 0, 0, 0, mainHwnd, NULL) - 1;
		DestroyMenu(effects);
		if (fxParams_fx == -1)
			return; // Cancelled.
	}

	int numParams = TrackFX_GetNumParams(fxParams_track, fxParams_fx);
	if (numParams == 0)
		return;
	HWND dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_FX_PARAMS_DLG), mainHwnd, fxParams_dialogProc);
	HWND params = GetDlgItem(dialog, ID_FX_PARAM);
	// Populate the parameter list.
	for (int p = 0; p < numParams; ++p) {
		TrackFX_GetParamName(fxParams_track, fxParams_fx, p, name, sizeof(name));
		// Append the parameter number to facilitate efficient navigation
		// and to ensure reporting where two consecutive parameters have the same name (#32).
		ostringstream ns;
		ns << name << " (" << p + 1 << ")";
		ComboBox_AddString(params, widen(ns.str()).c_str());
	}
	ComboBox_SetCurSel(params, 0); // Select the first initially.
	HWND slider = GetDlgItem(dialog, ID_FX_PARAM_VAL_SLIDER);
	SendMessage(slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, FXPARAMS_SLIDER_RANGE));
	SendMessage(slider, TBM_SETLINESIZE, 0, 1);
	fxParams_onParamChange(dialog, params);
	ShowWindow(dialog, SW_SHOWNORMAL);
}

void cmdFxParamsCurrentTrack(Command* command) {
	MediaTrack* currentTrack = GetLastTouchedTrack();
	if (!currentTrack)
		return;
	fxParams_begin(currentTrack);
}

void cmdFxParamsMaster(Command* command) {
	fxParams_begin(GetMasterTrack(0));
}

MediaTrack* peakWatcher_track = NULL;
bool peakWatcher_followTrack = false;
// What the user can choose to watch.
enum {
	PWT_DISABLED,
	PWT_FOLLOW,
	PWT_MASTER,
	PWT_CURRENT,
	PWT_PREVSPEC,
};
double peakWatcher_level = 0;
struct {
	bool notify;
	double peak;
	DWORD time;
} peakWatcher_channels[2] = {{true, -150, 0}, {true, -150, 0}};
int peakWatcher_hold = 0;
UINT_PTR peakWatcher_timer = 0;

void peakWatcher_reset() {
	for (int c = 0; c < ARRAYSIZE(peakWatcher_channels); ++c)
		peakWatcher_channels[c].peak = -150;
}

VOID CALLBACK peakWatcher_watcher(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	if (!peakWatcher_track && !peakWatcher_followTrack)
		return; // Disabled.
	MediaTrack* currentTrack = GetLastTouchedTrack();
	if (peakWatcher_followTrack && peakWatcher_track != currentTrack) {
		// We're following the current track and it changed.
		peakWatcher_track = currentTrack;
		peakWatcher_reset();
		if (!currentTrack)
			return; // No current track, so nothing to do.
	}

	for (int c = 0; c < ARRAYSIZE(peakWatcher_channels); ++c) {
		double newPeak = VAL2DB(Track_GetPeakInfo(peakWatcher_track, c));
		if (peakWatcher_hold == -1 // Hold disabled
			|| newPeak > peakWatcher_channels[c].peak
			|| (peakWatcher_hold != 0 && time > peakWatcher_channels[c].time + peakWatcher_hold)
		) {
			peakWatcher_channels[c].peak = newPeak;
			peakWatcher_channels[c].time = time;
			if (peakWatcher_channels[c].notify && newPeak > peakWatcher_level) {
				ostringstream s;
				s << fixed << setprecision(1);
				s << "chan " << c + 1 << ": " << newPeak;
				outputMessage(s);
			}
		}
	}
}

void peakWatcher_onOk(HWND dialog) {
	// Retrieve the notification state for channels.
	for (int c = 0; c < ARRAYSIZE(peakWatcher_channels); ++c) {
		HWND channel = GetDlgItem(dialog, ID_PEAK_CHAN1 + c);
		peakWatcher_channels[c].notify = Button_GetCheck(channel) == BST_CHECKED;
	}

	WCHAR inText[7];
	// Retrieve the entered maximum level.
	if (GetDlgItemText(dialog, ID_PEAK_LEVEL, inText, ARRAYSIZE(inText)) > 0) {
		peakWatcher_level = _wtof(inText);
		// Restrict the range.
		peakWatcher_level = max(min(peakWatcher_level, 40), -40);
	}

	// Retrieve the entered hold time.
	if (GetDlgItemText(dialog, ID_PEAK_HOLD, inText, ARRAYSIZE(inText)) > 0) {
		peakWatcher_hold = _wtoi(inText);
		// Restrict the range.
		peakWatcher_hold = max(min(peakWatcher_hold, 20000), -1);
	}

	// Set up according to what track the user chose to watch.
	// If the track is changing, reset peaks.
	HWND track = GetDlgItem(dialog, ID_PEAK_TRACK);
	int sel = ComboBox_GetCurSel(track);
	switch(sel) {
		case PWT_DISABLED:
			peakWatcher_track = NULL;
			peakWatcher_followTrack = false;
			KillTimer(NULL, peakWatcher_timer);
			peakWatcher_timer = 0;
			return;
		case PWT_FOLLOW:
			if (!peakWatcher_followTrack)
				peakWatcher_reset();
			peakWatcher_followTrack = true;
			peakWatcher_track = NULL;
			break;
		case PWT_MASTER: {
			MediaTrack* master = GetMasterTrack(0);
			if (peakWatcher_track != master)
				peakWatcher_reset();
			peakWatcher_track = master;
			peakWatcher_followTrack = false;
			break;
		}
		case PWT_CURRENT:
			MediaTrack* currentTrack = GetLastTouchedTrack();
			if (peakWatcher_track != currentTrack)
				peakWatcher_reset();
			peakWatcher_track = currentTrack;
			peakWatcher_followTrack = false;
			break;
		// PWT_PREVSPEC means to keep watching some previously specified track; no change.
	}

	if (!peakWatcher_timer) // Previously disabled.
		peakWatcher_timer = SetTimer(NULL, 0, 30, peakWatcher_watcher);
}

INT_PTR CALLBACK peakWatcher_dialogProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND:
			if (LOWORD(wParam) == ID_PEAK_RESET) {
				peakWatcher_reset();
				DestroyWindow(dialog);
				return TRUE;
			} else if (LOWORD(wParam) == IDOK) {
				peakWatcher_onOk(dialog);
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

void cmdPeakWatcher(Command* command) {
	MediaTrack* currentTrack = GetLastTouchedTrack();
	if (!currentTrack)
		return;
	ostringstream s;
	HWND dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_PEAK_WATCHER_DLG), mainHwnd, peakWatcher_dialogProc);
	HWND track = GetDlgItem(dialog, ID_PEAK_TRACK);

	// Populate the list of what to watch.
	char* name;
	ComboBox_AddString(track, L"Disabled");
	if (!peakWatcher_followTrack && !peakWatcher_track)
		ComboBox_SetCurSel(track, PWT_DISABLED);
	ComboBox_AddString(track, L"Follow current track");
	if (peakWatcher_followTrack)
		ComboBox_SetCurSel(track, PWT_FOLLOW);
	ComboBox_AddString(track, L"Master");
	MediaTrack* master = GetMasterTrack(0);
	if (peakWatcher_track == master)
		ComboBox_SetCurSel(track, PWT_MASTER);
	s << (int)GetSetMediaTrackInfo(currentTrack, "IP_TRACKNUMBER", NULL);
	if (name = (char*)GetSetMediaTrackInfo(currentTrack, "P_NAME", NULL))
		s << ": " << name;
	ComboBox_AddString(track, widen(s.str()).c_str());
	if (!peakWatcher_followTrack && peakWatcher_track == currentTrack)
		ComboBox_SetCurSel(track, PWT_CURRENT);
	s.str("");
	if (peakWatcher_track && !peakWatcher_followTrack && peakWatcher_track != master && peakWatcher_track != currentTrack) {
		// Watching a previously specified track.
		s << (int)GetSetMediaTrackInfo(peakWatcher_track, "IP_TRACKNUMBER", NULL);
		if (name = (char*)GetSetMediaTrackInfo(peakWatcher_track, "P_NAME", NULL))
			s << ": " << name;
		ComboBox_AddString(track, widen(s.str()).c_str());
		ComboBox_SetCurSel(track, PWT_PREVSPEC);
		s.str("");
	}

	for (int c = 0; c < ARRAYSIZE(peakWatcher_channels); ++c) {
		HWND channel = GetDlgItem(dialog, ID_PEAK_CHAN1 + c);
		Button_SetCheck(channel, peakWatcher_channels[c].notify ? BST_CHECKED : BST_UNCHECKED);
	}

	HWND level = GetDlgItem(dialog, ID_PEAK_LEVEL);
	SendMessage(level, EM_SETLIMITTEXT, 6, 0);
	s << fixed << setprecision(2);
	s << peakWatcher_level;
	SendMessage(level, WM_SETTEXT, 0, (LPARAM)widen(s.str()).c_str());
	s.str("");

	HWND hold = GetDlgItem(dialog, ID_PEAK_HOLD);
	SendMessage(hold, EM_SETLIMITTEXT, 5, 0);
	s << peakWatcher_hold;
	SendMessage(hold, WM_SETTEXT, 0, (LPARAM)widen(s.str()).c_str());

	ShowWindow(dialog, SW_SHOWNORMAL);
}

void cmdReportPeakWatcher(Command* command) {
	if (!peakWatcher_track && !peakWatcher_followTrack) {
		outputMessage("Peak watcher is disabled");
		return;
	}
	ostringstream s;
	s << fixed << setprecision(1);
	for (int c = 0; c < ARRAYSIZE(peakWatcher_channels); ++c) {
		if (c != 0)
			s << ", ";
		s << c + 1 << ": " << peakWatcher_channels[c].peak;
	}
	outputMessage(s);
}

void cmdIoMaster(Command* command) {
	// If the master track isn't visible, make it so temporarily.
	int prevVisible = GetMasterTrackVisibility();
	if (!(prevVisible & 1))
		SetMasterTrackVisibility(prevVisible | 1);
	HWND hwnd = getTrackVu(GetMasterTrack(0));
	if (!hwnd)
		return; // Really shouldn't happen.
	// Use MSAA to get the location of the I/O button.
	hwnd = GetAncestor(hwnd, GA_PARENT);
	IAccessible* acc = NULL;
	VARIANT varChild;
	DWORD childId;
	if (GetAppVersion()[0] <= '4')
		childId = 7;
	else
		childId = 5;
	if (AccessibleObjectFromEvent(hwnd, OBJID_CLIENT, childId, &acc, &varChild) != S_OK)
		return;
	long l, t, w, h;
	HRESULT res = acc->accLocation(&l, &t, &w, &h, varChild);
	acc->Release();
	if (res != S_OK)
		return;
	// Click it!
	POINT point = {l, t};
	ScreenToClient(hwnd, &point);
	SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(point.x, point.y));
	SendMessage(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(point.x, point.y));
	// Restore master invisibility if appropriate.
	if (!(prevVisible & 1))
		SetMasterTrackVisibility(prevVisible);
}

#endif // _WIN32

void cmdReportRippleMode(Command* command) {
	postCycleRippleMode(command->gaccel.accel.cmd);
}

void reportTracksWithState(const char* prefix, TrackStateCheck checkState) {
	ostringstream s;
	s << prefix << ": ";
	int count = 0;
	for (int i = 0; i < CountTracks(0); ++i) {
		MediaTrack* track = GetTrack(0, i);
		if (checkState(track)) {
			++count;
			if (count > 1)
				s << ", ";
			s << i + 1;
			char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);
			if (name && name[0])
				s << " " << name;
		}
	}
	if (count == 0)
		s << "none";
	outputMessage(s);
}

void cmdReportMutedTracks(Command* command) {
	reportTracksWithState("Muted", isTrackMuted);
}

void cmdReportSoloedTracks(Command* command) {
	reportTracksWithState("Soloed", isTrackSoloed);
}

void cmdReportArmedTracks(Command* command) {
	reportTracksWithState("Armed", isTrackArmed);
}

void cmdReportMonitoredTracks(Command* command) {
	reportTracksWithState("Monitored", isTrackMonitored);
}

void cmdReportPhaseInvertedTracks(Command* command) {
	reportTracksWithState("Phase inverted", isTrackPhaseInverted);
}

void cmdRemoveFocus(Command* command) {
	switch (fakeFocus) {
		case FOCUS_TRACK:
			cmdRemoveTracks(NULL);
			break;
		case FOCUS_ITEM:
			cmdRemoveItems(NULL);
			break;
		default:
			cmdRemoveTimeSelection(NULL);
	}
}

#ifdef _WIN32
void cmdFocusNearestMidiEvent(Command* command) {
	GUITHREADINFO guiThreadInfo;
	guiThreadInfo.cbSize = sizeof(GUITHREADINFO);
	GetGUIThreadInfo(guiThread, &guiThreadInfo);
	if (!guiThreadInfo.hwndFocus)
		return;
	double cursorPos = GetCursorPosition();
	for (int i = 0; i < ListView_GetItemCount(guiThreadInfo.hwndFocus); ++i) {
		WCHAR textW[50];
		// Get the text from the position column (1).
		ListView_GetItemText(guiThreadInfo.hwndFocus, i, 1, textW, ARRAYSIZE(textW));
		// Convert this to project time. text is always in measures.beats.
		double eventPos = parse_timestr_pos(narrow(textW).c_str(), 2);
		if (eventPos >= cursorPos) {
			// This item is at or just after the cursor.
			int oldFocus = ListView_GetNextItem(guiThreadInfo.hwndFocus, -1, LVNI_FOCUSED);
			// Focus and select this item.
			ListView_SetItemState(guiThreadInfo.hwndFocus, i,
				LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
			ListView_EnsureVisible (guiThreadInfo.hwndFocus, i, false);
			if (oldFocus != -1 && oldFocus != i) {
				// Unselect the previously focused item.
				ListView_SetItemState(guiThreadInfo.hwndFocus, oldFocus,
					0, LVIS_SELECTED);
			}
			break;
		}
	}
}
#endif // _WIN32

#define DEFACCEL {0, 0, 0}
const int MAIN_SECTION = 0;
const int MIDI_EVENT_LIST_SECTION = 32061;

Command COMMANDS[] = {
	// Commands we want to intercept.
	{MAIN_SECTION, {{0, 0, 40029}, NULL}, NULL, cmdUndo}, // Edit: Undo
	{MAIN_SECTION, {{0, 0, 40030}, NULL}, NULL, cmdRedo}, // Edit: Redo
	{MAIN_SECTION, {{0, 0, 40012}, NULL}, NULL, cmdSplitItems}, // Item: Split items at edit or play cursor
	{MAIN_SECTION, {{0, 0, 40061}, NULL}, NULL, cmdSplitItems}, // Item: Split items at time selection
	{MAIN_SECTION, {{0, 0, 40005}, NULL}, NULL, cmdRemoveTracks}, // Track: Remove tracks
	{MAIN_SECTION, {{0, 0, 40006}, NULL}, NULL, cmdRemoveItems}, // Item: Remove items
	{MAIN_SECTION, {{0, 0, 40201}, NULL}, NULL, cmdRemoveTimeSelection}, // Time selection: Remove contents of time selection (moving later items)
	{MAIN_SECTION, {{0, 0, 40119}, NULL}, NULL, cmdMoveItems}, // Item edit: Move items/envelope points right
	{MAIN_SECTION, {{0, 0, 40120}, NULL}, NULL, cmdMoveItems}, // Item edit: Move items/envelope points left
	{MAIN_SECTION, {{0, 0, 40225}, NULL}, NULL, cmdMoveItems}, // Item edit: Grow left edge of items
	{MAIN_SECTION, {{0, 0, 40226}, NULL}, NULL, cmdMoveItems}, // Item edit: Shrink left edge of items
	{MAIN_SECTION, {{0, 0, 40227}, NULL}, NULL, cmdMoveItems}, // Item edit: Shrink right edge of items
	{MAIN_SECTION, {{0, 0, 40228}, NULL}, NULL, cmdMoveItems}, // Item edit: Grow right edge of items
	// Our own commands.
#ifdef _WIN32
	{MAIN_SECTION, {DEFACCEL, "OSARA: View FX parameters for current track"}, "OSARA_FXPARAMS", cmdFxParamsCurrentTrack},
	{MAIN_SECTION, {DEFACCEL, "OSARA: View FX parameters for master track"}, "OSARA_FXPARAMSMASTER", cmdFxParamsMaster},
	{MAIN_SECTION, {DEFACCEL, "OSARA: View Peak Watcher"}, "OSARA_PEAKWATCHER", cmdPeakWatcher},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report Peak Watcher peaks"}, "OSARA_REPORTPEAKWATCHER", cmdReportPeakWatcher},
	{MAIN_SECTION, {DEFACCEL, "OSARA: View I/O for master track"}, "OSARA_IOMASTER", cmdIoMaster},
#endif // _WIN32
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report ripple editing mode"}, "OSARA_REPORTRIPPLE", cmdReportRippleMode},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report muted tracks"}, "OSARA_REPORTMUTED", cmdReportMutedTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report soloed tracks"}, "OSARA_REPORTSOLOED", cmdReportSoloedTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report record armed tracks"}, "OSARA_REPORTARMED", cmdReportArmedTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report tracks with record monitor on"}, "OSARA_REPORTMONITORED", cmdReportMonitoredTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report tracks with phase inverted"}, "OSARA_REPORTPHASED", cmdReportPhaseInvertedTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Remove items/tracks/contents of time selection (depending on focus)"}, "OSARA_REMOVE", cmdRemoveFocus},
#ifdef _WIN32
	{MIDI_EVENT_LIST_SECTION, {DEFACCEL, "OSARA: Focus event nearest edit cursor"}, "OSARA_FOCUSMIDIEVENT", cmdFocusNearestMidiEvent},
#endif
	{0, {}, NULL, NULL},
};
map<pair<int, int>, Command*> commandsMap;

/*** Initialisation, termination and inner workings. */

void postCommand(int command, int flag) {
	const auto it = postCommandsMap.find(command);
	if (it != postCommandsMap.end())
		it->second(command);
}

bool isHandlingCommand = false;

bool handleCommand(KbdSectionInfo* section, int command, int val, int valHw, int relMode, HWND hwnd) {
	if (isHandlingCommand)
		return false; // Prevent re-entrance.
	const auto it = commandsMap.find(make_pair(section->uniqueID, command));
	if (it != commandsMap.end()) {
		isHandlingCommand = true;
		it->second->execute(it->second);
		isHandlingCommand = false;
		return true;
	}
	return false;
}

bool handleMainCommandFallback(int command, int flag) {
	if (isHandlingCommand)
		return false; // Prevent re-entrance.
	const auto it = commandsMap.find(make_pair(MAIN_SECTION, command));
	if (it != commandsMap.end()) {
		isHandlingCommand = true;
		it->second->execute(it->second);
		isHandlingCommand = false;
		return true;
	}
	return false;
}

// Initialisation that must be done after REAPER_PLUGIN_ENTRYPOINT;
// e.g. because it depends on stuff registered by other plug-ins.
#ifdef _WIN32
VOID CALLBACK delayedInit(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	for (int i = 0; POST_CUSTOM_COMMANDS[i].id; ++i) {
		int cmd = NamedCommandLookup(POST_CUSTOM_COMMANDS[i].id);
		if (cmd)
			postCommandsMap.insert(make_pair(cmd, POST_CUSTOM_COMMANDS[i].execute));
	}
	KillTimer(NULL, event);
}
#endif // _WIN32

#ifdef _WIN32

void CALLBACK handleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG objId, long childId, DWORD thread, DWORD time) {
	if (event == EVENT_OBJECT_FOCUS && lastMessageHwnd && hwnd != lastMessageHwnd) {
		// Focus is moving. Clear our tweak to accName for the previous focus.
		// This avoids problems such as the last message repeating when a new project is opened (#17).
		accPropServices->ClearHwndProps(lastMessageHwnd, OBJID_CLIENT, CHILDID_SELF, &PROPID_ACC_NAME, 1);
		lastMessageHwnd = NULL;
	}
}
HWINEVENTHOOK winEventHook = NULL;

accelerator_register_t accelReg = {
	handleAccel,
	true,
};

#endif // _WIN32

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec) {
	if (rec) {
		// Load.
		if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc || REAPERAPI_LoadAPI(rec->GetFunc) != 0)
			return 0; // Incompatible.

		pluginHInstance = hInstance;
		mainHwnd = rec->hwnd_main;

#ifdef _WIN32
		if (CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER, IID_IAccPropServices, (void**)&accPropServices) != S_OK)
			return 0;
		guiThread = GetWindowThreadProcessId(mainHwnd, NULL);
		winEventHook = SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, hInstance, handleWinEvent, 0, guiThread, WINEVENT_INCONTEXT);
#endif
		NSA11yWrapper::init(); // initialize NSA11y wrapper for OS X accessibility API

		for (int i = 0; POST_COMMANDS[i].cmd; ++i)
			postCommandsMap.insert(make_pair(POST_COMMANDS[i].cmd, POST_COMMANDS[i].execute));
		rec->Register("hookpostcommand", (void*)postCommand);

		for (int i = 0; COMMANDS[i].execute; ++i) {
			if (COMMANDS[i].id) {
				// This is our own command.
				if (COMMANDS[i].section == MAIN_SECTION) {
					COMMANDS[i].gaccel.accel.cmd = rec->Register("command_id", (void*)COMMANDS[i].id);
					rec->Register("gaccel", &COMMANDS[i].gaccel);
				} else {
					custom_action_register_t action;
					action.uniqueSectionId = COMMANDS[i].section;
					action.idStr = COMMANDS[i].id;
					action.name = COMMANDS[i].gaccel.desc;
					COMMANDS[i].gaccel.accel.cmd = rec->Register("custom_action", &action);
				}
			}
			commandsMap.insert(make_pair(make_pair(COMMANDS[i].section, COMMANDS[i].gaccel.accel.cmd), &COMMANDS[i]));
		}
		// hookcommand can only handle actions for the main section, so we need hookcommand2.
		// According to SWS, hookcommand2 must be registered before hookcommand.
		rec->Register("hookcommand2", (void*)handleCommand);
		// #29: Unfortunately, actions triggered by user-defined actions don't trigger hookcommand2,
		// but they do trigger hookcommand. IMO, this is a REAPER bug.
		// Register hookcommand as well so custom actions at least work for the main section.
		rec->Register("hookcommand", (void*)handleMainCommandFallback);

#ifdef _WIN32
		rec->Register("accelerator", &accelReg);
		SetTimer(NULL, NULL, 0, delayedInit);
#endif
		return 1;

	} else {
		// Unload.
#ifdef _WIN32
		UnhookWinEvent(winEventHook);
		accPropServices->Release();
#endif
		NSA11yWrapper::destroy(); // release NSA11y Wrapper
		return 0;
	}
}

}
