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
#include <Windowsx.h>
#include <Commctrl.h>
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
#define REAPERAPI_WANT_GetTrack
#define REAPERAPI_WANT_TrackFX_GetNumParams
#define REAPERAPI_WANT_TrackFX_GetParamName
#define REAPERAPI_WANT_TrackFX_GetCount
#define REAPERAPI_WANT_TrackFX_GetFXName
#define REAPERAPI_WANT_TrackFX_GetParam
#define REAPERAPI_WANT_TrackFX_SetParam
#define REAPERAPI_WANT_TrackFX_FormatParamValue
#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>
#include "resource.h"

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

const int FXPARAMS_SLIDER_RANGE = 1000;
int fxParams_fx;
int fxParams_param;
double fxParams_val, fxParams_valMin, fxParams_valMax;
// The raw value adjustment for an adjustment of 1 on the slider.
double fxParams_valStep;
const int FXPARAMS_VAL_TEXT_SIZE = 50;
// We cache the value text for later comparison.
char fxParams_valText[FXPARAMS_VAL_TEXT_SIZE];

void fxParams_updateValueText(HWND slider) {
	// Convert to Unicode.
	wostringstream s;
	s << fxParams_valText;
	// Set the slider's accessible value to this text.
	accPropServices->SetHwndPropStr(slider, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_VALUE,
		s.str().c_str());
}

void fxParams_updateSlider(HWND slider) {
	SendMessage(slider, TBM_SETPOS, TRUE,
		(int)((fxParams_val - fxParams_valMin) / fxParams_valStep));
	if (TrackFX_FormatParamValue(currentTrack, fxParams_fx, fxParams_param, fxParams_val, fxParams_valText, FXPARAMS_VAL_TEXT_SIZE))
		fxParams_updateValueText(slider);
}

void fxParams_onParamChange(HWND dialog, HWND params) {
	fxParams_param = ComboBox_GetCurSel(params);
	fxParams_val = TrackFX_GetParam(currentTrack, fxParams_fx, fxParams_param, 
		&fxParams_valMin, &fxParams_valMax);
	fxParams_valStep = (fxParams_valMax - fxParams_valMin) / FXPARAMS_SLIDER_RANGE;
	HWND slider = GetDlgItem(dialog, ID_FX_PARAM_VAL_SLIDER);
	fxParams_updateSlider(slider);
}

void fxParams_onSliderChange(HWND slider) {
	int sliderVal = SendMessage(slider, TBM_GETPOS, 0, 0);
	double newVal = sliderVal * fxParams_valStep;
	if (newVal == fxParams_val)
		return; // This is due to our own snapping call (below).
	TrackFX_SetParam(currentTrack, fxParams_fx, fxParams_param, newVal);
	int step = (newVal > fxParams_val) ? 1 : -1;
	fxParams_val = newVal;

	// If the value text (if any) doesn't change, the value change is insignificant.
	// Snap to the next change in value text.
	// todo: Optimise; perhaps a binary search?
	for (; 0 <= sliderVal && sliderVal <= FXPARAMS_SLIDER_RANGE; sliderVal += step) {
		// Continually adding to a float accumulates inaccuracy,
		// so calculate the value from scratch each time.
		newVal = sliderVal * fxParams_valStep;
		char testText[FXPARAMS_VAL_TEXT_SIZE];
		if (!TrackFX_FormatParamValue(currentTrack, fxParams_fx, fxParams_param, newVal, testText, FXPARAMS_VAL_TEXT_SIZE))
			break; // Formatted values not supported.
		if (strncmp(testText, fxParams_valText, FXPARAMS_VAL_TEXT_SIZE) != 0) {
			// The value text is different, so this chang eis significant.
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
			if (LOWORD(wParam) == ID_FX_PARAM && HIWORD(wParam) == CBN_SELCHANGE)
				fxParams_onParamChange((HWND)dialog, (HWND)lParam);
			else if (LOWORD(wParam) == IDCANCEL) {
				DestroyWindow(dialog);
				return 1;
			}
		case WM_HSCROLL:
			if (GetWindowLong((HWND)lParam, GWL_ID) == ID_FX_PARAM_VAL_SLIDER)
				fxParams_onSliderChange((HWND)lParam);
			return 1;
		case WM_CLOSE:
			DestroyWindow(dialog);
			return 1;
	}
	return FALSE;
}

void cmdFxParams(Command* command) {
	if (!currentTrack)
		return;
	char name[256];

	int fxCount = TrackFX_GetCount(currentTrack);
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
			TrackFX_GetFXName(currentTrack, f, name, sizeof(name));
			itemInfo.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
			itemInfo.fType = MFT_STRING;
			itemInfo.wID = f + 1;
			itemInfo.dwTypeData = name;
			itemInfo.cch = sizeof(name);
			InsertMenuItem(effects, f, false, &itemInfo);
		}
		fxParams_fx = TrackPopupMenu(effects, TPM_NONOTIFY | TPM_RETURNCMD, 0, 0, 0, mainHwnd, NULL) - 1;
		DestroyMenu(effects);
		if (fxParams_fx == -1)
			return; // Cancelled.
	}

	int numParams = TrackFX_GetNumParams(currentTrack, fxParams_fx);
	if (numParams == 0)
		return;
	HWND dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_FX_PARAMS_DLG), mainHwnd, fxParams_dialogProc);
	HWND params = GetDlgItem(dialog, ID_FX_PARAM);
	// Populate the parameter list.
	for (int p = 0; p < numParams; ++p) {
		TrackFX_GetParamName(currentTrack, fxParams_fx, p, name, sizeof(name));
		ComboBox_AddString(params, name);
	}
	ComboBox_SetCurSel(params, 0); // Select the first initially.
	HWND slider = GetDlgItem(dialog, ID_FX_PARAM_VAL_SLIDER);
	SendMessage(slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, FXPARAMS_SLIDER_RANGE));
	SendMessage(slider, TBM_SETLINESIZE, 0, 1);
	fxParams_onParamChange(dialog, params);
	ShowWindow(dialog, SW_SHOWNORMAL);
}

Command COMMANDS[] = {
	{{DEFACCEL, "View FX parameter list for current track"}, "OSARA_FXPARAMS", cmdFxParams},
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
