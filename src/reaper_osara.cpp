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
#else
#include "osxa11y_wrapper.h" // NSA11y wrapper for OS X accessibility API
#endif
#define REAPERAPI_IMPLEMENT
#include "osara.h"
#include <WDL/db2val.h>
#include "resource.h"
#include "paramsUi.h"

using namespace std;

HINSTANCE pluginHInstance;
HWND mainHwnd;
#ifdef _WIN32
DWORD guiThread;
IAccPropServices* accPropServices = NULL;
#endif

// We cache the last reported time so we can report just the components which have changed.
int oldMeasure;
int oldBeat;
int oldBeatPercent;
int oldMinute;
int oldFrame;
int oldSecond;
int oldHour;
FakeFocus fakeFocus = FOCUS_NONE;
bool isShortcutHelpEnabled = false;
bool isSelectionContiguous = true;

/*** Utilities */

#ifdef _WIN32

wstring_convert<codecvt_utf8_utf16<WCHAR>, WCHAR> utf8Utf16;
wstring widen(const string& text) {
	try {
		return utf8Utf16.from_bytes(text);
	} catch (range_error) {
		// #38: Invalid UTF-8. This really shouldn't happen,
		// but it seems REAPER/extensions sometimes use ANSI strings instead of UTF-8.
		// This hack just widens the string without any encoding conversion.
		// This may result in strange characters, but it's better than a crash.
		wostringstream s;
		s << text.c_str();
		return s.str();
	}
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

string formatTime(double time, TimeFormat format, bool isLength, bool useCache) {
	ostringstream s;
	if (format == TF_RULER) {
		if (GetToggleCommandState(40365))
			format = TF_MINSEC;
		else if (GetToggleCommandState(40368))
			format = TF_SEC;
		else if (GetToggleCommandState(41973))
			format = TF_FRAME;
		else if (GetToggleCommandState(40370))
			format = TF_HMSF;
		else if (GetToggleCommandState(40369))
			format = TF_SAMPLE;
		else
			format = TF_MEASURE;
	}
	switch (format) {
		case TF_MEASURE: {
			int measure;
			double beat = TimeMap2_timeToBeats(NULL, time, &measure, NULL, NULL, NULL);
			int wholeBeat = (int)beat;
			if (!isLength) {
				++measure;
				++wholeBeat;
			}
			int beatPercent = (int)(beat * 100) % 100;
			if (!useCache || measure != oldMeasure) {
				if (isLength)
					s << measure << (measure == 1 ? " bar " : " bars ");
				else
					s << "bar " << measure << " ";
				oldMeasure = measure;
			}
			if (!useCache || wholeBeat != oldBeat) {
				if (isLength)
					s << wholeBeat << (wholeBeat == 1 ? " beat " : " beats ");
				else
					s << "beat " << wholeBeat << " ";
				oldBeat = wholeBeat;
			}
			if (!useCache || beatPercent != oldBeatPercent) {
				s << beatPercent << "%";
				oldBeatPercent = beatPercent;
			}
			break;
		}
		case TF_MINSEC: {
			// Minutes:seconds
			int minute = time / 60;
			time = fmod(time, 60);
			if (!useCache || oldMinute != minute) {
				s << minute << " min ";
				oldMinute = minute;
			}
			s << fixed << setprecision(3);
			s << time << " sec";
			break;
		}
		case TF_SEC: {
			// Seconds
			s << fixed << setprecision(3);
			s << time << " sec";
			break;
		}
		case TF_FRAME: {
			// Frames
			int frame = time * TimeMap_curFrameRate(0, NULL);
			if (!useCache || oldFrame != frame) {
				s << frame << (frame == 1 ? " frame" : " frames");
				oldFrame = frame;
			}
			break;
		}
		case TF_HMSF: {
			// Hours:minutes:seconds:frames
			int hour = time / 3600;
			time = fmod(time, 3600);
			if (!useCache || oldHour != hour) {
				s << hour << (hour == 1 ? " hour " : " hours ");
				oldHour = hour;
			}
			int minute = time / 60;
			time = fmod(time, 60);
			if (!useCache || oldMinute != minute) {
				s << minute << " min ";
				oldMinute = minute;
			}
			int second = time;
			if (!useCache || oldSecond != second) {
				s << second << " sec ";
				oldSecond = second;
			}
			time = time - second;
			int frame = time * TimeMap_curFrameRate(0, NULL);
			if (!useCache || oldFrame != frame) {
				s << frame << (frame == 1 ? " frame" : " frames");
				oldFrame = frame;
			}
			break;
		}
		case TF_SAMPLE: {
			char buf[20];
			format_timestr_pos(time, buf, sizeof(buf), 4);
			s << buf << " samples";
			break;
		}
	}
	// #31: Clear cache for other units to avoid confusion if they are used later.
	resetTimeCache(format);
	return s.str();
}

void resetTimeCache(TimeFormat excludeFormat) {
	if (excludeFormat != TF_MEASURE) {
		oldMeasure = 0;
		oldBeat = 0;
		// Ensure percent gets reported even if it is 0.
		// Otherwise, we would initially report nothing for a length of 0.
		oldBeatPercent = -1;
	}
	if (excludeFormat != TF_MINSEC && excludeFormat != TF_HMSF)
		oldMinute = 0;
	if (excludeFormat != TF_FRAME && excludeFormat != TF_HMSF)
		oldFrame = 0;
	if (excludeFormat != TF_HMSF) {
		oldSecond = 0;
		oldHour = 0;
	}
}

string formatCursorPosition(TimeFormat format=TF_RULER, bool useCache=true) {
	return formatTime(GetCursorPosition(), format, false, useCache);
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

void reportActionName(int command, KbdSectionInfo* section=NULL, bool skipCategory=true) {
	const char* name = kbd_getTextFromCmd(command, section);
	const char* start;
	if (skipCategory) {
		// Skip the category before the colon (if any).
		for (start = name; *start; ++start) {
			if (*start == ':') {
				name = start + 2;
				break;
			}
		}
	}
	ostringstream s;
	s << name;
	outputMessage(s);
}

typedef bool(*TrackStateCheck)(MediaTrack* track);

bool isTrackMuted(MediaTrack* track) {
	if (track == GetMasterTrack(0)) {
		// Method for normal tracks doesn't seem to work for master.
		return GetMasterMuteSoloFlags() & 1;
	}
	return *(bool*)GetSetMediaTrackInfo(track, "B_MUTE", NULL);
}

bool isTrackSoloed(MediaTrack* track) {
	if (track == GetMasterTrack(0)) {
		// Method for normal tracks doesn't seem to work for master.
		return GetMasterMuteSoloFlags() & 2;
	}
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

bool isTrackFxBypassed(MediaTrack* track) {
	return *(int*)GetSetMediaTrackInfo(track, "I_FXEN", NULL) == 0;
}

bool isTrackSelected(MediaTrack* track) {
	return *(int*)GetSetMediaTrackInfo(track, "I_SELECTED", NULL);
}

bool isItemSelected(MediaItem* item) {
	return *(bool*)GetSetMediaItemInfo(item, "B_UISEL", NULL);
}

/*** Code to execute after existing actions.
 * This is used to report messages regarding the effect of the command, etc.
 */

bool shouldReportFx = false;
void postGoToTrack(int command) {
	fakeFocus = FOCUS_TRACK;
	SetCursorContext(0, NULL);
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	ostringstream s;
	// Need to cast to size_t first to avoid "loses information" error with clang.
	int trackNum = (int)(size_t)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", NULL);
	if (trackNum <= 0)
		s << "master";
	else {
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
	}
	if (isTrackSelected(track)) {
		// One selected track is the norm, so don't report selected in this case.
		if (CountSelectedTracks(0) > 1)
			s << " selected";
	} else
		s << " unselected";
	if (isTrackMuted(track))
		s << " muted";
	if (isTrackSoloed(track))
		s << " soloed";
	if (isTrackArmed(track))
		s << " armed";
	if (isTrackPhaseInverted(track))
		s << " phase inverted";
	if (isTrackFxBypassed(track))
		s << " FX bypassed";
	if (trackNum > 0) { // Not master
		int itemCount = CountTrackMediaItems(track);
		s << " " << itemCount << (itemCount == 1 ? " item" : " items");
	}
	int count;
	if (shouldReportFx && (count = TrackFX_GetCount(track)) > 0) {
		s << "; FX: ";
		char name[256];
		for (int f = 0; f < count; ++f) {
			if (f > 0)
				s << ", ";
			TrackFX_GetFXName(track, f, name, sizeof(name));
			s << name;
		}
	}
	outputMessage(s);
	if (command) {
		// This command replaces the selection , so revert to contiguous selection.
		isSelectionContiguous = true;
	}
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

void postToggleTrackFxBypass(MediaTrack* track) {
	outputMessage(isTrackFxBypassed(track) ? "FX bypassed" : "FX active");
}

void postToggleTrackFxBypass(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	if (track == GetMasterTrack(0)) {
		// Make this work for the master track. It doesn't out of the box.
		Main_OnCommand(16, 0); // Track: Toggle FX bypass for master track
	}
	postToggleTrackFxBypass(track);
}

bool shouldReportScrub = true;

void postCursorMovement(int command) {
	fakeFocus = FOCUS_RULER;
	outputMessage(formatCursorPosition().c_str());
}

void postCursorMovementScrub(int command) {
	if (shouldReportScrub)
		postCursorMovement(command);
	else
		fakeFocus = FOCUS_RULER; // Set this even if we aren't reporting.
}

void postCursorMovementMeasure(int command) {
	fakeFocus = FOCUS_RULER;
	outputMessage(formatCursorPosition(TF_MEASURE).c_str());
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
			fakeFocus = FOCUS_MARKER;
			if (name[0])
				s << name << " marker" << " ";
			else
				s << "marker " << number << " ";
		}
	}
	if (region >= 0) {
		EnumProjectMarkers(region, NULL, NULL, NULL, &name, &number);
		fakeFocus = FOCUS_REGION;
		if (name[0])
			s << name << " region ";
		else
			s << "region " << number << " ";
	}
	double start, end;
	GetSet_LoopTimeRange(false, false, &start, &end, false);
	if (start != end) {
		if (cursorPos == start)
			s << "selection start ";
		if (cursorPos == end)
			s << "selection end ";
	}
	GetSet_LoopTimeRange(false, true, &start, &end, false);
	if (start != end) {
		if (cursorPos == start)
			s << "loop start ";
		if (cursorPos == end)
			s << "loop end ";
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
	// Figure out what track this belongs to.
	if (GetTrackEnvelopeByName(GetMasterTrack(0), name) == envelope)
		s << "master: ";
	for (int t = 0; t < CountTracks(0); ++t) {
		MediaTrack* track = GetTrack(0, t);
		if (GetTrackEnvelopeByName(track, name) == envelope) {
			s << t + 1;
			char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);
			if (trackName && trackName[0])
				s << " " << trackName;
			s << ": ";
		}
	}
	s << name << " envelope selected";
	outputMessage(s);
}

void postChangeTrackVolume(MediaTrack* track) {
	ostringstream s;
	s << fixed << setprecision(2);
	s << VAL2DB(*(double*)GetSetMediaTrackInfo(track, "D_VOL", NULL));
	outputMessage(s);
}

void postChangeTrackVolume(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	postChangeTrackVolume(track);
}

void postChangeMasterTrackVolume(int command) {
	postChangeTrackVolume(GetMasterTrack(0));
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

void postCopy(int command) {
	ostringstream s;
	int count;
	switch (GetCursorContext2(true)) {
		case 0: // Track
			if ((count = CountSelectedTracks(0)) > 0)
				s << count << (count == 1 ? " track" : " tracks") << " copied";
			break;
		case 1: // Item
			if ((count = CountSelectedMediaItems(0)) > 0)
				s << count << (count == 1 ? " item" : " items") << " copied";
			break;
		default:
			return;
	}
	outputMessage(s);
}

void postMoveToEnvelopePoint(int command) {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	// GetEnvelopePointByTime often returns the point before instead of right at the position.
	// Increment the cursor position a bit to work around this.
	int point = GetEnvelopePointByTime(envelope, GetCursorPosition() + 0.0001);
	if (point < 0)
		return;
	double value;
	bool selected;
	GetEnvelopePoint(envelope, point, NULL, &value, NULL, NULL, &selected);
	char name[50];
	GetEnvelopeName(envelope, name, sizeof(name));
	ostringstream s;
	s << "point " << point + 1 << " value ";
	char out[64];
	if (strcmp(name, "Volume") == 0) {
		mkvolstr(out, value);
		s << out;
	} else if (strcmp(name, "Pan") == 0) {
		// The sign is wrong for some reason. Flip it.
		mkpanstr(out, -value);
		s << out;
	} else {
		s << fixed << setprecision(2);
		s << value;
	}
	if (selected)
		s << " " << "selected";
	s << " " << formatCursorPosition();
	int numSel = 0;
	for (point = 0; point < CountEnvelopePoints(envelope); ++point) {
		GetEnvelopePoint(envelope, point, NULL, NULL, NULL, NULL, &selected);
		if (selected)
			++numSel;
	}
	if (numSel > 0)
		s << ", " << numSel << " selected";
	outputMessage(s);
}

void postMoveToTimeSig(int command) {
	double cursor = GetCursorPosition();
	int marker = FindTempoTimeSigMarker(0, cursor);
	double pos, bpm;
	int sigNum, sigDenom;
	GetTempoTimeSigMarker(0, marker, &pos, NULL, NULL, &bpm, &sigNum, &sigDenom, NULL);
	if (pos != cursor)
		return;
	fakeFocus = FOCUS_TIMESIG;
	ostringstream s;
	s << "tempo " << bpm << " time sig " << sigNum << "/" << sigDenom;
	s << formatCursorPosition();
	outputMessage(s);
}

int getStretchAtPos(MediaItem_Take* take, double pos, double itemStart) {
	// Stretch marker positions are relative to the start of the item.
	double posRel = pos - itemStart;
	if (posRel < 0)
		return -1;
	int index = GetTakeStretchMarker(take, -1, &posRel, NULL);
	if (index < 0)
		return -1;
	double stretchRel;
	// Get the real position; pos wasn't written.
	GetTakeStretchMarker(take, index, &stretchRel, NULL);
	if (stretchRel != posRel)
		return -1; // Marker not right at pos.
	return index;
}

double lastStretchPos = -1;

void postGoToStretch(int command) {
	int itemCount = CountSelectedMediaItems(0);
	if (itemCount == 0)
		return;
	double cursor = GetCursorPosition();
	// Check whether there is actually a stretch marker at this position,
	// as these commands also move to the start/end of items regardless of markers.
	bool found = false;
	for (int i = 0; i < itemCount; ++i) {
		MediaItem* item = GetSelectedMediaItem(0, i);
		MediaItem_Take* take = GetActiveTake(item);
		if (!take)
			continue;
		if (getStretchAtPos(take, cursor, *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL)) != -1) {
			found = true;
			break;
		}
	}
	ostringstream s;
	if (found) {
		fakeFocus = FOCUS_STRETCH;
		lastStretchPos = cursor;
		s << "stretch marker ";
	} else
		lastStretchPos = -1;
	s << formatCursorPosition();
	outputMessage(s);
	if (GetPlayPosition() != cursor)
		SetEditCurPos(cursor, true, true); // Seek playback.
}

void postTrackFxChain(int command) {
	if (GetLastTouchedTrack() == GetMasterTrack(0)) {
		// Make this work for the master track. It doesn't out of the box.
		Main_OnCommand(40846, 0); // Track: View FX chain for master track
	}
}

#ifdef _WIN32
void cmdIoMaster(Command* command);
void postTrackIo(int command) {
	if (GetLastTouchedTrack() == GetMasterTrack(0)) {
		// Make this work for the master track. It doesn't out of the box.
		cmdIoMaster(NULL);
	}
}
#endif // _WIN32

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
	{40001, postGoToTrack}, // Track: Insert new track
	{40280, postToggleTrackMute}, // Track: Mute/unmute tracks
	{40281, postToggleTrackSolo}, // Track: Solo/unsolo tracks
	{40294, postToggleTrackArm}, // Toggle record arming for current (last touched) track
	{40495, postCycleTrackMonitor}, // Track: Cycle track record monitor
	{40282, postInvertTrackPhase}, // Track: Invert track phase
	{40298, postToggleTrackFxBypass}, // Track: Toggle FX bypass for current track
	{40344, postToggleTrackFxBypass}, // Track: toggle FX bypass on all tracks
	{40104, postCursorMovementScrub}, // View: Move cursor left one pixel
	{40105, postCursorMovementScrub}, // View: Move cursor right one pixel
	{40042, postCursorMovement}, // Transport: Go to start of project
	{40043, postCursorMovement}, // Transport: Go to end of project
	{41042, postCursorMovementMeasure}, // Go forward one measure
	{41043, postCursorMovementMeasure}, // Go back one measure
	{41044, postCursorMovementMeasure}, // Go forward one beat
	{41045, postCursorMovementMeasure}, // Go back one beat
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
	{40743, postChangeMasterTrackVolume}, // Track: Nudge master track volume up
	{40744, postChangeMasterTrackVolume}, // Track: Nudge master track volume down
	{1011, postChangeHorizontalZoom}, // Zoom out horizontal
	{1012, postChangeHorizontalZoom}, // Zoom in horizontal
	{40283, postChangeTrackPan}, // Track: Nudge track pan left
	{40284, postChangeTrackPan}, // Track: Nudge track pan right
	{1155, postCycleRippleMode}, // Options: Cycle ripple editing mode
	{1068, postToggleRepeat}, // Transport: Toggle repeat
	{40125, postSwitchToTake}, // Take: Switch items to next take
	{40126, postSwitchToTake}, // Take: Switch items to previous take
	{40057, postCopy}, // Edit: Copy items/tracks/envelope points (depending on focus) ignoring time selection
	{41383, postCopy}, // Edit: Copy items/tracks/envelope points (depending on focus) within time selection, if any (smart copy)
	{41820, postMoveToTimeSig}, // Move edit cursor to previous tempo or time signature change
	{41821, postMoveToTimeSig}, // Move edit cursor to next tempo or time signature change
	{41860, postGoToStretch}, // Item: go to next stretch marker
	{41861, postGoToStretch}, // Item: go to previous stretch marker
	{40291, postTrackFxChain}, // Track: View FX chain for current track
#ifdef _WIN32
	{40293, postTrackIo}, // Track: View I/O for current track
#endif
	{0},
};
PostCustomCommand POST_CUSTOM_COMMANDS[] = {
	{"_XENAKIOS_NUDGSELTKVOLUP", postChangeTrackVolume}, // Xenakios/SWS: Nudge volume of selected tracks up
	{"_XENAKIOS_NUDGSELTKVOLDOWN", postChangeTrackVolume}, // Xenakios/SWS: Nudge volume of selected tracks down
	{"_SWS_BRMOVEEDITTOPREVENV", postMoveToEnvelopePoint}, // SWS/BR: Move edit cursor to previous envelope point
	{"_SWS_BRMOVEEDITTONEXTENV", postMoveToEnvelopePoint}, // SWS/BR: Move edit cursor to next envelope point
	{"_SWS_BRMOVEEDITSELPREVENV", postMoveToEnvelopePoint}, // SWS/BR: Move edit cursor to previous envelope point and select it
	{"_SWS_BRMOVEEDITSELNEXTENV", postMoveToEnvelopePoint}, // SWS/BR: Move edit cursor to next envelope point and select it
	{"_SWS_BRMOVEEDITTOPREVENVADDSELL", postMoveToEnvelopePoint}, // SWS/BR: Move edit cursor to previous envelope point and add to selection
	{"_SWS_BRMOVEEDITTONEXTENVADDSELL", postMoveToEnvelopePoint}, // SWS/BR: Move edit cursor to next envelope point and add to selection
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

HWND getSendContainer(HWND hwnd) {
	WCHAR className[21] = L"\0";
	GetClassName(hwnd, className, ARRAYSIZE(className));
	if (wcscmp(className, L"Button") != 0)
		return NULL;
	hwnd = GetWindow(hwnd, GW_HWNDPREV);
	if (!hwnd)
		return NULL;
	GetClassName(hwnd, className, ARRAYSIZE(className));
	if (wcscmp(className, L"Static") != 0)
		return NULL;
	hwnd = GetAncestor(hwnd, GA_PARENT);
	if (!hwnd)
		return NULL;
	GetClassName(hwnd, className, ARRAYSIZE(className));
	if (wcscmp(className, L"REAPERVirtWndDlgHost") != 0)
		return NULL;
	return hwnd;
}

void sendMenu(HWND sendWindow) {
	// #24: The controls are exposed via MSAA,
	// but this is difficult for most users to use, especially with the broken MSAA implementation.
	// Present them in a menu.
	IAccessible* acc = NULL;
	VARIANT child;
	if (AccessibleObjectFromEvent(sendWindow, OBJID_CLIENT, CHILDID_SELF, &acc, &child) != S_OK)
		return;
	long count = 0;
	if (acc->get_accChildCount(&count) != S_OK || count == 0) {
		acc->Release();
		return;
	}
	HMENU menu = CreatePopupMenu();
	MENUITEMINFO itemInfo;
	itemInfo.cbSize = sizeof(MENUITEMINFO);
	itemInfo.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
	itemInfo.fType = MFT_STRING;
	child.vt = VT_I4;
	int item = 0;
	for (long c = 1; c <= count; ++c, ++item) {
		child.lVal = c;
		VARIANT role;
		if (acc->get_accRole(child, &role) != S_OK || role.vt != VT_I4)
			continue;
		if (role.lVal != ROLE_SYSTEM_PUSHBUTTON && role.lVal != ROLE_SYSTEM_COMBOBOX)
			continue;
		BSTR name = NULL;
		if (acc->get_accName(child, &name) != S_OK || !name)
			continue;
		itemInfo.wID = c;
		itemInfo.dwTypeData = name;
		itemInfo.cch = SysStringLen(name);
		InsertMenuItem(menu, item, true, &itemInfo);
		SysFreeString(name);
	}
	child.lVal = TrackPopupMenu(menu, TPM_NONOTIFY | TPM_RETURNCMD, 0, 0, 0, mainHwnd, NULL);
	DestroyMenu(menu);
	if (child.lVal == -1) {
		acc->Release();
		return;
	}
	// Click the selected control.
	long l, t, w, h;
	HRESULT res = acc->accLocation(&l, &t, &w, &h, child);
	acc->Release();
	if (res != S_OK)
		return;
	POINT point = {l, t};
	ScreenToClient(sendWindow, &point);
	SendMessage(sendWindow, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(point.x, point.y));
	SendMessage(sendWindow, WM_LBUTTONUP, 0, MAKELPARAM(point.x, point.y));
}

// Handle keyboard keys which can't be bound to actions.
// REAPER's "accelerator" hook isn't enough because it doesn't get called in some windows.
LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
	if (code != HC_ACTION && wParam != VK_APPS && wParam != VK_RETURN) {
		// Return early if we're not interested in the key.
		return CallNextHookEx(NULL, code, wParam, lParam);
	}
	HWND focus = GetFocus();
	if (!focus)
		return CallNextHookEx(NULL, code, wParam, lParam);
	WCHAR className[22] = L"\0";
	GetClassName(focus, className, ARRAYSIZE(className));
	HWND window;
	if (wParam == VK_APPS && lParam & 0x80000000) {
		if (wcscmp(className, L"REAPERTrackListWindow") == 0
			|| wcscmp(className, L"REAPERtrackvu") == 0
		) {
			// Reaper doesn't handle the applications key for these windows.
			// Display the appropriate context menu depending on fakeFocus.
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
		} else if (window = getSendContainer(focus)) {
			sendMenu(window);
			return 1;
		}
	} else if ((wParam == VK_APPS || (wParam == VK_RETURN && GetKeyState(VK_CONTROL) & 0x8000))
		&& !(lParam & 0x80000000) // Key down
		&& wcscmp(className, L"SysListView32") == 0
	) {
		// REAPER doesn't allow you to do the equivalent of double click or right click in several ListViews.
		int item = ListView_GetNextItem(focus, -1, LVNI_FOCUSED);
		if (item != -1) {
			RECT rect;
			ListView_GetItemRect(focus, item, &rect, LVIR_BOUNDS);
			POINT point = {rect.left + 10, rect.top + 10};
			ClientToScreen(focus, &point);
			SetCursorPos(point.x, point.y);
			if (wParam == VK_APPS) {
				// Applications key right clicks.
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
			} else {
				// Control+enter double clicks.
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			}
			return 1;
		}
	}
	return CallNextHookEx(NULL, code, wParam, lParam);
}
HHOOK keyboardHook = NULL;

#endif // _WIN32

/*** Our commands/commands we want to intercept.
 * Each command should have a function and should be added to the COMMANDS array below.
 */

int int0 = 0;
int int1 = 1;

void moveToTrack(int direction, bool clearSelection=true, bool select=true) {
	int count = CountTracks(0);
	if (count == 0)
		return;
	int num;
	MediaTrack* origTrack = GetLastTouchedTrack();
	if (origTrack) {
		num = (int)(size_t)GetSetMediaTrackInfo(origTrack, "IP_TRACKNUMBER", NULL);
		if (num >= 0) // Not master
			--num; // We need 0-based.
		num += direction;
	} else {
		// #47: Deleting the last track results in no last touched track.
		// Therefore, navigate to the last track.
		num = count - 1;
	}
	MediaTrack* track = origTrack;
	// We use -1 for the master track.
	for (; -1 <= num && num < count; num += direction) {
		if (num == -1) {
			if (!(GetMasterTrackVisibility() & 1))
				continue; // Invisible.
			track = GetMasterTrack(0);
		} else {
			track = GetTrack(0, num);
			MediaTrack* parent = (MediaTrack*)GetSetMediaTrackInfo(track, "P_PARTRACK", NULL);
			if (parent
				&& *(int*)GetSetMediaTrackInfo(parent, "I_FOLDERDEPTH", NULL) == 1
				&& *(int*)GetSetMediaTrackInfo(parent, "I_FOLDERCOMPACT", NULL) == 2
			) {
				// This track is inside a closed folder, so skip it.
				if (direction == 1 && num == count - 1) {
					// We're moving forward and we're on the last track.
					// Therefore, go backward.
					// Note that this can't happen when the user moves backward
					// because the first track can never be inside a folder.
					direction = -1;
				}
				continue;
			}
			if (!IsTrackVisible(track, false))
				continue;
		}
		break;
	}
	bool wasSelected = isTrackSelected(track);
	if (!select || track != origTrack || !wasSelected) {
		// We're moving to a different track
		// or we're on the same track but it's unselected.
		if (clearSelection || select)
			Undo_BeginBlock();
		if (clearSelection) {
			Main_OnCommand(40297, 0); // Track: Unselect all tracks
			// The master track has to be unselected separately.
			GetSetMediaTrackInfo(GetMasterTrack(0), "I_SELECTED", &int0);
			isSelectionContiguous = true;
		}
		// Always select so this will become the last touched track.
		// The track might already be selected,
		// so we must first unselected.
		GetSetMediaTrackInfo(track, "I_SELECTED", &int0);
		GetSetMediaTrackInfo(track, "I_SELECTED", &int1);
		if (!wasSelected && !select)
			GetSetMediaTrackInfo(track, "I_SELECTED", &int0);
		if (clearSelection || select)
			Undo_EndBlock("Change Track Selection", 0);
	}
	postGoToTrack(0);
}

void cmdGoToNextTrack(Command* command) {
	moveToTrack(1);
}

void cmdGoToPrevTrack(Command* command) {
	moveToTrack(-1);
}

void cmdGoToNextTrackKeepSel(Command* command) {
	moveToTrack(1, false, isSelectionContiguous);
}

void cmdGoToPrevTrackKeepSel(Command* command) {
	moveToTrack(-1, false, isSelectionContiguous);
}

bool bFalse = false;
bool bTrue = true;

MediaItem* currentItem = NULL;
void moveToItem(int direction, bool clearSelection=true, bool select=true) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	double cursor = GetCursorPosition();
	int count = CountTrackMediaItems(track);
	double pos;
	int start = direction == 1 ? 0 : count - 1;
	if (currentItem && ValidatePtr((void*)currentItem, "MediaItem*")
		&& (MediaTrack*)GetSetMediaItemInfo(currentItem, "P_TRACK", NULL) == track
	) {
		pos = *(double*)GetSetMediaItemInfo(currentItem, "D_POSITION", NULL);
		if (direction == 1 ? pos <= cursor : pos >= cursor) {
			// The cursor is right at or has moved past the item to which the user last moved.
			// Therefore, start at the adjacent item.
			// This is faster and also allows the user to move to items which start at the same position.
			// Need to cast to size_t first to avoid "loses information" error with clang.
			start = (int)(size_t)GetSetMediaItemInfo(currentItem, "IP_ITEMNUMBER", NULL) + direction;
			if (start < 0 || start >= count) {
				// There's no adjacent item in this direction,
				// so move to the current one again.
				start -= direction;
			}
		}
	} else
		currentItem = NULL; // Invalid.

	for (int i = start; 0 <= i && i < count; i += direction) {
		MediaItem* item = GetTrackMediaItem(track, i);
		pos = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
		if (direction == 1 ? pos < cursor : pos > cursor)
			continue; // Not the right direction.
		currentItem = item;
		if (clearSelection || select)
			Undo_BeginBlock();
		if (clearSelection) {
			Main_OnCommand(40289, 0); // Item: Unselect all items
			isSelectionContiguous = true;
		}
		if (select)
			GetSetMediaItemInfo(item, "B_UISEL", &bTrue);
		if (clearSelection || select)
			Undo_EndBlock("Change Item Selection", 0);
		SetEditCurPos(pos, true, true); // Seek playback.

		// Report the item.
		fakeFocus = FOCUS_ITEM;
		SetCursorContext(1, NULL);
		ostringstream s;
		s << i + 1;
		MediaItem_Take* take = GetActiveTake(item);
		if (take)
			s << " " << GetTakeName(take);
		if (isItemSelected(item)) {
			// One selected item is the norm, so don't report selected in this case.
			if (CountSelectedMediaItems(0) > 1)
				s << " selected";
		} else
			s << " unselected";
		s << " " << formatCursorPosition();
		outputMessage(s);
		return;
	}
}

void cmdMoveToNextItem(Command* command) {
	moveToItem(1);
}

void cmdMoveToPrevItem(Command* command) {
	moveToItem(-1);
}

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

void cmdPaste(Command* command) {
	int oldItems = CountMediaItems(0);
	int oldTracks = CountTracks(0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	ostringstream s;
	int added;
	if ((added = CountTracks(0) - oldTracks) > 0)
		s << added << (added == 1 ? " track" : " tracks") << " added";
	else if ((added = CountMediaItems(0) - oldItems) > 0)
		s << added << (added == 1 ? " item" : " items") << " added";
	else
		s << "nothing pasted";
	outputMessage(s);
}

void cmdhRemoveTracks(int command) {
	int oldCount = CountTracks(0);
	Main_OnCommand(command, 0);
	int removed = oldCount - CountTracks(0);
	ostringstream s;
	s << removed << (removed == 1 ? " track" : " tracks") << " removed";
	outputMessage(s);
}

void cmdRemoveTracks(Command* command) {
	cmdhRemoveTracks(command->gaccel.accel.cmd);
}

void cmdhRemoveItems(int command) {
	int oldCount = CountMediaItems(0);
	Main_OnCommand(command, 0);
	int removed = oldCount - CountMediaItems(0);
	ostringstream s;
	s << removed << (removed == 1 ? " item" : " items") << " removed";
	outputMessage(s);
}

void cmdRemoveItems(Command* command) {
	cmdhRemoveItems(command->gaccel.accel.cmd);
}

void cmdCut(Command* command) {
	switch (GetCursorContext2(true)) {
		case 0: // Track
			cmdhRemoveTracks(command->gaccel.accel.cmd);
			return;
		case 1: // Item
			cmdhRemoveItems(command->gaccel.accel.cmd);
			return;
	}
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

void cmdToggleMasterTrackFxBypass(Command* command) {
	// #42: This really should be a post command, but hookpostcommand doesn't fire.
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	postToggleTrackFxBypass(GetMasterTrack(0));
}

void cmdDeleteMarker(Command* command) {
	int count = CountProjectMarkers(0, NULL, NULL);
	Main_OnCommand(40613, 0); // Markers: Delete marker near cursor
	if (CountProjectMarkers(0, NULL, NULL) != count)
		outputMessage("marker deleted");
}

void cmdDeleteRegion(Command* command) {
	int count = CountProjectMarkers(0, NULL, NULL);
	Main_OnCommand(40615, 0); // Markers: Delete region near cursor
	if (CountProjectMarkers(0, NULL, NULL) != count)
		outputMessage("region deleted");
}

void cmdDeleteTimeSig(Command* command) {
	int count = CountTempoTimeSigMarkers(0);
	Main_OnCommand(40617, 0); // Markers: Delete time signature marker near cursor
	if (CountTempoTimeSigMarkers(0) != count)
		outputMessage("time signature deleted");
}

void cmdRemoveStretch(Command* command) {
	MediaItem* item = GetSelectedMediaItem(0, 0);
	if (!item)
		return;
	MediaItem_Take* take = GetActiveTake(item);
	if (!take)
		return;
	int count = GetTakeNumStretchMarkers(take);
	Main_OnCommand(41859, 0); // Item: remove stretch marker at current position
	if (GetTakeNumStretchMarkers(take) != count)
		outputMessage("stretch marker deleted");
}

void cmdMoveToNextItemKeepSel(Command* command) {
	moveToItem(1, false, isSelectionContiguous);
}

void cmdMoveToPrevItemKeepSel(Command* command) {
	moveToItem(-1, false, isSelectionContiguous);
}

#ifdef _WIN32
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

void cmdReportSelection(Command* command) {
	ostringstream s;
	int count = 0;
	int t;
	MediaTrack* track;
	switch (fakeFocus) {
		case FOCUS_TRACK: {
			if (isTrackSelected(GetMasterTrack(0))) {
				s << "master";
				count = 1;
			}
			for (t = 0; t < CountTracks(0); ++t) {
				track = GetTrack(0, t);
				if (isTrackSelected(track)) {
					++count;
					if (count > 1)
						s << ", ";
					s << t + 1;
					char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);
					if (name && name[0])
						s << " " << name;
				}
			}
			break;
		}
		case FOCUS_ITEM: {
			for (t = 0; t < CountTracks(0); ++t) {
				track = GetTrack(0, t);
				for (int i = 0; i < CountTrackMediaItems(track); ++i) {
					MediaItem* item = GetTrackMediaItem(track, i);
					if (isItemSelected(item)) {
						++count;
						if (count > 1)
							s << ", ";
						s << t + 1 << "." << i + 1;
						MediaItem_Take* take = GetActiveTake(item);
						if (take)
							s << " " << GetTakeName(take);
					}
				}
			}
			break;
		}
		case FOCUS_RULER: {
			double start, end;
			GetSet_LoopTimeRange(false, false, &start, &end, false);
			if (start != end) {
				s << "start " << formatTime(start, TF_RULER, false, false) << ", "
					<< "end " << formatTime(end, TF_RULER, false, false) << ", "
					<< "length " << formatTime(end - start, TF_RULER, true, false);
				count = 1;
				resetTimeCache();
			}
			break;
		}
	}
	if (count == 0)
		s << "No selection";
	outputMessage(s);
}

void cmdRemoveFocus(Command* command) {
	switch (fakeFocus) {
		case FOCUS_TRACK:
			cmdhRemoveTracks(40005); // Track: Remove tracks
			break;
		case FOCUS_ITEM:
			cmdhRemoveItems(40006); // Item: Remove items
			break;
		case FOCUS_MARKER:
			cmdDeleteMarker(NULL);
			break;
		case FOCUS_REGION:
			cmdDeleteRegion(NULL);
			break;
		case FOCUS_TIMESIG:
			cmdDeleteTimeSig(NULL);
			break;
		case FOCUS_STRETCH:
			cmdRemoveStretch(NULL);
			break;
		default:
			cmdRemoveTimeSelection(NULL);
	}
}

void cmdShortcutHelp(Command* command) {
	isShortcutHelpEnabled = !isShortcutHelpEnabled;
	outputMessage(isShortcutHelpEnabled ? "shortcut help on" : "shortcut help off");
}

void cmdReportCursorPosition(Command* command) {
	TimeFormat tf;
	if (lastCommandRepeatCount == 0)
		tf = TF_RULER;
	else if (GetToggleCommandState(40366) || GetToggleCommandState(41918)) // Rule unit is measures/min:sec
		tf = TF_MINSEC;
	double pos = GetPlayState() & 1 ? GetPlayPosition() : GetCursorPosition();
	outputMessage(formatTime(pos, tf, false, false));
}

void cmdToggleSelection(Command* command) {
	if (isSelectionContiguous) {
		isSelectionContiguous = false;
		outputMessage("noncontiguous selection");
		return;
	}
	bool select;
	switch (fakeFocus) {
		case FOCUS_TRACK: {
			MediaTrack* track = GetLastTouchedTrack();
			if (!track)
				return;
			select = !isTrackSelected(track);
			GetSetMediaTrackInfo(track, "I_SELECTED", (select ? &int1 : &int0));
			break;
		}
		case FOCUS_ITEM:
			if (!currentItem)
				return;
			select = !isItemSelected(currentItem);
			GetSetMediaItemInfo(currentItem, "B_UISEL", (select ? &bTrue : &bFalse));
			break;
		default:
			return;
	}
	outputMessage(select ? "selected" : "unselected");
}

void cmdMoveStretch(Command* command) {
	if (lastStretchPos == -1)
		return;
	int itemCount = CountSelectedMediaItems(0);
	if (itemCount == 0)
		return;
	double cursor = GetCursorPosition();
	bool done = false;
	for (int i = 0; i < itemCount; ++i) {
		MediaItem* item = GetSelectedMediaItem(0, i);
		MediaItem_Take* take = GetActiveTake(item);
		if (!take)
			continue;
		double itemStart = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
		int index = getStretchAtPos(take, lastStretchPos, itemStart);
		if (index == -1)
			continue;
		// Stretch marker positions are relative to the start of the item.
		double destPos = cursor - itemStart;
		SetTakeStretchMarker(take, index, destPos, NULL);
		done = true;
	}
	if (done)
		outputMessage("stretch marker moved");
}

#ifdef _WIN32
// See the Configuration section of the code below.
void cmdConfig(Command* command);

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
	{MAIN_SECTION, {{0, 0, 40285}, NULL}, NULL, cmdGoToNextTrack}, // Track: Go to next track
	{MAIN_SECTION, {{0, 0, 40286}, NULL}, NULL, cmdGoToPrevTrack}, // Track: Go to previous track
	{MAIN_SECTION, {{0, 0, 40287}, NULL}, NULL, cmdGoToNextTrackKeepSel}, // Track: Go to next track (leaving other tracks selected)
	{MAIN_SECTION, {{0, 0, 40288}, NULL}, NULL, cmdGoToPrevTrackKeepSel}, // Track: Go to previous track (leaving other tracks selected)
	{MAIN_SECTION, {{0, 0, 40417}, NULL}, NULL, cmdMoveToNextItem}, // Item navigation: Select and move to next item
	{MAIN_SECTION, {{0, 0, 40416}, NULL}, NULL, cmdMoveToPrevItem}, // Item navigation: Select and move to previous item
	{MAIN_SECTION, {{0, 0, 40029}, NULL}, NULL, cmdUndo}, // Edit: Undo
	{MAIN_SECTION, {{0, 0, 40030}, NULL}, NULL, cmdRedo}, // Edit: Redo
	{MAIN_SECTION, {{0, 0, 40012}, NULL}, NULL, cmdSplitItems}, // Item: Split items at edit or play cursor
	{MAIN_SECTION, {{0, 0, 40061}, NULL}, NULL, cmdSplitItems}, // Item: Split items at time selection
	{MAIN_SECTION, {{0, 0, 40058}, NULL}, NULL, cmdPaste}, // Item: Paste items/tracks
	{MAIN_SECTION, {{0, 0, 40005}, NULL}, NULL, cmdRemoveTracks}, // Track: Remove tracks
	{MAIN_SECTION, {{0, 0, 40006}, NULL}, NULL, cmdRemoveItems}, // Item: Remove items
	{MAIN_SECTION, {{0, 0, 40059}, NULL}, NULL, cmdCut}, // Edit: Cut items/tracks/envelope points (depending on focus) ignoring time selection
	{MAIN_SECTION, {{0, 0, 41384}, NULL}, NULL, cmdCut}, // Edit: Cut items/tracks/envelope points (depending on focus) within time selection, if any (smart cut)
	{MAIN_SECTION, {{0, 0, 40201}, NULL}, NULL, cmdRemoveTimeSelection}, // Time selection: Remove contents of time selection (moving later items)
	{MAIN_SECTION, {{0, 0, 40119}, NULL}, NULL, cmdMoveItems}, // Item edit: Move items/envelope points right
	{MAIN_SECTION, {{0, 0, 40120}, NULL}, NULL, cmdMoveItems}, // Item edit: Move items/envelope points left
	{MAIN_SECTION, {{0, 0, 40225}, NULL}, NULL, cmdMoveItems}, // Item edit: Grow left edge of items
	{MAIN_SECTION, {{0, 0, 40226}, NULL}, NULL, cmdMoveItems}, // Item edit: Shrink left edge of items
	{MAIN_SECTION, {{0, 0, 40227}, NULL}, NULL, cmdMoveItems}, // Item edit: Shrink right edge of items
	{MAIN_SECTION, {{0, 0, 40228}, NULL}, NULL, cmdMoveItems}, // Item edit: Grow right edge of items
	{MAIN_SECTION, {{0, 0, 16}, NULL}, NULL, cmdToggleMasterTrackFxBypass}, // Track: Toggle FX bypass for master track
	{MAIN_SECTION, {{0, 0, 40613}, NULL}, NULL, cmdDeleteMarker}, // Markers: Delete marker near cursor
	{MAIN_SECTION, {{0, 0, 40615}, NULL}, NULL, cmdDeleteRegion}, // Markers: Delete region near cursor
	{MAIN_SECTION, {{0, 0, 40617}, NULL}, NULL, cmdDeleteTimeSig}, // Markers: Delete time signature marker near cursor
	{MAIN_SECTION, {{0, 0, 41859}, NULL}, NULL, cmdRemoveStretch}, // Item: remove stretch marker at current position
	// Our own commands.
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move to next item (leaving other items selected)"}, "OSARA_NEXTITEMKEEPSEL", cmdMoveToNextItemKeepSel},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move to previous item (leaving other items selected)"}, "OSARA_PREVITEMKEEPSEL", cmdMoveToPrevItemKeepSel},
#ifdef _WIN32
	{MAIN_SECTION, {DEFACCEL, "OSARA: View parameters for current track/item (depending on focus)"}, "OSARA_PARAMS", cmdParamsFocus},
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
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report track/item/time selection (depending on focus)"}, "OSARA_REPORTSEL", cmdReportSelection},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Remove items/tracks/contents of time selection/markers (depending on focus)"}, "OSARA_REMOVE", cmdRemoveFocus},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Toggle shortcut help"}, "OSARA_SHORTCUTHELP", cmdShortcutHelp},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report edit/play cursor position"}, "OSARA_CURSORPOS", cmdReportCursorPosition},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Enable noncontiguous selection/toggle selection of current track/item (depending on focus)"}, "OSARA_TOGGLESEL", cmdToggleSelection},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move last focused stretch marker to current edit cursor position"}, "OSARA_MOVESTRETCH", cmdMoveStretch},
#ifdef _WIN32
	{MAIN_SECTION, {DEFACCEL, "OSARA: Configuration"}, "OSARA_CONFIG", cmdConfig},
	{MIDI_EVENT_LIST_SECTION, {DEFACCEL, "OSARA: Focus event nearest edit cursor"}, "OSARA_FOCUSMIDIEVENT", cmdFocusNearestMidiEvent},
#endif
	{0, {}, NULL, NULL},
};
map<pair<int, int>, Command*> commandsMap;

/*** Configuration
 * For new settings, appropriate code needs to be added to loadConfig, config_onOk and cmdConfig.
 ***/

const char CONFIG_SECTION[] = "osara";

void loadConfig() {
	// GetExtState returns an empty string (not NULL) if the key doesn't exist.
	shouldReportScrub = GetExtState(CONFIG_SECTION, "reportScrub")[0] != '0';
	shouldReportFx = GetExtState(CONFIG_SECTION, "reportFx")[0] == '1';
}

#ifdef _WIN32

void config_onOk(HWND dialog) {
	HWND control = GetDlgItem(dialog, ID_CONFIG_REPORT_SCRUB);
	shouldReportScrub = Button_GetCheck(control) == BST_CHECKED;
	SetExtState(CONFIG_SECTION, "reportScrub", shouldReportScrub ? "1" : "0", true);
	control = GetDlgItem(dialog, ID_CONFIG_REPORT_FX);
	shouldReportFx = Button_GetCheck(control) == BST_CHECKED;
	SetExtState(CONFIG_SECTION, "reportFx", shouldReportFx ? "1" : "0", true);
}

INT_PTR CALLBACK config_dialogProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK) {
				config_onOk(dialog);
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

void cmdConfig(Command* command) {
	HWND dialog = CreateDialog(pluginHInstance, MAKEINTRESOURCE(ID_CONFIG_DLG), mainHwnd, config_dialogProc);

	HWND control = GetDlgItem(dialog, ID_CONFIG_REPORT_SCRUB);
	Button_SetCheck(control, shouldReportScrub ? BST_CHECKED : BST_UNCHECKED);
	control = GetDlgItem(dialog, ID_CONFIG_REPORT_FX);
	Button_SetCheck(control, shouldReportFx ? BST_CHECKED : BST_UNCHECKED);

	ShowWindow(dialog, SW_SHOWNORMAL);
}

#endif // _WIN32

/*** Initialisation, termination and inner workings. */

void postCommand(int command, int flag) {
	const auto it = postCommandsMap.find(command);
	if (it != postCommandsMap.end())
		it->second(command);
}

bool isHandlingCommand = false;
Command* lastCommand = NULL;
DWORD lastCommandTime = 0;
int lastCommandRepeatCount;

bool handleCommand(KbdSectionInfo* section, int command, int val, int valHw, int relMode, HWND hwnd) {
	if (isHandlingCommand)
		return false; // Prevent re-entrance.
	const auto it = commandsMap.find(make_pair(section->uniqueID, command));
	if (it != commandsMap.end()
		// Allow shortcut help to be disabled.
		&& (!isShortcutHelpEnabled || it->second->execute == cmdShortcutHelp)
	) {
		isHandlingCommand = true;
		DWORD now = GetTickCount();
		if (it->second == lastCommand && now - lastCommandTime < 500)
			++lastCommandRepeatCount;
		else
			lastCommandRepeatCount = 0;
		lastCommandTime = now;
		it->second->execute(it->second);
		lastCommand = it->second;
		isHandlingCommand = false;
		return true;
	} else if (isShortcutHelpEnabled) {
		reportActionName(command, section, false);
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

#ifdef _WIN32

// Initialisation that must be done after REAPER_PLUGIN_ENTRYPOINT;
// e.g. because it depends on stuff registered by other plug-ins.
VOID CALLBACK delayedInit(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	for (int i = 0; POST_CUSTOM_COMMANDS[i].id; ++i) {
		int cmd = NamedCommandLookup(POST_CUSTOM_COMMANDS[i].id);
		if (cmd)
			postCommandsMap.insert(make_pair(cmd, POST_CUSTOM_COMMANDS[i].execute));
	}
	KillTimer(NULL, event);
}

void CALLBACK handleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG objId, long childId, DWORD thread, DWORD time) {
	if (event == EVENT_OBJECT_FOCUS) {
		if (lastMessageHwnd && hwnd != lastMessageHwnd) {
			// Focus is moving. Clear our tweak to accName for the previous focus.
			// This avoids problems such as the last message repeating when a new project is opened (#17).
			accPropServices->ClearHwndProps(lastMessageHwnd, OBJID_CLIENT, CHILDID_SELF, &PROPID_ACC_NAME, 1);
			lastMessageHwnd = NULL;
		}
		HWND tempWindow;
		if (tempWindow = getSendContainer(hwnd)) {
			// #24: This is a button for a send in the Track I/O window.
			// Tweak the name so the user knows what send it's for.
			// Unfortunately, we can't annotate the accessible for the container.
			// First, get the name of the send.
			HWND child = GetTopWindow(tempWindow);
			WCHAR name[50];
			if (GetWindowText(child, name, ARRAYSIZE(name)) == 0)
				return;
			wostringstream focusName;
			focusName << name;
			// Now, get the original name of the button.
			if (GetWindowText(hwnd, name, ARRAYSIZE(name)) == 0)
				return;
			focusName << L": " << name;
			accPropServices->SetHwndPropStr(hwnd, objId, childId, PROPID_ACC_NAME, focusName.str().c_str());
		}
	}
}
HWINEVENTHOOK winEventHook = NULL;

#endif // _WIN32

extern "C" {

REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec) {
	if (rec) {
		// Load.
		if (rec->caller_version != REAPER_PLUGIN_VERSION || !rec->GetFunc || REAPERAPI_LoadAPI(rec->GetFunc) != 0)
			return 0; // Incompatible.

		pluginHInstance = hInstance;
		mainHwnd = rec->hwnd_main;
		loadConfig();
		resetTimeCache();

#ifdef _WIN32
		if (CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER, IID_IAccPropServices, (void**)&accPropServices) != S_OK)
			return 0;
		guiThread = GetWindowThreadProcessId(mainHwnd, NULL);
		winEventHook = SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, hInstance, handleWinEvent, 0, guiThread, WINEVENT_INCONTEXT);
#else
		NSA11yWrapper::init();
#endif

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
		SetTimer(NULL, NULL, 0, delayedInit);
		keyboardHook = SetWindowsHookEx(WH_KEYBOARD, keyboardHookProc, NULL, guiThread);
#endif
		return 1;

	} else {
		// Unload.
#ifdef _WIN32
		UnhookWindowsHookEx(keyboardHook);
		UnhookWinEvent(winEventHook);
		accPropServices->Release();
#else
		NSA11yWrapper::destroy();
#endif
		return 0;
	}
}

}
