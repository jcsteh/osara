/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Main plug-in code
 * Author: James Teh <jamie@nvaccess.org>
 * Copyright 2014-2017 NV Access Limited
 * License: GNU General Public License version 2.0
 */

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
#include <functional>
#include <regex>
#include <math.h>
#ifdef _WIN32
// We only need this on Windows and it apparently causes compilation issues on Mac.
#include <codecvt>
#else
#include "osxa11y_wrapper.h" // NSA11y wrapper for OS X accessibility API
#endif
#include <WDL/win32_utf8.h>
#define REAPERAPI_IMPLEMENT
#include "osara.h"
#include <WDL/db2val.h>
#include "resource.h"
#include "paramsUi.h"
#include "peakWatcher.h"

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

// #59: postGoToMarker reports the marker/region at the cursor.
// This can be misleading when jumping to a marker/region with a specific number
// if that marker/region doesn't exist or there are two at the same position.
// This function reports only if the number matches the target number.
void postGoToSpecificMarker(int command) {
	int count = CountProjectMarkers(0, NULL, NULL);
	int wantNum;
	bool wantReg = false;
	// Work out the desired marker/region number based on the command id.
	if (command == 40160)
		wantNum = 10;
	else if (40161 <= command && command <= 40169)
		wantNum = command - 40160;
	else if (command == 41760) {
		wantReg = true;
		wantNum = 10;
	} else if (41761 <= command && command <= 41769) {
		wantReg = true;
		wantNum = command - 41760;
	} else
		return; // Shouldn't happen.
	for (int i = 0; i < count; ++i) {
		bool reg;
		int num;
		const char* name;
		EnumProjectMarkers(i, &reg, NULL, NULL, &name, &num);
		if (num != wantNum || reg != wantReg)
			continue;
		fakeFocus = reg ? FOCUS_REGION : FOCUS_MARKER;
		ostringstream s;
		if (name[0])
			s << name << (reg ? " region " : " marker ");
		else
			s << (reg ? "region " : "marker ") << num << " ";
		s << formatCursorPosition();
		outputMessage(s);
		return;
	}
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

void addTakeFxNames(MediaItem_Take* take, ostringstream &s) {
	if (!shouldReportFx)
		return;
	int count = TakeFX_GetCount(take);
	if (count == 0)
		return;
	s << "; FX: ";
	char name[256];
	for (int f = 0; f < count; ++f) {
		if (f > 0)
			s << ", ";
		TakeFX_GetFXName(take, f, name, sizeof(name));
		s << name;
	}
}

void postSwitchToTake(int command) {
	MediaItem* item = GetSelectedMediaItem(0, 0);
	if (!item)
		return;
	MediaItem_Take* take = GetActiveTake(item);
	if (!take)
		return;
	ostringstream s;
	s << (int)(size_t)GetSetMediaItemTakeInfo(take, "IP_TAKENUMBER", NULL) + 1 << " "
		<< GetTakeName(take);
	addTakeFxNames(take, s);
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

void postMoveToTimeSig(int command) {
	double cursor = GetCursorPosition();
	// FindTempoTimeSigMarker often returns the point before instead of right at the position.
	// Increment the cursor position a bit to work around this.
	int marker = FindTempoTimeSigMarker(0, cursor + 0.0001);
	double pos, bpm;
	int sigNum, sigDenom;
	GetTempoTimeSigMarker(0, marker, &pos, NULL, NULL, &bpm, &sigNum, &sigDenom, NULL);
	if (pos != cursor)
		return;
	fakeFocus = FOCUS_TIMESIG;
	ostringstream s;
	s << "tempo " << bpm;
	if (sigNum > 0)
		s << " time sig " << sigNum << "/" << sigDenom;
	s << " " << formatCursorPosition();
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

void postToggleMetronome(int command) {
	outputMessage(GetToggleCommandState(command) ? "metronome on" : "metronome off");
}

void postToggleMasterTrackVisible(int command) {
	outputMessage(GetToggleCommandState(command) ? "master track visible" : "master track hidden");
}

bool shouldReportTransport = true;
void postChangeTransportState(int command) {
	if (!shouldReportTransport)
		return;
	int state = GetPlayState();
	if (state & 2)
		outputMessage("pause");
	else if (state & 4)
		outputMessage("record");
	else if (state & 1)
		outputMessage("play");
	else
		outputMessage("stop");
}

void postSelectMultipleItems(int command) {
	int count = CountSelectedMediaItems(0);
	ostringstream s;
	s << count << (count == 1 ? " item" : " items") << " selected";
	outputMessage(s);
}

void postMoveEnvelopePoint(int command) {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope)
		return;
	fakeFocus = FOCUS_ENVELOPE;
	// GetEnvelopePointByTime often returns the point before instead of right at the position.
	// Increment the cursor position a bit to work around this.
	int point = GetEnvelopePointByTime(envelope, GetCursorPosition() + 0.0001);
	if (point < 0)
		return;
	double value;
	bool selected;
	GetEnvelopePoint(envelope, point, NULL, &value, NULL, NULL, &selected);
	if (!selected)
		return; // Not moved.
	char out[64];
	Envelope_FormatValue(envelope, value, out, sizeof(out));
	outputMessage(out);
}

void postRenameTrack(int command) {
	if (!GetLastTouchedTrack())
		return;
	// #82: On Windows, this will end up as the label of the track name text box.
	outputMessage("Track name");
}

void postToggleItemMute(int command) {
	MediaItem* item = GetSelectedMediaItem(0, 0);
	if (!item)
		return;
	outputMessage(*(bool*)GetSetMediaItemInfo(item, "B_MUTE", NULL) ?
		"muted" : "unmuted");
}

void postSetSelectionEnd(int command) {
	outputMessage("set selection end");
	fakeFocus = FOCUS_RULER;
}

void postToggleMasterMono(int command) {
	outputMessage(GetToggleCommandState(command) ? "master mono" : "master stereo");
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
	{40318, postCursorMovement}, // Item navigation: Move cursor left to edge of item
	{40319, postCursorMovement}, // Item navigation: Move cursor right to edge of item
	{40646, postCursorMovement}, // View: Move cursor left to grid division
	{40647, postCursorMovement}, // View: Move cursor right to grid division
	{41042, postCursorMovementMeasure}, // Go forward one measure
	{41043, postCursorMovementMeasure}, // Go back one measure
	{41044, postCursorMovementMeasure}, // Go forward one beat
	{41045, postCursorMovementMeasure}, // Go back one beat
	{1041, postCycleTrackFolderState}, // Track: Cycle track folder state
	{1042, postCycleTrackFolderCollapsed}, // Track: Cycle track folder collapsed state
	{40172, postGoToMarker}, // Markers: Go to previous marker/project start
	{40173, postGoToMarker}, // Markers: Go to next marker/project end
	{40161, postGoToSpecificMarker}, // Markers: Go to marker 01
	{40162, postGoToSpecificMarker}, // Markers: Go to marker 02
	{40163, postGoToSpecificMarker}, // Markers: Go to marker 03
	{40164, postGoToSpecificMarker}, // Markers: Go to marker 04
	{40165, postGoToSpecificMarker}, // Markers: Go to marker 05
	{40166, postGoToSpecificMarker}, // Markers: Go to marker 06
	{40167, postGoToSpecificMarker}, // Markers: Go to marker 07
	{40168, postGoToSpecificMarker}, // Markers: Go to marker 08
	{40169, postGoToSpecificMarker}, // Markers: Go to marker 09
	{40160, postGoToSpecificMarker}, // Markers: Go to marker 10
	{41761, postGoToSpecificMarker}, // Regions: Go to region 01 after current region finishes playing (smooth seek)
	{41762, postGoToSpecificMarker}, // Regions: Go to region 02 after current region finishes playing (smooth seek)
	{41763, postGoToSpecificMarker}, // Regions: Go to region 03 after current region finishes playing (smooth seek)
	{41764, postGoToSpecificMarker}, // Regions: Go to region 04 after current region finishes playing (smooth seek)
	{41765, postGoToSpecificMarker}, // Regions: Go to region 05 after current region finishes playing (smooth seek)
	{41766, postGoToSpecificMarker}, // Regions: Go to region 06 after current region finishes playing (smooth seek)
	{41767, postGoToSpecificMarker}, // Regions: Go to region 07 after current region finishes playing (smooth seek)
	{41768, postGoToSpecificMarker}, // Regions: Go to region 08 after current region finishes playing (smooth seek)
	{41769, postGoToSpecificMarker}, // Regions: Go to region 09 after current region finishes playing (smooth seek)
	{41760, postGoToSpecificMarker}, // Regions: Go to region 10 after current region finishes playing (smooth seek)
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
	{40364, postToggleMetronome}, // Options: Toggle metronome
	{40075, postToggleMasterTrackVisible}, // View: Toggle master track visible
	{40044, postChangeTransportState}, // Transport: Play/stop
	{40073, postChangeTransportState}, // Transport: Play/pause
	{40317, postChangeTransportState}, // Transport: Play (skip time selection)
	{1013, postChangeTransportState}, // Transport: Record
	{40718, postSelectMultipleItems}, // Item: Select all items on selected tracks in current time selection
	{40421, postSelectMultipleItems}, // Item: Select all items in track
	{40034, postSelectMultipleItems}, // Item grouping: Select all items in groups
	{40117, postMoveEnvelopePoint}, // Item edit: Move items/envelope points up one track/a bit
	{40118, postMoveEnvelopePoint}, // Item edit: Move items/envelope points down one track/a bit
	{40696, postRenameTrack}, // Track: Rename last touched track
	{40175, postToggleItemMute}, // Item properties: Toggle mute
	{40626, postSetSelectionEnd}, // Time selection: Set end point
	{40917, postToggleMasterMono}, // Master track: Toggle stereo/mono (L+R)
	{0},
};
PostCustomCommand POST_CUSTOM_COMMANDS[] = {
	{"_XENAKIOS_NUDGSELTKVOLUP", postChangeTrackVolume}, // Xenakios/SWS: Nudge volume of selected tracks up
	{"_XENAKIOS_NUDGSELTKVOLDOWN", postChangeTrackVolume}, // Xenakios/SWS: Nudge volume of selected tracks down
	{"_FNG_ENVDOWN", postMoveEnvelopePoint}, // SWS/FNG: Move selected envelope points down
	{"_FNG_ENVUP", postMoveEnvelopePoint}, // SWS/FNG: Move selected envelope points up
	{NULL},
};
map<int, PostCommandExecute> postCommandsMap;
map<int, string> POST_COMMAND_MESSAGES = {
	{40625, "set selection start"}, // Time selection: Set start point
	{40222, "set loop start"}, // Loop points: Set start point
	{40223, "set loop end"}, // Loop points: Set end point
};

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
	data.index = (int)(size_t)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", NULL);
	if (data.index == -1) // Master
		data.index = 0;
	if (GetMasterTrackVisibility() & 1)
		data.index += 1;
	data.retHwnd = NULL;
	data.foundCount = 0;
	WNDENUMPROC callback = [] (HWND testHwnd, LPARAM lParam) -> BOOL {
		GetTrackVuData* data = (GetTrackVuData*)lParam;
		WCHAR className[14];
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

HWND getSendContainer(HWND hwnd) {
	WCHAR className[21] = L"\0";
	GetClassNameW(hwnd, className, ARRAYSIZE(className));
	if (wcscmp(className, L"Button") != 0)
		return NULL;
	hwnd = GetWindow(hwnd, GW_HWNDPREV);
	if (!hwnd)
		return NULL;
	GetClassNameW(hwnd, className, ARRAYSIZE(className));
	if (wcscmp(className, L"Static") != 0)
		return NULL;
	hwnd = GetAncestor(hwnd, GA_PARENT);
	if (!hwnd)
		return NULL;
	GetClassNameW(hwnd, className, ARRAYSIZE(className));
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
		// Make sure this stays around until the InsertMenuItem call.
		string nameN = narrow(name);
		itemInfo.dwTypeData = (char*)nameN.c_str();
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

void clickIoButton(MediaTrack* track, bool rightClick=false) {
	HWND hwnd = getTrackVu(track);
	if (!hwnd)
		return; // Really shouldn't happen.
	// Use MSAA to get the location of the I/O button.
	hwnd = GetAncestor(hwnd, GA_PARENT);
	IAccessible* acc = NULL;
	VARIANT varChild;
	DWORD childId = track == GetMasterTrack(0) ? 5 : 7;
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
	SendMessage(hwnd,
		rightClick ? WM_RBUTTONDOWN : WM_LBUTTONDOWN,
		rightClick ? MK_RBUTTON : MK_LBUTTON,
		MAKELPARAM(point.x, point.y));
	SendMessage(hwnd,
		rightClick ? WM_RBUTTONUP : WM_LBUTTONUP, 0,
		MAKELPARAM(point.x, point.y));
}

bool maybeSwitchToFxPluginWindow() {
	HWND window = GetForegroundWindow();
	char name[4];
	if (GetWindowText(window, name, sizeof(name)) == 0)
		return false;
	if (strncmp(name, "FX: ", 4) != 0)
		return false;
	// Descend.
	if (!(window = GetWindow(window, GW_CHILD)))
		return false;
	// This is a property page containing the plugin window among other things.
	if (!(window = GetWindow(window, GW_HWNDLAST)))
		return false;
	// Descend.
	if (!(window = GetWindow(window, GW_CHILD)))
		return false;
	// This should get the plugin window.
	if (!(window = GetWindow(window, GW_HWNDLAST)))
		return false;
	HWND oldFocus = GetFocus();
	SetFocus(window);
	if (GetFocus() == oldFocus) // Focus didn't move
		return false;
	return true;
}

// Handle keyboard keys which can't be bound to actions.
// REAPER's "accelerator" hook isn't enough because it doesn't get called in some windows.
LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
	if (code != HC_ACTION && wParam != VK_APPS && wParam != VK_RETURN && wParam != VK_F6) {
		// Return early if we're not interested in the key.
		return CallNextHookEx(NULL, code, wParam, lParam);
	}
	HWND focus = GetFocus();
	if (!focus)
		return CallNextHookEx(NULL, code, wParam, lParam);
	WCHAR className[22] = L"\0";
	GetClassNameW(focus, className, ARRAYSIZE(className));
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
					if (GetKeyState(VK_CONTROL) & 0x8000) // Track area
						TrackPopupMenu(GetContextMenu(0), 0, 0, 0, 0, mainHwnd, NULL);
					else if (GetKeyState(VK_MENU) & 0x8000) {
						// Routing. Can't be retrieved with GetContextMenu.
						MediaTrack* track = GetLastTouchedTrack();
						if (!track)
							return 0;
						clickIoButton(track, true);
					} else {
						// Track input. Can't be retrieved with GetContextMenu.
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
	} else if (wParam == VK_F6 && !(lParam & 0x80000000)) {
		if (maybeSwitchToFxPluginWindow())
			return 1;
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
	if (count == 0) {
		outputMessage("No tracks");
		return;
	}
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
		if (*(bool*)GetSetMediaItemInfo(item, "B_MUTE", NULL))
			s << " muted";
		if (*(char*)GetSetMediaItemInfo(item, "C_LOCK", NULL) & 1)
			s << " locked";
		int takeCount = CountTakes(item);
		if (takeCount > 1)
			s << " " << takeCount << " takes";
		s << " " << formatCursorPosition();
		addTakeFxNames(take, s);
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
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	int oldPoints = 0;
	if (envelope)
		oldPoints = CountEnvelopePoints(envelope);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	ostringstream s;
	int added;
	if ((added = CountTracks(0) - oldTracks) > 0)
		s << added << (added == 1 ? " track" : " tracks") << " added";
	else if ((added = CountMediaItems(0) - oldItems) > 0)
		s << added << (added == 1 ? " item" : " items") << " added";
	else if (envelope && (added = CountEnvelopePoints(envelope) - oldPoints) > 0)
		s << added << (added == 1 ? " point" : " points") << " added";
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

void cmdhDeleteEnvelopePoints(int command) {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope)
		return;
	int oldCount = CountEnvelopePoints(envelope);
	Main_OnCommand(command, 0);
	int removed = oldCount - CountEnvelopePoints(envelope);
	ostringstream s;
	s << removed << (removed == 1 ? " point" : " points") << " removed";
	outputMessage(s);
}

void cmdDeleteEnvelopePoints(Command* command) {
	cmdhDeleteEnvelopePoints(command->gaccel.accel.cmd);
}

void cmdCut(Command* command) {
	switch (GetCursorContext2(true)) {
		case 0: // Track
			cmdhRemoveTracks(command->gaccel.accel.cmd);
			return;
		case 1: // Item
			cmdhRemoveItems(command->gaccel.accel.cmd);
			return;
		case 2: // Envelope
			cmdhDeleteEnvelopePoints(command->gaccel.accel.cmd);
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

void cmdClearTimeLoopSel(Command* command) {
	double start, end;
	GetSet_LoopTimeRange(false, false, &start, &end, false);
	double old = start + end;
	GetSet_LoopTimeRange(false, true, &start, &end, false);
	old += start + end;
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	GetSet_LoopTimeRange(false, false, &start, &end, false);
	double cur = start + end;
	GetSet_LoopTimeRange(false, true, &start, &end, false);
	cur += start + end;
	if (old != cur)
		outputMessage("Cleared time/loop selection");
}

void cmdUnselAllTracksItemsPoints(Command* command) {
	int old = CountSelectedTracks(0) + CountSelectedMediaItems(0)
		+ (GetSelectedEnvelope(0) ? 1 : 0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	int cur = CountSelectedTracks(0) + CountSelectedMediaItems(0)
		+ (GetSelectedEnvelope(0) ? 1 : 0);
	if (old != cur)
		outputMessage("Unselected tracks/items/envelope points");
}

void cmdMidiMoveCursor(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	ostringstream s;
	s << formatCursorPosition();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int notes;
	MIDI_CountEvts(take, &notes, NULL, NULL);
	double now = GetCursorPosition();
	int count = 0;
	// todo: Optimise; perhaps a binary search?
	for (int n = 0; n < notes; ++n) {
		double start;
		MIDI_GetNote(take, n, NULL, NULL, &start, NULL, NULL, NULL, NULL);
		start = MIDI_GetProjTimeFromPPQPos(take, start);
		if (start > now)
			break;
		if (start == now)
			++count;
	}
	if (count > 0)
		s << " " << count << (count == 1 ? " note" : " notes");
		outputMessage(s);
}

const string getMidiNoteName(int pitch) {
	static char* names[] = {"c", "c sharp", "d", "d sharp", "e", "f",
		"f sharp", "g", "g sharp", "a", "a sharp", "b"};
	int octave = pitch / 12 - 1;
	pitch %= 12;
	ostringstream s;
	s << names[pitch] << " " << octave;
	return s.str();
}

void cmdMidiMoveToNote(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	double now = GetCursorPosition();

	bool selAtCur = false;
	int note = -1;
	double start;
	while ((note = MIDI_EnumSelNotes(take, note)) != -1) {
		MIDI_GetNote(take, note, NULL, NULL, &start, NULL, NULL, NULL, NULL);
		start = MIDI_GetProjTimeFromPPQPos(take, start);
		if (start == now) {
			selAtCur = true;
			break;
		}
	}

	if (!selAtCur) {
		// There are selected notes which aren't at the cursor.
		// In this case, these actions move relative to these selected notes,
		// but we want to move relative to the edit cursor.
		// Clear the selection to make these actions use the edit cursor.
		MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
		// Move back a tiny bit so notes right at our start position are always treated as next.
		// SetEditCurPos isn't respected here for some reason.
		MIDIEditor_OnCommand(editor, 40185); // Edit: Move edit cursor left one pixel
	}

	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	note = MIDI_EnumSelNotes(take, -1);
	if (note == -1) {
		// We might have moved the edit cursor.
		SetEditCurPos(now, true, false);
		return;
	}
	int pitch;
	MIDI_GetNote(take, note, NULL, NULL, &start, NULL, NULL, &pitch, NULL);
	start = MIDI_GetProjTimeFromPPQPos(take, start);
	SetEditCurPos(start, false, false);
	ostringstream s;
	s << getMidiNoteName(pitch) << " " << formatCursorPosition();
	outputMessage(s);
}

void cmdMidiMovePitchCursor(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	int pitch = MIDIEditor_GetSetting_int(editor, "active_note_row");
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	MediaTrack* track = (MediaTrack*)GetSetMediaItemTakeInfo(take, "P_TRACK", NULL);
	outputMessage(getMidiNoteName(pitch));
}

void cmdMidiDeleteEvents(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int oldCount = MIDI_CountEvts(take, NULL, NULL, NULL);
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	int removed = oldCount - MIDI_CountEvts(take, NULL, NULL, NULL);
	ostringstream s;
	s << removed << (removed == 1 ? " event" : " events") << " removed";
	outputMessage(s);
}

void cmdMoveToNextItemKeepSel(Command* command) {
	moveToItem(1, false, isSelectionContiguous);
}

void cmdMoveToPrevItemKeepSel(Command* command) {
	moveToItem(-1, false, isSelectionContiguous);
}

#ifdef _WIN32

void cmdIoMaster(Command* command) {
	// If the master track isn't visible, make it so temporarily.
	int prevVisible = GetMasterTrackVisibility();
	if (!(prevVisible & 1))
		SetMasterTrackVisibility(prevVisible | 1);
	clickIoButton(GetMasterTrack(0));
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
		case FOCUS_ENVELOPE:
			cmdhDeleteEnvelopePoints(40333); // Envelope: Delete all selected points
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
	Undo_BeginBlock();
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
	Undo_EndBlock("Move stretch marker", UNDO_STATE_ITEMS);
	if (done)
		outputMessage("stretch marker moved");
}

void reportPeak(MediaTrack* track, int channel) {
	ostringstream s;
	s << fixed << setprecision(1);
	s << VAL2DB(Track_GetPeakInfo(track, channel));
	outputMessage(s);
}

void cmdReportPeakCurrentC1(Command* command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	reportPeak(track, 0);
}

void cmdReportPeakCurrentC2(Command* command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	reportPeak(track, 1);
}

void cmdReportPeakMasterC1(Command* command) {
	reportPeak(GetMasterTrack(0), 0);
}

void cmdReportPeakMasterC2(Command* command) {
	reportPeak(GetMasterTrack(0), 1);
}

void cmdDeleteAllTimeSigs(Command* command) {
	Undo_BeginBlock();
	int count = CountTempoTimeSigMarkers(0);
	if (!count)
		return;
	for (int i = count - 1; i >= 0; --i)
		DeleteTempoTimeSigMarker(0, i);
	Undo_EndBlock("Delete all time signature markers", UNDO_STATE_ALL);
	outputMessage("Deleted all time signature markers");
}

const regex RE_ENVELOPE_STATE("<(AUX|HW)?(\\S+)[^]*?\\sACT (0|1)[^]*?\\sVIS (0|1)[^]*?\\sARM (0|1)");
bool selectEnvelopeUseTake = false;
void cmdhSelectEnvelope(int direction) {
	// If we're focused on a track or item, use the envelopes associated therewith.
	// If we're focused on an envelope, use what we last used.
	// This is necessary because the first envelope selection will focus the envelope
	// and we want to allow the user to move past the first.
	if (fakeFocus == FOCUS_TRACK)
		selectEnvelopeUseTake = false;
	else if (fakeFocus == FOCUS_ITEM)
		selectEnvelopeUseTake = true;
	else if (fakeFocus != FOCUS_ENVELOPE)
		return; // No envelopes for focus.
	MediaTrack* track = NULL;
	int count;
	function<TrackEnvelope*(int)> getEnvelope;
	if (selectEnvelopeUseTake) {
		MediaItem* item = GetSelectedMediaItem(0, 0);
		if (!item)
			return;
		MediaItem_Take* take = GetActiveTake(item);
		if (!take)
			return;
		count = CountTakeEnvelopes(take);
		getEnvelope = [take] (int index) { return GetTakeEnvelope(take, index); };
	} else {
		track = GetLastTouchedTrack();
		if (!track)
			return;
		count = CountTrackEnvelopes(track);
		getEnvelope = [track] (int index) { return GetTrackEnvelope(track, index); };
	}
	if (count == 0) {
		outputMessage(selectEnvelopeUseTake ? "no take envelopes" : "no track envelopes");
		return;
	}

	TrackEnvelope* origEnv = GetSelectedEnvelope(0);
	int start = direction == 1 ? 0 : count - 1;
	// Find the current envelope.
	int origIndex = -1;
	int index;
	TrackEnvelope* env;
	for (index = start; 0 <= index && index < count; index += direction) {
		env = getEnvelope(index);
		if (env == origEnv) {
			origIndex = index;
			break;
		}
	}
	if (origIndex == -1) {
		// The current envelope isn't for this track/take.
		origEnv = NULL;
		// Start at the start.
		origIndex = start - direction;
	}

	// Get the next envelope in the requested direction.
	cmatch m;
	index = origIndex;
	for (; ;) {
		index += direction;
		if (index < 0 || index >= count) {
			if (origEnv) {
				// We started after the start, so wrap around.
				index = start;
			} else {
				// We started at the start, so there are no more.
				env = NULL;
				break;
			}
		}
		env = getEnvelope(index);
		char state[100];
		GetEnvelopeStateChunk(env, state, sizeof(state), false);
		regex_search(state, m, RE_ENVELOPE_STATE);
		if (env == origEnv) {
			// We're back where we started. Don't try to go any further.
			break;
		}
		if (!m.empty() && m.str(4)[0] == '0')
			continue; // Invisible, so skip.
		break; // We found our envelope!
	}
	if (!env) {
		outputMessage("no visible envelopes");
		return;
	}

	SetCursorContext(2, env);
	fakeFocus = FOCUS_ENVELOPE;
	ostringstream s;
	if (!m.empty() && m.str(1).compare("AUX") == 0) {
		// Send envelope. Get the name of the send.
		string envType = '<' + m.str(2); // e.g. <VOLENV
		int sendCount = GetTrackNumSends(track, 0);
		for (int i = 0; i < sendCount; ++i) {
			TrackEnvelope* sendEnv = (TrackEnvelope*)GetSetTrackSendInfo(track, 0, i, "P_ENV", (void*)envType.c_str());
			if (sendEnv == env) {
				MediaTrack* sendTrack = (MediaTrack*)GetSetTrackSendInfo(track, 0, i, "P_DESTTRACK", NULL);
				s << (int)(size_t)GetSetMediaTrackInfo(sendTrack, "IP_TRACKNUMBER", NULL) << " ";
				char* trackName = (char*)GetSetMediaTrackInfo(sendTrack, "P_NAME", NULL);
				if (trackName)
					s << trackName << " ";
			}
		}
	}
	char name[50];
	GetEnvelopeName(env, name, sizeof(name));
	s << name << " envelope";
	if (!m.empty()) {
		if (m.str(3)[0] == '0')
			s << " bypassed";
		if (m.str(5)[0] == '1')
			s << " armed";
	}
	outputMessage(s);
}

void cmdSelectNextEnvelope(Command* command) {
	cmdhSelectEnvelope(1);
}

void cmdSelectPreviousEnvelope(Command* command) {
	cmdhSelectEnvelope(-1);
}

void moveToEnvelopePoint(int direction, bool clearSelection=true) {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope)
		return;
	int count = CountEnvelopePoints(envelope);
	if (count == 0)
		return;
	double now = GetCursorPosition();
	// Get the point at or before the cursr.
	int point = GetEnvelopePointByTime(envelope, now);
	if (point < 0) {
		if (direction == -1)
			return;
		++point;
	}
	double time, value;
	bool selected;
	GetEnvelopePoint(envelope, point, &time, &value, NULL, NULL, &selected);
	if ((direction == 1 && time < now)
		// If this point is at the cursor, skip it only if it's selected.
		// This allows you to easily get to a point at the cursor
		// while still allowing you to move beyond it once you do.
		|| (direction == 1 && selected && time == now)
		// Moving backward should skip the point at the cursor.
		|| (direction == -1 && time >= now)
	) {
		// This isn't the point we want. Try the next.
		int newPoint = point + direction;
		if (0 <= newPoint && newPoint < count) {
			point = newPoint;
			GetEnvelopePoint(envelope, point, &time, &value, NULL, NULL, &selected);
		}
	}
	if (direction == 1 ? time < now : time > now)
		return; // No point in this direction.
	fakeFocus = FOCUS_ENVELOPE;
	if (clearSelection)
		Main_OnCommand(40331, 0); // Envelope: Unselect all points
	SetEnvelopePoint(envelope, point, NULL, NULL, NULL, NULL, &bTrue, &bTrue);
	SetEditCurPos(time, true, true);
	ostringstream s;
	s << "point " << point + 1 << " value ";
	char out[64];
	Envelope_FormatValue(envelope, value, out, sizeof(out));
	s << out;
	if (!clearSelection) {
		int numSel = 0;
		for (point = 0; point < count; ++point) {
			GetEnvelopePoint(envelope, point, NULL, NULL, NULL, NULL, &selected);
			if (selected)
				++numSel;
			if (numSel == 2)
				break; // Don't care above this.
		}
		// One selected point is the norm, so don't report selected in this case.
		if (numSel > 1)
			s << " selected";
	}
	s << " " << formatCursorPosition();
	outputMessage(s);
}

void cmdMoveToNextEnvelopePoint(Command* command) {
	moveToEnvelopePoint(1);
}

void cmdMoveToPrevEnvelopePoint(Command* command) {
	moveToEnvelopePoint(-1);
}

void cmdMoveToNextEnvelopePointKeepSel(Command* command) {
	moveToEnvelopePoint(1, false);
}

void cmdMoveToPrevEnvelopePointKeepSel(Command* command) {
	moveToEnvelopePoint(-1, false);
}

// See the Configuration section of the code below.
void cmdConfig(Command* command);

#ifdef _WIN32
void cmdFocusNearestMidiEvent(Command* command) {
	GUITHREADINFO guiThreadInfo;
	guiThreadInfo.cbSize = sizeof(GUITHREADINFO);
	GetGUIThreadInfo(guiThread, &guiThreadInfo);
	if (!guiThreadInfo.hwndFocus)
		return;
	double cursorPos = GetCursorPosition();
	for (int i = 0; i < ListView_GetItemCount(guiThreadInfo.hwndFocus); ++i) {
		char text[50];
		// Get the text from the position column (1).
		ListView_GetItemText(guiThreadInfo.hwndFocus, i, 1, text, sizeof(text));
		// Convert this to project time. text is always in measures.beats.
		double eventPos = parse_timestr_pos(text, 2);
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
const int MIDI_EDITOR_SECTION = 32060;

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
	{MAIN_SECTION, {{0, 0, 40333}, NULL}, NULL, cmdDeleteEnvelopePoints}, // Envelope: Delete all selected points
	{MAIN_SECTION, {{0, 0, 40089}, NULL}, NULL, cmdDeleteEnvelopePoints}, // Envelope: Delete all points in time selection
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
	{MAIN_SECTION, {{0, 0, 40020}, NULL}, NULL, cmdClearTimeLoopSel}, // Time selection: Remove time selection and loop point selection
	{MAIN_SECTION, {{0, 0, 40769}, NULL}, NULL, cmdUnselAllTracksItemsPoints}, // Unselect all tracks/items/envelope points
	{MIDI_EDITOR_SECTION, {{0, 0, 40036}, NULL}, NULL, cmdMidiMoveCursor}, // View: Go to start of file
	{MIDI_EDITOR_SECTION, {{0, 0, 40037}, NULL}, NULL, cmdMidiMoveCursor}, // View: Go to end of file
	{MIDI_EDITOR_SECTION, {{0, 0, 40047}, NULL}, NULL, cmdMidiMoveCursor}, // Edit: Move edit cursor left by grid
	{MIDI_EDITOR_SECTION, {{0, 0, 40048}, NULL}, NULL, cmdMidiMoveCursor}, // Edit: Move edit cursor right by grid
	{MIDI_EDITOR_SECTION, {{0, 0, 40682}, NULL}, NULL, cmdMidiMoveCursor}, // Edit: Move edit cursor right one measure
	{MIDI_EDITOR_SECTION, {{0, 0, 40683}, NULL}, NULL, cmdMidiMoveCursor}, // Edit: Move edit cursor left one measure
	{MIDI_EDITOR_SECTION, {{0, 0, 40413}, NULL}, NULL, cmdMidiMoveToNote}, // Navigate: Select next note
	{MIDI_EDITOR_SECTION, {{0, 0, 40414}, NULL}, NULL, cmdMidiMoveToNote}, // Navigate: Select previous note
	{MIDI_EDITOR_SECTION, {{0, 0, 40049}, NULL}, NULL, cmdMidiMovePitchCursor}, // Edit: Increase pitch cursor one semitone
	{MIDI_EDITOR_SECTION, {{0, 0, 40050}, NULL}, NULL, cmdMidiMovePitchCursor}, // Edit: Decrease pitch cursor one semitone
	{MIDI_EDITOR_SECTION, {{0, 0, 40667}, NULL}, NULL, cmdMidiDeleteEvents}, // Edit: Delete events
	// Our own commands.
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move to next item (leaving other items selected)"}, "OSARA_NEXTITEMKEEPSEL", cmdMoveToNextItemKeepSel},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move to previous item (leaving other items selected)"}, "OSARA_PREVITEMKEEPSEL", cmdMoveToPrevItemKeepSel},
#ifdef _WIN32
	{MAIN_SECTION, {DEFACCEL, "OSARA: View parameters for current track/item (depending on focus)"}, "OSARA_PARAMS", cmdParamsFocus},
	{MAIN_SECTION, {DEFACCEL, "OSARA: View FX parameters for current track/take (depending on focus)"}, "OSARA_FXPARAMS", cmdFxParamsFocus},
	{MAIN_SECTION, {DEFACCEL, "OSARA: View FX parameters for master track"}, "OSARA_FXPARAMSMASTER", cmdFxParamsMaster},
#endif
	{MAIN_SECTION, {DEFACCEL, "OSARA: View Peak Watcher"}, "OSARA_PEAKWATCHER", cmdPeakWatcher},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report Peak Watcher value for channel 1 of first track"}, "OSARA_REPORTPEAKWATCHERT1C1", cmdReportPeakWatcherT1C1},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report Peak Watcher value for channel 2 of first track"}, "OSARA_REPORTPEAKWATCHERT1C2", cmdReportPeakWatcherT1C2},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report Peak Watcher value for channel 1 of second track"}, "OSARA_REPORTPEAKWATCHERT2C1", cmdReportPeakWatcherT2C1},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report Peak Watcher value for channel 2 of second track"}, "OSARA_REPORTPEAKWATCHERT2C2", cmdReportPeakWatcherT2C2},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Reset Peak Watcher for first track"}, "OSARA_RESETPEAKWATCHERT1", cmdResetPeakWatcherT1},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Reset Peak Watcher for second track"}, "OSARA_RESETPEAKWATCHERT2", cmdResetPeakWatcherT2},
#ifdef _WIN32
	{MAIN_SECTION, {DEFACCEL, "OSARA: View I/O for master track"}, "OSARA_IOMASTER", cmdIoMaster},
#endif // _WIN32
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report ripple editing mode"}, "OSARA_REPORTRIPPLE", cmdReportRippleMode},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report muted tracks"}, "OSARA_REPORTMUTED", cmdReportMutedTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report soloed tracks"}, "OSARA_REPORTSOLOED", cmdReportSoloedTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report record armed tracks"}, "OSARA_REPORTARMED", cmdReportArmedTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report tracks with record monitor on"}, "OSARA_REPORTMONITORED", cmdReportMonitoredTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report tracks with phase inverted"}, "OSARA_REPORTPHASED", cmdReportPhaseInvertedTracks},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report track/item/time selection (depending on focus)"}, "OSARA_REPORTSEL", cmdReportSelection},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Remove items/tracks/contents of time selection/markers/envelope points (depending on focus)"}, "OSARA_REMOVE", cmdRemoveFocus},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Toggle shortcut help"}, "OSARA_SHORTCUTHELP", cmdShortcutHelp},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report edit/play cursor position"}, "OSARA_CURSORPOS", cmdReportCursorPosition},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Enable noncontiguous selection/toggle selection of current track/item (depending on focus)"}, "OSARA_TOGGLESEL", cmdToggleSelection},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move last focused stretch marker to current edit cursor position"}, "OSARA_MOVESTRETCH", cmdMoveStretch},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report current peak for channel 1 of current track"}, "OSARA_REPORTPEAKCURRENTC1", cmdReportPeakCurrentC1},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report current peak for channel 2 of current track"}, "OSARA_REPORTPEAKCURRENTC2", cmdReportPeakCurrentC2},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report current peak for channel 1 of master track"}, "OSARA_REPORTPEAKMASTERC1", cmdReportPeakMasterC1},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Report current peak for channel 2 of master track"}, "OSARA_REPORTPEAKMASTERC2", cmdReportPeakMasterC2},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Delete all time signature markers"}, "OSARA_DELETEALLTIMESIGS", cmdDeleteAllTimeSigs},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Select next track/take envelope (depending on focus)"}, "OSARA_SELECTNEXTENV", cmdSelectNextEnvelope},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Select previous track/take envelope (depending on focus)"}, "OSARA_SELECTPREVENV", cmdSelectPreviousEnvelope},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move to next envelope point"}, "OSARA_NEXTENVPOINT", cmdMoveToNextEnvelopePoint},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move to previous envelope point"}, "OSARA_PREVENVPOINT", cmdMoveToPrevEnvelopePoint},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move to next envelope point (leaving other points selected)"}, "OSARA_NEXTENVPOINTKEEPSEL", cmdMoveToNextEnvelopePointKeepSel},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Move to previous envelope point (leaving other points selected)"}, "OSARA_PREVENVPOINTKEEPSEL", cmdMoveToPrevEnvelopePointKeepSel},
	{MAIN_SECTION, {DEFACCEL, "OSARA: Configuration"}, "OSARA_CONFIG", cmdConfig},
#ifdef _WIN32
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
	shouldReportTransport = GetExtState(CONFIG_SECTION, "reportTransport")[0] != '0';
}

void config_onOk(HWND dialog) {
	shouldReportScrub = IsDlgButtonChecked(dialog, ID_CONFIG_REPORT_SCRUB) == BST_CHECKED;
	SetExtState(CONFIG_SECTION, "reportScrub", shouldReportScrub ? "1" : "0", true);
	shouldReportFx = IsDlgButtonChecked(dialog, ID_CONFIG_REPORT_FX) == BST_CHECKED;
	SetExtState(CONFIG_SECTION, "reportFx", shouldReportFx ? "1" : "0", true);
	shouldReportTransport = IsDlgButtonChecked(dialog, ID_CONFIG_REPORT_TRANSPORT) == BST_CHECKED;
	SetExtState(CONFIG_SECTION, "reportTransport", shouldReportTransport ? "1" : "0", true);
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

	CheckDlgButton(dialog, ID_CONFIG_REPORT_SCRUB, shouldReportScrub ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(dialog, ID_CONFIG_REPORT_FX, shouldReportFx ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(dialog, ID_CONFIG_REPORT_TRANSPORT, shouldReportTransport ? BST_CHECKED : BST_UNCHECKED);

	ShowWindow(dialog, SW_SHOWNORMAL);
}

/*** Initialisation, termination and inner workings. */

void postCommand(int command, int flag) {
	const auto it = postCommandsMap.find(command);
	if (it != postCommandsMap.end()) {
		it->second(command);
		return;
	}
	const auto mIt = POST_COMMAND_MESSAGES.find(command);
	if (mIt != POST_COMMAND_MESSAGES.end())
		outputMessage(mIt->second);
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
			if (GetWindowTextW(child, name, ARRAYSIZE(name)) == 0)
				return;
			wostringstream focusName;
			focusName << name;
			// Now, get the original name of the button.
			if (GetWindowTextW(hwnd, name, ARRAYSIZE(name)) == 0)
				return;
			focusName << L": " << name;
			accPropServices->SetHwndPropStr(hwnd, objId, childId, PROPID_ACC_NAME, focusName.str().c_str());
		}
	}
}
HWINEVENTHOOK winEventHook = NULL;

// Several windows in REAPER report as dialogs/property pages, but they aren't really.
// This includes the main window.
// Annotate these to prevent screen readers from potentially reading a spurious caption.
void annotateSpuriousDialogs(HWND hwnd) {
	VARIANT role;
	role.vt = VT_I4;
	role.lVal = hwnd == mainHwnd ? ROLE_SYSTEM_CLIENT : ROLE_SYSTEM_GROUPING;
	accPropServices->SetHwndProp(hwnd, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_ROLE, role);
	// If the previous hwnd is static text, oleacc will use this as the name.
	// This is never correct for these windows, so override it.
	if (GetWindowTextLength(hwnd) == 0)
		accPropServices->SetHwndPropStr(hwnd, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, L"");
	for (HWND child = FindWindowExW(hwnd, NULL, L"#32770", NULL); child;
		child = FindWindowExW(hwnd, child, L"#32770", NULL)
	)
		annotateSpuriousDialogs(child);
}

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
		annotateSpuriousDialogs(mainHwnd);
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

#ifndef _WIN32
// Mac resources
#include <swell-dlggen.h>
#include "reaper_osara.rc_mac_dlg"
#include <swell-menugen.h>
#include "reaper_osara.rc_mac_menu"
#endif
