/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Main header
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2014-2022 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#ifndef _OSARA_H
#define _OSARA_H

#ifdef _WIN32
# include <windows.h>
#else
// Disable warnings for SWELL, since we don't have any control over those.
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
# include <windows.h>
# pragma clang diagnostic pop
#endif
#include <string>
#include <sstream>

#define REAPERAPI_MINIMAL
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
#define REAPERAPI_WANT_parse_timestr_len
#define REAPERAPI_WANT_parse_timestr_pos
#define REAPERAPI_WANT_TimeMap2_QNToTime
#define REAPERAPI_WANT_GetMasterTrackVisibility
#define REAPERAPI_WANT_SetMasterTrackVisibility
#define REAPERAPI_WANT_SetCursorContext
#define REAPERAPI_WANT_GetPlayPosition
#define REAPERAPI_WANT_SetEditCurPos
#define REAPERAPI_WANT_CountMediaItems
#define REAPERAPI_WANT_GetSet_LoopTimeRange
#define REAPERAPI_WANT_CountTrackMediaItems
#define REAPERAPI_WANT_GetSetMediaItemTakeInfo
#define REAPERAPI_WANT_kbd_getTextFromCmd
// GetCursorContext always seems to return 1.
#define REAPERAPI_WANT_GetCursorContext2
#define REAPERAPI_WANT_CountSelectedMediaItems
#define REAPERAPI_WANT_CountSelectedTracks
#define REAPERAPI_WANT_mkvolstr
#define REAPERAPI_WANT_mkpanstr
#define REAPERAPI_WANT_parsepanstr
#define REAPERAPI_WANT_GetExtState
#define REAPERAPI_WANT_SetExtState
#define REAPERAPI_WANT_GetEnvelopePoint
#define REAPERAPI_WANT_GetEnvelopePointEx
#define REAPERAPI_WANT_GetEnvelopePointByTimeEx
#define REAPERAPI_WANT_CountEnvelopePoints
#define REAPERAPI_WANT_CountEnvelopePointsEx
#define REAPERAPI_WANT_format_timestr_pos
#define REAPERAPI_WANT_GetPlayState
#define REAPERAPI_WANT_TimeMap_curFrameRate
#define REAPERAPI_WANT_GetTrackMediaItem
#define REAPERAPI_WANT_Undo_BeginBlock
#define REAPERAPI_WANT_Undo_EndBlock
#define REAPERAPI_WANT_IsTrackVisible
#define REAPERAPI_WANT_GetMasterMuteSoloFlags
#define REAPERAPI_WANT_CountProjectMarkers
#define REAPERAPI_WANT_CountTempoTimeSigMarkers
#define REAPERAPI_WANT_FindTempoTimeSigMarker
#define REAPERAPI_WANT_GetTempoTimeSigMarker
#define REAPERAPI_WANT_GetTakeNumStretchMarkers
#define REAPERAPI_WANT_GetTakeStretchMarker
#define REAPERAPI_WANT_SetTakeStretchMarker
#define REAPERAPI_WANT_ValidatePtr
#define REAPERAPI_WANT_DeleteTempoTimeSigMarker
#define REAPERAPI_WANT_MIDIEditor_GetActive
#define REAPERAPI_WANT_MIDIEditor_GetTake
#define REAPERAPI_WANT_MIDIEditor_GetSetting_str
#define REAPERAPI_WANT_MIDI_CountEvts
#define REAPERAPI_WANT_MIDI_GetNote
#define REAPERAPI_WANT_MIDI_SetNote
#define REAPERAPI_WANT_MIDI_GetCC
#define REAPERAPI_WANT_MIDI_SetCC
#define REAPERAPI_WANT_MIDI_GetProjTimeFromPPQPos
#define REAPERAPI_WANT_MIDI_EnumSelNotes
#define REAPERAPI_WANT_MIDI_EnumSelCC
#define REAPERAPI_WANT_MIDIEditor_GetSetting_int
#define REAPERAPI_WANT_MIDIEditor_GetSetting_str
#define REAPERAPI_WANT_MIDIEditor_OnCommand
#define REAPERAPI_WANT_TakeFX_GetNumParams
#define REAPERAPI_WANT_TakeFX_GetParamName
#define REAPERAPI_WANT_TakeFX_GetCount
#define REAPERAPI_WANT_TakeFX_GetFXName
#define REAPERAPI_WANT_TakeFX_GetParam
#define REAPERAPI_WANT_TakeFX_SetParam
#define REAPERAPI_WANT_TakeFX_FormatParamValue
#define REAPERAPI_WANT_plugin_getapi
#define REAPERAPI_WANT_Envelope_FormatValue
#define REAPERAPI_WANT_CountTrackEnvelopes
#define REAPERAPI_WANT_GetTrackEnvelope
#define REAPERAPI_WANT_CountTakeEnvelopes
#define REAPERAPI_WANT_GetTakeEnvelope
#define REAPERAPI_WANT_GetEnvelopeStateChunk
#define REAPERAPI_WANT_GetSetTrackSendInfo
#define REAPERAPI_WANT_GetTrackNumSends
#define REAPERAPI_WANT_CountTakes
#define REAPERAPI_WANT_SetEnvelopePointEx
#define REAPERAPI_WANT_StuffMIDIMessage
#define REAPERAPI_WANT_PlayTrackPreview
#define REAPERAPI_WANT_StopTrackPreview
#define REAPERAPI_WANT_OnPauseButton
#define REAPERAPI_WANT_OnPlayButton
#define REAPERAPI_WANT_OnStopButton
#define REAPERAPI_WANT_TrackFX_GetRecCount
#define REAPERAPI_WANT_CountAutomationItems
#define REAPERAPI_WANT_GetSetAutomationItemInfo
#define REAPERAPI_WANT_Track_GetPeakHoldDB
#define REAPERAPI_WANT_Master_GetPlayRate
#define REAPERAPI_WANT_ShowPopupMenu
#define REAPERAPI_WANT_GetMediaItemTake_Track
#define REAPERAPI_WANT_GetMediaTrackInfo_Value
#define REAPERAPI_WANT_GetTrackMIDINoteName
#define REAPERAPI_WANT_get_config_var
#define REAPERAPI_WANT_projectconfig_var_addr
#define REAPERAPI_WANT_projectconfig_var_getoffs
#define REAPERAPI_WANT_EnumProjects
#define REAPERAPI_WANT_GetProjectName
#define REAPERAPI_WANT_GetProjectTimeOffset
#define REAPERAPI_WANT_plugin_register
#define REAPERAPI_WANT_GetTrackUIVolPan
#define REAPERAPI_WANT_GetGlobalAutomationOverride
#define REAPERAPI_WANT_SetGlobalAutomationOverride
#define REAPERAPI_WANT_GetTrackAutomationMode
#define REAPERAPI_WANT_CountSelectedTracks2
#define REAPERAPI_WANT_GetSelectedTrack2
#define REAPERAPI_WANT_SetTrackAutomationMode
#define REAPERAPI_WANT_SetMediaTrackInfo_Value
#define REAPERAPI_WANT_MIDI_EnumSelEvts
#define REAPERAPI_WANT_GetMediaItemTake_Item
#define REAPERAPI_WANT_GetMediaItem_Track
#define REAPERAPI_WANT_IsMediaItemSelected
#define REAPERAPI_WANT_GetMediaItemInfo_Value
#define REAPERAPI_WANT_GetMediaItem
#define REAPERAPI_WANT_KBD_OnMainActionEx
#define REAPERAPI_WANT_TrackFX_GetChainVisible
#define REAPERAPI_WANT_TakeFX_GetChainVisible
#define REAPERAPI_WANT_TrackFX_GetEnabled
#define REAPERAPI_WANT_TakeFX_GetEnabled
#define REAPERAPI_WANT_Master_GetTempo
#define REAPERAPI_WANT_CountTCPFXParms
#define REAPERAPI_WANT_GetTCPFXParm
#define REAPERAPI_WANT_GetMediaItemTakeInfo_Value
#define REAPERAPI_WANT_SetMixerScroll
#define REAPERAPI_WANT_GetSetAutomationItemInfo_String
#define REAPERAPI_WANT_TrackFX_FormatParamValueNormalized
#define REAPERAPI_WANT_GetNumTakeMarkers
#define REAPERAPI_WANT_GetTakeMarker
#define REAPERAPI_WANT_GetTrackStateChunk
#define REAPERAPI_WANT_GetToggleCommandState2
#define REAPERAPI_WANT_SectionFromUniqueID
#define REAPERAPI_WANT_GetFocusedFX
#define REAPERAPI_WANT_GetTake
#define REAPERAPI_WANT_GetTrackUIMute
#define REAPERAPI_WANT_GetResourcePath
#define REAPERAPI_WANT_get_ini_file
#define REAPERAPI_WANT_TrackFX_AddByName
#define REAPERAPI_WANT_TrackFX_Delete
#define REAPERAPI_WANT_GetSetTrackGroupMembership
#define REAPERAPI_WANT_GetSetTrackGroupMembershipHigh
#define REAPERAPI_WANT_GetSetProjectInfo_String
#define REAPERAPI_WANT_SetOnlyTrackSelected
#define REAPERAPI_WANT_MIDI_GetEvt
#define REAPERAPI_WANT_TrackFX_GetParamFromIdent
#define REAPERAPI_WANT_TrackFX_GetNamedConfigParm
#include <reaper/reaper_plugin.h>
#include <reaper/reaper_plugin_functions.h>

const char CONFIG_SECTION[] = "osara";

const int MAIN_SECTION = 0;
const int MIDI_EVENT_LIST_SECTION = 32061;
const int MEDIA_EXPLORER_SECTION = 32063;

// Needed for REAPER API functions which take a bool as an input pointer.
static bool bFalse = false;
static bool bTrue = true;

typedef struct Command {
	int section;
	gaccel_register_t gaccel;
	const char* id;
	void (*execute)(Command*);
} Command;
extern int lastCommandRepeatCount;
extern DWORD lastCommandTime;
extern bool isShortcutHelpEnabled;

extern HINSTANCE pluginHInstance;
extern HWND mainHwnd;

// We maintain our own idea of focus for context sensitivity.
enum FakeFocus {
	FOCUS_NONE = 0,
	FOCUS_TRACK,
	FOCUS_ITEM,
	FOCUS_RULER,
	FOCUS_MARKER,
	FOCUS_REGION,
	FOCUS_TIMESIG,
	FOCUS_STRETCH,
	FOCUS_ENVELOPE,
	FOCUS_AUTOMATIONITEM,
	FOCUS_NOTE,
	FOCUS_CC,
	FOCUS_TAKEMARKER
};
extern enum FakeFocus fakeFocus;

extern bool isSelectionContiguous;
extern bool shouldMoveToAutoItem;
extern int lastCommand;

bool shouldReportTimeMovement() ;
void outputMessage(const std::string& message, bool interrupt = true);
void outputMessage(std::ostringstream& message, bool interrupt = true);

typedef enum {
	TF_NONE,
	TF_MEASURE,
	TF_MINSEC,
	TF_SEC,
	TF_FRAME,
	TF_HMSF,
	TF_SAMPLE
} TimeFormat;
const TimeFormat TF_RULER = TF_NONE;
std::string formatTime(double time, TimeFormat format=TF_RULER, bool isLength=false, bool useCache=true, bool includeZeros=true, bool includeProjectStartOffset=true);
void resetTimeCache(TimeFormat excludeFormat=TF_NONE);
std::string formatNoteLength(double start, double end);
std::string formatCursorPosition(TimeFormat format=TF_RULER, bool useCache=true);
const char* getActionName(int command, KbdSectionInfo* section=nullptr, bool skipCategory=true);

bool isTrackSelected(MediaTrack* track);

#ifdef _WIN32
#include <string>
#include <oleacc.h>

std::wstring widen(const std::string& text);
std::string narrow(const std::wstring& text);

extern IAccPropServices* accPropServices;

// uia.cpp
bool initializeUia();
bool terminateUia();
bool shouldUseUiaNotifications();
bool sendUiaNotification(const std::string& message, bool interrupt = true);

#else
// These macros exist on Windows but aren't defined by Swell for Mac.
#define ComboBox_GetCurSel(hwnd) (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0)
#define ComboBox_SetCurSel(hwnd, index) (int)SendMessage(hwnd, CB_SETCURSEL, (WPARAM)index, 0)
#define ComboBox_AddString(hwnd, str) (int)SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)str)
#define ComboBox_ResetContent(hwnd) (int)SendMessage(hwnd, CB_RESETCONTENT, 0, 0)
#endif

bool isClassName(HWND hwnd, std::string className);

extern bool isHandlingCommand;
void reportTransportState(int state);
void reportRepeat(bool repeat);
void postGoToTrack(int command, MediaTrack* track);
void formatPan(double pan, std::ostringstream& output);
IReaperControlSurface* createSurface();
// envelopeCommands.cpp
extern bool selectedEnvelopeIsTake;
// exports.cpp
void registerExports(reaper_plugin_info_t* rec);
// translation.cpp
void initTranslation();
void translateDialog(HWND dialog);

#endif
