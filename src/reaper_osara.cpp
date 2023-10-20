/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Main plug-in code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2014-2023 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#ifdef _WIN32
#include <initguid.h>
#include <oleacc.h>
#include <Windowsx.h>
#include <Commctrl.h>
#endif
// Must be defined before any C++ STL header is included.
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <string>
#include <sstream>
#include <map>
#include <iomanip>
#include <cassert>
#include <math.h>
#include <optional>
#include <set>
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
#include "config.h"
#include "resource.h"
#include "paramsUi.h"
#include "peakWatcher.h"
#include "midiEditorCommands.h"
#include "envelopeCommands.h"
#include "buildVersion.h"
#include "fxChain.h"
#include "translation.h"

using namespace std;
using namespace fmt::literals;

HINSTANCE pluginHInstance;
HWND mainHwnd;
#ifdef _WIN32
DWORD guiThread;
IAccPropServices* accPropServices = NULL;
#endif

// We cache the last reported time so we can report just the components which have changed.
int oldMeasure;
int oldBeat;
int oldbeatFraction;
int oldMinute;
int oldFrame;
int oldSecond;
int oldHour;
FakeFocus fakeFocus = FOCUS_NONE;
bool isShortcutHelpEnabled = false;
bool isSelectionContiguous = true;
int lastCommand = 0;
DWORD lastCommandTime = 0;
int lastCommandRepeatCount;
MediaItem* currentItem = nullptr;

/*** Utilities */

bool muteNextMessage = false;
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

void outputMessage(const string& message, bool interrupt) {
	if (muteNextMessage && isHandlingCommand) {
		return;
	}
	if (shouldUseUiaNotifications()) {
		if (sendUiaNotification(message, interrupt)) {
			return;
		}
	}
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
		accPropServices->SetHwndPropStr(
				guiThreadInfo.hwndFocus, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, widen(procMessage).c_str()
		);
		lastMessage = procMessage;
	} else {
		accPropServices->SetHwndPropStr(
				guiThreadInfo.hwndFocus, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, widen(message).c_str()
		);
		lastMessage = message;
	}
	// Fire a nameChange event so ATs will report this text.
	NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, guiThreadInfo.hwndFocus, OBJID_CLIENT, CHILDID_SELF);
	lastMessageHwnd = guiThreadInfo.hwndFocus;
}

#else // _WIN32

void outputMessage(const string& message, bool interrupt) {
	if (muteNextMessage && isHandlingCommand) {
		return;
	}
	NSA11yWrapper::osxa11y_announce(message);
}

#endif // _WIN32

void outputMessage(ostringstream& message, bool interrupt) {
	outputMessage(message.str(), interrupt);
}

string formatTime(
		double time, TimeFormat timeFormat, bool isLength, FormatTimeCacheRequest cache, bool includeZeros,
		bool includeProjectStartOffset
) {
	const bool useCache = cache == FT_USE_CACHE || (cache == FT_CACHE_DEFAULT && !settings::reportFullTimeMovement);
	ostringstream s;
	HWND midiEditor = MIDIEditor_GetActive();
	if (timeFormat == TF_RULER) {
		if (midiEditor && GetParent(GetFocus()) == midiEditor) {
			KbdSectionInfo* section = SectionFromUniqueID(MIDI_EDITOR_SECTION);
			if (GetToggleCommandState2(section, 40737)) {
				timeFormat = TF_MEASURETICK;
			} else {
				timeFormat = TF_MEASURE;
			}
		} else if (GetToggleCommandState(40365)) {
			timeFormat = TF_MINSEC;
		} else if (GetToggleCommandState(40368)) {
			timeFormat = TF_SEC;
		} else if (GetToggleCommandState(41973)) {
			timeFormat = TF_FRAME;
		} else if (GetToggleCommandState(40370)) {
			timeFormat = TF_HMSF;
		} else if (GetToggleCommandState(40369)) {
			timeFormat = TF_SAMPLE;
		} else {
			timeFormat = TF_MEASURE;
		}
	}
	if (!isLength && includeProjectStartOffset && timeFormat != TF_MEASURE && timeFormat != TF_MEASURETICK &&
			timeFormat != TF_SAMPLE) {
		time += GetProjectTimeOffset(nullptr, false);
	}
	switch (timeFormat) {
		case TF_MEASURE:
		case TF_MEASURETICK: {
			int measure;
			int measureLength;
			double beat = TimeMap2_timeToBeats(NULL, time, &measure, &measureLength, NULL, NULL);
			int wholeBeat = (int)beat;
			int beatFraction = lround((beat - wholeBeat) * 100);
			if (beatFraction == 100) {
				beatFraction = 0;
				++wholeBeat;
			}
			if (wholeBeat == measureLength) {
				wholeBeat = 0;
				++measure;
			}
			if (!isLength) {
				++measure;
				++wholeBeat;
				if (includeProjectStartOffset) {
					int size = 0;
					int index = projectconfig_var_getoffs("projmeasoffs", &size);
					assert(size == sizeof(int));
					measure += *(int*)projectconfig_var_addr(nullptr, index);
				}
			}
			if (timeFormat == TF_MEASURETICK) {
				assert(midiEditor);
				MediaItem_Take* take = MIDIEditor_GetTake(midiEditor);
				MediaItem* item = GetMediaItemTake_Item(take);
				beatFraction = beatFraction * getItemPPQ(item) / 100;
			}
			if (!useCache || measure != oldMeasure) {
				if (isLength) {
					if (includeZeros || measure != 0) {
						// Translators: Used when reporting a length of time in measures.
						// {} will be replaced with the number of measures; e.g.
						// "2 bars".
						s << format(translate_plural("{} bar", "{} bars", measure), measure) << " ";
					}
				} else {
					// Translators: Used when reporting the measure of a time position.
					// {} will be replaced with the measure number; e.g. "bar 2".
					s << format(translate("bar {}"), measure) << " ";
				}
				oldMeasure = measure;
			}
			if (!useCache || wholeBeat != oldBeat) {
				if (isLength) {
					if (includeZeros || wholeBeat != 0) {
						// Translators: Used when reporting a length of time in beats.
						// {} will be replaced with the number of beats; e.g. "2 beats".
						s << format(translate_plural("{} beat", "{} beats", wholeBeat), wholeBeat) << " ";
					}
				} else {
					// Translators: Used when reporting the beat of a time position.
					// {} will be replaced with the beat number; e.g. "beat 2".
					s << format(translate("beat {}"), wholeBeat) << " ";
				}
				oldBeat = wholeBeat;
			}
			if (!useCache || beatFraction != oldbeatFraction) {
				if (includeZeros || beatFraction != 0) {
					if (timeFormat == TF_MEASURE) {
						s << beatFraction << "%";
					} else {
						// Translators: used when reporting a time in ticks. {} will be replaced
						// with the number of ticks; e.g. "2 ticks".
						s << format(translate_plural("{} tick", "{} ticks", beatFraction), beatFraction);
					}
				}
				oldbeatFraction = beatFraction;
			}
			break;
		}
		case TF_MINSEC: {
			// Minutes:seconds
			int minute = (int)(time / 60);
			time = fmod(time, 60);
			if (!useCache || oldMinute != minute) {
				// Translators: Used when reporting a time in minutes. {} will be
				// replaced with the number of minutes; e.g. "2 min".
				s << format(translate("{} min"), minute) << " ";
				oldMinute = minute;
			}
			// Translators: Used when reporting a time in seconds. {:.3f} will be
			// replaced with the number of seconds; e.g. "2 sec".
			s << format(translate("{:#.3f} sec"), time);
			break;
		}
		case TF_SEC: {
			// Seconds
			s << format(translate("{:.3f} sec"), time);
			break;
		}
		case TF_FRAME: {
			// Frames
			int frame = (int)(time * TimeMap_curFrameRate(0, nullptr));
			if (!useCache || oldFrame != frame) {
				// Translators: Used when reporting a time in frames. {} will be
				// replaced with the number of frames; e.g. "2 frames".
				s << format(translate_plural("{} frame", "{} frames", frame), frame);
				oldFrame = frame;
			}
			break;
		}
		case TF_HMSF: {
			// Hours:minutes:seconds:frames
			int hour = (int)(time / 3600);
			time = fmod(time, 3600);
			if (!useCache || oldHour != hour) {
				// Translators: used when reporting a time in hours. {} will be replaced
				// with the number of hours; e.g. "2 hours".
				s << format(translate_plural("{} hour", "{} hours", hour), hour) << " ";
				oldHour = hour;
			}
			int minute = (int)(time / 60);
			time = fmod(time, 60);
			if (!useCache || oldMinute != minute) {
				s << format(translate("{} min"), minute) << " ";
				oldMinute = minute;
			}
			int second = (int)time;
			if (!useCache || oldSecond != second) {
				// Translators: Used when reporting a time in seconds. {} will be
				// replaced with the number of seconds; e.g. "2 sec".
				s << format(translate("{} sec"), second) << " ";
				oldSecond = second;
			}
			time = time - second;
			int frame = (int)(time * TimeMap_curFrameRate(0, NULL));
			if (!useCache || oldFrame != frame) {
				s << format(translate_plural("{} frame", "{} frames", frame), frame);
				oldFrame = frame;
			}
			break;
		}
		case TF_SAMPLE: {
			char buf[20];
			format_timestr_pos(time, buf, sizeof(buf), 4);
			// Translators: Used when reporting a time in samples. {} will be replaced
			// with the number of samples; e.g. "2 samples".
			s << format(translate("{} samples"), buf);
			break;
		}
		default:
			assert(false);
	}
	// #31: Clear cache for other units to avoid confusion if they are used later.
	resetTimeCache(timeFormat);
	return s.str();
}

void resetTimeCache(TimeFormat excludeFormat) {
	if (excludeFormat != TF_MEASURE && excludeFormat != TF_MEASURETICK) {
		oldMeasure = 0;
		oldBeat = 0;
		// Ensure percent/ticks get reported even if 0.
		// Otherwise, we would initially report nothing for a length of 0.
		oldbeatFraction = -1;
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

string formatNoteLength(double start, double end) {
	int measureLength;
	double startBeats;
	double endBeats;
	TimeMap2_timeToBeats(nullptr, start, nullptr, &measureLength, &startBeats, nullptr);
	TimeMap2_timeToBeats(NULL, end, NULL, NULL, &endBeats, NULL);
	double lengthBeats = endBeats - startBeats;
	int bars = int(lengthBeats) / measureLength;
	int remBeats = int(lengthBeats) % measureLength;
	int fraction = lround((lengthBeats - int(lengthBeats)) * 100);
	if (fraction > 99) {
		fraction = 0;
		++remBeats;
	}
	if (remBeats == measureLength) {
		remBeats = 0;
		++bars;
	}
	const bool useTicks = GetToggleCommandState2(SectionFromUniqueID(MIDI_EDITOR_SECTION), 40737);
	if (useTicks) {
		MediaItem_Take* take = MIDIEditor_GetTake(MIDIEditor_GetActive());
		MediaItem* item = GetMediaItemTake_Item(take);
		fraction = fraction * getItemPPQ(item) / 100;
	}
	ostringstream s;
	if (bars > 0) {
		s << format(translate_plural("{} bar", "{} bars", bars), bars) << " ";
	}
	if (remBeats > 0) {
		s << format(translate_plural("{} beat", "{} beats", remBeats), remBeats) << " ";
	}
	if (fraction > 0) {
		if (useTicks) {
			s << format(translate_plural("{} tick", "{} ticks", fraction), fraction);
		} else {
			s << fraction << "%";
		}
	}
	return s.str();
}

string formatCursorPosition(TimeFormat format, FormatTimeCacheRequest cache) {
	return formatTime(GetCursorPosition(), format, false, cache);
}

string formatFolderState(MediaTrack* track) {
	ostringstream s;
	int state = (int)GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH");
	if (state == 0) {
		// Translators: A track which isn't a folder.
		s << translate("track");
	} else if (state == 1 && GetMediaTrackInfo_Value(track, "P_PARTRACK")) {
		// Translators: A track which is a nested folder.
		s << translate("nested folder");
	} else if (state == 1) {
		// Translators: A track which is a folder.
		s << translate("folder");
	} else {
		// Translators: A track which ends its folder.
		s << translate("end of folder");
		// find the folder being ended by this track
		MediaTrack* folderTrack = track;
		for (int i = state; i < 0; ++i) {
			folderTrack = GetParentTrack(folderTrack);
		}
		if (!folderTrack) { // shouldn't happen
			return "";
		}
		char* folderTrackName = (char*)GetSetMediaTrackInfo(folderTrack, "P_NAME", nullptr);
		if (settings::reportTrackNumbers || !folderTrackName[0]) {
			s << " " << (int)(size_t)GetSetMediaTrackInfo(folderTrack, "IP_TRACKNUMBER", NULL);
		}
		if (folderTrackName[0]) {
			s << " " << folderTrackName;
		}
	}
	return s.str();
}

const char* getFolderCompacting(MediaTrack* track) {
	switch (*(int*)GetSetMediaTrackInfo(track, "I_FOLDERCOMPACT", NULL)) {
		case 0:
			// Translators: An open track folder.
			return translate("open");
		case 1:
			// Translators: An open (but small visually) track folder.
			return translate("small");
		case 2:
			// Translators: A closed track folder.
			return translate("closed");
	}
	return ""; // Should never happen.
}

const char* getActionName(int command, KbdSectionInfo* section, bool skipCategory) {
	const char* name = kbd_getTextFromCmd(command, section);
	if (skipCategory) {
		const char* start;
		// Skip the category before the colon (if any).
		for (start = name; *start; ++start) {
			if (*start == ':') {
				name = start + 2;
				break;
			}
		}
	}
	return name;
}

bool isTrackMuted(MediaTrack* track) {
	bool muted = false;
	GetTrackUIMute(track, &muted);
	return muted;
}

bool isTrackSoloed(MediaTrack* track) {
	if (track == GetMasterTrack(0)) {
		// Method for normal tracks doesn't seem to work for master.
		return GetMasterMuteSoloFlags() & 2;
	}
	return *(int*)GetSetMediaTrackInfo(track, "I_SOLO", NULL);
}

bool isTrackDefeatingSolo(MediaTrack* track) {
	auto defeat = (bool*)GetSetMediaTrackInfo(track, "B_SOLO_DEFEAT", nullptr);
	// This will be null in REAPER < 6.30.
	return defeat ? *defeat : false;
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

bool isPosInItem(double pos, MediaItem* item) {
	double start = GetMediaItemInfo_Value(item, "D_POSITION");
	double end = GetMediaItemInfo_Value(item, "D_POSITION") + GetMediaItemInfo_Value(item, "D_LENGTH");
	return (start <= pos && pos <= end);
}

bool isFreeItemPositioningEnabled(MediaTrack* track) {
	return *(bool*)GetSetMediaTrackInfo(track, "B_FREEMODE", nullptr);
}

const char* automationModeAsString(int mode) {
	// this works for track automation mode and global automation override.
	switch (mode) {
		case -1:
			// Translators: An automation mode.
			return translate("none");
		case 0:
			// Translators: An automation mode.
			return translate("trim/read");
		case 1:
			// Translators: An automation mode.
			return translate("read");
		case 2:
			// Translators: An automation mode.
			return translate("touch");
		case 3:
			// Translators: An automation mode.
			return translate("write");
		case 4:
			// Translators: An automation mode.
			return translate("latch");
		case 5:
			// Translators: An automation mode.
			return translate("latch preview");
		case 6:
			// Translators: An automation mode.
			return translate("bypass");
		default:
			// Translators: An automation mode OSARA doesn't know about.
			return translate("unknown");
	}
}

const char* recordingModeAsString(int mode) {
	switch (mode) { // fixme: this list is incomplete, but the other modes are currently not used by Osara.
		case 0:
			// Translators: A recording mode.
			return translate("input");
		case 1:
			// Translators: A recording mode.
			return translate("output (stereo)");
		case 2:
			// Translators: A recording mode.
			return translate("disabled");
		case 3:
			// Translators: A recording mode.
			return translate("output (stereo, latency compensated)");
		case 4:
			// Translators: A recording mode.
			return translate("output (midi)");
		case 5:
			// Translators: A recording mode.
			return translate("output (mono)");
		case 6:
			// Translators: A recording mode.
			return translate("output (mono, latency compensated)");
		case 7:
			// Translators: A recording mode.
			return translate("midi overdub");
		case 8:
			// Translators: A recording mode.
			return translate("midi replace");
		case 9:
			// Translators: A recording mode.
			return translate("midi touch-replace");
		case 16:
			// Translators: A recording mode.
			return translate("midi latch-replace");
		default:
			// Translators: A recording mode OSARA doesn't know about.
			return translate("unknown");
	}
}

bool isTrackInClosedFolder(MediaTrack* track) {
	MediaTrack* parent = (MediaTrack*)GetSetMediaTrackInfo(track, "P_PARTRACK", nullptr);
	// Folders can be nested, so we need to check all ancestor tracks, not just
	// the parent.
	while (parent) {
		if (*(int*)GetSetMediaTrackInfo(parent, "I_FOLDERDEPTH", NULL) == 1 &&
				*(int*)GetSetMediaTrackInfo(parent, "I_FOLDERCOMPACT", NULL) == 2) {
			return true;
		}
		parent = (MediaTrack*)GetSetMediaTrackInfo(parent, "P_PARTRACK", nullptr);
	}
	return false;
}

MediaItem* getItemWithFocus() {
	// try to provide information based on the last item spoken by osara if it is selected
	if (currentItem && ValidatePtr((void*)currentItem, "MediaItem*") && IsMediaItemSelected(currentItem))
		return currentItem;
	if (CountSelectedMediaItems(0) > 0)
		return GetSelectedMediaItem(0, 0);
	return nullptr;
}

bool shouldReportTimeMovement() {
	if (settings::reportTimeMovementWhilePlaying) {
		return true;
	}
	// Don't report if playing.
	return !(GetPlayState() & 1);
}

INT_PTR CALLBACK reviewMessage_dialogProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL) {
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

void reviewMessage(const char* title, const char* message) {
	HWND dialog = CreateDialog(
			pluginHInstance, MAKEINTRESOURCE(ID_MESSAGE_REVIEW_DLG), GetForegroundWindow(), reviewMessage_dialogProc
	);
	SetWindowText(dialog, title);
	SetDlgItemText(dialog, ID_MSGREV_TEXT, message);
	ShowWindow(dialog, SW_SHOWNORMAL);
}

unsigned int getConfigUndoMask() {
	int size{0};
	unsigned int* undomask = static_cast<unsigned int*>(get_config_var("undomask", &size));
	if (!undomask || (size != sizeof(unsigned int)))
		return 0;
	return *undomask;
}

struct {
	const char* displayName;
	const char* name;
} TRACK_GROUP_TOGGLES[] = {
		{_t("volume lead"), "VOLUME_LEAD"},
		{_t("volume follow"), "VOLUME_FOLLOW"},
		{_t("VCA lead"), "VOLUME_VCA_LEAD"},
		{_t("VCA follow"), "VOLUME_VCA_FOLLOW"},
		{_t("pan lead"), "PAN_LEAD"},
		{_t("pan follow"), "PAN_FOLLOW"},
		{_t("width lead"), "WIDTH_LEAD"},
		{_t("width follow"), "WIDTH_FOLLOW"},
		{_t("mute lead"), "MUTE_LEAD"},
		{_t("mute follow"), "MUTE_FOLLOW"},
		{_t("solo lead"), "SOLO_LEAD"},
		{_t("solo follow"), "SOLO_FOLLOW"},
		{_t("record arm lead"), "RECARM_LEAD"},
		{_t("record arm follow"), "RECARM_FOLLOW"},
		{_t("polarity lead"), "POLARITY_LEAD"},
		{_t("polarity follow"), "POLARITY_FOLLOW"},
		{_t("automation mode lead"), "AUTOMODE_LEAD"},
		{_t("automation mode follow"), "AUTOMODE_FOLLOW"},
		{_t("reverse volume"), "VOLUME_REVERSE"},
		{_t("reverse pan"), "PAN_REVERSE"},
		{_t("reverse width"), "WIDTH_REVERSE"},
		{_t("do not lead when following"), "NO_LEAD_WHEN_FOLLOW"},
		{_t("VCA pre-FX follow"), "VOLUME_VCA_FOLLOW_ISPREFX"},
};

bool isTrackGrouped(MediaTrack* track) {
	for (auto& toggle : TRACK_GROUP_TOGGLES) {
		if (GetSetTrackGroupMembership(track, toggle.name, 0, 0) ||
				GetSetTrackGroupMembershipHigh(track, toggle.name, 0, 0)) {
			return true;
		}
	}
	return false;
}

// Format a double d to precision decimal places, stripping trailing zeroes.
// If plus is true, a "+" prefix will be included for a positive number.
string formatDouble(double d, int precision, bool plus) {
	string s = format(plus ? "{:+.{}f}" : "{:.{}f}", d, precision);
	size_t pos = s.find_last_not_of("0");
	if (s[pos] == '.') {
		// also strip the trailing decimal point
		pos -= 1;
	}
	auto stripped = s.substr(0, pos + 1);
	if (stripped == "+0" || stripped == "-0") {
		return "0";
	}
	return stripped;
}

// Functions exported from SWS
const char* (*NF_GetSWSTrackNotes)(MediaTrack* track) = nullptr;

/*** Code to execute after existing actions.
 * This is used to report messages regarding the effect of the command, etc.
 */

bool shouldMoveToAutoItem = false;

void postGoToTrack(int command, MediaTrack* track) {
	fakeFocus = FOCUS_TRACK;
	selectedEnvelopeIsTake = false;
	shouldMoveToAutoItem = false;
	SetCursorContext(0, NULL);
	if (!track)
		return;
	ostringstream s;
	auto separate = [&s]() {
		if (s.tellp() > 0) {
			s << " ";
		}
	};
	int trackNum = (int)(size_t)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", NULL);
	if (trackNum <= 0) {
		// Translators: Reported when navigating to the master track.
		s << translate("master");
	} else if (settings::reportTrackNumbers) {
		s << trackNum;
	}
	if (isTrackSelected(track)) {
		// One selected track is the norm, so don't report selected in this case.
		if (CountSelectedTracks(0) > 1) {
			separate();
			s << translate("selected");
		}
	} else {
		separate();
		s << translate("unselected");
	}
	const bool armed = isTrackArmed(track);
	auto pAutoArm = (bool*)GetSetMediaTrackInfo(track, "B_AUTO_RECARM", nullptr);
	// This will be null in REAPER < 6.30.
	const bool autoArm = pAutoArm ? *pAutoArm : false;
	// If auto armed, don't report this before the track name.
	if (armed && !autoArm) {
		separate();
		s << translate("armed");
	}
	if (isTrackMuted(track)) {
		separate();
		s << translate("muted");
	}
	if (isTrackSoloed(track)) {
		separate();
		s << translate("soloed");
	}
	if (isTrackDefeatingSolo(track)) {
		separate();
		s << translate("defeating solo");
	}
	if (isTrackPhaseInverted(track)) {
		separate();
		s << translate("phase inverted");
	}
	if (isTrackFxBypassed(track)) {
		separate();
		s << translate("FX bypassed");
	}
	if (trackNum > 0) { // Not master
		int folderDepth = (int)GetMediaTrackInfo_Value(track, "I_FOLDERDEPTH");
		if (folderDepth == 1) { // Folder
			separate();
			s << getFolderCompacting(track);
			separate();
			s << formatFolderState(track);
		}
		separate();
		char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
		if (trackName && trackName[0]) {
			s << trackName;
		} else if (!settings::reportTrackNumbers) {
			// There's no name and track number reporting is disabled. We report the
			// number in lieu of the name.
			s << trackNum;
		}
		if (folderDepth < 0) { // end of folder
			separate();
			s << formatFolderState(track);
		}
		if (armed && autoArm) {
			separate();
			s << translate("armed");
		}
	}
	if (isTrackGrouped(track)) {
		// Translators: Reported when navigating to a track which is grouped.
		s << " " << translate("grouped");
	}
	if (NF_GetSWSTrackNotes && NF_GetSWSTrackNotes(track)[0]) {
		// Translators: Reported when navigating to a track which has track notes.
		s << " " << translate("notes");
	}
	if (trackNum > 0) { // Not master
		int itemCount = CountTrackMediaItems(track);
		// Translators: Reported when navigating tracks to indicate how many items
		// the track has. {} will be replaced by the number of items; e.g.
		// "2 items".
		if (itemCount > 0) {
			s << " " << format(translate_plural("{} item", "{} items", itemCount), itemCount);
		}
		if (isFreeItemPositioningEnabled(track)) {
			s << " " << translate("free item positioning");
		}
	}
	int count;
	if (settings::reportFx && (count = TrackFX_GetCount(track)) > 0) {
		// Translators: Reported when navigating tracks before listing the effects on
		// the track.
		s << "; " << translate("FX:") << " ";
		char name[256];
		for (int f = 0; f < count; ++f) {
			if (f > 0)
				s << ", ";
			TrackFX_GetFXName(track, f, name, sizeof(name));
			shortenFxName(name, s);
			if (!TrackFX_GetEnabled(track, f)) {
				s << " " << translate("bypassed");
			}
			int deltaParam = TrackFX_GetParamFromIdent(track, f, ":delta");
			if (deltaParam != -1 && TrackFX_GetParam(track, f, deltaParam, nullptr, nullptr)) {
				s << " " << translate("delta");
			}
		}
	}
	outputMessage(s);
	if (command) {
		// This command replaces the selection , so revert to contiguous selection.
		isSelectionContiguous = true;
	}
}

void postGoToTrack(int command) {
	postGoToTrack(command, GetLastTouchedTrack());
}

void postToggleTrackMute(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(isTrackMuted(track) ? translate("muted") : translate("unmuted"));
}

void postToggleTrackSolo(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(isTrackSoloed(track) ? translate("soloed") : translate("unsoloed"));
}

void postToggleTrackArm(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(isTrackArmed(track) ? translate("armed") : translate("unarmed"));
}

void postCycleTrackMonitor(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	switch (*(int*)GetSetMediaTrackInfo(track, "I_RECMON", NULL)) {
		case 0:
			outputMessage(translate("record monitor off"));
			break;
		case 1:
			// Translators: Record monitor set to normal.
			outputMessage(translate_ctxt("record monitor", "normal"));
			break;
		case 2:
			// Translators: Record monitor set to not when playing.
			outputMessage(translate("not when playing"));
	}
}

void postInvertTrackPhase(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(isTrackPhaseInverted(track) ? translate("phase inverted") : translate("phase normal"));
}

void postToggleTrackFxBypass(MediaTrack* track) {
	outputMessage(isTrackFxBypassed(track) ? translate("FX bypassed") : translate("FX active"));
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

void postToggleMasterTrackFxBypass(int command) {
	postToggleTrackFxBypass(GetMasterTrack(0));
}

void postToggleAllTracksFxBypass(int command) {
	int count = CountTracks(nullptr);
	if (count == 0) {
		return;
	}
	bool bypassed = false;
	for (int t = 0; t < count; ++t) {
		MediaTrack* track = GetTrack(nullptr, t);
		if (isTrackFxBypassed(track)) {
			bypassed = true;
			break;
		}
	}
	outputMessage(bypassed ? translate("all tracks FX bypassed") : translate("all tracks FX active"));
}

void postToggleLastFocusedFxDeltaSolo(int command) {
	outputMessage(GetToggleCommandState(command) ? translate("enabled delta solo") : translate("disabled delta solo"));
}

void postCursorMovement(int command) {
	fakeFocus = FOCUS_RULER;
	if (shouldReportTimeMovement()) {
		outputMessage(formatCursorPosition().c_str());
	}
}

void postCursorMovementScrub(int command) {
	if (settings::reportScrub)
		postCursorMovement(command);
	else
		fakeFocus = FOCUS_RULER; // Set this even if we aren't reporting.
}

void postItemNormalize(int command) {
	int selectedItemsCount = CountSelectedMediaItems(0);
	if (selectedItemsCount == 0) {
		outputMessage(translate("no selected items"));
		return;
	}
	if (command == 40254) {
		// Item properties: Normalize multiple items to common gain
		// Translators: {} will be replaced with the number of items; e.g.
		// "2 items normalized to common gain".
		outputMessage(format(
				translate_plural("{} item normalized to common gain", "{} items normalized to common gain", selectedItemsCount),
				selectedItemsCount
		));
	} else {
		// Translators: {} will be replaced with the number of items; e.g.
		// "2 items normalized".
		outputMessage(
				format(translate_plural("{} item normalized", "{} items normalized", selectedItemsCount), selectedItemsCount)
		);
	}
}

void postCycleTrackFolderState(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	outputMessage(formatFolderState(track));
}

void postCycleTrackFolderCollapsed(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	if (*(int*)GetSetMediaTrackInfo(track, "I_FOLDERDEPTH", nullptr) != 1) {
		outputMessage(translate("not a folder"));
		return;
	}
	outputMessage(getFolderCompacting(track));
}

int findRegionEndingAt(double wantedEndPos) {
	double start = wantedEndPos;
	for (;;) {
		if (start == 0) {
			return -1;
		}
		// GetLastMarkerAndCurRegion doesn't return a region at its end position,
		// so subtract a bit.
		double tempPos = max(start - 0.001, 0);
		int region;
		GetLastMarkerAndCurRegion(nullptr, tempPos, nullptr, &region);
		if (region < 0) {
			return -1;
		}
		double end;
		EnumProjectMarkers(region, nullptr, &start, &end, nullptr, nullptr);
		if (end == wantedEndPos) {
			return region;
		}
		// If there are overlapping regions, GetLastMarkerAndCurRegion will return
		// the region which starts nearest to the given position. There might be a
		// region which starts earlier but ends earlier. The next iteration will
		// try the region just prior to this region's start position.
	}
	return -1;
}

void postGoToMarker(int command) {
	ostringstream s;
	int marker, region;
	double markerPos;
	double cursorPos = GetCursorPosition();
	GetLastMarkerAndCurRegion(nullptr, cursorPos, &marker, &region);
	const char* name;
	int number;
	if (marker >= 0) {
		EnumProjectMarkers(marker, NULL, &markerPos, NULL, &name, &number);
		if (markerPos == cursorPos) {
			fakeFocus = FOCUS_MARKER;
			if (name[0]) {
				// Translators: Reported when moving to a named project marker. {} will
				// be replaced with the marker's name; e.g. "intro marker".
				s << format(translate("{} marker"), name) << " ";
			} else {
				// Translators: Reported when moving to an unnamed project marker. {}
				// will be replaced with the marker's name; e.g. "marker 2".
				s << format(translate("marker {}"), number) << " ";
			}
		}
	}
	double start, end;
	if (region >= 0) {
		EnumProjectMarkers(region, nullptr, &start, &end, &name, &number);
		if (start == cursorPos) {
			fakeFocus = FOCUS_REGION;
			if (name[0]) {
				// Translators: Reported when moving to the start of a named region. {}
				// will be replaced with the region's name; e.g. "intro region start".
				s << format(translate("{} region start"), name) << " ";
			} else {
				// Translators: Reported when moving to the start of an unnamed region.
				// {} will be replaced with the region's number; e.g.
				// "region 2 start".
				s << format(translate("region {} start"), number) << " ";
			}
		}
	}
	region = findRegionEndingAt(cursorPos);
	if (region >= 0) {
		EnumProjectMarkers(region, nullptr, nullptr, nullptr, &name, &number);
		fakeFocus = FOCUS_REGION;
		if (name[0]) {
			// Translators: Reported when moving to the end of a named region. {}
			// will be replaced with the region's name; e.g. "intro region end".
			s << format(translate("{} region end"), name) << " ";
		} else {
			// Translators: Reported when moving to the end of an unnamed region.
			// {} will be replaced with the region's number; e.g.
			// "region 2 end".
			s << format(translate("region {} end"), number) << " ";
		}
	}
	GetSet_LoopTimeRange(false, false, &start, &end, false);
	if (start != end) {
		if (cursorPos == start) {
			// Translators: Reported when moving by marker and the cursor lands at the
			// start of the time selection.
			s << translate("selection start") << " ";
		}
		if (cursorPos == end) {
			// Translators: Reported when moving by marker and the cursor lands at the
			// end of the time selection.
			s << translate("selection end") << " ";
		}
	}
	GetSet_LoopTimeRange(false, true, &start, &end, false);
	if (start != end) {
		if (cursorPos == start) {
			// Translators: Reported when moving by marker and the cursor lands at the
			// loop start point.
			s << translate("loop start") << " ";
		}
		if (cursorPos == end) {
			// Translators: Reported when moving by marker and the cursor lands at the
			// loop end point.
			s << translate("loop end") << " ";
		}
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
		if (name[0]) {
			if (reg) {
				// Translators: used when reporting a named region. {} will be
				// replaced with the name of the region; e.g. "intro region"
				s << format(translate("{} region"), name);
			} else {
				// Translators: used when reporting a named marker. {} will be
				// replaced with the name of the marker; e.g. "v2 marker"
				s << format(translate("{} marker"), name);
			}
		} else { // unnamed
			if (reg) {
				// Translators: used to report an unnamed region. {} is replaced with the region number.
				s << format(translate("region {}"), num);
			} else {
				// Translators: used to report an unnamed marker. {} is replaced with the marker number.
				s << format(translate("marker {}"), num);
			}
		}
		s << " " << formatCursorPosition();
		outputMessage(s);
		return;
	}
}

void postChangeVolumeH(double volume, int command, const char* commandMessage) {
	ostringstream s;
	if (lastCommand != command)
		s << commandMessage << " ";
	s << formatDouble(VAL2DB(volume), 2, true);
	outputMessage(s);
}

void postChangeTrackVolume(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	double volume = 0.0;
	if (!GetTrackUIVolPan(track, &volume, NULL))
		return;
	postChangeVolumeH(volume, command, translate("Track"));
}

void postChangeMasterTrackVolume(int command) {
	MediaTrack* track = GetMasterTrack(0);
	double volume = 0.0;
	if (!GetTrackUIVolPan(track, &volume, NULL))
		return;
	postChangeVolumeH(volume, command, translate("Master"));
}

void postChangeItemVolume(int command) {
	MediaItem* item = getItemWithFocus();
	if (!item)
		return;
	double volume = GetMediaItemInfo_Value(item, "D_VOL");
	postChangeVolumeH(volume, command, translate("Item"));
}

void postChangeTakeVolume(int command) {
	MediaItem* item = getItemWithFocus();
	if (!item)
		return;
	MediaItem_Take* take = GetActiveTake(item);
	if (!take) {
		return;
	}
	double volume = GetMediaItemTakeInfo_Value(take, "D_VOL");
	volume = fabs(volume); // volume is negative if take polarity is flipped
	postChangeVolumeH(volume, command, translate("Take"));
}

void postChangeHorizontalZoom(int command) {
	double hZoom = GetHZoomLevel();
	// Translators: Reported when zooming in or out horizontally. {} will be
	// replaced with the number of pixels per second; e.g. 100 pixels/second.
	outputMessage(format(translate("{} pixels/second"), formatDouble(hZoom, 1)));
}

void formatPan(double pan, ostringstream& output) {
	pan *= 100.0;
	if (pan == 0) {
		// Translators: Panned to the center.
		output << translate("center");
	} else if (pan < 0) {
		// Translators: Panned to the left. {:g} will be replaced with the amount;
		// e.g. "20% left".
		output << format(translate("{:g}% left"), -pan);
	} else {
		// Translators: Panned to the right. {:g} will be replaced with the amount;
		// e.g. "20% right".
		output << format(translate("{:g}% right"), pan);
	}
}

void postChangeTrackPan(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	double pan = 0.0;
	if (!GetTrackUIVolPan(track, NULL, &pan))
		return;
	ostringstream s;
	formatPan(pan, s);
	outputMessage(s);
}

void postCycleRippleMode(int command) {
	if (GetToggleCommandState(40310)) {
		outputMessage(translate("ripple per-track"));
	} else if (GetToggleCommandState(40311)) {
		outputMessage(translate("ripple all tracks"));
	} else {
		outputMessage(translate("ripple off"));
	}
}

void reportRepeat(bool repeat) {
	outputMessage(repeat ? translate("repeat on") : translate("repeat off"));
}

void postToggleRepeat(int command) {
	reportRepeat(GetToggleCommandState(1068)); // Transport: Toggle repeat
}

void addTakeFxNames(MediaItem_Take* take, ostringstream& s) {
	if (!settings::reportFx)
		return;
	int count = TakeFX_GetCount(take);
	if (count == 0)
		return;
	// Translators: Reported when switching takes before listing the effects on
	// the take.
	s << "; " << translate("FX:") << " ";
	char name[256];
	for (int f = 0; f < count; ++f) {
		if (f > 0)
			s << ", ";
		TakeFX_GetFXName(take, f, name, sizeof(name));
		shortenFxName(name, s);
	}
}

void postSwitchToTake(int command) {
	MediaItem* item = GetSelectedMediaItem(0, 0);
	if (!item)
		return;
	MediaItem_Take* take = GetActiveTake(item);
	if (!take) {
		if (CountTakes(item) == 0) {
			outputMessage(translate("no takes"));
		} else {
			outputMessage(translate("empty take lane"));
		}
		return;
	}
	ostringstream s;
	s << (int)(size_t)GetSetMediaItemTakeInfo(take, "IP_TAKENUMBER", NULL) + 1 << " " << GetTakeName(take);
	addTakeFxNames(take, s);
	outputMessage(s);
}

void postCopy(int command) {
	int count;
	switch (GetCursorContext2(true)) {
		case 0: // Track
			if ((count = CountSelectedTracks(0)) > 0) {
				// Translators: Reported when copying tracks. {} will be replaced with
				// the number of tracks; e.g. "2 tracks copied".
				outputMessage(format(translate_plural("{} track copied", "{} tracks copied", count), count));
			}
			return;
		case 1: // Item
			if ((count = CountSelectedMediaItems(0)) > 0) {
				// Translators: Reported when copying items. {} will be replaced with
				// the number of items; e.g. "2 items copied".
				outputMessage(format(translate_plural("{} item copied", "{} items copied", count), count));
			}
			return;
		case 2: // Envelope
			reportCopiedEnvelopePointsOrAutoItems();
			return;
		default:
			return;
	}
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
	// Translators: Reported when moving to a tempo change. {} will be replaced
	// with the tempo in bpm; e.g. "tempo 100".
	s << format(translate("tempo {}"), bpm);
	if (sigNum > 0) {
		// Translators: Reported when moving to a time signature change. {num} will
		// be replaced with the time signature numerator. {denom} will be replaced
		// with the time signature denominator. For example: "time sig 6/8".
		s << " " << format(translate("time sig {num}/{denom}"), "num"_a = sigNum, "denom"_a = sigDenom);
	}
	s << " " << formatCursorPosition();
	outputMessage(s);
}

int getStretchAtPos(MediaItem_Take* take, double pos, double itemStart, double playRate) {
	// Stretch marker positions are relative to the start of the item and the
	// take's play rate.
	double posRel = (pos - itemStart) * playRate;
	const double STRETCH_FUZZ_FACTOR = 0.00003; // approximately 1 sample at 48000.
	double posRelAdj = posRel - STRETCH_FUZZ_FACTOR;
	if (posRelAdj < 0)
		return -1;
	int index = GetTakeStretchMarker(take, -1, &posRelAdj, NULL);
	if (index < 0)
		return -1;
	double stretchRel;
	// Get the real position; pos wasn't written.
	GetTakeStretchMarker(take, index, &stretchRel, NULL);
	if (abs(stretchRel - posRel) > STRETCH_FUZZ_FACTOR)
		return -1; // Marker not near to  pos.
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
		double itemStart = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
		double playRate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
		if (getStretchAtPos(take, cursor, itemStart, playRate) != -1) {
			found = true;
			break;
		}
	}
	ostringstream s;
	if (found) {
		fakeFocus = FOCUS_STRETCH;
		lastStretchPos = cursor;
		// Translators: Reported when moving to a stretch marker.
		s << translate("stretch marker") << " ";
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

void cmdIoMaster(Command* command);

void postTrackIo(int command) {
	if (GetLastTouchedTrack() == GetMasterTrack(0)) {
		// Make this work for the master track. It doesn't out of the box.
		cmdIoMaster(NULL);
	}
}

void postToggleMetronome(int command) {
	outputMessage(GetToggleCommandState(command) ? translate("metronome on") : translate("metronome off"));
}

void postToggleMasterTrackVisible(int command) {
	outputMessage(GetToggleCommandState(command) ? translate("master track visible") : translate("master track hidden"));
}

void reportTransportState(int state) {
	if (!settings::reportTransport)
		return;
	if (state & 2) {
		outputMessage(translate("pause"));
	} else if (state & 4) {
		outputMessage(translate("record"));
	} else if (state & 1) {
		outputMessage(translate("play"));
	} else {
		outputMessage(translate("stop"));
	}
}

void postChangeTransportState(int command) {
	reportTransportState(GetPlayState());
}

void postSelectMultipleItems(int command) {
	int count = CountSelectedMediaItems(0);
	// Translators: Reported when items are selected. {} will be replaced with
	// the number of items; e.g. "2 items selected".
	outputMessage(format(translate_plural("{} item selected", "{} items selected", count), count));
	// Items have just been selected, so the user almost certainly wants to operate on items.
	fakeFocus = FOCUS_ITEM;
	selectedEnvelopeIsTake = true;
	SetCursorContext(1, NULL);
}

void postRenameTrack(int command) {
	if (!GetLastTouchedTrack())
		return;
	// #82: On Windows, this will end up as the label of the track name text box.
	// Translators: Reported when prompting for the name of a track.
	outputMessage(translate("Track name"));
}

bool isItemMuted(MediaItem* item) {
	return *(bool*)GetSetMediaItemInfo(item, "B_MUTE", NULL);
}

void postToggleItemMute(int command) {
	int muteCount = 0;
	int unmuteCount = 0;
	int count = CountSelectedMediaItems(0);
	if (count == 0)
		return;
	if (count == 1) {
		outputMessage(isItemMuted(GetSelectedMediaItem(0, 0)) ? translate("muted") : translate("unmuted"));
		return;
	}
	for (int i = 0; i < count; ++i) {
		if (isItemMuted(GetSelectedMediaItem(0, i)))
			++muteCount;
		else
			++unmuteCount;
	}
	ostringstream s;
	if (muteCount > 0) {
		// Translators: Reported when multiple items are muted. {} will be replaced
		// with the number of items; e.g. "2 items muted".
		s << format(translate_plural("{} item muted", "{} items muted", muteCount), muteCount);
		if (unmuteCount > 0) {
			s << ", ";
		}
	}
	if (unmuteCount > 0) {
		// Translators: Reported when multiple items are unmuted. {} will be
		// replaced with the number of items; e.g. "2 items unmuted".
		s << format(translate_plural("{} item unmuted", "{} items unmuted", unmuteCount), unmuteCount);
	}
	outputMessage(s);
}

void postToggleItemSolo(int command) {
	bool soloed = true;
	int itemCount = CountMediaItems(0);
	int selectedCount = CountSelectedMediaItems(0);
	if (selectedCount == 0)
		return;
	for (int i = 0; i < itemCount; ++i) {
		MediaItem* item = GetMediaItem(0, i);
		if ((!isItemSelected(item)) && (!isItemMuted(item))) {
			soloed = false;
			break;
		}
	}
	if (selectedCount == 1) {
		outputMessage(soloed ? translate("soloed") : translate("unsoloed"));
		return;
	}
	if (soloed) {
		// Translators: Reported when multiple items are soloed. {} will be replaced
		// with the number of items; e.g. "2 items soloed".
		outputMessage(format(translate_plural("{} item soloed", "{} items soloed", selectedCount), selectedCount));
	} else {
		// Translators: Reported when multiple items are unsoloed. {} will be
		// replaced with the number of items; e.g. "2 items unsoloed".
		outputMessage(format(translate_plural("{} item unsoloed", "{} items unsoloed", selectedCount), selectedCount));
	}
}

void postSetSelectionEnd(int command) {
	outputMessage(translate("set selection end"));
	fakeFocus = FOCUS_RULER;
}

void postToggleMasterMono(int command) {
	outputMessage(GetToggleCommandState(command) ? translate("master mono") : translate("master stereo"));
}

void postToggleAutoCrossfade(int command) {
	outputMessage(GetToggleCommandState(command) ? translate("crossfade on") : translate("crossfade off"));
}

void postToggleLocking(int command) {
	outputMessage(GetToggleCommandState(command) ? translate("locking on") : translate("locking off"));
}

void postToggleSoloInFront(int command) {
	outputMessage(
			GetToggleCommandState(command) ? translate("solo in front") :
																		 // Translators: Solo in front was turned off.
					translate("normal solo")
	);
}

void postAdjustPlayRate(int command) {
	double rate = Master_GetPlayRate(nullptr);
	// Translators: Reported when the play rate is adjusted. {} will be replaced
	// with the play rate; e.g. "1.5 play rate".
	outputMessage(format(translate("{} play rate"), formatDouble(rate, 6)));
}

void postToggleMonitoringFxBypass(int command) {
	outputMessage(GetToggleCommandState(command) ? translate("FX bypassed") : translate("fx active"));
}

void postCycleRecordMode(int command) {
	if (GetToggleCommandState(40252)) {
		// Translators: Record mode set to normal.
		outputMessage(translate("normal record"));
	} else if (GetToggleCommandState(40253)) {
		outputMessage(translate("selected item auto-punch"));
	} else if (GetToggleCommandState(40076)) {
		outputMessage(translate("time selection auto-punch"));
	}
}

void postChangeGlobalAutomationOverride(int command) {
	ostringstream s;
	// Translators: When changing the global automation override, reported prior
	// to the chosen automation mode.
	s << translate("override") << " ";
	s << automationModeAsString(GetGlobalAutomationOverride());
	outputMessage(s);
}

void postReverseTake(int command) {
	int count = CountSelectedMediaItems(0);
	if (count == 0) {
		outputMessage(translate("no items selected"));
		return;
	}
	// Translators: Reported when reversing takes. {} will be replaced by the
	// number of takes; e.g. "2 takes reversed".
	outputMessage(format(translate_plural("{} take reversed", "{} takes reversed", count), count));
}

void postTogglePreRoll(int command) {
	outputMessage(
			GetToggleCommandState(command) ? translate("enabled pre roll before recording")
																		 : translate("disabled pre roll before recording")
	);
}

void postToggleCountIn(int command) {
	outputMessage(
			GetToggleCommandState(command) ? translate("enabled count in before recording")
																		 : translate("disabled count in before recording")
	);
}

void postTakeChannelMode(int command) {
	int count = CountSelectedMediaItems(0);
	if (count == 0) {
		outputMessage(translate("no items selected"));
		return;
	}
	const char* mode;
	switch (command) {
		case 40176: {
			// Translators: A take channel mode.
			mode = translate_ctxt("take channel mode", "normal");
			break;
		}
		case 40179: {
			// Translators: A take channel mode.
			mode = translate("mono (left)");
			break;
		}
		case 40178: {
			// Translators: A take channel mode.
			mode = translate("mono (downmix)");
			break;
		}
		case 40180: {
			// Translators: A take channel mode.
			mode = translate("mono (right)");
			break;
		}
		default: {
			// Translators: A take channel mode OSARA doesn't know about.
			mode = translate("unknown mode");
		}
	}
	// Translators: Reported when setting the channel mode of takes.
	// {count} will be replaced with the number of takes affected.
	// {mode} will be replaced with the mode being set.
	// For example: "set 2 takes to mono (left)"
	outputMessage(format(
			translate_plural("set {count} take to {mode}", "set {count} takes to {mode}", count), "count"_a = count,
			"mode"_a = mode
	));
}

void postChangeTempo(int command) {
	double tempo = Master_GetTempo();
	// Translators: Reported when changing the tempo. {} will be replaced with
	// the new tempo; e.g. "50 bpm".
	outputMessage(format(translate("{} bpm"), tempo));
}

void postTogglePlaybackPositionFollowsTimebase(int command) {
	outputMessage(
			GetToggleCommandState(command)
					? translate("enabled playback position follows project timebase when changing tempo")
					: translate("disabled playback position follows project timebase when changing tempo")
	);
}

void postTogglePreservePitchWhenPlayRateChanged(int command) {
	outputMessage(
			GetToggleCommandState(command) ? translate("preserving pitch when changing play rate")
																		 : translate("not preserving pitch when changing play rate")
	);
}

void postSetItemEnd(int command) {
	MediaItem* item = getItemWithFocus();
	if (!item)
		return;
	int selCount = CountSelectedMediaItems(0);
	if (selCount > 1) {
		// Translators: Reported when setting the ends of multiple items to the end
		// of their source media. {} will be replaced with the number of items; e.g.
		// "2 item ends set to source media end"
		outputMessage(format(translate("{} item ends set to source media end"), selCount));
	} else {
		double endPos = GetMediaItemInfo_Value(item, "D_POSITION") + GetMediaItemInfo_Value(item, "D_LENGTH");
		// Translators: Reported when setting the end of a single item to its source
		// media end. {} will be replaced with the end time; e.g.
		// "item end set to source media end: bar 3 beat 1 25%"
		outputMessage(format(
				translate("item end set to source media end: {}"), formatTime(endPos, TF_RULER, false, FT_NO_CACHE, true)
		));
	}
}

void postGoToTakeMarker(int command) {
	int itemCount = CountSelectedMediaItems(0);
	if (itemCount == 0) {
		return;
	}
	double cursor = GetCursorPosition();
	ostringstream s;
	for (int i = 0; i < itemCount; ++i) {
		MediaItem* item = GetSelectedMediaItem(0, i);
		MediaItem_Take* take = GetActiveTake(item);
		if (!take) {
			continue;
		}
		double itemStart = *(double*)GetSetMediaItemInfo(item, "D_POSITION", NULL);
		double playRate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
		// Take marker positions are relative to the start of the item and the
		// take's play rate.
		double cursorRel = (cursor - itemStart) * playRate;
		int markerCount = GetNumTakeMarkers(take);
		for (int m = 0; m < markerCount; ++m) {
			char name[100];
			double markerPos = GetTakeMarker(take, m, name, sizeof(name), nullptr);
			if (markerPos == cursorRel) {
				// Translators: Reported when moving to a take marker. {} will be
				// replaced with the name of the marker; e.g. "fix take marker".
				s << format(translate("{} take marker"), name) << " ";
				fakeFocus = FOCUS_TAKEMARKER;
			}
		}
	}
	s << formatCursorPosition();
	outputMessage(s);
	if (GetPlayPosition() != cursor) {
		SetEditCurPos(cursor, true, true); // Seek playback.
	}
}

void postSelectMultipleTracks(int command) {
	int count = CountSelectedTracks(nullptr);
	// Translators: Reported when an action selects tracks. {} will be replaced
	// with the number of tracks; e.g. "2 tracks selected".
	outputMessage(format(translate_plural("{} track selected", "{} tracks selected", count), count));
}

void postSelectAll(int command) {
	switch (GetCursorContext2(true)) {
		case 0: // Track
			postSelectMultipleTracks(0);
			return;
		case 1: // Item
			postSelectMultipleItems(0);
			return;
		case 2: // Envelope
			postSelectMultipleEnvelopePoints(0);
			return;
		default:
			return;
	}
}

void postToggleTrackSoloDefeat(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track) {
		return;
	}
	// We don't use isTrackDefeatingSolo() because it returns false even if
	// this is REAPER < 6.30. We want to report nothing if this REAPER is too old
	// to support this.
	auto defeat = (bool*)GetSetMediaTrackInfo(track, "B_SOLO_DEFEAT", nullptr);
	if (!defeat) {
		return;
	}
	outputMessage(*defeat ? translate("defeating solo") : translate("not defeating solo"));
}

void postChangeTransientDetectionSensitivity(int command) {
	double sensitivity = *(double*)get_config_var("transientsensitivity", nullptr) * 100;
	// Translators: report transient sensitivity. {:g} is replaced with the sensitivity percentage;
	// E.g. "13% sensitivity"
	outputMessage(format(translate("{:g}% sensitivity"), sensitivity));
}

void postChangeTransientDetectionThreshold(int command) {
	double threshold = *(double*)get_config_var("transientthreshold", nullptr);
	// Translators: Reported when changing the transient detection threshold.
	// {:g} will be replaced with the threshold; e.g. "{} dB threshold".
	outputMessage(format(translate("{:g} dB threshold"), threshold));
}

void postToggleEnvelopePointsMoveWithMediaItems(int command) {
	outputMessage(
			GetToggleCommandState(command) ? translate("enabled envelope points move with media items")
																		 : translate("disabled envelope points move with media items")
	);
}

void postToggleFreeItemPositioning(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track) {
		return;
	}
	outputMessage(
			*(bool*)GetSetMediaTrackInfo(track, "B_FREEMODE", nullptr)
					? translate("enabled free item positioning")
					: translate("disabled free item positioning")
	);
}

void postChangeItemRate(int command) {
	MediaItem* item = getItemWithFocus();
	if (!item) {
		return;
	}
	MediaItem_Take* take = GetActiveTake(item);
	if (!take) {
		return;
	}
	double rate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
	// Translators: Used when changing item rate. {} is replaced by the new rate. E.G. "1.0 item rate"
	outputMessage(format(translate("{} item rate"), formatDouble(rate, 6)));
}

void postChangeItemPitch(int command) {
	MediaItem* item = getItemWithFocus();
	if (!item) {
		return;
	}
	MediaItem_Take* take = GetActiveTake(item);
	if (!take) {
		return;
	}
	double pitch = GetMediaItemTakeInfo_Value(take, "D_PITCH");
	// Translators: Used when changing item PITCH. {} is replaced by the new PITCH. E.G. "-1.0 SEMITONES"
	outputMessage(format(translate("{} semitones"), formatDouble(pitch, 6, true)));
}

void postToggleTakePreservePitch(int command) {
	MediaItem* item = getItemWithFocus();
	if (!item) {
		return;
	}
	MediaItem_Take* take = GetActiveTake(item);
	if (!take) {
		return;
	}
	bool isPreserving = *(bool*)GetSetMediaItemTakeInfo(take, "B_PPITCH", nullptr);
	outputMessage(
			isPreserving ? translate("enabled preserve pitch when changing item rate")
									 : translate("disabled preserve pitch when changing item rate")
	);
}

void postChangeVerticalZoom(int command) {
	int size = 0;
	int index = projectconfig_var_getoffs("vzoom2", &size);
	assert(size == sizeof(int));
	int zoom = *(int*)projectconfig_var_addr(nullptr, index);
	switch (zoom) {
		case 0:
			outputMessage(translate("minimum vertical zoom"));
			return;
		case 2:
			outputMessage(translate("small vertical zoom"));
			return;
		case 6:
			outputMessage(translate("medium vertical zoom"));
			return;
		case 16:
			outputMessage(translate("large vertical zoom"));
			return;
		case 40:
			outputMessage(translate("maximum vertical zoom"));
			return;
	}
	// Translators: Used when reporting the vertical zoom level as a number.
	// {} will be replaced with the number; e.g. "35 vertical zoom".
	outputMessage(translate(format("{} vertical zoom", zoom)));
}

void postMExplorerChangeVolume(int cmd, HWND hwnd) {
	HWND w = GetDlgItem(hwnd, 997);
	if (!w) { // support Reaper versions before 6.65
		w = GetDlgItem(hwnd, 1047);
	}
	const int sz = 10;
	char text[sz];
	if (!GetWindowText(w, text, sz)) {
		return;
	}
	outputMessage(text);
}

typedef void (*PostCommandExecute)(int);

typedef struct PostCommand {
	int cmd;
	PostCommandExecute execute;
} PostCommand;

typedef struct MidiPostCommand : PostCommand {
	bool supportedInMidiEventList = false;
	bool changesValueInMidiEventList = false;
} MidiPostCommand;

// For commands registered by other plug-ins.
typedef struct {
	const char* id;
	PostCommandExecute execute;
} PostCustomCommand;

PostCommand POST_COMMANDS[] = {
		{40001, postGoToTrack}, // Track: Insert new track
		{6, postToggleTrackMute}, // Track: Toggle mute for selected tracks
		{40280, postToggleTrackMute}, // Track: Mute/unmute tracks
		{40281, postToggleTrackSolo}, // Track: Solo/unsolo tracks
		{9, postToggleTrackArm}, // Track: Toggle record arm for selected tracks
		{40294, postToggleTrackArm}, // Toggle record arming for current (last touched) track
		{40495, postCycleTrackMonitor}, // Track: Cycle track record monitor
		{40282, postInvertTrackPhase}, // Track: Invert track phase
		{40298, postToggleTrackFxBypass}, // Track: Toggle FX bypass for current track
		{16, postToggleMasterTrackFxBypass}, // Track: Toggle FX bypass for master track
		{40344, postToggleAllTracksFxBypass}, // Track: toggle FX bypass on all tracks
		{42455, postToggleLastFocusedFxDeltaSolo}, // FX: Toggle delta solo for last focused FX
		{40104, postCursorMovementScrub}, // View: Move cursor left one pixel
		{40105, postCursorMovementScrub}, // View: Move cursor right one pixel
		{40042, postCursorMovement}, // Transport: Go to start of project
		{40043, postCursorMovement}, // Transport: Go to end of project
		{40108, postItemNormalize}, // Item properties: Normalize items
		{40254, postItemNormalize}, // Item properties: Normalize multiple items to common gain
		{40318, postCursorMovement}, // Item navigation: Move cursor left to edge of item
		{40319, postCursorMovement}, // Item navigation: Move cursor right to edge of item
		{40646, postCursorMovement}, // View: Move cursor left to grid division
		{40647, postCursorMovement}, // View: Move cursor right to grid division
		{41040, postCursorMovement}, // Move edit cursor to start of next measure
		{41041, postCursorMovement}, // Move edit cursor to start of current measure
		{41042, postCursorMovement}, // Go forward one measure
		{41043, postCursorMovement}, // Go back one measure
		{41044, postCursorMovement}, // Go forward one beat
		{41045, postCursorMovement}, // Go back one beat
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
		{41190, postChangeHorizontalZoom}, // View: Set horizontal zoom to default project setting
		{40283, postChangeTrackPan}, // Track: Nudge track pan left
		{40284, postChangeTrackPan}, // Track: Nudge track pan right
		{1155, postCycleRippleMode}, // Options: Cycle ripple editing mode
		{1068, postToggleRepeat}, // Transport: Toggle repeat
		{40125, postSwitchToTake}, // Take: Switch items to next take
		{40126, postSwitchToTake}, // Take: Switch items to previous take
		{40057, postCopy}, // Edit: Copy items/tracks/envelope points (depending on focus) ignoring time selection
		{41383, postCopy
		}, // Edit: Copy items/tracks/envelope points (depending on focus) within time selection, if any (smart copy)
		{41820, postMoveToTimeSig}, // Move edit cursor to previous tempo or time signature change
		{41821, postMoveToTimeSig}, // Move edit cursor to next tempo or time signature change
		{41860, postGoToStretch}, // Item: go to next stretch marker
		{41861, postGoToStretch}, // Item: go to previous stretch marker
		{40291, postTrackFxChain}, // Track: View FX chain for current track
		{40293, postTrackIo}, // Track: View I/O for current track
		{40364, postToggleMetronome}, // Options: Toggle metronome
		{40075, postToggleMasterTrackVisible}, // View: Toggle master track visible
		{40044, postChangeTransportState}, // Transport: Play/stop
		{40073, postChangeTransportState}, // Transport: Play/pause
		{40328, postChangeTransportState}, // Transport: Play/stop (move edit cursor on stop)
		{40317, postChangeTransportState}, // Transport: Play (skip time selection)
		{1013, postChangeTransportState}, // Transport: Record
		{40718, postSelectMultipleItems}, // Item: Select all items on selected tracks in current time selection
		{40421, postSelectMultipleItems}, // Item: Select all items in track
		{40034, postSelectMultipleItems}, // Item grouping: Select all items in groups
		{40717, postSelectMultipleItems}, // Item: Select all items in current time selection
		{40117, postMoveEnvelopePoint}, // Item edit: Move items/envelope points up one track/a bit
		{40118, postMoveEnvelopePoint}, // Item edit: Move items/envelope points down one track/a bit
		{40696, postRenameTrack}, // Track: Rename last touched track
		{40175, postToggleItemMute}, // Item properties: Toggle mute
		{41561, postToggleItemSolo}, // Item properties: Toggle solo
		{40626, postSetSelectionEnd}, // Time selection: Set end point
		{40917, postToggleMasterMono}, // Master track: Toggle stereo/mono (L+R)
		{40041, postToggleAutoCrossfade}, // Options: Toggle auto-crossfade on/off
		{1135, postToggleLocking}, // Options: Toggle locking
		{40745, postToggleSoloInFront}, // Options: Solo in front
		{40522, postAdjustPlayRate}, // Transport: Increase playrate by ~6% (one semitone)
		{40523, postAdjustPlayRate}, // Transport: Decrease playrate by ~6% (one semitone)
		{40524, postAdjustPlayRate}, // Transport: Increase playrate by ~0.6% (10 cents)
		{40525, postAdjustPlayRate}, // Transport: Decrease playrate by ~0.6% (10 cents)
		{40521, postAdjustPlayRate}, // Set playrate to 1.0
		{41884, postToggleMonitoringFxBypass}, // Monitoring FX: Toggle bypass
		{40881, postChangeGlobalAutomationOverride}, // Global automation override: All automation in latch mode
		{42022, postChangeGlobalAutomationOverride}, // Global automation override: All automation in latch preview mode
		{40879, postChangeGlobalAutomationOverride}, // Global automation override: All automation in read mode
		{40880, postChangeGlobalAutomationOverride}, // Global automation override: All automation in touch mode
		{40878, postChangeGlobalAutomationOverride}, // Global automation override: All automation in trim/read mode
		{40882, postChangeGlobalAutomationOverride}, // Global automation override: All automation in write mode
		{40885, postChangeGlobalAutomationOverride}, // Global automation override: Bypass all automation
		{40876, postChangeGlobalAutomationOverride
		}, // Global automation override: No override (set automation modes per track)
		{41051, postReverseTake}, // Item properties: Toggle take reverse
		{41819, postTogglePreRoll}, // Pre-roll: Toggle pre-roll on record
		{40176, postTakeChannelMode}, // Item properties: Set take channel mode to normal
		{40179, postTakeChannelMode}, // Item properties: Set take channel mode to mono (left)
		{40178, postTakeChannelMode}, // Item properties: Set take channel mode to mono (downmix)
		{40180, postTakeChannelMode}, // Item properties: Set take channel mode to mono (right)
		{41130, postChangeTempo}, // Tempo: Decrease current project tempo 01 BPM
		{41129, postChangeTempo}, // Tempo: Increase current project tempo 01 BPM
		{41138, postChangeTempo}, // Tempo: Decrease current project tempo 0.1 BPM
		{41136, postChangeTempo}, // Tempo: Decrease current project tempo 10 BPM
		{41132, postChangeTempo}, // Tempo: Decrease current project tempo 10 percent
		{41134, postChangeTempo}, // Tempo: Decrease current project tempo 50 percent (half)
		{41137, postChangeTempo}, // Tempo: Increase current project tempo 0.1 BPM
		{41135, postChangeTempo}, // Tempo: Increase current project tempo 10 BPM
		{41131, postChangeTempo}, // Tempo: Increase current project tempo 10 percent
		{41133, postChangeTempo}, // Tempo: Increase current project tempo 100 percent (double)
		{40671, postTogglePreservePitchWhenPlayRateChanged
		}, // Transport: Toggle preserve pitch in audio items when changing master playrate
		{41925, postChangeItemVolume}, // Item: Nudge items volume +1dB
		{41924, postChangeItemVolume}, // Item: Nudge items volume -1dB
		{41927, postChangeTakeVolume}, // Take: Nudge active takes volume +1dB
		{41926, postChangeTakeVolume}, // Take: Nudge active takes volume -1dB
		{40612, postSetItemEnd}, // Item: Set item end to source media end
		{40630, postCursorMovement}, // Go to start of time selection
		{40631, postCursorMovement}, // Go to end of time selection
		{40632, postCursorMovement}, // Go to start of loop
		{40633, postCursorMovement}, // Go to end of loop
		{42393, postGoToTakeMarker}, // Item: Set cursor to previous take marker in selected items
		{42394, postGoToTakeMarker}, // Item: Set cursor to next take marker in selected items
		{40296, postSelectMultipleTracks}, // Track: Select all tracks
		{40332, postSelectMultipleEnvelopePoints}, // Envelope: Select all points
		{40035, postSelectAll}, // Select all items/tracks/envelope points (depending on focus)
		{41199, postToggleTrackSoloDefeat}, // Track: Toggle track solo defeat
		{41536, postChangeTransientDetectionSensitivity}, // Transient detection sensitivity: Increase
		{41537, postChangeTransientDetectionSensitivity}, // Transient detection sensitivity: decrease
		{40218, postChangeTransientDetectionThreshold}, // Transient detection threshold: Increase
		{40219, postChangeTransientDetectionThreshold}, // Transient detection threshold: Decrease
		{40070, postToggleEnvelopePointsMoveWithMediaItems}, // Options: Envelope points move with media items
		{40641, postToggleFreeItemPositioning}, // Track properties: Toggle free item positioning
		{40520, postChangeItemRate}, // Item properties: Decrease item rate by ~0.6% (10 cents)
		{40800, postChangeItemRate}, // Item properties: Decrease item rate by ~0.6% (10 cents), clear 'preserve pitch'
		{40518, postChangeItemRate}, // Item properties: Decrease item rate by ~6% (one semitone)
		{40798, postChangeItemRate}, // Item properties: Decrease item rate by ~6% (one semitone), clear 'preserve pitch'
		{40519, postChangeItemRate}, // Item properties: Increase item rate by ~0.6% (10 cents)
		{40799, postChangeItemRate}, // Item properties: Increase item rate by ~0.6% (10 cents), clear 'preserve pitch'
		{40517, postChangeItemRate}, // Item properties: Increase item rate by ~6% (one semitone)
		{40797, postChangeItemRate}, // Item properties: Increase item rate by ~6% (one semitone), clear 'preserve pitch'
		{42374, postChangeItemRate}, // Item properties: Set item rate from user-supplied source media tempo/bpm...
		{40652, postChangeItemRate}, // Item properties: Set item rate to 1.0
		{40207, postChangeItemPitch}, // Item properties: Pitch item down one cent
		{40206, postChangeItemPitch}, // Item properties: Pitch item up one cent
		{40205, postChangeItemPitch}, // Item properties: Pitch item down one semitone
		{40204, postChangeItemPitch}, // Item properties: Pitch item up one semitone
		{40516, postChangeItemPitch}, // Item properties: Pitch item down one octave
		{40515, postChangeItemPitch}, // Item properties: Pitch item up one octave
		{40653, postChangeItemPitch}, // Item properties: Reset item pitch
		{40566, postToggleTakePreservePitch}, // Item properties: Toggle take preserve pitch
		{40796, postToggleTakePreservePitch}, // Item properties: Clear take preserve pitch
		{40795, postToggleTakePreservePitch}, // Item properties: Set take preserve pitch
		{40110, postChangeVerticalZoom}, // View: Toggle track zoom to minimum height
		{40111, postChangeVerticalZoom}, // View: Zoom in vertical
		{40112, postChangeVerticalZoom}, // View: Zoom out vertical
		{40113, postChangeVerticalZoom}, // View: Toggle track zoom to maximum height
		{0},
};
MidiPostCommand MIDI_POST_COMMANDS[] = {
		{40006, postMidiSelectEvents, true}, // Edit: Select all events
		{40049, postMidiMovePitchCursor}, // Edit: Increase pitch cursor one semitone
		{40050, postMidiMovePitchCursor}, // Edit: Decrease pitch cursor one semitone
		{40177, postMidiChangePitch, true, true}, // Edit: Move notes up one semitone
		{40178, postMidiChangePitch, true, true}, // Edit: Move notes down one semitone
		{40179, postMidiChangePitch, true, true}, // Edit: Move notes up one octave
		{40180, postMidiChangePitch, true, true}, // Edit: Move notes down one octave
		{40181, postMidiMoveStart}, // Edit: Move notes left one pixel
		{40182, postMidiMoveStart}, // Edit: Move notes right one pixel
		{40183, postMidiMoveStart, true, true}, // Edit: Move notes left one grid unit
		{40184, postMidiMoveStart, true, true}, // Edit: Move notes right one grid unit
		{40187, postMidiMovePitchCursor}, // Edit: Increase pitch cursor one octave
		{40188, postMidiMovePitchCursor}, // Edit: Decrease pitch cursor one octave
		{40234, postMidiSwitchCCLane}, // CC: Next CC lane
		{40235, postMidiSwitchCCLane}, // CC: Previous CC lane
		{40434, postMidiSelectNotes, true}, // Select all notes with the same pitch
		{40444, postMidiChangeLength}, // Edit: Lengthen notes one pixel
		{40445, postMidiChangeLength}, // Edit: Shorten notes one pixel
		{40446, postMidiChangeLength, true, true}, // Edit: Lengthen notes one grid unit
		{40447, postMidiChangeLength, true, true}, // Edit: Shorten notes one grid unit
		{40462, postMidiChangeVelocity, true, true}, // Edit: Note velocity +01
		{40463, postMidiChangeVelocity, true, true}, // Edit: Note velocity +10
		{40464, postMidiChangeVelocity, true, true}, // Edit: Note velocity -01
		{40465, postMidiChangeVelocity, true, true}, // Edit: Note velocity -10
		{40501, postMidiSelectEvents}, // Invert selection
		{40633, postMidiChangeLength, true, true}, // Edit: Set note lengths to grid size
		{40676, postMidiChangeCCValue, true, true}, // Edit: Increase value a little bit for CC events
		{40677, postMidiChangeCCValue, true, true}, // Edit: Decrease value a little bit for CC events
		{40746, postMidiSelectNotes, true}, // Edit: Select all notes in time selection
		{40877, postMidiSelectNotes, true}, // Edit: Select all notes starting in time selection
		{40765, postMidiChangeLength}, // Edit: Make notes legato, preserving note start times
		{41026, postMidiChangePitch, true, true}, // Edit: Move notes up one semitone ignoring scale/key
		{41027, postMidiChangePitch, true, true}, // Edit: Move notes down one semitone ignoring scale/key
		{40481, postToggleMidiInputsAsStepInput, true}, // Options: MIDI inputs as step input mode
		{40053, postToggleFunctionKeysAsStepInput, true}, // Options: F1-F12 as step input mode
		{1014, postMidiToggleSnap}, // View: Toggle snap to grid
		{1139, postToggleRepeat}, // Transport: Toggle repeat
		{1011, postMidiChangeZoom}, // View: Zoom out horizontally
		{1012, postMidiChangeZoom}, // View: Zoom in horizontally
};
PostCustomCommand POST_CUSTOM_COMMANDS[] = {
		{"_XENAKIOS_NUDGSELTKVOLUP", postChangeTrackVolume}, // Xenakios/SWS: Nudge volume of selected tracks up
		{"_XENAKIOS_NUDGSELTKVOLDOWN", postChangeTrackVolume}, // Xenakios/SWS: Nudge volume of selected tracks down
		{"_XENAKIOS_NUDMASVOL1DBU", postChangeMasterTrackVolume}, // Xenakios/SWS: Nudge master volume 1 dB up
		{"_XENAKIOS_NUDMASVOL1DBD", postChangeMasterTrackVolume}, // Xenakios/SWS: Nudge master volume 1 dB down
		{"_XENAKIOS_SETMASTVOLTO0", postChangeMasterTrackVolume}, // Xenakios/SWS: Set master volume to 0 dB
		{"_FNG_ENVDOWN", postMoveEnvelopePoint}, // SWS/FNG: Move selected envelope points down
		{"_FNG_ENVUP", postMoveEnvelopePoint}, // SWS/FNG: Move selected envelope points up
		{"_XENAKIOS_SELITEMSUNDEDCURSELTX", postSelectMultipleItems
		}, // Xenakios/SWS: Select items under edit cursor on selected tracks
		{"_BR_CYCLE_RECORD_MODES", postCycleRecordMode}, // SWS/BR: Options - Cycle through record modes
		{"_SWS_AWCOUNTRECTOG", postToggleCountIn}, // SWS/AW: Toggle count-in before recording
		{"_SWS_SELNEARESTNEXTFOLDER", postGoToTrack}, // SWS: Select nearest next folder
		{"_SWS_SELNEARESTPREVFOLDER", postGoToTrack}, // SWS: Select nearest previous folder
		{"_BR_OPTIONS_PLAYBACK_TEMPO_CHANGE", postTogglePlaybackPositionFollowsTimebase
		}, // SWS/BR: Options - Toggle "Playback position follows project timebase when changing tempo"
		{"_XENAKIOS_NUDGETAKEVOLDOWN", postChangeTakeVolume}, // Xenakios/SWS: Nudge active take volume down
		{"_XENAKIOS_NUDGETAKEVOLUP", postChangeTakeVolume}, // Xenakios/SWS: Nudge active take volume up
		{"_XENAKIOS_NUDGEITEMVOLDOWN", postChangeItemVolume}, // Xenakios/SWS: Nudge item volume down
		{"_XENAKIOS_NUDGEITEMVOLUP", postChangeItemVolume}, // Xenakios/SWS: Nudge item volume up
		{NULL},
};

using MExplorerPostExecute = void (*)(int, HWND);
map<int, MExplorerPostExecute> mExplorerPostCommands{
		{42178, postMExplorerChangeVolume}, // Preview: decrease volume by 1 dB
		{42177, postMExplorerChangeVolume}, // Preview: increase volume by 1 dB
};

map<int, PostCommandExecute> postCommandsMap;
map<int, string> POST_COMMAND_MESSAGES = {
		{40625, _t("set selection start")}, // Time selection: Set start point
		{40222, _t("set loop start")}, // Loop points: Set start point
		{40223, _t("set loop end")}, // Loop points: Set end point
		{40781, _t("grid whole")}, // Grid: Set to 1
		{40780, _t("grid half")}, // Grid: Set to 1/2
		{40775, _t("grid thirty second")}, // Grid: Set to 1/32
		{40779, _t("grid quarter")}, // Grid: Set to 1/4
		{41214, _t("grid quarter triplet")}, // Grid: Set to 1/6 (1/4 triplet)
		{40776, _t("grid sixteenth")}, // Grid: Set to 1/16
		{41213, _t("grid sixteenth triplet")}, // Grid: Set to 1/24 (1/16 triplet)
		{40778, _t("grid eighth")}, // Grid: Set to 1/8
		{40777, _t("grid eighth triplet")}, // Grid: Set to 1/12 (1/8 triplet)
		{41212, _t("grid thirty second triplet")}, // Grid: Set to 1/48 (1/32 triplet)
		{40774, _t("grid sixty forth")}, // Grid: Set to 1/64
		{41047, _t("grid one hundred twenty eighth")}, // Grid: Set to 1/128
		{40339, _t("all tracks unmuted")}, // Track: Unmute all tracks
		{40340, _t("all tracks unsoloed")}, // Track: Unsolo all tracks
		{40491, _t("all tracks unarmed")}, // Track: Unarm all tracks for recording
		{42467, _t("all delta solos reset")}, // FX: Clear delta solo for all project FX
};
const set<int> MOVE_FROM_PLAY_CURSOR_COMMANDS = {
		40104, // View: Move cursor left one pixel
		40105, // View: Move cursor right one pixel
		41042, // Go forward one measure
		41043, // Go back one measure
		41044, // Go forward one beat
		41045, // Go back one beat
		41041, // Move edit cursor to start of current measure
		41040, // Move edit cursor to start of next measure
		40646, // View: Move cursor left to grid division
		40647, // View: Move cursor right to grid division
};

map<int, PostCommandExecute> midiPostCommandsMap;
map<int, pair<PostCommandExecute, bool>> midiEventListPostCommandsMap;
map<int, string> MIDI_POST_COMMAND_MESSAGES = {
		{40204, _t("grid whole")}, // Grid: Set to 1
		{40203, _t("grid half")}, // Grid: Set to 1/2
		{40190, _t("grid thirty second")}, // Grid: Set to 1/32
		{40201, _t("grid quarter")}, // Grid: Set to 1/4
		{40199, _t("grid quarter triplet")}, // Grid: Set to 1/6 (1/4 triplet)
		{40192, _t("grid sixteenth")}, // Grid: Set to 1/16
		{40191, _t("grid sixteenth triplet")}, // Grid: Set to 1/24 (1/16 triplet)
		{40197, _t("grid eighth")}, // Grid: Set to 1/8
		{40193, _t("grid eighth triplet")}, // Grid: Set to 1/12 (1/8 triplet)
		{40189, _t("grid thirty second triplet")}, // Grid: Set to 1/48 (1/32 triplet)
		{41020, _t("grid sixty forth")}, // Grid: Set to 1/64
		{41019, _t("grid one hundred twenty eighth")}, // Grid: Set to 1/128
		{41081, _t("length whole")}, // Set length for next inserted note: 1
		{41079, _t("length half")}, // Set length for next inserted note: 1/2
		{41067, _t("length thirty second")}, // Set length for next inserted note: 1/32
		{41076, _t("length quarter")}, // Set length for next inserted note: 1/4
		{41075, _t("length quarter triplet")}, // Set length for next inserted note: 1/4T
		{41070, _t("length sixteenth")}, // Set length for next inserted note: 1/16
		{41069, _t("length sixteenth triplet")}, // Set length for next inserted note: 1/16T
		{41073, _t("length eighth")}, // Set length for next inserted note: 1/8
		{41072, _t("length eighth triplet")}, // Set length for next inserted note: 1/8T
		{41066, _t("length thirty second triplet")}, // Set length for next inserted note: 1/32T
		{41064, _t("length sixty forth")}, // Set length for next inserted note: 1/64
		{41062, _t("length one hundred twenty eighth")}, // Set length for next inserted note: 1/128
};

struct ToggleCommandMessage {
	const char* onMsg;
	const char* offMsg;
};

// Messages for toggle actions. Specify null messages to report nothing. If a
// toggle action isn't included here or in another of OSARA's action maps,
// OSARA will fall back to reporting the toggle state and the action name.
map<pair<int, int>, ToggleCommandMessage> TOGGLE_COMMAND_MESSAGES = {
		// {{sectionId, actionId}, {onMsg, offMsg}}, // actionName
		// Specify nullptr to report nothing for a particular message.
		// Main section toggles
		{{MAIN_SECTION, 40346}, {_t("full screen"), _t("normal screen")}}, // Toggle fullscreen
		{{MAIN_SECTION, 50124}, {_t("showed Media Explorer"), _t("hid Media Explorer")}
		}, // Media explorer: Show/hide media explorer
		{{MAIN_SECTION, 41973}, {_t("absolute frames"), nullptr}}, // View: Time unit for ruler: Absolute frames
		{{MAIN_SECTION, 40370}, {_t("hours:minutes:seconds:frames"), nullptr}
		}, // View: Time unit for ruler: Hours:Minutes:Seconds:Frames
		{{MAIN_SECTION, 40367}, {_t("measures.beats"), nullptr}}, // View: Time unit for ruler: Measures.Beats
		{{MAIN_SECTION, 40365}, {_t("minutes:seconds"), nullptr}}, // View: Time unit for ruler: Minutes:Seconds
		{{MAIN_SECTION, 40369}, {_t("samples"), nullptr}}, // View: Time unit for ruler: Samples
		{{MAIN_SECTION, 40368}, {_t("seconds"), nullptr}}, // View: Time unit for ruler: Seconds
		{{MAIN_SECTION, 42365}, {_t("absolute frames secondary"), nullptr}
		}, // View: Secondary time unit for ruler: Absolute frames
		{{MAIN_SECTION, 42364}, {_t("hours:minutes:seconds:frames secondary"), nullptr}
		}, // View: Secondary time unit for ruler: Hours:Minutes:Seconds:Frames
		{{MAIN_SECTION, 42361}, {_t("minutes:seconds secondary"), nullptr}
		}, // View: Secondary time unit for ruler: Minutes:Seconds
		{{MAIN_SECTION, 42360}, {_t("no secondary time unit"), nullptr}}, // View: Secondary time unit for ruler: None
		{{MAIN_SECTION, 42363}, {_t("samples secondary"), nullptr}}, // View: Secondary time unit for ruler: Samples
		{{MAIN_SECTION, 42362}, {_t("seconds secondary"), nullptr}}, // View: Secondary time unit for ruler: Seconds
		{{MAIN_SECTION, 14}, {_t("master muted"), _t("master unmuted")}}, // Track: Toggle mute for master track
		// Reducing verbeage when toggling and momentarily switching to alt keymap layers (Reaper 7)
		{{MAIN_SECTION, 24801}, {_t("default key map"), nullptr}}, // Main action section: Set override to default
		{{MAIN_SECTION, 24802}, {_t("recording key map"), nullptr}}, // Main action section: Toggle override to recording
		{{MAIN_SECTION, 24803}, {_t("alt 1"), nullptr}}, // Main action section: Toggle override to alt-1
		{{MAIN_SECTION, 24804}, {_t("alt 2"), nullptr}}, // Main action section: Toggle override to alt-2
		{{MAIN_SECTION, 24805}, {_t("alt 3"), nullptr}}, // Main action section: Toggle override to alt-3
		{{MAIN_SECTION, 24806}, {_t("alt 4"), nullptr}}, // Main action section: Toggle override to alt-4
		{{MAIN_SECTION, 24807}, {_t("alt 5"), nullptr}}, // Main action section: Toggle override to alt-5
		{{MAIN_SECTION, 24808}, {_t("alt 6"), nullptr}}, // Main action section: Toggle override to alt-6
		{{MAIN_SECTION, 24809}, {_t("alt 7"), nullptr}}, // Main action section: Toggle override to alt-7
		{{MAIN_SECTION, 24810}, {_t("alt 8"), nullptr}}, // Main action section: Toggle override to alt-8
		{{MAIN_SECTION, 24811}, {_t("alt 9"), nullptr}}, // Main action section: Toggle override to alt-9
		{{MAIN_SECTION, 24812}, {_t("alt 10"), nullptr}}, // Main action section: Toggle override to alt-10
		{{MAIN_SECTION, 24813}, {_t("alt 11"), nullptr}}, // Main action section: Toggle override to alt-11
		{{MAIN_SECTION, 24814}, {_t("alt 12"), nullptr}}, // Main action section: Toggle override to alt-12
		{{MAIN_SECTION, 24815}, {_t("alt 13"), nullptr}}, // Main action section: Toggle override to alt-13
		{{MAIN_SECTION, 24816}, {_t("alt 14"), nullptr}}, // Main action section: Toggle override to alt-14
		{{MAIN_SECTION, 24817}, {_t("alt 15"), nullptr}}, // Main action section: Toggle override to alt-15
		{{MAIN_SECTION, 24818}, {_t("alt 16"), nullptr}}, // Main action section: Toggle override to alt-16
		{{MAIN_SECTION, 24851}, {_t("default momentary"), nullptr}
		}, // Main action section: Momentarily set override to default
		{{MAIN_SECTION, 24852}, {_t("recording key map momentary"), nullptr}
		}, // Main action section: Momentarily set override to recording
		{{MAIN_SECTION, 24853}, {_t("alt 1 momentary"), nullptr}}, // Main action section: Momentarily set override to alt-1
		{{MAIN_SECTION, 24854}, {_t("alt 2 momentary"), nullptr}}, // Main action section: Momentarily set override to alt-2
		{{MAIN_SECTION, 24855}, {_t("alt 3 momentary"), nullptr}}, // Main action section: Momentarily set override to alt-3
		{{MAIN_SECTION, 24856}, {_t("alt 4 momentary"), nullptr}}, // Main action section: Momentarily set override to alt-4
		{{MAIN_SECTION, 24857}, {_t("alt 5 momentary"), nullptr}}, // Main action section: Momentarily set override to alt-5
		{{MAIN_SECTION, 24858}, {_t("alt 6 momentary"), nullptr}}, // Main action section: Momentarily set override to alt-6
		{{MAIN_SECTION, 24859}, {_t("alt 7 momentary"), nullptr}}, // Main action section: Momentarily set override to alt-7
		{{MAIN_SECTION, 24860}, {_t("alt 8 momentary"), nullptr}}, // Main action section: Momentarily set override to alt-8
		{{MAIN_SECTION, 24861}, {_t("alt 9 momentary"), nullptr}}, // Main action section: Momentarily set override to alt-9
		{{MAIN_SECTION, 24862}, {_t("alt 10 momentary"), nullptr}
		}, // Main action section: Momentarily set override to alt-10
		{{MAIN_SECTION, 24863}, {_t("alt 11 momentary"), nullptr}
		}, // Main action section: Momentarily set override to alt-11
		{{MAIN_SECTION, 24864}, {_t("alt 12 momentary"), nullptr}
		}, // Main action section: Momentarily set override to alt-12
		{{MAIN_SECTION, 24865}, {_t("alt 13 momentary"), nullptr}
		}, // Main action section: Momentarily set override to alt-13
		{{MAIN_SECTION, 24866}, {_t("alt 14 momentary"), nullptr}
		}, // Main action section: Momentarily set override to alt-14
		{{MAIN_SECTION, 24867}, {_t("alt 15 momentary"), nullptr}
		}, // Main action section: Momentarily set override to alt-15
		{{MAIN_SECTION, 24868}, {_t("alt 16 momentary"), nullptr}
		}, // Main action section: Momentarily set override to alt-16
		// Media Explorer toggles
		{{MEDIA_EXPLORER_SECTION, 1011}, {_t("auto playing"), _t("not auto playing")}}, // Autoplay: Toggle on/off
		{{MEDIA_EXPLORER_SECTION, 1068}, {_t("repeating previews"), _t("not repeating previews")}
		}, // Preview: Toggle repeat on/off
		{{MEDIA_EXPLORER_SECTION, 1012}, {_t("starting on bar"), _t("not starting on bar")}}, // Start on bar: Toggle on/off
		{{MEDIA_EXPLORER_SECTION, 40068}, {_t("preserving pitch"), _t("not preserving pitch")}
		}, // Options: Preserve pitch when tempo-matching or changing play rate
		{{MEDIA_EXPLORER_SECTION, 42239}, {_t("reset pitch"), nullptr}}, // Preview: reset pitch
		{{MEDIA_EXPLORER_SECTION, 40023}, {_t("tempo matching"), _t("not tempo  matching")}}, // Tempo match: Toggle on/off
		{{MEDIA_EXPLORER_SECTION, 40021}, {_t("half time"), _t("normal time")}}, // Tempo match: /2
		{{MEDIA_EXPLORER_SECTION, 40022}, {_t("double time"), _t("normal time")}}, // Tempo match: x2
		{{MEDIA_EXPLORER_SECTION, 40008}, {_t("docked Media Explorer"), _t("removed Media Explorer from dock")}
		}, // Dock Media Explorer in Docker
		// MIDI Editor toggles
		{{MIDI_EDITOR_SECTION, 40042}, {_t("Piano roll view"), nullptr}}, // Mode: Piano Roll
		{{MIDI_EDITOR_SECTION, 40043}, {_t("Named notes view"), nullptr}}, // Mode: Named Notes (Drum Map)
		{{MIDI_EDITOR_SECTION, 40056}, {_t("Event list view"), nullptr}}, // Mode: Event List
		{{MIDI_EDITOR_SECTION, 40056}, {_t("Event list view"), nullptr}}, // Mode: Event List
		{{MIDI_EDITOR_SECTION, 40954}, {_t("Notation view"), nullptr}}, // Mode: Notation
		{{MIDI_EDITOR_SECTION, 40449}, {_t("Rectangle notes"), nullptr}}, // View: Show events as rectangles (normal mode)
		{{MIDI_EDITOR_SECTION, 40448}, {_t("Triangle notes"), nullptr}}, // View: Show events as triangles (drum mode)
		{{MIDI_EDITOR_SECTION, 40450}, {_t("Diamond notes"), nullptr}}, // View: Show events as diamonds (drum mode)
		{{MIDI_EDITOR_SECTION, 40632}, {_t("Showed velocity numbers on notes"), _t("Hid velocity numbers on notes")}
		}, // View: Show velocity numbers on notes
		{{MIDI_EDITOR_SECTION, 40045}, {_t("Showed note names"), _t("Hid note names")}}, // View: Show note names
		{{MIDI_EDITOR_SECTION, 41295}, {_t("Inserted notes matching grid"), nullptr}
		}, // Set length for next inserted note: grid
};

/*** Code related to context menus and other UI that isn't just actions.
 * This includes code to access REAPER context menus, but also code to display
 * our own in some cases where REAPER doesn't provide one.
 */

bool showReaperContextMenu(const int menu) {
	if (fakeFocus != FOCUS_TRACK && menu != 0) {
		// Only tracks support more than one menu.
		return false;
	}
	switch (fakeFocus) {
		// todo: Fix visual positioning.
		case FOCUS_TRACK:
			if (menu == 0) {
				ShowPopupMenu("track_input", 0, 0, nullptr, nullptr, 0, 0);
			} else if (menu == 1) {
				ShowPopupMenu("track_panel", 0, 0, nullptr, nullptr, 0, 0);
			} else if (menu == 2) {
				ShowPopupMenu("track_routing", 0, 0, nullptr, nullptr, 0, 0);
			} else {
				return false;
			}
			return true;
		case FOCUS_ITEM:
			ShowPopupMenu("item", 0, 0, nullptr, nullptr, 0, 0);
			return true;
		case FOCUS_RULER:
			ShowPopupMenu("ruler", 0, 0, nullptr, nullptr, 0, 0);
			return true;
		case FOCUS_ENVELOPE: {
			int point = getEnvelopePointAtCursor();
			ShowPopupMenu("envelope_point", 0, 0, nullptr, nullptr, point, currentAutomationItem + 1);
			return true;
		}
		case FOCUS_AUTOMATIONITEM:
			ShowPopupMenu("envelope_item", 0, 0, nullptr, nullptr, currentAutomationItem + 1, 0);
			return true;
		default:
			break;
	}
	return false;
}

bool isClassName(HWND hwnd, string className) {
	char buffer[50];
	if (GetClassName(hwnd, buffer, sizeof(buffer)) == 0) {
		return false;
	}
	return className.compare(buffer) == 0;
}

#ifdef _WIN32

HWND getSendContainer(HWND hwnd) {
	if (!isClassName(hwnd, "Button")) {
		return nullptr;
	}
	hwnd = GetWindow(hwnd, GW_HWNDPREV);
	if (!isClassName(hwnd, "Static")) {
		return nullptr;
	}
	hwnd = GetAncestor(hwnd, GA_PARENT);
	if (!isClassName(hwnd, "REAPERVirtWndDlgHost")) {
		return nullptr;
	}
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
	// MIIM_TYPE is deprecated, but win32_utf8 still relies on it.
	itemInfo.fMask = MIIM_TYPE | MIIM_ID;
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

bool isTrackViewWindow(HWND hwnd) {
	WCHAR className[22] = L"\0";
	GetClassNameW(hwnd, className, ARRAYSIZE(className));
	return wcscmp(className, L"REAPERTrackListWindow") == 0 || wcscmp(className, L"REAPERtrackvu") == 0 ||
			wcscmp(className, L"REAPERTCPDisplay") == 0;
}

bool isListView(HWND hwnd) {
	return isClassName(hwnd, "SysListView32");
}

bool isMidiEditorEventListView(HWND hwnd) {
	return isListView(hwnd) && isClassName(GetAncestor(hwnd, GA_PARENT), "REAPERmidieditorwnd");
}

void sendNameChangeEventToMidiEditorEventListItem(HWND hwnd) {
	int child = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
	if (child == -1) {
		return;
	}
	NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, hwnd, OBJID_CLIENT, child + 1);
}

HWND getPreferenceDescHwnd(HWND pref) {
	// A preference control is always in a property page inside a dialog.
	// There may be nested property pages.
	// So, we test for at least two ancestor HWNDs.
	HWND parent = GetParent(pref);
	if (!parent) {
		return nullptr;
	}
	HWND dialog = GetAncestor(parent, GA_ROOT);
	if (dialog == parent || dialog == mainHwnd) {
		return nullptr;
	}
	// Group boxes aren't preference controls.
	if (isClassName(pref, "Button") && (GetWindowLong(pref, GWL_STYLE) & BS_GROUPBOX) == BS_GROUPBOX) {
		return nullptr;
	}
	// if the control id for the description isn't in this root window, it's not
	// the Preferences dialog.
	return GetDlgItem(dialog, 1259);
}

UINT_PTR annotatePreferenceDescriptionTimer = 0;

void CALLBACK annotatePreferenceDescription(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	KillTimer(nullptr, annotatePreferenceDescriptionTimer);
	annotatePreferenceDescriptionTimer = 0;
	HWND focus = GetFocus();
	HWND desc = getPreferenceDescHwnd(focus);
	if (!desc) {
		return;
	}
	char text[1000];
	if (GetWindowText(desc, text, sizeof(text)) == 0) {
		return;
	}
	// Set the accDescription on the control.
	accPropServices->SetHwndPropStr(
			focus, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_DESCRIPTION, widen(string(text)).c_str()
	);
	NotifyWinEvent(EVENT_OBJECT_DESCRIPTIONCHANGE, focus, OBJID_CLIENT, CHILDID_SELF);
}

bool maybeAnnotatePreferenceDescription() {
	if (annotatePreferenceDescriptionTimer) {
		// Cancel previous annotation if focus moved before it could complete.
		KillTimer(nullptr, annotatePreferenceDescriptionTimer);
		annotatePreferenceDescriptionTimer = 0;
	}
	HWND focus = GetFocus();
	HWND desc = getPreferenceDescHwnd(focus);
	if (!desc) {
		// Not a preference.
		return false;
	}
	// Move the mouse to the control.
	RECT rect;
	GetWindowRect(focus, &rect);
	SetCursorPos(rect.left, rect.top);
	// The description takes some time to appear, so we must use a timer.
	annotatePreferenceDescriptionTimer = SetTimer(nullptr, 0, 300, annotatePreferenceDescription);
	return true;
}

// Overide the tab/shift+tab key in Save dialogs so it can reach REAPER specific
// controls: Create subdirectory for project, etc.
// Tab seems to completely skip these controls, even though WS_TABSTOP and
// WS_EX_CONTROLPARENT are set correctly.
bool maybeFixTabInSaveDialog(bool previous) {
	HWND focus = GetFocus();
	HWND parent = GetParent(focus);
	if (
		// Save as type combo box. The REAPER specific controls are after this.
		!(isClassName(focus, "ComboBox") &&
			isClassName(parent, "FloatNotifySink")) &&
		// A REAPER specific control in the Save dialog.
		!(isClassName(parent, "#32770") &&
			isClassName(GetParent(parent), "FloatNotifySink")) &&
		// The "Hide Folders" toolbar in the save dialog. The REAPER specific
		// controls are before this.
		!(isClassName(focus, "ToolbarWindow32") &&
			isClassName(GetWindow(focus, GW_HWNDPREV), "DUIViewWndClassName"))
	) {
		return false;
	}
	HWND target = GetNextDlgTabItem(GetForegroundWindow(), focus, previous);
	SetFocus(target);
	return true;
}

// Handle keyboard keys which can't be bound to actions.
// REAPER's "accelerator" hook isn't enough because it doesn't get called in some windows.
LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam) {
	const bool isKeyDown = !(lParam & 0x80000000);
	if (!isKeyDown || code != HC_ACTION ||
			(wParam != VK_APPS && wParam != VK_CONTROL && wParam != VK_F10 && wParam != VK_RETURN && wParam != VK_F6 &&
			 wParam != 'B' && wParam != VK_TAB && wParam != VK_DOWN && wParam != VK_UP && wParam != VK_SPACE)) {
		// Return early if we're not interested in the key.
		return CallNextHookEx(nullptr, code, wParam, lParam);
	}
	HWND focus = GetFocus();
	if (!focus) {
		return CallNextHookEx(NULL, code, wParam, lParam);
	}
	const bool shift = GetKeyState(VK_SHIFT) & 0x8000;
	const bool alt = GetKeyState(VK_MENU) & 0x8000;
	const bool control = GetKeyState(VK_CONTROL) & 0x8000;
	const bool isContextMenu = wParam == VK_APPS ||
			// Alt+shift+f10 should not display a context menu, as that would override
			// a command in the key map. Control+alt+shift+f10 should, though.
			(wParam == VK_F10 && shift && (!alt || control));
	if (isContextMenu && isTrackViewWindow(focus)) {
		// Reaper doesn't handle the applications key for these windows and it
		// doesn't work even when bound to an action. Shift+f10 does work when bound
		// to an action, but then it interferes with FX plugin UIs.
		// Display the appropriate context menu depending on fakeFocus.
		if (alt && (wParam != VK_F10 || control)) {
			// Alt+applications or control+alt+shift+f10.
			showReaperContextMenu(2);
		} else if (control) {
			// Control+applications or control+shift+f10.
			showReaperContextMenu(1);
		} else {
			// Applications or shift+f10.
			showReaperContextMenu(0);
		}
		return 1;
	}
	if (wParam == VK_CONTROL) {
		if (cancelPendingMidiPreviewNotesOff()) {
			previewNotesOff(true);
			return 1;
		}
	}
	HWND window;
	if (isContextMenu && (window = getSendContainer(focus))) {
		sendMenu(window);
		return 1;
	}
	if ((isContextMenu || (wParam == VK_RETURN && control)) && isListView(focus)) {
		// REAPER doesn't allow you to do the equivalent of double click or right click in several ListViews.
		int item = ListView_GetNextItem(focus, -1, LVNI_FOCUSED);
		if (item != -1) {
			RECT rect;
			ListView_GetItemRect(focus, item, &rect, LVIR_BOUNDS);
			POINT point;
			if (GetWindowLong(focus, GWL_ID) == 1071) {
				// In the Project Render Metadata list, we need to click in the centre of
				// the item.
				point = {rect.left + (rect.right - rect.left) / 2, rect.top + (rect.bottom - rect.top) / 2};
			} else {
				// #478: Clicking in the centre of the Media Explorer list misbehaves for
				// some users, so use the top left for most lists.
				point = {rect.left, rect.top};
			}
			ClientToScreen(focus, &point);
			SetCursorPos(point.x, point.y);
			if (isContextMenu) {
				// Applications/f10 key right clicks.
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
	} else if (wParam == VK_F6) {
		if (maybeSwitchToFxPluginWindow()) {
			return 1;
		}
	} else if (wParam == 'B' && control) {
		maybeReportFxChainBypass(true);
	} else if (wParam == VK_TAB && !alt) {
		if (maybeFixTabInSaveDialog(shift)) {
			return 1;
		}
		if (control && maybeSwitchFxTab(shift)) {
			return 1;
		}
	} else if (wParam == VK_DOWN && alt && !shift && !control) {
		// Alt+downArrow.
		if (maybeOpenFxPresetDialog()) {
			return 1;
		}
	} else if(control && !shift &&
		(wParam == VK_DOWN || wParam == VK_UP || wParam == VK_SPACE)
		&& isListView(focus)
		// exclude the second list in the Edit custom action dialog because Ctrl+up/down reorder items.
		&& GetWindowLong(focus, GWL_ID) != 1322
	) {
		SendMessage(focus, WM_KEYDOWN, wParam, lParam);
		return 1;
	}
	return CallNextHookEx(nullptr, code, wParam, lParam);
}

HHOOK keyboardHook = nullptr;

#endif // _WIN32

/*** Our commands/commands we want to intercept.
 * Each command should have a function and should be added to the COMMANDS array below.
 */

int int0 = 0;
int int1 = 1;

void moveToTrack(int direction, bool clearSelection = true, bool select = true) {
	unsigned int undoMask = getConfigUndoMask();
	bool makeUndoPoint = undoMask & 1 << 4;
	int count = CountTracks(0);
	if (count == 0) {
		// Translators: Reported when there are no tracks to navigate to.
		outputMessage(translate("no tracks"));
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
		origTrack = GetTrack(nullptr, num);
	}
	MediaTrack* track = origTrack;
	// We use -1 for the master track.
	for (; -1 <= num && num < count; num += direction) {
		if (num == -1) {
			if (!(GetMasterTrackVisibility() & 1)) {
				// Invisible.
				if (direction == -1) {
					// We're moving backward and we're on the master track, which is
					// invisible. Return to the original track.
					track = origTrack;
					break;
				}
				continue;
			}
			track = GetMasterTrack(0);
		} else {
			track = GetTrack(nullptr, num);
			if (!IsTrackVisible(track, false) || isTrackInClosedFolder(track)) {
				// This track is invisible or inside a closed folder, so skip it.
				if (direction == 1 && num == count - 1) {
					// We're moving forward and we're on the last track, which is
					// invisible. Return to the original track.
					track = origTrack;
					break;
				}
				continue;
			}
		}
		break;
	}
	bool wasSelected = isTrackSelected(track);
	if (!select || track != origTrack || !wasSelected) {
		// We're moving to a different track
		// or we're on the same track but it's unselected.
		if ((clearSelection || select) && makeUndoPoint)
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
		Main_OnCommand(40913, 0); // Track: Vertical scroll selected tracks into view (TCP)
		SetMixerScroll(track); // MCP
		if (!wasSelected && !select)
			GetSetMediaTrackInfo(track, "I_SELECTED", &int0);
		if ((clearSelection || select) && makeUndoPoint)
			Undo_EndBlock(translate("Change Track Selection"), 0);
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

void cmdGoToFirstTrack(Command* command) {
	MediaTrack* track = GetTrack(nullptr, 0);
	if (!track) {
		return;
	}
	SetOnlyTrackSelected(track);
	postGoToTrack(0);
}

void cmdGoToLastTrack(Command* command) {
	int trackNo = CountTracks(nullptr) - 1;
	if (trackNo < 0) {
		return;
	}
	MediaTrack* track = GetTrack(nullptr, trackNo);
	if (!track) {
		return;
	}
	SetOnlyTrackSelected(track);
	postGoToTrack(trackNo);
}

void moveToItem(int direction, bool clearSelection = true, bool select = true) {
	unsigned int undoMask = getConfigUndoMask();
	bool makeUndoPoint = undoMask & 1;
	MediaTrack* track = GetLastTouchedTrack();
	if (!track)
		return;
	double cursor = GetCursorPosition();
	int count = CountTrackMediaItems(track);
	double pos;
	int start = direction == 1 ? 0 : count - 1;
	if (currentItem && ValidatePtr((void*)currentItem, "MediaItem*") &&
			(MediaTrack*)GetSetMediaItemInfo(currentItem, "P_TRACK", NULL) == track) {
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
		if ((clearSelection || select) && makeUndoPoint)
			Undo_BeginBlock();
		if (clearSelection) {
			Main_OnCommand(40289, 0); // Item: Unselect all items
			isSelectionContiguous = true;
		}
		if (select)
			GetSetMediaItemInfo(item, "B_UISEL", &bTrue);
		if ((clearSelection || select) && makeUndoPoint)
			Undo_EndBlock(translate("Change Item Selection"), 0);
		SetEditCurPos(pos, true, true); // Seek playback.
		fakeFocus = FOCUS_ITEM;
		selectedEnvelopeIsTake = true;
		SetCursorContext(1, NULL);
		if (!shouldReportTimeMovement()) {
			return;
		}

		// Report the item.
		ostringstream s;
		s << i + 1;
		if (isItemSelected(item)) {
			// One selected item is the norm, so don't report selected in this case.
			if (CountSelectedMediaItems(0) > 1) {
				s << " " << translate("selected");
			}
		} else {
			s << " " << translate("unselected");
		}
		if (*(bool*)GetSetMediaItemInfo(item, "B_MUTE", NULL)) {
			s << " " << translate("muted");
		}
		if (*(char*)GetSetMediaItemInfo(item, "C_LOCK", NULL) & 1) {
			// Translators: Used when navigating items to indicate that an item is
			// locked.
			s << " " << translate("locked");
		}
		int groupId = *(int*)GetSetMediaItemInfo(item, "I_GROUPID", nullptr);
		if (groupId) {
			// Translators: Used when navigating items to indicate that an item is
			// grouped. {} will be replaced with the group number; e.g. "group 1".
			s << " " << format(translate("group {}"), groupId);
		}
		MediaItem_Take* take = GetActiveTake(item);
		if (take) {
			s << " " << GetTakeName(take);
		}
		int takeCount = CountTakes(item);
		if (takeCount > 1) {
			// Translators: Used when navigating items to indicate the number of
			// takes. {} will be replaced with the number; e.g. "2 takes".
			s << " " << format(translate("{} takes"), takeCount);
		}
		s << " " << formatCursorPosition();
		addTakeFxNames(take, s);
		outputMessage(s);
		return;
	}
}

void cmdMoveToNextItem(Command* command) {
	if (shouldMoveToAutoItem) {
		moveToAutomationItem(1);
	} else {
		moveToItem(1);
	}
}

void cmdMoveToPrevItem(Command* command) {
	if (shouldMoveToAutoItem) {
		moveToAutomationItem(-1);
	} else {
		moveToItem(-1);
	}
}

void cmdUndo(Command* command) {
	const char* text = Undo_CanUndo2(0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	if (!text)
		return;
	// Translators: Reported when undoing an action. {}
	// will be replaced with the name of the action; e.g. "undo Remove tracks".
	outputMessage(format(translate("undo {}"), text));
}

void cmdRedo(Command* command) {
	const char* text = Undo_CanRedo2(0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	if (!text)
		return;
	// Translators: Reported when redoing an action. {}
	// will be replaced with the name of the action; e.g. "redo Remove tracks".
	outputMessage(format(translate("redo {}"), text));
}

void cmdSplitItems(Command* command) {
	int oldCount = CountMediaItems(0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	int added = CountMediaItems(0) - oldCount;
	// Translators: Reported when items are added. {} will be replaced with the
	// number of items; e.g. "2 items added".
	outputMessage(format(translate_plural("{} item added", "{} items added", added), added));
}

void cmdPaste(Command* command) {
	int oldItems = CountMediaItems(0);
	int oldTracks = CountTracks(0);
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	int oldPoints = 0;
	int oldAutoItems = 0;
	if (envelope) {
		oldPoints = countEnvelopePointsIncludingAutoItems(envelope);
		oldAutoItems = CountAutomationItems(envelope);
	}
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	int added;
	// We want to report both tracks and items if both got added; e.g.
	// "1 track 2 items added".
	ostringstream s;
	if ((added = CountTracks(0) - oldTracks) > 0) {
		// Translators: Reported when tracks are added. Other things might be added
		// at the same time (e.g. items), so other messages may surround this.
		// {} will be replaced with the number of tracks; e.g. "2 tracks".
		s << format(translate_plural("{} track", "{} tracks", added), added);
	}
	if ((added = CountMediaItems(0) - oldItems) > 0) {
		if (s.tellp() > 0) {
			s << " ";
		}
		// Translators: Reported when items are added. Other things might be added
		// at the same time (e.g. tracks), so other messages may surround this.
		// {} will be replaced with the number of items; e.g. "2 items".
		s << format(translate_plural("{} item", "{} items", added), added);
	}
	if (s.tellp() > 0) {
		// Translators: Reported after the number of tracks and/or items added.
		s << " " << translate("added");
		outputMessage(s);
		return;
	}
	if (envelope && (added = countEnvelopePointsIncludingAutoItems(envelope) - oldPoints) > 0) {
		// Translators: Reported when envelope points are added. {} will be replaced
		// with the number of points; e.g. "2 points added".
		outputMessage(format(translate_plural("{} point added", "{} points added", added), added));
	} else if (envelope && (added = CountAutomationItems(envelope) - oldAutoItems) > 0) {
		// Translators: Reported when automation items are added. {} will be
		// replaced with the number of items; e.g. "2 automation items added".
		outputMessage(format(translate_plural("{} automation item added", "{} automation items added", added), added));
	} else {
		outputMessage(translate("nothing pasted"));
	}
}

void cmdhRemoveTracks(int command) {
	int oldCount = CountTracks(0);
	Main_OnCommand(command, 0);
	int removed = oldCount - CountTracks(0);
	// Translators: Reported when tracks are removed. {} will be replaced with the
	// number of tracks; e.g. "2 tracks removed".
	outputMessage(format(translate_plural("{} track removed", "{} tracks removed", removed), removed));
}

void cmdRemoveTracks(Command* command) {
	cmdhRemoveTracks(command->gaccel.accel.cmd);
}

void cmdRemoveOrCopyAreaOfItems(Command* command) {
	double start, end;
	GetSet_LoopTimeRange(false, true, &start, &end, false);
	int selItems = CountSelectedMediaItems(nullptr);
	auto countAffected = [start, end](auto getFunc, int totalCount) {
		int count = 0;
		for (int i = 0; i < totalCount; ++i) {
			MediaItem* item = getFunc(nullptr, i);
			double itemStart = GetMediaItemInfo_Value(item, "D_POSITION");
			double itemEnd = itemStart + GetMediaItemInfo_Value(item, "D_LENGTH");
			if ((start < itemStart && itemStart < end) || (start < itemEnd && itemEnd < end) ||
					(itemStart < start && start < itemEnd) || (itemStart < end && end < itemEnd) ||
					(start == itemStart && end == itemEnd)) {
				++count;
			}
		}
		return count;
	};
	if (start == end) {
		outputMessage(translate("no time selection"));
	} else {
		switch (command->gaccel.accel.cmd) {
			case 40060: // Item: Copy selected area of items
			case 40014: { // Item: Copy loop of selected area of audio items
				if (selItems == 0) {
					outputMessage(translate("no items selected"));
					break;
				}
				int count = countAffected(GetSelectedMediaItem, selItems);
				// Translators: used for  "Item: Copy selected area of items".
				// {} is replaced by the number of items affected.
				outputMessage(format(
						translate_plural("selected area of {} item copied", "selected area of {} items copied", count), count
				));
				break;
			}
			default: {
				int count = 0;
				if (selItems == 0) { // these commands treat no item selection as if all items are selected
					count = countAffected(GetMediaItem, CountMediaItems(nullptr));
				} else {
					count = countAffected(GetSelectedMediaItem, selItems);
				}
				// Translators: used for  "Item: Cut selected area of items" and "Item:
				// Remove selected area of items".  {} is replaced by the number of items
				// affected.
				outputMessage(format(
						translate_plural("selected area of {} item removed", "selected area of {} items removed", count), count
				));
			}
		}
	}
	Main_OnCommand(command->gaccel.accel.cmd, 0);
}

void cmdhRemoveItems(int command) {
	int oldCount = CountMediaItems(0);
	Main_OnCommand(command, 0);
	int removed = oldCount - CountMediaItems(0);
	// Translators: Reported when items are removed. {} will be replaced with the
	// number of items; e.g. "2 items removed".
	outputMessage(format(translate_plural("{} item removed", "{} items removed", removed), removed));
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
		case 2: // Envelope
			cmdhDeleteEnvelopePointsOrAutoItems(command->gaccel.accel.cmd);
			return;
	}
}

void cmdRemoveTimeSelection(Command* command) {
	double start, end;
	GetSet_LoopTimeRange(false, false, &start, &end, false);
	Main_OnCommand(40201, 0); // Time selection: Remove contents of time selection (moving later items)
	if (start != end) {
		outputMessage(translate("contents of time selection removed"));
	}
}

void cmdMoveItemEdge(Command* command) {
	MediaItem* item = getItemWithFocus();
	if (!item) {
		outputMessage(translate("no items selected"));
		Main_OnCommand(command->gaccel.accel.cmd, 0);
		return;
	}
	ostringstream s;
	auto cache = FT_USE_CACHE;
	if (lastCommand != command->gaccel.accel.cmd) {
		s << getActionName(command->gaccel.accel.cmd) << " ";
		cache = FT_NO_CACHE;
	}
	double oldStart = GetMediaItemInfo_Value(item, "D_POSITION");
	double oldEnd = oldStart + GetMediaItemInfo_Value(item, "D_LENGTH");
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	if (!shouldReportTimeMovement()) {
		return;
	}
	double newStart = GetMediaItemInfo_Value(item, "D_POSITION");
	double newEnd = newStart + GetMediaItemInfo_Value(item, "D_LENGTH");
	if (newStart != oldStart)
		s << formatTime(newStart, TF_RULER, false, cache);
	else if (newEnd != oldEnd)
		s << formatTime(newEnd, TF_RULER, false, cache);
	else {
		// Translators: Reported when moving items to indicate that no movement
		// occurred.
		s << translate("no change");
	}
	outputMessage(s);
}

void cmdMoveItemsOrEnvPoint(Command* command) {
	if (GetCursorContext2(true) == 2) { // Envelope
		cmdMoveSelEnvelopePoints(command);
	} else {
		cmdMoveItemEdge(command);
	}
}

void cmdDeleteMarker(Command* command) {
	int count = CountProjectMarkers(0, NULL, NULL);
	Main_OnCommand(40613, 0); // Markers: Delete marker near cursor
	if (CountProjectMarkers(0, NULL, NULL) != count)
		outputMessage(translate("marker deleted"));
}

void cmdDeleteRegion(Command* command) {
	int count = CountProjectMarkers(0, NULL, NULL);
	Main_OnCommand(40615, 0); // Markers: Delete region near cursor
	if (CountProjectMarkers(0, NULL, NULL) != count)
		outputMessage(translate("region deleted"));
}

void cmdDeleteTimeSig(Command* command) {
	int count = CountTempoTimeSigMarkers(0);
	Main_OnCommand(40617, 0); // Markers: Delete time signature marker near cursor
	if (CountTempoTimeSigMarkers(0) != count)
		outputMessage(translate("time signature deleted"));
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
		outputMessage(translate("stretch marker deleted"));
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
		outputMessage(translate("cleared time/loop selection"));
}

void cmdUnselAllTracksItemsPoints(Command* command) {
	int old = CountSelectedTracks(0) + CountSelectedMediaItems(0) + (GetSelectedEnvelope(0) ? 1 : 0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	int cur = CountSelectedTracks(0) + CountSelectedMediaItems(0) + (GetSelectedEnvelope(0) ? 1 : 0);
	if (old != cur)
		outputMessage(translate("unselected tracks/items/envelope points"));
}

void cmdSwitchProjectTab(Command* command) {
	ReaProject* oldProj = EnumProjects(-1, nullptr, 0);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	ReaProject* newProj = EnumProjects(-1, nullptr, 0);
	if (newProj == oldProj) {
		return;
	}
	char newName[200];
	GetProjectName(newProj, newName, sizeof(newName));
	if (newName[0]) {
		outputMessage(newName);
	} else {
		// Translators: Reported when switching to a project tab containing an
		// unsaved project.
		outputMessage(translate("[Unsaved]"));
	}
	// The peak watcher needs to know when the project tab changes
	peakWatcher::onSwitchTab();
}

void cmdMoveToNextItemKeepSel(Command* command) {
	if (fakeFocus == FOCUS_ENVELOPE || fakeFocus == FOCUS_AUTOMATIONITEM) {
		moveToAutomationItem(1, false, isSelectionContiguous);
	} else {
		moveToItem(1, false, isSelectionContiguous);
	}
}

void cmdMoveToPrevItemKeepSel(Command* command) {
	if (fakeFocus == FOCUS_ENVELOPE || fakeFocus == FOCUS_AUTOMATIONITEM) {
		moveToAutomationItem(-1, false, isSelectionContiguous);
	} else {
		moveToItem(-1, false, isSelectionContiguous);
	}
}

void cmdPropertiesFocus(Command* command) {
	if (shouldMoveToAutoItem) {
		Main_OnCommand(42090, 0); // Envelope: Automation item properties...
	} else {
		Main_OnCommand(40009, 0); // Item properties: Show media item/take properties
	}
}

void cmdIoMaster(Command* command) {
	Main_OnCommand(42235, 0); // Track: View routing and I/O for master track
}

void cmdReportRippleMode(Command* command) {
	postCycleRippleMode(command->gaccel.accel.cmd);
}

string formatTrackRange(int start, const char* startName, int end, const char* endName, const char* separator) {
	ostringstream s;
	s << start;
	if (startName && startName[0]) {
		s << " " << startName;
	}
	const string startText = s.str();
	const int diff = end - start;
	if (diff == 0) {
		// Single track. Just report that track.
		return startText;
	}
	s.str("");
	s << end;
	if (endName && endName[0]) {
		s << " " << endName;
	}
	const string endText = s.str();
	if (diff == 1) {
		// Two consecutive tracks, so report them separately.
		s.str("");
		s << startText << separator << endText;
		return s.str();
	}
	// Translators: Used when reporting a range of tracks. {start} will be
	// replaced with the first track. {end} will be replaced with the last track
	// in the range. For example: "1 drums through 5 piano"
	return format(translate("{start} through {end}"), "start"_a = startText, "end"_a = endText);
}

template<typename Func>
string formatTracksWithState(
		const char* prefix, Func checkState, bool includeMaster, bool multiLine, bool outputIfNone = true
) {
	const char* separator = multiLine ? "\r\n" : ", ";
	int rangeStart = 0;
	char* startingTrackName;
	char* prevTrackName;
	bool inActiveRange = false;
	ostringstream s;

	if (prefix) {
		s << prefix << ":" << (multiLine ? separator : " ");
	}

	int count = 0;
	if (includeMaster) {
		MediaTrack* master = GetMasterTrack(nullptr);
		if (checkState(master)) {
			++count;
			s << translate("master") << separator;
		}
	}

	int trackCount = CountTracks(0);
	for (int i = 0; i < trackCount; ++i) {
		const int trackNumber = i + 1;
		MediaTrack* track = GetTrack(nullptr, i);
		char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
		if (multiLine) {
			// We don't summarise ranges in this case. We output each track.
			if (checkState(track)) {
				++count;
				if (count > 1) {
					s << separator;
				}
				if (settings::reportTrackNumbers) {
					s << trackNumber;
				}
				if (name && name[0]) {
					if (settings::reportTrackNumbers) {
						s << " ";
					}
					s << name;
				} else if (!settings::reportTrackNumbers) {
					// There's no name and track number reporting is disabled. We report
					// the number in lieu of the name.
					s << i + 1;
				}
			}
			continue;
		}

		if (checkState(track)) {
			prevTrackName = name;
			if (trackNumber == trackCount) {
				// No more tracks. Output the last range.
				++count;
				if (count > 1) {
					s << separator;
				}
				s << formatTrackRange(
						inActiveRange ? rangeStart : trackCount, inActiveRange ? startingTrackName : name, trackCount, name,
						separator
				);
				break;
			}
			if (inActiveRange) {
				// Not interested in tracks within a range, only those at each end.
				continue;
			}
			inActiveRange = true;
			rangeStart = trackNumber;
			startingTrackName = name;
		} else {
			if (inActiveRange) {
				// This track doesn't match. Output the previous range.
				++count;
				if (count > 1) {
					s << separator;
				}
				s << formatTrackRange(rangeStart, startingTrackName, i, prevTrackName, separator);
				startingTrackName = nullptr;
				rangeStart = 0;
				inActiveRange = false;
			}
		}
	}

	if (count == 0) {
		if (!outputIfNone) {
			return "";
		}
		// Translators: Used when reporting all tracks which are muted, soloed, etc.
		// to indicate that no tracks are muted, soloed, etc.
		s << translate("none");
	}
	return s.str();
}

template<typename Func> void reportTracksWithState(const char* prefix, Func checkState, bool includeMaster) {
	bool multiLine = lastCommandRepeatCount == 1;
	string s = formatTracksWithState(multiLine ? nullptr : prefix, checkState, includeMaster, multiLine);
	if (multiLine) {
		reviewMessage(prefix, s.c_str());
	} else {
		outputMessage(s);
	}
}

void cmdReportMutedTracks(Command* command) {
	reportTracksWithState(translate("Muted"), isTrackMuted, /* includeMaster */ true);
}

void cmdReportSoloedTracks(Command* command) {
	bool multiLine = lastCommandRepeatCount == 1;
	ostringstream s;
	s << formatTracksWithState(translate("soloed"), isTrackSoloed, /* includeMaster */ true, multiLine);
	string defeat = formatTracksWithState(
			translate("defeating solo"), isTrackDefeatingSolo, /* includeMaster */ false, multiLine,
			/* outputIfNone */ false
	);
	if (!defeat.empty()) {
		s << (multiLine ? "\r\n\r\n" : "; ") << defeat;
	}
	if (multiLine) {
		reviewMessage(translate("Soloed"), s.str().c_str());
	} else {
		outputMessage(s);
	}
}

void cmdReportArmedTracks(Command* command) {
	reportTracksWithState(translate("Armed"), isTrackArmed, /* includeMaster */ false);
}

void cmdReportMonitoredTracks(Command* command) {
	reportTracksWithState(
			translate("Monitored"), isTrackMonitored,
			/* includeMaster */ false
	);
}

void cmdReportPhaseInvertedTracks(Command* command) {
	reportTracksWithState(
			translate("Phase inverted"), isTrackPhaseInverted,
			/* includeMaster */ false
	);
}

template<typename Func> string formatItemsWithState(Func stateCheck, bool multiLine) {
	const char* separator = multiLine ? "\r\n" : ", ";
	ostringstream s;
	int count = 0;
	for (int t = 0; t < CountTracks(nullptr); ++t) {
		MediaTrack* track = GetTrack(nullptr, t);
		for (int i = 0; i < CountTrackMediaItems(track); ++i) {
			MediaItem* item = GetTrackMediaItem(track, i);
			if (stateCheck(item)) {
				++count;
				if (count > 1) {
					s << separator;
				}
				s << t + 1 << "." << i + 1;
				MediaItem_Take* take = GetActiveTake(item);
				if (take)
					s << " " << GetTakeName(take);
			}
		}
	}
	return s.str();
}

void cmdReportSelection(Command* command) {
	const bool multiLine = lastCommandRepeatCount == 1;
	const char* separator = multiLine ? "\r\n" : ", ";
	ostringstream s;
	// If we're showing a reviewable message, report all selection contexts.
	// Otherwise, only show the selection associated with the focus.
	if (fakeFocus == FOCUS_RULER || multiLine) {
		if (multiLine) {
			s << translate("Time selection:") << separator;
		}
		double start, end;
		GetSet_LoopTimeRange(false, false, &start, &end, false);
		if (start != end) {
			s <<
					// Translators: Used when reporting the time selection. {} will be
					// replaced with the start time; e.g. "start bar 2 beat 1 0%".
					format(translate("start {}"), formatTime(start, TF_RULER, false, FT_NO_CACHE)) << separator <<
					// Translators: Used when reporting the time selection. {} will be
					// replaced with the end time; e.g. "end bar 4 beat 1 0%".
					format(translate("end {}"), formatTime(end, TF_RULER, false, FT_NO_CACHE)) << separator <<
					// Translators: Used when reporting the time selection. {} will be
					// replaced with the length; e.g. "length 2 bars 0 beats 0%".
					format(translate("length {}"), formatTime(end - start, TF_RULER, true, FT_NO_CACHE));
			resetTimeCache();
		} else if (multiLine) {
			s << translate("none");
		} else {
			s << translate("no time selection");
		}
	}

	if (fakeFocus == FOCUS_TRACK || multiLine) {
		if (multiLine) {
			if (s.tellp() > 0) {
				s << separator << separator;
			}
			s << translate("Selected tracks:") << separator;
		}
		s << formatTracksWithState(
				nullptr, isTrackSelected,
				/* includeMaster */ true, multiLine, /* outputIfNone */ multiLine
		);
		if (!multiLine && s.tellp() == 0) {
			s << translate("no selected tracks");
		}
	}

	if (fakeFocus == FOCUS_ITEM || multiLine) {
		if (multiLine) {
			if (s.tellp() > 0) {
				s << separator << separator;
			}
			s << translate("Selected items:") << separator;
		}
		string items = formatItemsWithState(isItemSelected, multiLine);
		if (items.empty()) {
			s << (multiLine ? translate("none") : translate("no selected items"));
		} else {
			s << items;
		}
	}

	if (s.tellp() == 0) {
		outputMessage(translate("no selection"));
		return;
	}
	if (multiLine) {
		reviewMessage("Selection", s.str().c_str());
	} else {
		outputMessage(s);
	}
}

int countTakeMarkersInSelectedTakes() {
	int itemCount = CountSelectedMediaItems(0);
	if (itemCount == 0) {
		return 0;
	}
	int markerCount = 0;
	for (int i = 0; i < itemCount; ++i) {
		MediaItem* item = GetSelectedMediaItem(0, i);
		MediaItem_Take* take = GetActiveTake(item);
		if (!take) {
			continue;
		}
		markerCount += GetNumTakeMarkers(take);
	}
	return markerCount;
}

void cmdhDeleteTakeMarkers(int command) {
	int oldCount = countTakeMarkersInSelectedTakes();
	Main_OnCommand(command, 0);
	int removed = oldCount - countTakeMarkersInSelectedTakes();
	// Translators: Reported when deleting take markers. {} will be replaced with
	// the number of markers; e.g. "2 take markers deleted".
	outputMessage(format(translate_plural("take marker deleted", "{} take markers deleted", removed), removed));
}

void cmdDeleteTakeMarkers(Command* command) {
	cmdhDeleteTakeMarkers(command->gaccel.accel.cmd);
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
			cmdhDeleteEnvelopePointsOrAutoItems(40333, true, false); // Envelope: Delete all selected points
			break;
		case FOCUS_AUTOMATIONITEM:
			cmdhDeleteEnvelopePointsOrAutoItems(42086, false, true); // Envelope: Delete automation items
			break;
		case FOCUS_TAKEMARKER:
			cmdhDeleteTakeMarkers(42386); // Item: Delete take marker at cursor);
			break;
		default:
			cmdRemoveTimeSelection(NULL);
	}
}

void cmdShortcutHelp(Command* command) {
	isShortcutHelpEnabled = !isShortcutHelpEnabled;
	outputMessage(isShortcutHelpEnabled ? translate("shortcut help on") : translate("shortcut help off"));
}

void cmdReportCursorPosition(Command* command) {
	TimeFormat tf;
	if (lastCommandRepeatCount == 0) {
		// Use primary ruler unit.
		tf = TF_RULER;
	} else if (GetToggleCommandState(42361)) {
		tf = TF_MINSEC;
	} else if (GetToggleCommandState(42362)) {
		tf = TF_SEC;
	} else if (GetToggleCommandState(42363)) {
		tf = TF_SAMPLE;
	} else if (GetToggleCommandState(42364)) {
		tf = TF_HMSF;
	} else if (GetToggleCommandState(42365)) {
		tf = TF_FRAME;
	} else {
		tf = TF_RULER;
	}
	int state = GetPlayState();
	double pos = state & 1 ? GetPlayPosition() : GetCursorPosition();
	ostringstream s;
	s << formatTime(pos, tf, false, FT_NO_CACHE) << " ";
	if (state & 2) {
		s << translate("paused");
	} else if (state & 4) {
		s << translate("recording");
	} else if (state & 1) {
		s << translate("playing");
	} else {
		s << translate("stopped");
	}
	outputMessage(s);
}

void cmdToggleSelection(Command* command) {
	if (isSelectionContiguous) {
		isSelectionContiguous = false;
		outputMessage(translate("noncontiguous selection"));
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
		case FOCUS_AUTOMATIONITEM:
			select = toggleCurrentAutomationItemSelection();
			break;
		case FOCUS_ENVELOPE: {
			optional<bool> selectOpt = toggleCurrentEnvelopePointSelection();
			if (!selectOpt)
				return;
			select = *selectOpt;
			break;
		}
		default:
			return;
	}
	outputMessage(select ? translate("selected") : translate("unselected"));
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
		double playRate = *(double*)GetSetMediaItemTakeInfo(take, "D_PLAYRATE", NULL);
		int index = getStretchAtPos(take, lastStretchPos, itemStart, playRate);
		if (index == -1)
			continue;
		// Stretch marker positions are relative to the start of the item and the
		// take's play rate.
		double destPos = (cursor - itemStart) * playRate;
		SetTakeStretchMarker(take, index, destPos, NULL);
		done = true;
	}
	Undo_EndBlock(translate("Move stretch marker"), UNDO_STATE_ITEMS);
	if (done) {
		outputMessage(translate("stretch marker moved"));
	}
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
	Undo_EndBlock(translate("Delete all time signature markers"), UNDO_STATE_ALL);
	outputMessage(translate("deleted all time signature markers"));
}

void moveToTransient(bool previous) {
	bool wasPlaying = GetPlayState() & 1;
	if (wasPlaying) {
		// Moving to transients can be slow, so pause/stop playback so it doesn't drift
		// and potentially cause repeats.
		if (previous && lastCommandRepeatCount > 0) {
			// The user is moving back more than one, so just stop playback;
			// we don't want the edit cursor to be routed to the play position first.
			// Otherwise, the user would just keep hitting the same transient.
			OnStopButton();
		} else {
			// Pause playback so the edit cursor will be routed to the play position.
			// This means we find the transient relative to the play position.
			OnPauseButton();
		}
	}
	if (previous)
		Main_OnCommand(40376, 0); // Item navigation: Move cursor to previous transient in items
	else
		Main_OnCommand(40375, 0); // Item navigation: Move cursor to next transient in items
	if (wasPlaying)
		OnPlayButton();
}

void cmdMoveToNextTransient(Command* command) {
	moveToTransient(false);
}

void cmdMoveToPreviousTransient(Command* command) {
	moveToTransient(true);
}

void cmdShowContextMenu1(Command* command) {
	showReaperContextMenu(0);
}

void cmdShowContextMenu2(Command* command) {
	showReaperContextMenu(1);
}

void cmdShowContextMenu3(Command* command) {
	showReaperContextMenu(2);
}

void cmdReportAutomationMode(Command* command) {
	// This reports the global automation override if set, otherwise the current track automation mode.
	MediaTrack* track = GetLastTouchedTrack();
	const int globalMode = GetGlobalAutomationOverride();
	if (globalMode >= 0) {
		// Translators: Used to report global automation override mode.  {} is
		// replaced with the mode; e.g. "Global automation override latch
		// preview"
		outputMessage(format(translate("global automation override {}"), automationModeAsString(globalMode)));
	} else {
		// Translators: Used to report track automation override mode.  {} is
		// replaced with the mode; e.g. "track automation override latch
		// preview"
		outputMessage(format(translate("track automation mode {}"), automationModeAsString(GetTrackAutomationMode(track))));
	}
}

void cmdToggleGlobalAutomationLatchPreview(Command* command) {
	if (GetGlobalAutomationOverride() == 5) { // in latch preview mode
		SetGlobalAutomationOverride(-1);
		outputMessage(translate("global automation override off"));
	} else { // not in latch preview.
		SetGlobalAutomationOverride(5);
		outputMessage(translate("global automation override latch preview"));
	}
}

void cmdCycleTrackAutomation(Command* command) {
	int count = CountSelectedTracks2(0, true);
	if (count == 0) {
		outputMessage(translate("no selected tracks"));
		return;
	}
	int oldmode = GetTrackAutomationMode(GetLastTouchedTrack());
	int newmode = oldmode + 1;
	if (newmode > 5)
		newmode = 0;
	for (int tracknum = 0; tracknum < count; ++tracknum) {
		SetTrackAutomationMode(GetSelectedTrack2(0, tracknum, true), newmode);
	}
	// Translators: Report the track automation mode. {} is replaced with the
	// automation mode; e.g. "automation mode trim/read for selected tracks"
	outputMessage(format(translate("{} automation mode for selected tracks"), automationModeAsString(newmode)));
}

void cmdCycleMidiRecordingMode(Command* command) {
	int count = CountSelectedTracks2(0, false);
	if (count == 0) {
		outputMessage(translate("no selected tracks"));
		return;
	}
	int oldmode = (int)GetMediaTrackInfo_Value(GetLastTouchedTrack(), "I_RECMODE");
	int newmode = 0;
	switch (oldmode) {
		case 0: // input (audio or MIDI)
			newmode = 4;
			break;
		case 4: // MIDI output
			newmode = 7;
			break;
		case 7: // MIDI overdub
			newmode = 8;
			break;
		case 8: // MIDI replace
			newmode = 9;
			break;
		case 9: // MIDI touch-replace
			newmode = 16;
			break;
		case 16: // MIDI latch-replace
			newmode = 0;
			break;
		default: // when not in one of the MIDI rec. modes set to 'input (audio or MIDI)'
			newmode = 0;
	}
	for (int tracknum = 0; tracknum < count; ++tracknum) {
		MediaTrack* track = GetSelectedTrack2(0, tracknum, false);
		SetMediaTrackInfo_Value(track, "I_RECMODE", newmode);
	}
	outputMessage(recordingModeAsString(newmode));
}

void cmdNudgeTimeSelection(Command* command) {
	bool first = (lastCommand != command->gaccel.accel.cmd);
	double oldStart, oldEnd, newStart, newEnd;
	GetSet_LoopTimeRange(false, false, &oldStart, &oldEnd, false);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	GetSet_LoopTimeRange(false, false, &newStart, &newEnd, false);
	if (!shouldReportTimeMovement()) {
		return;
	}
	auto cache = first ? FT_NO_CACHE : FT_USE_CACHE;
	ostringstream s;
	if (newStart != oldStart) {
		if (first) {
			s << translate("time selection start") << " ";
		}
		s << formatTime(newStart, TF_RULER, false, cache, false);
	} else if (newEnd != oldEnd) {
		if (first) {
			s << translate("time selection end") << " ";
		}
		s << formatTime(newEnd, TF_RULER, false, cache, false);
	}
	outputMessage(s);
}

void cmdAbout(Command* command) {
	ostringstream s;
	// Translators: OSARA's full name presented in the About dialog.
	s << translate("OSARA: Open Source Accessibility for the REAPER Application") << "\r\n"
		<<
			// Translators: osara version. {} is replaced with the version; e.g.
			// "Version: 2021.1pre-588,0531135a"
			format(translate("Version: {}"), OSARA_VERSION) << "\r\n"
		<< OSARA_COPYRIGHT;
	reviewMessage(translate("About OSARA"), s.str().c_str());
}

// The Transient Detection Settings dialog deliberately passes most keys to the
// main section. This makes it impossible for keyboard users to navigate.
// To work around this, when this dialog is opened, we register an accelerator
// hook which passes tab, arrow keys, etc. to the dialog.
accelerator_register_t transDetect_accelReg;

int transDetect_translateAccel(MSG* msg, accelerator_register_t* accelReg) {
	HWND transDialog = (HWND)accelReg->user;
	if (!IsWindow(transDialog)) {
		// Dialog was closed. We don't need this hook any more.
		plugin_register("-accelerator", accelReg);
		return 0; // Normal handling.
	}
	if (msg->message != WM_KEYDOWN || GetParent(msg->hwnd) != transDialog) {
		return 0; // Normal handling.
	}
	switch (msg->wParam) {
		case VK_TAB:
		case VK_RIGHT:
		case VK_LEFT:
		case VK_UP:
		case VK_DOWN:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_HOME:
		case VK_END:
		case VK_SPACE:
			return -1; // pass to window.
		default:
			break;
	}
	return 0; // Normal handling.
}

void cmdTransientDetectionSettings(Command* command) {
	if (GetToggleCommandState(command->gaccel.accel.cmd)) {
		// Dialog is showing. Just run the command to dismiss it.
		Main_OnCommand(command->gaccel.accel.cmd, 0);
		plugin_register("-accelerator", &transDetect_accelReg);
		return;
	}
	transDetect_accelReg.translateAccel = &transDetect_translateAccel;
	transDetect_accelReg.isLocal = true;
	// We must register the hook before the dialog appears or it won't work.
	plugin_register("accelerator", &transDetect_accelReg);
	// Open the dialog.
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	transDetect_accelReg.user = GetForegroundWindow(); // The dialog.
}

void cmdInsertMarker(Command* command) {
	if (!shouldReportTimeMovement()) {
		Main_OnCommand(command->gaccel.accel.cmd, 0);
		return;
	}
	int count = CountProjectMarkers(nullptr, nullptr, nullptr);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	if (CountProjectMarkers(nullptr, nullptr, nullptr) == count) {
		return; // Not inserted.
	}
	int marker;
	GetLastMarkerAndCurRegion(nullptr, GetCursorPosition(), &marker, nullptr);
	if (marker < 0) {
		return;
	}
	int number;
	EnumProjectMarkers(marker, nullptr, nullptr, nullptr, nullptr, &number);
	// Translators: Reported when inserting a marker. {} will be replaced with the
	// number of the new marker; e.g. "marker 2 inserted".
	outputMessage(format(translate("marker {} inserted"), number));
}

void cmdInsertRegion(Command* command) {
	if (!shouldReportTimeMovement()) {
		Main_OnCommand(command->gaccel.accel.cmd, 0);
		return;
	}
	int oldCount = CountProjectMarkers(nullptr, nullptr, nullptr);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	int newCount = CountProjectMarkers(nullptr, nullptr, nullptr);
	if (newCount == oldCount) {
		return; // Not inserted.
	}
	double selStart;
	GetSet_LoopTimeRange(false, false, &selStart, nullptr, false);
	int region;
	GetLastMarkerAndCurRegion(nullptr, selStart, nullptr, &region);
	if (region < 0) {
		return;
	}
	// if there are multiple regions starting at the same position, REAPER might
	// not return the region just added. Find the most recently added.
	for (int m = region + 1; m < newCount; ++m) {
		bool isRegion;
		double start;
		EnumProjectMarkers(m, &isRegion, &start, nullptr, nullptr, nullptr);
		if (start > selStart) {
			break;
		}
		assert(start == selStart);
		if (!isRegion) {
			continue;
		}
		region = m;
	}
	int number;
	EnumProjectMarkers(region, nullptr, nullptr, nullptr, nullptr, &number);
	// Translators: Reported when inserting a region. {} will be replaced with the
	// number of the new region; e.g. "region 2 inserted".
	outputMessage(format(translate("region {} inserted"), number));
}

void cmdChangeItemGroup(Command* command) {
	MediaItem* item = getItemWithFocus();
	if (!item) {
		Main_OnCommand(command->gaccel.accel.cmd, 0);
		return;
	}
	int selCount = CountSelectedMediaItems(nullptr);
	int oldGroupId = *(int*)GetSetMediaItemInfo(item, "I_GROUPID", nullptr);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	int newGroupId = *(int*)GetSetMediaItemInfo(item, "I_GROUPID", nullptr);
	if (newGroupId) {
		// Translators: Reported when adding items to a group. {count} will be
		// replaced with the number of items. {group} will be replaced with the
		// group number. For example: "2 items added to group 1".
		outputMessage(format(
				translate_plural("item added to group {group}", "{count} items added to group {group}", selCount),
				"count"_a = selCount, "group"_a = newGroupId
		));
	} else if (oldGroupId) {
		// Translators: Reported when removing items from a group. {count} will be
		// replaced with the number of items. {group} will be replaced with the group
		// number. For example: "2 items removed from group 1".
		outputMessage(format(
				translate_plural("item removed from group {group}", "{count} items removed from group {group}", selCount),
				"count"_a = selCount, "group"_a = oldGroupId
		));
	}
}

void cmdReportTrackGroups(Command* command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track) {
		return;
	}
	map<int, vector<const char*>> groups;
	for (auto& toggle : TRACK_GROUP_TOGGLES) {
		int mask = GetSetTrackGroupMembership(track, toggle.name, 0, 0);
		for (int g = 0; g < 32; ++g) {
			if (mask & (1 << g)) {
				groups[g].push_back(toggle.displayName);
			}
		}
	}
	ostringstream s;
	for (auto [group, toggles] : groups) {
		if (s.tellp() > 0) {
			s << "; ";
		}
		ostringstream desc;
		desc << "TRACK_GROUP_NAME:" << group + 1;
		char groupName[200];
		GetSetProjectInfo_String(nullptr, desc.str().c_str(), groupName, false);
		if (groupName[0]) {
			s << groupName;
		} else {
			s << group + 1;
		}
		s << ": ";
		bool first = true;
		for (auto* toggle : toggles) {
			if (first) {
				first = false;
			} else {
				s << ", ";
			}
			s << translate(toggle);
		}
	}
	if (s.tellp() == 0) {
		outputMessage(translate("track not grouped"));
		return;
	}
	outputMessage(s);
}

void cmdMuteNextMessage(Command* command) {
	muteNextMessage = true;
}

void cmdToggleLoopSegScrub(Command* command) {
	if (settings::moveFromPlayCursor && (GetPlayState() & 1)) {
		SetEditCurPos(GetPlayPosition(), false, false);
	}
	Main_OnCommand(command->gaccel.accel.cmd, 0);
}

void cmdReportRegionMarkerItems(Command* command) {
	const bool multiLine = lastCommandRepeatCount == 1;
	ostringstream s;
	auto separate = [&s, multiLine]() {
		if (s.tellp()) {
			s << (multiLine ? "\r\n" : ", ");
		}
	};
	double pos = GetPlayState() ? GetPlayPosition() : GetCursorPosition();
	double start, end;
	bool isrgn;
	int number;
	const char* name{nullptr};
	int idx = 0;
	while (EnumProjectMarkers(idx++, &isrgn, &start, &end, &name, &number) > 0) {
		if (!(isrgn && start <= pos && pos <= end)) {
			continue;
		}
		separate();
		if (name[0]) {
			s << name;
		} else {
			// Translators: used to report an unnamed region. {} is replaced with the region number.
			s << format(translate("region {}"), number);
		}
	}
	int markerIdx;
	GetLastMarkerAndCurRegion(nullptr, pos, &markerIdx, nullptr);
	if (markerIdx >= 0) {
		EnumProjectMarkers(markerIdx, nullptr, nullptr, nullptr, &name, &number);
		separate();
		if (name[0]) {
			s << name;
		} else {
			// Translators: used to report an unnamed marker. {} is replaced with the marker number.
			s << format(translate("marker {}"), number);
		}
	}
	separate();
	s << formatItemsWithState(
			[pos](MediaItem* item) -> bool {
				MediaTrack* track = GetMediaItem_Track(item);
				return (isTrackSelected(track) && isPosInItem(pos, item));
			},
			multiLine
	);
	if (multiLine) {
		// Translators: The title of the review message for the action "OSARA: Report regions, last project marker and items
		// on selected tracks at current position".
		reviewMessage(translate("At Current Position"), s.str().c_str());
	} else {
		outputMessage(s);
	}
}

#define DEFACCEL \
	{ 0, 0, 0 }

Command COMMANDS[] = {
		// Commands we want to intercept.
		{MAIN_SECTION, {{0, 0, 40285}, NULL}, NULL, cmdGoToNextTrack}, // Track: Go to next track
		{MAIN_SECTION, {{0, 0, 40286}, NULL}, NULL, cmdGoToPrevTrack}, // Track: Go to previous track
		{MAIN_SECTION, {{0, 0, 40287}, NULL}, NULL, cmdGoToNextTrackKeepSel
		}, // Track: Go to next track (leaving other tracks selected)
		{MAIN_SECTION, {{0, 0, 40288}, NULL}, NULL, cmdGoToPrevTrackKeepSel
		}, // Track: Go to previous track (leaving other tracks selected)
		{MAIN_SECTION, {{0, 0, 40417}, NULL}, NULL, cmdMoveToNextItem}, // Item navigation: Select and move to next item
		{MAIN_SECTION, {{0, 0, 40416}, NULL}, NULL, cmdMoveToPrevItem}, // Item navigation: Select and move to previous item
		{MAIN_SECTION, {{0, 0, 40029}, NULL}, NULL, cmdUndo}, // Edit: Undo
		{MAIN_SECTION, {{0, 0, 40030}, NULL}, NULL, cmdRedo}, // Edit: Redo
		{MAIN_SECTION, {{0, 0, 40012}, NULL}, NULL, cmdSplitItems}, // Item: Split items at edit or play cursor
		{MAIN_SECTION, {{0, 0, 40061}, NULL}, NULL, cmdSplitItems}, // Item: Split items at time selection
		{MAIN_SECTION, {{0, 0, 40058}, NULL}, NULL, cmdPaste
		}, // Item: Paste items/tracks (old-style handling of hidden tracks)
		{MAIN_SECTION, {{0, 0, 42398}, NULL}, NULL, cmdPaste}, // Item: Paste items/tracks
		{MAIN_SECTION, {{0, 0, 40062}, NULL}, NULL, cmdPaste}, // Track: Duplicate tracks
		{MAIN_SECTION, {{0, 0, 40005}, NULL}, NULL, cmdRemoveTracks}, // Track: Remove tracks
		{MAIN_SECTION, {{0, 0, 40337}, NULL}, NULL, cmdRemoveTracks}, // Track: Cut tracks
		{MAIN_SECTION, {{0, 0, 40006}, NULL}, NULL, cmdRemoveItems}, // Item: Remove items
		{MAIN_SECTION, {{0, 0, 40699}, NULL}, NULL, cmdRemoveItems}, // Edit: Cut items
		{MAIN_SECTION, {{0, 0, 40333}, NULL}, NULL, cmdDeleteEnvelopePoints}, // Envelope: Delete all selected points
		{MAIN_SECTION, {{0, 0, 40089}, NULL}, NULL, cmdDeleteEnvelopePoints
		}, // Envelope: Delete all points in time selection
		{MAIN_SECTION, {{0, 0, 40336}, NULL}, NULL, cmdDeleteEnvelopePoints}, // Envelope: Cut selected points
		{MAIN_SECTION, {{0, 0, 40325}, NULL}, NULL, cmdDeleteEnvelopePoints}, // Envelope: Cut points within time selection
		{MAIN_SECTION, {{0, 0, 40059}, NULL}, NULL, cmdCut
		}, // Edit: Cut items/tracks/envelope points (depending on focus) ignoring time selection
		{MAIN_SECTION, {{0, 0, 40201}, NULL}, NULL, cmdRemoveTimeSelection
		}, // Time selection: Remove contents of time selection (moving later items)
		{MAIN_SECTION, {{0, 0, 40312}, NULL}, NULL, cmdRemoveOrCopyAreaOfItems}, // Item: Remove selected area of items
		{MAIN_SECTION, {{0, 0, 40307}, NULL}, NULL, cmdRemoveOrCopyAreaOfItems}, // Item: Cut selected area of items
		{MAIN_SECTION, {{0, 0, 40060}, NULL}, NULL, cmdRemoveOrCopyAreaOfItems}, // Item: Copy selected area of items
		{MAIN_SECTION, {{0, 0, 40014}, NULL}, NULL, cmdRemoveOrCopyAreaOfItems
		}, // Item: Copy loop of selected area of audio items
		{MAIN_SECTION, {{0, 0, 40119}, NULL}, NULL, cmdMoveItemsOrEnvPoint}, // Item edit: Move items/envelope points right
		{MAIN_SECTION, {{0, 0, 40120}, NULL}, NULL, cmdMoveItemsOrEnvPoint}, // Item edit: Move items/envelope points left
		{MAIN_SECTION, {{0, 0, 40793}, NULL}, NULL, cmdMoveItemsOrEnvPoint
		}, // Item edit: Move items/envelope points left by grid size
		{MAIN_SECTION, {{0, 0, 40794}, NULL}, NULL, cmdMoveItemsOrEnvPoint
		}, // Item edit: Move items/envelope points right by grid size
		{MAIN_SECTION, {{0, 0, 40225}, NULL}, NULL, cmdMoveItemEdge}, // Item edit: Grow left edge of items
		{MAIN_SECTION, {{0, 0, 40226}, NULL}, NULL, cmdMoveItemEdge}, // Item edit: Shrink left edge of items
		{MAIN_SECTION, {{0, 0, 40227}, NULL}, NULL, cmdMoveItemEdge}, // Item edit: Shrink right edge of items
		{MAIN_SECTION, {{0, 0, 40228}, NULL}, NULL, cmdMoveItemEdge}, // Item edit: Grow right edge of items
		{MAIN_SECTION, {{0, 0, 40613}, NULL}, NULL, cmdDeleteMarker}, // Markers: Delete marker near cursor
		{MAIN_SECTION, {{0, 0, 40615}, NULL}, NULL, cmdDeleteRegion}, // Markers: Delete region near cursor
		{MAIN_SECTION, {{0, 0, 40617}, NULL}, NULL, cmdDeleteTimeSig}, // Markers: Delete time signature marker near cursor
		{MAIN_SECTION, {{0, 0, 41859}, NULL}, NULL, cmdRemoveStretch}, // Item: remove stretch marker at current position
		{MAIN_SECTION, {{0, 0, 40020}, NULL}, NULL, cmdClearTimeLoopSel
		}, // Time selection: Remove time selection and loop point selection
		{MAIN_SECTION, {{0, 0, 40769}, NULL}, NULL, cmdUnselAllTracksItemsPoints
		}, // Unselect all tracks/items/envelope points
		{MAIN_SECTION, {{0, 0, 40915}, NULL}, NULL, cmdInsertEnvelopePoint
		}, // Envelope: Insert new point at current position (remove nearby points)
		{MAIN_SECTION, {{0, 0, 40106}, NULL}, NULL, cmdInsertEnvelopePoint
		}, // Envelope: Insert new point at current position (do not remove nearby points)
		{MAIN_SECTION, {{0, 0, 40860}, NULL}, NULL, cmdSwitchProjectTab}, // Close current project tab
		{MAIN_SECTION, {{0, 0, 41816}, NULL}, NULL, cmdSwitchProjectTab}, // Item: Open associated project in new tab
		{MAIN_SECTION, {{0, 0, 40859}, NULL}, NULL, cmdSwitchProjectTab}, // New project tab
		{MAIN_SECTION, {{0, 0, 41929}, NULL}, NULL, cmdSwitchProjectTab}, // New project tab (ignore default template)
		{MAIN_SECTION, {{0, 0, 40861}, NULL}, NULL, cmdSwitchProjectTab}, // Next project tab
		{MAIN_SECTION, {{0, 0, 40862}, NULL}, NULL, cmdSwitchProjectTab}, // Previous project tab
		{MAIN_SECTION, {{0, 0, 40320}, NULL}, NULL, cmdNudgeTimeSelection}, // Time selection: Nudge left edge left
		{MAIN_SECTION, {{0, 0, 40321}, NULL}, NULL, cmdNudgeTimeSelection}, // Time selection: Nudge left edge right
		{MAIN_SECTION, {{0, 0, 40322}, NULL}, NULL, cmdNudgeTimeSelection}, // Time selection: Nudge right edge left
		{MAIN_SECTION, {{0, 0, 40323}, NULL}, NULL, cmdNudgeTimeSelection}, // Time selection: Nudge right edge right
		{MAIN_SECTION, {{0, 0, 40039}, NULL}, NULL, cmdNudgeTimeSelection}, // Time selection: Nudge left
		{MAIN_SECTION, {{0, 0, 40040}, NULL}, NULL, cmdNudgeTimeSelection}, // Time selection: Nudge right
		{MAIN_SECTION, {{0, 0, 40037}, NULL}, NULL, cmdNudgeTimeSelection
		}, // Time selection: Shift left (by time selection length)
		{MAIN_SECTION, {{0, 0, 40038}, NULL}, NULL, cmdNudgeTimeSelection
		}, // Time selection: Shift right (by time selection length)
		{MAIN_SECTION, {{0, 0, 40803}, NULL}, NULL, cmdNudgeTimeSelection
		}, // Time selection: Swap left edge of time selection to next transient in items
		{MAIN_SECTION, {{0, 0, 40802}, NULL}, NULL, cmdNudgeTimeSelection
		}, // Time selection: Extend time selection to next transient in items
		{MAIN_SECTION, {{0, 0, 41142}, NULL}, NULL, cmdToggleTrackEnvelope
		}, // FX: Show/hide track envelope for last touched FX parameter
		{MAIN_SECTION, {{0, 0, 40406}, NULL}, NULL, cmdToggleTrackEnvelope}, // Track: Toggle track volume envelope visible
		{MAIN_SECTION, {{0, 0, 40407}, NULL}, NULL, cmdToggleTrackEnvelope}, // Track: Toggle track pan envelope visible
		{MAIN_SECTION, {{0, 0, 40408}, NULL}, NULL, cmdToggleTrackEnvelope
		}, // Track: Toggle track pre-FX volume envelope visible
		{MAIN_SECTION, {{0, 0, 40409}, NULL}, NULL, cmdToggleTrackEnvelope
		}, // Track: Toggle track pre-FX pan envelope visible
		{MAIN_SECTION, {{0, 0, 40867}, NULL}, NULL, cmdToggleTrackEnvelope}, // Track: Toggle track mute envelope visible
		{MAIN_SECTION, {{0, 0, 40693}, NULL}, NULL, cmdToggleTakeEnvelope}, // Take: Toggle take volume envelope
		{MAIN_SECTION, {{0, 0, 40694}, NULL}, NULL, cmdToggleTakeEnvelope}, // Take: Toggle take pan envelope
		{MAIN_SECTION, {{0, 0, 41612}, NULL}, NULL, cmdToggleTakeEnvelope}, // Take: Toggle take pitch envelope
		{MAIN_SECTION, {{0, 0, 40695}, NULL}, NULL, cmdToggleTakeEnvelope}, // Take: Toggle take mute envelope
		{MAIN_SECTION, {{0, 0, 42386}, NULL}, NULL, cmdDeleteTakeMarkers}, // Item: Delete take marker at cursor
		{MAIN_SECTION, {{0, 0, 42387}, NULL}, NULL, cmdDeleteTakeMarkers}, // Item: Delete all take markers
		{MAIN_SECTION, {{0, 0, 41208}, NULL}, NULL, cmdTransientDetectionSettings
		}, // Transient detection sensitivity/threshold: Adjust...
		{MAIN_SECTION, {{0, 0, 40157}, NULL}, NULL, cmdInsertMarker}, // Markers: Insert marker at current position
		{MAIN_SECTION, {{0, 0, 40174}, NULL}, NULL, cmdInsertRegion}, // Markers: Insert region from time selection
		{MAIN_SECTION, {{0, 0, 40032}, NULL}, NULL, cmdChangeItemGroup}, // Item grouping: Group items
		{MAIN_SECTION, {{0, 0, 40033}, NULL}, NULL, cmdChangeItemGroup}, // Item grouping: Remove items from group
		{MAIN_SECTION, {{0, 0, 41187}, NULL}, NULL, cmdToggleLoopSegScrub
		}, // Scrub: Toggle looped-segment scrub at edit cursor
		{MIDI_EDITOR_SECTION, {{0, 0, 40036}, NULL}, NULL, cmdMidiMoveCursor}, // View: Go to start of file
		{MIDI_EVENT_LIST_SECTION, {{0, 0, 40036}, NULL}, NULL, cmdMidiMoveCursor}, // View: Go to start of file
		{MIDI_EDITOR_SECTION, {{0, 0, 40037}, NULL}, NULL, cmdMidiMoveCursor}, // View: Go to end of file
		{MIDI_EVENT_LIST_SECTION, {{0, 0, 40037}, NULL}, NULL, cmdMidiMoveCursor}, // View: Go to end of file
		{MIDI_EDITOR_SECTION, {{0, 0, 40440}, NULL}, NULL, cmdMidiMoveCursor
		}, // Navigate: Move edit cursor to start of selected events
		{MIDI_EDITOR_SECTION, {{0, 0, 40639}, NULL}, NULL, cmdMidiMoveCursor
		}, // Navigate: Move edit cursor to end of selected events
		{MIDI_EDITOR_SECTION, {{0, 0, 40046}, NULL}, NULL, cmdMidiNoteSplitOrJoin}, // Edit: Split notes
		{MIDI_EDITOR_SECTION, {{0, 0, 40047}, NULL}, NULL, cmdMidiMoveCursor}, // Navigate: Move edit cursor left by grid
		{MIDI_EDITOR_SECTION, {{0, 0, 40048}, NULL}, NULL, cmdMidiMoveCursor}, // Navigate: Move edit cursor right by grid
		{MIDI_EDITOR_SECTION, {{0, 0, 40185}, NULL}, NULL, cmdMidiMoveCursor}, // Edit: Move edit cursor left one pixel
		{MIDI_EDITOR_SECTION, {{0, 0, 40186}, NULL}, NULL, cmdMidiMoveCursor}, // Edit: Move edit cursor right one pixel
		{MIDI_EDITOR_SECTION, {{0, 0, 40456}, NULL}, NULL, cmdMidiNoteSplitOrJoin}, // Edit: Join notes
		{MIDI_EVENT_LIST_SECTION, {{0, 0, 40456}, NULL}, NULL, cmdMidiNoteSplitOrJoin}, // Edit: Join notes
		{MIDI_EDITOR_SECTION, {{0, 0, 40682}, NULL}, NULL, cmdMidiMoveCursor
		}, // Navigate: Move edit cursor right one measure
		{MIDI_EDITOR_SECTION, {{0, 0, 40683}, NULL}, NULL, cmdMidiMoveCursor
		}, // Navigate: Move edit cursor left one measure
		{MIDI_EDITOR_SECTION, {{0, 0, 40667}, NULL}, NULL, cmdMidiDeleteEvents}, // Edit: Delete events
		{MIDI_EVENT_LIST_SECTION, {{0, 0, 40667}, NULL}, NULL, cmdMidiDeleteEvents}, // Edit: Delete events
		{MIDI_EDITOR_SECTION, {{0, 0, 40051}, NULL}, NULL, cmdMidiInsertNote}, // Edit: Insert note at edit cursor
		{MIDI_EDITOR_SECTION, {{0, 0, 1000}, NULL}, NULL, cmdMidiInsertNote
		}, // Edit: Insert note at edit cursor (no advance edit cursor)
		{MIDI_EDITOR_SECTION, {{0, 0, 40835}, NULL}, NULL, cmdMidiMoveToTrack}, // Activate next MIDI track
		{MIDI_EVENT_LIST_SECTION, {{0, 0, 40835}, NULL}, NULL, cmdMidiMoveToTrack}, // Activate next MIDI track
		{MIDI_EDITOR_SECTION, {{0, 0, 40836}, NULL}, NULL, cmdMidiMoveToTrack}, // Activate previous MIDI track
		{MIDI_EVENT_LIST_SECTION, {{0, 0, 40836}, NULL}, NULL, cmdMidiMoveToTrack}, // Activate previous MIDI track
		{MIDI_EDITOR_SECTION, {{0, 0, 40664}, NULL}, NULL, cmdMidiToggleSelCC
		}, // Edit: Toggle selection of all CC events under selected notes
#ifdef _WIN32
		{MIDI_EDITOR_SECTION, {{0, 0, 40762}, NULL}, NULL, cmdMidiFilterWindow}, // Filter: Show/hide filter window...
		{MIDI_EDITOR_SECTION, {{0, 0, 40471}, NULL}, NULL, cmdMidiFilterWindow
		}, // Filter: Enable/disable event filter and show/hide filter window...
		{MIDI_EVENT_LIST_SECTION, {{0, 0, 40762}, NULL}, NULL, cmdMidiFilterWindow}, // Filter: Show/hide filter window...
		{MIDI_EVENT_LIST_SECTION, {{0, 0, 40471}, NULL}, NULL, cmdMidiFilterWindow
		}, // Filter: Enable/disable event filter and show/hide filter window...
#endif
		// Our own commands.
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Move to next item (leaving other items selected)")},
		 "OSARA_NEXTITEMKEEPSEL",
		 cmdMoveToNextItemKeepSel},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Move to previous item (leaving other items selected)")},
		 "OSARA_PREVITEMKEEPSEL",
		 cmdMoveToPrevItemKeepSel},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: View properties for current media item/take/automation item (depending on focus)")},
		 "OSARA_PROPERTIES",
		 cmdPropertiesFocus},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: View parameters for current track/item/FX (depending on focus)")},
		 "OSARA_PARAMS",
		 cmdParamsFocus},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: View FX parameters for current track/take (depending on focus)")},
		 "OSARA_FXPARAMS",
		 cmdFxParamsFocus},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: View FX parameters for master track")},
		 "OSARA_FXPARAMSMASTER",
		 cmdFxParamsMaster},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Configure Peak Watcher for current track/track FX (depending on focus)")},
		 "OSARA_PEAKWATCHER",
		 cmdPeakWatcher},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report Peak Watcher value for first watcher first channel")},
		 "OSARA_REPORTPEAKWATCHERT1C1",
		 cmdReportPeakWatcherW1C1},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report Peak Watcher value for first watcher second channel")},
		 "OSARA_REPORTPEAKWATCHERT1C2",
		 cmdReportPeakWatcherW1C2},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report Peak Watcher value for second watcher first channel")},
		 "OSARA_REPORTPEAKWATCHERT2C1",
		 cmdReportPeakWatcherW2C1},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report Peak Watcher value for second watcher second channel")},
		 "OSARA_REPORTPEAKWATCHERT2C2",
		 cmdReportPeakWatcherW2C2},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Reset Peak Watcher first watcher")},
		 "OSARA_RESETPEAKWATCHERT1",
		 cmdResetPeakWatcherW1},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Reset Peak Watcher second watcher")},
		 "OSARA_RESETPEAKWATCHERT2",
		 cmdResetPeakWatcherW2},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Pause/resume Peak Watcher")}, "OSARA_PAUSEPEAKWATCHER", cmdPausePeakWatcher},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Report ripple editing mode")}, "OSARA_REPORTRIPPLE", cmdReportRippleMode},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Report muted tracks")}, "OSARA_REPORTMUTED", cmdReportMutedTracks},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Report soloed tracks")}, "OSARA_REPORTSOLOED", cmdReportSoloedTracks},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Report record armed tracks")}, "OSARA_REPORTARMED", cmdReportArmedTracks},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report tracks with record monitor on")},
		 "OSARA_REPORTMONITORED",
		 cmdReportMonitoredTracks},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report tracks with phase inverted")},
		 "OSARA_REPORTPHASED",
		 cmdReportPhaseInvertedTracks},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report track/item/time selection (depending on focus)")},
		 "OSARA_REPORTSEL",
		 cmdReportSelection},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Remove items/tracks/contents of time selection/markers/envelope points (depending on focus)")
		 },
		 "OSARA_REMOVE",
		 cmdRemoveFocus},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Toggle shortcut help")}, "OSARA_SHORTCUTHELP", cmdShortcutHelp},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report edit/play cursor position and transport state")},
		 "OSARA_CURSORPOS",
		 cmdReportCursorPosition},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Enable noncontiguous selection/toggle selection of current track/item (depending on focus)")
		 },
		 "OSARA_TOGGLESEL",
		 cmdToggleSelection},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Move last focused stretch marker to current edit cursor position")},
		 "OSARA_MOVESTRETCH",
		 cmdMoveStretch},
		{MAIN_SECTION,
		 {DEFACCEL,
			_t("OSARA: Report level in peak dB at play cursor for channel 1 of current track (reports input level instead when track is armed)"
			)},
		 "OSARA_REPORTPEAKCURRENTC1",
		 cmdReportPeakCurrentC1},
		{MAIN_SECTION,
		 {DEFACCEL,
			_t("OSARA: Report level in peak dB at play cursor for channel 2 of current track (reports input level instead when track is armed)"
			)},
		 "OSARA_REPORTPEAKCURRENTC2",
		 cmdReportPeakCurrentC2},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report level in peak dB at play cursor for channel 1 of master track")},
		 "OSARA_REPORTPEAKMASTERC1",
		 cmdReportPeakMasterC1},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report level in peak dB at play cursor for channel 2 of master track")},
		 "OSARA_REPORTPEAKMASTERC2",
		 cmdReportPeakMasterC2},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Delete all time signature markers")},
		 "OSARA_DELETEALLTIMESIGS",
		 cmdDeleteAllTimeSigs},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Select next track/take envelope (depending on focus)")},
		 "OSARA_SELECTNEXTENV",
		 cmdSelectNextEnvelope},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Select previous track/take envelope (depending on focus)")},
		 "OSARA_SELECTPREVENV",
		 cmdSelectPreviousEnvelope},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Move to next envelope point")},
		 "OSARA_NEXTENVPOINT",
		 cmdMoveToNextEnvelopePoint},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Move to previous envelope point")},
		 "OSARA_PREVENVPOINT",
		 cmdMoveToPrevEnvelopePoint},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Move to next envelope point (leaving other points selected)")},
		 "OSARA_NEXTENVPOINTKEEPSEL",
		 cmdMoveToNextEnvelopePointKeepSel},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Move to previous envelope point (leaving other points selected)")},
		 "OSARA_PREVENVPOINTKEEPSEL",
		 cmdMoveToPrevEnvelopePointKeepSel},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Move to next transient")}, "OSARA_NEXTTRANSIENT", cmdMoveToNextTransient},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Move to previous transient")},
		 "OSARA_PREVTRANSIENT",
		 cmdMoveToPreviousTransient},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Show first context menu (depending on focus)")},
		 "OSARA_CONTEXTMENU1",
		 cmdShowContextMenu1},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Show second context menu (depending on focus)")},
		 "OSARA_CONTEXTMENU2",
		 cmdShowContextMenu2},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Show third context menu (depending on focus)")},
		 "OSARA_CONTEXTMENU3",
		 cmdShowContextMenu3},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Cycle through midi recording modes of selected tracks")},
		 "OSARA_CYCLEMIDIRECORDINGMODE",
		 cmdCycleMidiRecordingMode},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Configuration")}, "OSARA_CONFIG", cmdConfig},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report global / Track Automation Mode")},
		 "OSARA_REPORTAUTOMATIONMODE",
		 cmdReportAutomationMode},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Toggle global automation override between latch preview and off")},
		 "OSARA_TOGGLEGLOBALAUTOMATIONLATCHPREVIEW",
		 cmdToggleGlobalAutomationLatchPreview},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Cycle automation mode of selected tracks")},
		 "OSARA_CYCLETRACKAUTOMATION",
		 cmdCycleTrackAutomation},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: About")}, "OSARA_ABOUT", cmdAbout},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report groups for current track")},
		 "OSARA_REPORTTRACKGROUPS",
		 cmdReportTrackGroups},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Mute next message from OSARA")}, "OSARA_MUTENEXTMESSAGE", cmdMuteNextMessage},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Report regions, last project marker and items on selected tracks at current position")},
		 "OSARA_REPORTREGIONMARKERITEMS",
		 cmdReportRegionMarkerItems},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Go to first track")}, "OSARA_GOTOFIRSTTRACK", cmdGoToFirstTrack},
		{MAIN_SECTION, {DEFACCEL, _t("OSARA: Go to last track")}, "OSARA_GOTOLASTTRACK", cmdGoToLastTrack},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Cycle shape of selected envelope points")},
		 "OSARA_CYCLEENVELOPEPOINTSHAPE",
		 cmdCycleEnvelopePointShape},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Toggle track/take volume envelope visibility (depending on focus)")},
		 "OSARA_TOGGLEVOLUMEENVELOPE",
		 cmdToggleVolumeEnvelope},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Toggle track/take pan envelope visibility (depending on focus)")},
		 "OSARA_TOGGLEPANENVELOPE",
		 cmdTogglePanEnvelope},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Toggle track/take mute envelope visibility (depending on focus)")},
		 "OSARA_TOGGLEMUTEENVELOPE",
		 cmdToggleMuteEnvelope},
		{MAIN_SECTION,
		 {DEFACCEL, _t("OSARA: Toggle track pre-FX pan or take pitch envelope visibility (depending on focus)")},
		 "OSARA_TOGGLEPREFXPANTAKEPITCHENVELOPE",
		 cmdTogglePreFXPanOrTakePitchEnvelope},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Enable noncontiguous selection/toggle selection of current chord/note")},
		 "OSARA_MIDITOGGLESEL",
		 cmdMidiToggleSelection},
		{MIDI_EDITOR_SECTION, {DEFACCEL, _t("OSARA: Move to next chord")}, "OSARA_NEXTCHORD", cmdMidiMoveToNextChord},
		{MIDI_EDITOR_SECTION, {DEFACCEL, _t("OSARA: Move to previous chord")}, "OSARA_PREVCHORD", cmdMidiMoveToPreviousChord
		},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to next chord and add to selection")},
		 "OSARA_NEXTCHORDKEEPSEL",
		 cmdMidiMoveToNextChordKeepSel},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to previous chord and add to selection")},
		 "OSARA_PREVCHORDKEEPSEL",
		 cmdMidiMoveToPreviousChordKeepSel},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to next note in chord")},
		 "OSARA_NEXTNOTE",
		 cmdMidiMoveToNextNoteInChord},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to previous note in chord")},
		 "OSARA_PREVNOTE",
		 cmdMidiMoveToPreviousNoteInChord},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to next note in chord and add to selection")},
		 "OSARA_NEXTNOTEKEEPSEL",
		 cmdMidiMoveToNextNoteInChordKeepSel},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to previous note in chord and add to selection")},
		 "OSARA_PREVNOTEKEEPSEL",
		 cmdMidiMoveToPreviousNoteInChordKeepSel},
		{MIDI_EDITOR_SECTION, {DEFACCEL, _t("OSARA: Move to next CC")}, "OSARA_NEXTCC", cmdMidiMoveToNextCC},
		{MIDI_EDITOR_SECTION, {DEFACCEL, _t("OSARA: Move to previous CC")}, "OSARA_PREVCC", cmdMidiMoveToPreviousCC},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to next CC and add to selection")},
		 "OSARA_NEXTCCKEEPSEL",
		 cmdMidiMoveToNextCCKeepSel},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to previous CC and add to selection")},
		 "OSARA_PREVCCKEEPSEL",
		 cmdMidiMoveToPreviousCCKeepSel},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to previous midi item on track")},
		 "OSARA_MIDIPREVITEM",
		 cmdMidiMoveToPrevItem},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Move to next midi item on track")},
		 "OSARA_MIDINEXTITEM",
		 cmdMidiMoveToNextItem},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Select all notes with the same pitch starting in time selection")},
		 "OSARA_SELSAMEPITCHTIMESEL",
		 cmdMidiSelectSamePitchStartingInTimeSelection},
		{MIDI_EDITOR_SECTION,
		 {DEFACCEL, _t("OSARA: Mute next message from OSARA")},
		 "OSARA_ME_MUTENEXTMESSAGE",
		 cmdMuteNextMessage},
#ifdef _WIN32
		{MIDI_EVENT_LIST_SECTION,
		 {DEFACCEL, _t("OSARA: Focus event nearest edit cursor")},
		 "OSARA_FOCUSMIDIEVENT",
		 cmdFocusNearestMidiEvent},
#endif
		{MIDI_EVENT_LIST_SECTION,
		 {DEFACCEL, _t("OSARA: Mute next message from OSARA")},
		 "OSARA_ML_MUTENEXTMESSAGE",
		 cmdMuteNextMessage},
		{MEDIA_EXPLORER_SECTION,
		 {DEFACCEL, _t("OSARA: Mute next message from OSARA")},
		 "OSARA_MX_MUTENEXTMESSAGE",
		 cmdMuteNextMessage},
		{0, {}, NULL, NULL},
};
map<pair<int, int>, Command*> commandsMap;

/*** Initialisation, termination and inner workings. */

bool isHandlingCommand = false;

bool handlePostCommand(int section, int command, int val = 63, int valHw = -1, int relMode = 0, HWND hwnd = nullptr) {
	if (section == MAIN_SECTION) {
		const auto postIt = postCommandsMap.find(command);
		if (postIt != postCommandsMap.end()) {
			isHandlingCommand = true;
			if (settings::moveFromPlayCursor) {
				const auto cursorIt = MOVE_FROM_PLAY_CURSOR_COMMANDS.find(command);
				if (cursorIt != MOVE_FROM_PLAY_CURSOR_COMMANDS.end()) {
					if (GetPlayState() & 1) { // Playing
						SetEditCurPos(GetPlayPosition(), false, false);
					}
				}
			}
			// #244: If the command was triggered via MIDI, pass the MIDI data when
			// executing the command so that toggles, etc. work as expected.
			KBD_OnMainActionEx(command, val, valHw, relMode, hwnd, nullptr);
			postIt->second(command);
			lastCommand = command;
			lastCommandTime = GetTickCount();
			isHandlingCommand = false;
			return true;
		}
		const auto mIt = POST_COMMAND_MESSAGES.find(command);
		if (mIt != POST_COMMAND_MESSAGES.end()) {
			isHandlingCommand = true;
			KBD_OnMainActionEx(command, val, valHw, relMode, hwnd, nullptr);
			outputMessage(translate(mIt->second));
			lastCommandTime = GetTickCount();
			isHandlingCommand = false;
			return true;
		}
	} else if (section == MIDI_EDITOR_SECTION) {
		const auto it = midiPostCommandsMap.find(command);
		if (it != midiPostCommandsMap.end()) {
			isHandlingCommand = true;
			HWND editor = MIDIEditor_GetActive();
			MIDIEditor_OnCommand(editor, command);
			it->second(command);
			lastCommandTime = GetTickCount();
			isHandlingCommand = false;
			return true;
		}
		const auto mIt = MIDI_POST_COMMAND_MESSAGES.find(command);
		if (mIt != MIDI_POST_COMMAND_MESSAGES.end()) {
			isHandlingCommand = true;
			HWND editor = MIDIEditor_GetActive();
			MIDIEditor_OnCommand(editor, command);
			outputMessage(translate(mIt->second));
			lastCommandTime = GetTickCount();
			isHandlingCommand = false;
			return true;
		}
	} else if (section == MIDI_EVENT_LIST_SECTION) {
		const auto it = midiEventListPostCommandsMap.find(command);
		if (it != midiEventListPostCommandsMap.end()) {
			isHandlingCommand = true;
			lastCommandTime = GetTickCount();
			HWND editor = MIDIEditor_GetActive();
			MIDIEditor_OnCommand(editor, command);
			it->second.first(command);
#ifdef _WIN32
			if (it->second.second) { // changesValueInMidiEventList
				HWND focus = GetFocus();
				if (focus && isMidiEditorEventListView(focus)) {
					sendNameChangeEventToMidiEditorEventListItem(focus);
				}
			}
#endif
			isHandlingCommand = false;
			return true;
		}
	} else if (section == MEDIA_EXPLORER_SECTION) {
		const auto it = mExplorerPostCommands.find(command);
		if (it != mExplorerPostCommands.end()) {
			isHandlingCommand = true;
			SendMessage(hwnd, WM_COMMAND, command, 0);
			it->second(command, hwnd);
			lastCommandTime = GetTickCount();
			isHandlingCommand = false;
			return true;
		}
	}
	return false;
}

bool handleToggleCommand(KbdSectionInfo* section, int command, int val, int valHw, int relMode, HWND hwnd) {
	const auto entry = TOGGLE_COMMAND_MESSAGES.find({section->uniqueID, command});
	if (entry != TOGGLE_COMMAND_MESSAGES.end() && !entry->second.onMsg && !entry->second.offMsg) {
		return false; // Ignore.
	}
	int oldState = GetToggleCommandState2(section, command);
	if (oldState == -1) {
		return false; // Not a toggle action.
	}
	HWND oldFocus = GetFocus();
	isHandlingCommand = true;
	switch (section->uniqueID) {
		case MAIN_SECTION:
			KBD_OnMainActionEx(command, val, valHw, relMode, hwnd, nullptr);
			break;
		case MIDI_EDITOR_SECTION:
		case MIDI_EVENT_LIST_SECTION: {
			HWND editor = MIDIEditor_GetActive();
			MIDIEditor_OnCommand(editor, command);
			break;
		}
		case MEDIA_EXPLORER_SECTION:
			SendMessage(hwnd, WM_COMMAND, command, 0);
			break;
		default:
			isHandlingCommand = false;
			return false; // We can't send commands for this section.
	}
	isHandlingCommand = false;
	if (oldFocus != GetFocus()) {
		// Don't report if the focus changes. The focus changing is better
		// feedback and we don't want to interrupt that.
		return true;
	}
	int newState = GetToggleCommandState2(section, command);
	if (oldState == newState) {
		return true; // No change, report nothing.
	}
	if (entry != TOGGLE_COMMAND_MESSAGES.end()) {
		const char* message = newState ? entry->second.onMsg : entry->second.offMsg;
		if (message) {
			outputMessage(translate(message));
		}
		return true;
	}
	// Generic feedback.
	ostringstream s;
	s << (newState ? translate("enabled") : translate("disabled")) << " "
		<< getActionName(command, section, /* skipCategory */ false);
	outputMessage(s);
	return true;
}

bool handleCommand(KbdSectionInfo* section, int command, int val, int valHw, int relMode, HWND hwnd) {
	if (isHandlingCommand)
		return false; // Prevent re-entrance.
	constexpr int MAIN_ALT1_SECTION = 1;
	constexpr int MAIN_ALT16_SECTION = 16;
	constexpr int MAIN_ALT_REC_SECTION = 100;
	if ((MAIN_ALT1_SECTION <= section->uniqueID && section->uniqueID <= MAIN_ALT16_SECTION) ||
			section->uniqueID == MAIN_ALT_REC_SECTION) {
		// This is a main alt-1 through alt-16 section or the main (alt recording)
		// section. Map this to the main section. Otherwise, some REAPER functions
		// won't behave correctly. This also makes things easier for our own code,
		// since we don't need to special case these alt sections everywhere.
		section = SectionFromUniqueID(MAIN_SECTION);
	}
	const auto it = commandsMap.find(make_pair(section->uniqueID, command));
	if (it != commandsMap.end()
			// Allow shortcut help to be disabled.
			&& (!isShortcutHelpEnabled || it->second->execute == cmdShortcutHelp)) {
		isHandlingCommand = true;
		if (it->second->gaccel.accel.cmd == lastCommand && GetTickCount() - lastCommandTime < 500) {
			++lastCommandRepeatCount;
		} else {
			lastCommandRepeatCount = 0;
		}
		it->second->execute(it->second);
		lastCommand = it->second->gaccel.accel.cmd;
		lastCommandTime = GetTickCount();
		isHandlingCommand = false;
		if (it->second->execute != cmdMuteNextMessage) {
			muteNextMessage = false;
		}
		return true;
	}
	// Allow "Main action section: Momentarily set override" actions to pass
	// through shortcut help so that users can learn about shortcuts in those
	// alternative sections.
	constexpr int ACTION_MOMENTARY_DEFAULT = 24851;
	constexpr int ACTION_MOMENTARY_ALT16 = 24868;
	if (isShortcutHelpEnabled && (command < ACTION_MOMENTARY_DEFAULT || command > ACTION_MOMENTARY_ALT16)) {
		outputMessage(getActionName(command, section, false));
		return true;
	}
	if (handlePostCommand(section->uniqueID, command, val, valHw, relMode, hwnd)) {
		muteNextMessage = false;
		return true;
	}
	if (handleSettingCommand(command)) {
		muteNextMessage = false;
		return true;
	}
	if (handleToggleCommand(section, command, val, valHw, relMode, hwnd)) {
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
		if (it->second->gaccel.accel.cmd == lastCommand && GetTickCount() - lastCommandTime < 500) {
			++lastCommandRepeatCount;
		} else {
			lastCommandRepeatCount = 0;
		}
		it->second->execute(it->second);
		lastCommand = it->second->gaccel.accel.cmd;
		lastCommandTime = GetTickCount();
		isHandlingCommand = false;
		if (it->second->execute != cmdMuteNextMessage) {
			muteNextMessage = false;
		}
		return true;
	} else if (handlePostCommand(MAIN_SECTION, command)) {
		muteNextMessage = false;
		return true;
	}
	return false;
}

IReaperControlSurface* surface = nullptr;

// Initialisation that must be done after REAPER_PLUGIN_ENTRYPOINT;
// e.g. because it depends on stuff registered by other plug-ins.
void CALLBACK delayedInit(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
#ifdef _WIN32
	initializeUia();
#endif
	surface = createSurface();
	plugin_register("csurf_inst", (void*)surface);
	NF_GetSWSTrackNotes = (decltype(NF_GetSWSTrackNotes))plugin_getapi("NF_GetSWSTrackNotes");
	for (int i = 0; POST_CUSTOM_COMMANDS[i].id; ++i) {
		int cmd = NamedCommandLookup(POST_CUSTOM_COMMANDS[i].id);
		if (cmd)
			postCommandsMap.insert(make_pair(cmd, POST_CUSTOM_COMMANDS[i].execute));
	}
	KillTimer(NULL, event);
}

#ifdef _WIN32

void annotateAccRole(HWND hwnd, long role) {
	VARIANT var;
	var.vt = VT_I4;
	var.lVal = role;
	accPropServices->SetHwndProp(hwnd, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_ROLE, var);
}

// Several windows in REAPER report as dialogs/property pages, but they aren't really.
// This includes the main window.
// Annotate these to prevent screen readers from potentially reading a spurious caption.
void annotateSpuriousDialog(HWND hwnd) {
	annotateAccRole(hwnd, hwnd == mainHwnd || hwnd == GetForegroundWindow() ? ROLE_SYSTEM_CLIENT : ROLE_SYSTEM_GROUPING);
	// If the previous hwnd is static text, oleacc will use this as the name.
	// This is never correct for these windows, so override it.
	if (GetWindowTextLength(hwnd) == 0) {
		accPropServices->SetHwndPropStr(hwnd, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, L"");
	}
}

void annotateSpuriousDialogs(HWND hwnd) {
	annotateSpuriousDialog(hwnd);
	for (HWND child = FindWindowExW(hwnd, NULL, L"#32770", NULL); child;
			 child = FindWindowExW(hwnd, child, L"#32770", NULL))
		annotateSpuriousDialogs(child);
}

UINT_PTR annotateFxDialogTimer = 0;

void CALLBACK annotateFxDialog(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	KillTimer(nullptr, annotateFxDialogTimer);
	annotateFxDialogTimer = 0;
	if (!GetFocusedFX(nullptr, nullptr, nullptr)) {
		return;
	}
	annotateSpuriousDialogs(GetForegroundWindow());
}

HWND prevForegroundHwnd = nullptr;
DWORD prevForegroundTime = 0;
HWND prevPrevForegroundHwnd = nullptr;

void CALLBACK
handleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG objId, long childId, DWORD thread, DWORD time) {
	if (event == EVENT_OBJECT_FOCUS) {
		HWND foreground = GetForegroundWindow();
		if (foreground != prevForegroundHwnd) {
			// The foreground window has changed.
			// hack: A bug in earlier versions of JUCE breaks OSARA UIA events when
			// the JUCE UI is dismissed. Hiding and showing our UIA HWND seems to fix
			// this.
			resetUia();
			if (
					// #747: When opening the Render to file name dialog, REAPER very briefly
					// opens and then closes the full Render to File dialog first. Use a
					// short timeout to prevent this code from running in that case.
					time - prevForegroundTime > 50 &&
					IsWindowVisible(prevPrevForegroundHwnd) &&
					!IsWindowVisible(prevForegroundHwnd)) {
				// The previous foreground window has closed. For example, this happens
				// when you open the FX chain for a track with no FX and dismiss the Add
				// FX dialog. It also happens in the Menu Editor when you add an
				// action and then dismiss the Actions dialog. REAPER returns focus to the
				// track view in this case. We redirect focus to the foreground window
				// before the last (the FX chain in the former example), which is more
				// useful to the user.
				SetForegroundWindow(prevPrevForegroundHwnd);
			}
			prevPrevForegroundHwnd = prevForegroundHwnd;
			prevForegroundHwnd = foreground;
			prevForegroundTime = time;
		}
		bool focusIsTrackView = isTrackViewWindow(hwnd);
		if (focusIsTrackView) {
			// Give these objects a non-generic role so NVDA doesn't fall back to
			// screen scraping, which causes spurious messages to be reported.
			annotateAccRole(hwnd, ROLE_SYSTEM_PANE);
		}

		if (isMidiEditorEventListView(hwnd)) {
			maybeHandleEventListItemFocus(hwnd, childId);
		}
		if (lastMessageHwnd && hwnd != lastMessageHwnd) {
			// Focus is moving. Clear our tweak to accName for the previous focus.
			// This avoids problems such as the last message repeating when a new project is opened (#17).
			bool lastWasTrackView = isTrackViewWindow(lastMessageHwnd);
			if (lastWasTrackView) {
				// These objects can get a bogus name from oleacc, so set an empty name
				// instead of clearing it.
				accPropServices->SetHwndPropStr(lastMessageHwnd, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, L" ");
			} else {
				accPropServices->ClearHwndProps(lastMessageHwnd, OBJID_CLIENT, CHILDID_SELF, &PROPID_ACC_NAME, 1);
			}
			if (lastWasTrackView && focusIsTrackView) {
				// REAPER 6.0 moves focus between two track view windows in some cases.
				// For example, if you select a track, REAPERTCPDisplay gets focus. If
				// you select an item, REAPERTrackListWindow gets focus.
				// Sometimes, focus moves after the action executes. Therefore, repeat
				// the message so the user doesn't miss it.
				outputMessage(lastMessage);
			} else {
				lastMessageHwnd = nullptr;
			}
		}
		HWND tempWindow;
		if ((tempWindow = getSendContainer(hwnd))) {
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
		} else if (childId > CHILDID_SELF) {
			maybeReportFxChainBypassDelayed();
		} else {
			maybeAnnotatePreferenceDescription();
		}
	} else if (event == EVENT_OBJECT_SHOW) {
		if (isClassName(hwnd, "#32770")) {
			if (GetFocusedFX(nullptr, nullptr, nullptr)) {
				annotateFxDialog(0, 0, 0, 0);
			} else {
				// This might be an FX dialog, but REAPER won't answer that query
				// immediately, so delay it a bit.
				if (annotateFxDialogTimer) {
					KillTimer(nullptr, annotateFxDialogTimer);
				}
				annotateFxDialogTimer = SetTimer(nullptr, 0, 200, annotateFxDialog);
			}
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
		initTranslation();
		peakWatcher::initialize();

#ifdef _WIN32
		if (CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER, IID_IAccPropServices, (void**)&accPropServices) !=
				S_OK) {
			return 0;
		}
		guiThread = GetWindowThreadProcessId(mainHwnd, NULL);
		winEventHook = SetWinEventHook(
				EVENT_OBJECT_SHOW, EVENT_OBJECT_FOCUS, hInstance, handleWinEvent, 0, guiThread, WINEVENT_INCONTEXT
		);
		annotateSpuriousDialogs(mainHwnd);
#else
		NSA11yWrapper::init();
#endif

		for (int i = 0; POST_COMMANDS[i].cmd; ++i)
			postCommandsMap.insert(make_pair(POST_COMMANDS[i].cmd, POST_COMMANDS[i].execute));

		for (auto& midiPostCommand : MIDI_POST_COMMANDS) {
			midiPostCommandsMap.insert(make_pair(midiPostCommand.cmd, midiPostCommand.execute));
			if (midiPostCommand.supportedInMidiEventList) {
				midiEventListPostCommandsMap.insert(make_pair(
						midiPostCommand.cmd, make_pair(midiPostCommand.execute, midiPostCommand.changesValueInMidiEventList)
				));
			}
		}

		for (int i = 0; COMMANDS[i].execute; ++i) {
			if (COMMANDS[i].id) {
				// This is our own command.
				if (COMMANDS[i].section == MAIN_SECTION) {
					COMMANDS[i].gaccel.accel.cmd = rec->Register("command_id", (void*)COMMANDS[i].id);
					COMMANDS[i].gaccel.desc = translate(COMMANDS[i].gaccel.desc);
					rec->Register("gaccel", &COMMANDS[i].gaccel);
				} else {
					custom_action_register_t action;
					action.uniqueSectionId = COMMANDS[i].section;
					action.idStr = COMMANDS[i].id;
					action.name = translate(COMMANDS[i].gaccel.desc);
					COMMANDS[i].gaccel.accel.cmd = rec->Register("custom_action", &action);
				}
			}
			commandsMap.insert(make_pair(make_pair(COMMANDS[i].section, COMMANDS[i].gaccel.accel.cmd), &COMMANDS[i]));
		}
		registerSettingCommands();
		// hookcommand can only handle actions for the main section, so we need hookcommand2.
		// According to SWS, hookcommand2 must be registered before hookcommand.
		rec->Register("hookcommand2", (void*)handleCommand);
		// #29: Unfortunately, actions triggered by user-defined actions don't trigger hookcommand2,
		// but they do trigger hookcommand. IMO, this is a REAPER bug.
		// Register hookcommand as well so custom actions at least work for the main section.
		rec->Register("hookcommand", (void*)handleMainCommandFallback);

		registerExports(rec);
		SetTimer(nullptr, 0, 0, delayedInit);
#ifdef _WIN32
		keyboardHook = SetWindowsHookEx(WH_KEYBOARD, keyboardHookProc, nullptr, guiThread);
#endif
		return 1;

	} else {
		// Unload.
		delete surface;
#ifdef _WIN32
		UnhookWindowsHookEx(keyboardHook);
		UnhookWinEvent(winEventHook);
		terminateUia();
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
#include "config.rc_mac_dlg"
#include <swell-menugen.h>
#include "reaper_osara.rc_mac_menu"
#include "config.rc_mac_menu"
#endif
