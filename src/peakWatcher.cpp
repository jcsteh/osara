/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Peak Watcher code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2022 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include <math.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <variant>
#include <vector>
#include <utility>
// osara.h includes windows.h, which must be included before other Windows
// headers.
#include "osara.h"
#ifdef _WIN32
#include <Commctrl.h>
#include <Windowsx.h>
#endif
#include <WDL/win32_utf8.h>
#include <WDL/db2val.h>
#include <WDL/wdltypes.h>
#include "fxChain.h"
#include "resource.h"
#include "translation.h"

using namespace std;

namespace peakWatcher {

const double NO_LEVEL = -150.0;

// Peak Watcher can watch various types of targets; e.g. tracks, track FX.
using NoTarget = monostate;
using TrackFx = pair<MediaTrack*, int>;
using Target = variant<NoTarget, MediaTrack*, TrackFx>;

// std::get isn't supported until MacOS 10.14. Grr.
template<typename T, typename V>
T& varGet(V& var) {
	// This will dereference a null pointer if the variant doesn't contain this
	// type!
	return *get_if<T>(&var);
}

class Watcher;

// Peak Watcher can watch various types of levels; e.g. peak dB, momentary LUFS.
struct LevelType {
	const char* name;
	bool (*isSupported)(const Target& target);
	bool separateChannels : 1;
	// If true, smaller values are more significant to the user than larger ones.
	// This means that values less than the notification level will be reported
	// and the minimum value will be held if appropriate.
	// If false, larger values are more significant to the user than smaller ones.
	// This means that values greater than the notification level will be reported
	// and the maximum value will be held if appropriate.
	bool isSmallerSignificant : 1;
	double (*getLevel)(Watcher& watcher, int channel);
	void (*reset)(Watcher& watcher);

	bool isLevelSignificant(double a, double b) const {
		if (this->isSmallerSignificant) {
			if (b == NO_LEVEL && a != NO_LEVEL) {
				return true;
			}
			return a < b;
		}
		return a > b;
	}
};

void describeTrack(MediaTrack* track, ostringstream& s) {
	int trackNum = (int)(size_t)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER",
		nullptr);
	if (trackNum <= 0) {
		s << translate("master");
	} else {
		s << translate("track") << " ";
		char* name = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
		if (name && name[0]) {
			s << name;
		} else {
			s << trackNum;
		}
	}
}

void describeTarget(Target& target, ostringstream& s) {
	if (MediaTrack** track = get_if<MediaTrack*>(&target)) {
		describeTrack(*track, s);
	} else if (TrackFx* tfx = get_if<TrackFx>(&target)) {
		describeTrack(tfx->first, s);
		s << ", ";
		char name[256];
		TrackFX_GetFXName(tfx->first, tfx->second, name, sizeof(name));
		shortenFxName(name, s);
	}
}

const int NUM_CHANNELS = 2;

// Peak Watcher can watch one or more values. Each "watcher" consists of a
// target, level type and other parameters that determine how/when it is
// reported.
class Watcher {
	public:
	unsigned int levelType = 0;
	const LevelType& levelTypeInfo();
	Target target;
	bool follow = false;
	void description(ostringstream& s);
	double notifyLevel = 0;
	// Hold time in ms; -1 disabled, 0 forever.
	int hold = 0;
	struct {
		bool notify = true;
		double peak = NO_LEVEL;
		DWORD time = 0;
	} channels[NUM_CHANNELS];

	bool isDisabled() {
		return holds_alternative<NoTarget>(target);
	}

	Target getLatestFollowTarget() {
		if (holds_alternative<MediaTrack*>(this->target)) {
			return GetLastTouchedTrack();
		}
		return this->target;
	}

	bool isValid() {
		if (MediaTrack** track = get_if<MediaTrack*>(&this->target)) {
			return !!*track;
		}
		return true;
	}

	void reset() {
		for (auto& channel : this->channels) {
			channel.peak = NO_LEVEL;
		}
		const LevelType& levelType = this->levelTypeInfo();
		if (!this->isDisabled() && levelType.reset) {
			levelType.reset(*this);
		}
	}
};

const int NUM_WATCHERS = 2;
Watcher watchers[NUM_WATCHERS];

const char* WATCHER_NAMES[NUM_WATCHERS] = {
	// translate firstString begin
	"1st watcher",
	"2nd watcher",
	// translate firstString end
};
const char* CHANNEL_NAMES[NUM_CHANNELS] = {
	// translate firstString begin
	"1st chan",
	"2nd chan",
	// translate firstString end
};

UINT_PTR timer = 0;

const char FX_LOUDNESS_METER[] = "loudness_meter";

double getLoudnessMeterParam(Watcher& watcher,
	int configParam, double configValue, int queryParam
) {
	assert(holds_alternative<MediaTrack*>(watcher.target));
	MediaTrack* track = varGet<MediaTrack*>(watcher.target);
	int fx = TrackFX_AddByName(track, FX_LOUDNESS_METER, /* recFX */ false,
		0 /* don't create */);
	if (fx == -1) {
		// Add the effect.
		fx = TrackFX_AddByName(track, FX_LOUDNESS_METER, /* recFX */ false,
			1 /* create if not found */);
		if (fx == -1) {
			// Effect doesn't exist!
			return NO_LEVEL;
		}
		// Turn off all level types.
		for (int param = 0; param <= 6; ++param) {
			TrackFX_SetParam(track, fx, param, 0.0);
		}
		// Turn off reset on playback start.
		TrackFX_SetParam(track, fx, 10, 0.0);
		// set output loudness values as automation to all
		TrackFX_SetParam(track, fx, 14, 1.0);
	}
	// Ensure the level type we need is turned on. We do this here rather than
	// when adding the effect because we might have already added the effect for
	// another value earlier if two different values are being watched on the same
	// track.
	TrackFX_SetParam(track, fx, configParam, configValue);
	return TrackFX_GetParam(track, fx, queryParam, nullptr, nullptr);
}

void deleteLoudnessMeter(Watcher& watcher) {
	assert(holds_alternative<MediaTrack*>(watcher.target));
	MediaTrack* track = varGet<MediaTrack*>(watcher.target);
	int fx = TrackFX_AddByName(track, FX_LOUDNESS_METER, /* recFX */ false,
		0 /* don't create */);
	if (fx != -1) {
		TrackFX_Delete(track, fx);
	}
}

bool isTrackLevelTypeSupported(const Target& target) {
	return holds_alternative<MediaTrack*>(target);
}

const char FXPARM_GAIN_REDUCTION[] = "GainReduction_dB";

const LevelType LEVEL_TYPES[] = {
	// translate firstString begin
	{"peak dB",
		/* isSupported */ isTrackLevelTypeSupported,
		/* separateChannels */ true,
		/* isSmallerSignificant */ false,
		/* getValue */ [](Watcher& watcher, int channel) {
			assert(holds_alternative<MediaTrack*>(watcher.target));
			MediaTrack* track = varGet<MediaTrack*>(watcher.target);
			// #119: We use Track_GetPeakHoldDB even when Peak Watcher's hold
			//  functionality is disabled because we only measure every 30 ms and we
			// might miss peaks.
			// Undocumented: Track_GetPeakHoldDB returns hundredths of a dB.
			double newPeak = Track_GetPeakHoldDB(track, channel, false) * 100.0;
			// We have the peak now, so reset REAPER's hold.
			Track_GetPeakHoldDB(track, channel, true);
			return newPeak;
		},
		/* reset */ nullptr,
	},
	{"integrated LUFS",
		/* isSupported */ isTrackLevelTypeSupported,
		/* separateChannels */ false,
		/* isSmallerSignificant */ false,
		/* getValue */ [](Watcher& watcher, int channel) {
			return getLoudnessMeterParam(watcher, 6, 1.0, 20);
		},
		/* reset */ deleteLoudnessMeter,
	},
	{"momentary LUFS",
		/* isSupported */ isTrackLevelTypeSupported,
		/* separateChannels */ false,
		/* isSmallerSignificant */ false,
		/* getValue */ [](Watcher& watcher, int channel) {
			return getLoudnessMeterParam(watcher, 3, 1.0, 18);
		},
		/* reset */ deleteLoudnessMeter,
	},
	{"short term LUFS",
		/* isSupported */ isTrackLevelTypeSupported,
		/* separateChannels */ false,
		/* isSmallerSignificant */ false,
		/* getValue */ [](Watcher& watcher, int channel) {
			return getLoudnessMeterParam(watcher, 4, 1.0, 19);
		},
		/* reset */ deleteLoudnessMeter,
	},
	{"loudness range LU",
		/* isSupported */ isTrackLevelTypeSupported,
		/* separateChannels */ false,
		/* isSmallerSignificant */ false,
		/* getValue */ [](Watcher& watcher, int channel) {
			return getLoudnessMeterParam(watcher, 5, 1.0, 21);
		},
		/* reset */ deleteLoudnessMeter,
	},
	{"integrated RMS",
		/* isSupported */ isTrackLevelTypeSupported,
		/* separateChannels */ false,
		/* isSmallerSignificant */ false,
		/* getValue */ [](Watcher& watcher, int channel) {
			return getLoudnessMeterParam(watcher, 2, 1.0, 17);
		},
		/* reset */ deleteLoudnessMeter,
	},
	{"momentary RMS",
		/* isSupported */ isTrackLevelTypeSupported,
		/* separateChannels */ false,
		/* isSmallerSignificant */ false,
		/* getValue */ [](Watcher& watcher, int channel) {
			return getLoudnessMeterParam(watcher, 1, 1.0, 16);
		},
		/* reset */ deleteLoudnessMeter,
	},
	{"true peak dBTP",
		/* isSupported */ isTrackLevelTypeSupported,
		/* separateChannels */ false,
		/* isSmallerSignificant */ false,
		/* getValue */ [](Watcher& watcher, int channel) {
			return getLoudnessMeterParam(watcher, 0, 1.0, 15);
		},
		/* reset */ deleteLoudnessMeter,
	},
	{"gain reduction dB",
		/* isSupported */ [](const Target& target) {
			const TrackFx* tfx = get_if<TrackFx>(&target);
			if (!tfx) {
				return false;
			}
			char text[1];
			return TrackFX_GetNamedConfigParm(tfx->first, tfx->second,
				FXPARM_GAIN_REDUCTION, text, sizeof(text));
		},
		/* separateChannels */ false,
		/* isSmallerSignificant */ true,
		/* getValue */ [](Watcher& watcher, int channel) {
			const TrackFx* tfx = get_if<TrackFx>(&watcher.target);
			assert(tfx);
			char text[10];
			if (!TrackFX_GetNamedConfigParm(tfx->first, tfx->second,
					FXPARM_GAIN_REDUCTION, text, sizeof(text))) {
				return NO_LEVEL;
			}
			return stod(text);
		},
		/* reset */ nullptr,
	},
	// translate firstString end
};
constexpr unsigned int NUM_LEVEL_TYPES = sizeof(LEVEL_TYPES) /
	sizeof(LevelType);

const LevelType& Watcher::levelTypeInfo() {
	return LEVEL_TYPES[this->levelType];
}

void Watcher::description(ostringstream& s) {
	if (this->isDisabled()) {
		s << translate("not configured");
		return;
	}
	if (this->follow) {
		if (holds_alternative<MediaTrack*>(this->target)) {
			s << translate("following last touched track");
		}
	} else {
		describeTarget(target, s);
	}
	s << " " << translate(this->levelTypeInfo().name);
}

void resetWatcher(int watcherIndex, bool report=false) {
	Watcher& watcher = watchers[watcherIndex];
	watcher.reset();
	if (report) {
		if (watcher.isDisabled()) {
			// Translators: Reported when the user tries to reset a Peak Watcher
			// value, but that watcher is disabled.
			outputMessage(translate("watcher disabled"));
		} else {
			// Translators: Reported when the user resets a Peak Watcher value.
			outputMessage(translate("reset"));
		}
	}
}

bool isWatchingMultipleValues() {
	int count = 0;
	for (Watcher& watcher : watchers) {
		if (!watcher.isDisabled()) {
			++count;
		}
		if (count > 1) {
			break;
		}
	}
	return count > 1;
}

void CALLBACK tick(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	ostringstream s;
	s << fixed << setprecision(1);
	const bool multiple = isWatchingMultipleValues();
	for (int w = 0; w < NUM_WATCHERS; ++w) {
		Watcher& watcher = watchers[w];
		if (watcher.isDisabled()) {
			continue;
		}
		const LevelType& levelType = watcher.levelTypeInfo();
		if (watcher.follow) {
			Target latest = watcher.getLatestFollowTarget();
			if (latest != watcher.target) {
				// We're following a target and it changed.
				watcher.reset();
				watcher.target = latest;
				if (!watcher.isValid()) {
					continue; // No current target, so nothing to do.
				}
			}
		}

		// If this level type doesn't care about separate channels, we only need
		// to process one channel.
		const int numChannels = levelType.separateChannels ?
			NUM_CHANNELS : 1;
		bool watcherReported = false;
		for (int c = 0; c < numChannels; ++c) {
			auto& chan = watcher.channels[c];
			double newPeak = levelType.getLevel(watcher, c) ;
			if (watcher.hold == -1 // Hold disabled
				|| levelType.isLevelSignificant(newPeak, chan.peak)
				|| (watcher.hold != 0 && time > chan.time + watcher.hold)
			) {
				chan.peak = newPeak;
				chan.time = time;
				if (chan.notify &&
						levelType.isLevelSignificant(newPeak, watcher.notifyLevel)) {
					if (s.tellp() > 0) {
						s << ", ";
					}
					if (!watcherReported && multiple) {
						// Only report which watcher if watching more than one target.
						s << translate(WATCHER_NAMES[w]) << " ";
						watcherReported = true;
					}
					if (levelType.separateChannels) {
						s << translate(CHANNEL_NAMES[c]) << " ";
					}
					s << newPeak;
					outputMessage(s);
				}
			}
		}
		if (!levelType.separateChannels) {
			// Copy the value to the other channels for on-demand reporting.
			for (int c = 1; c < NUM_CHANNELS; ++c) {
				watcher.channels[c].peak = watcher.channels[0].peak;
			}
		}
	}
}

void start() {
	timer = SetTimer(nullptr, 0, 30, tick);
}

void stop() {
	KillTimer(nullptr, timer);
	timer = 0;
}

bool isWatchingAnything() {
	for (Watcher& watcher : watchers) {
		if (!watcher.isDisabled()) {
			return true;
		}
	}
	return false;
}

Target getFocusedTarget() {
	int trackNum, itemNum, fx;
	int type = GetFocusedFX(&trackNum, &itemNum, &fx);
	if (type == 1) { // Track
		MediaTrack* track = trackNum == 0 ?
			GetMasterTrack(nullptr) : GetTrack(nullptr, trackNum - 1);
		return TrackFx(track, fx);
	}

	switch (fakeFocus) {
		case FOCUS_TRACK:
			return GetLastTouchedTrack();
			break;
		default:
			break;
	}
	return NoTarget();
}

vector<unsigned int> getSupportedLevelTypes(const Target& target) {
	vector<unsigned int> types;
	for (unsigned int t = 0; t < NUM_LEVEL_TYPES; ++t) {
		const LevelType& type = LEVEL_TYPES[t];
		if (type.isSupported(target)) {
			types.push_back(t);
		}
	}
	return types;
}

class Dialog {
	private:
	HWND dialog;
	Target target;
	Watcher& watcher;
	vector<unsigned int> supportedLevelTypes;

	void onOk() {
		bool targetChanged = false;
		if (this->watcher.target != this->target) {
			this->watcher.reset();
			this->watcher.target = this->target;
			targetChanged = true;
		}

		this->watcher.follow = IsDlgButtonChecked(this->dialog, ID_PEAK_FOLLOW)
			== BST_CHECKED;

		// Retrieve the level type.
		HWND typeSel = GetDlgItem(this->dialog, ID_PEAK_TYPE);
		unsigned int newType = this->supportedLevelTypes[ComboBox_GetCurSel(typeSel)];
		if (newType != this->watcher.levelType) {
			// If the target changed, we already reset.
			if (!targetChanged) {
				this->watcher.reset();
			}
			this->watcher.levelType = newType;
		}

		// Retrieve the notification state for channels.
		for (int c = 0; c < NUM_CHANNELS; ++c) {
			this->watcher.channels[c].notify =
				IsDlgButtonChecked(this->dialog, ID_PEAK_CHAN1 + c) == BST_CHECKED;
		}

		char inText[7];
		// Retrieve the entered maximum level.
		if (GetDlgItemText(this->dialog, ID_PEAK_LEVEL, inText, sizeof(inText)) > 0) {
			this->watcher.notifyLevel = atof(inText);
			// Restrict the range.
			this->watcher.notifyLevel =
				max(min(this->watcher.notifyLevel, 40.0), -40.0);
		}

		// Retrieve the hold choice/time.
		if (IsDlgButtonChecked(this->dialog, ID_PEAK_HOLD_DISABLED) == BST_CHECKED) {
			this->watcher.hold = -1;
		} else if (IsDlgButtonChecked(this->dialog, ID_PEAK_HOLD_FOREVER) ==
				BST_CHECKED) {
			this->watcher.hold = 0;
		} else if (GetDlgItemText(this->dialog, ID_PEAK_HOLD_TIME, inText,
				sizeof(inText)) > 0) {
			this->watcher.hold = atoi(inText);
			// Restrict the range.
			this->watcher.hold = max(min(this->watcher.hold, 20000), 1);
		}

		if (!timer) { // Previously disabled or paused.
			start();
		}
	}

	static INT_PTR CALLBACK dialogProc(HWND dialogHwnd, UINT msg,
		WPARAM wParam, LPARAM lParam
	) {
		Dialog* dialog = (Dialog*)GetWindowLongPtr(dialogHwnd, GWLP_USERDATA);
		switch (msg) {
			case WM_COMMAND: {
				int id = LOWORD(wParam);
				if (ID_PEAK_HOLD_DISABLED <= id && id <= ID_PEAK_HOLD_FOR) {
					EnableWindow(GetDlgItem(dialogHwnd, ID_PEAK_HOLD_TIME),
						id == ID_PEAK_HOLD_FOR ? BST_CHECKED : BST_UNCHECKED);
				} else if (id == ID_PEAK_RESET) {
					dialog->watcher.reset();
					DestroyWindow(dialogHwnd);
					delete dialog;
					return TRUE;
				} else if (id == IDOK) {
					dialog->onOk();
					DestroyWindow(dialogHwnd);
					delete dialog;
					return TRUE;
				} else if (id == IDCANCEL) {
					DestroyWindow(dialogHwnd);
					delete dialog;
					return TRUE;
				} else if (id == ID_PEAK_DISABLE) {
					dialog->watcher.reset();
					dialog->watcher.target = NoTarget();
					if (!isWatchingAnything()) {
						stop();
					}
					DestroyWindow(dialogHwnd);
					delete dialog;
					return TRUE;
				}
				break;
			}
			case WM_CLOSE:
				DestroyWindow(dialogHwnd);
				delete dialog;
				return TRUE;
		}
		return FALSE;
	}

	public:
	Dialog(Target target, Watcher& watcher,
			vector<unsigned int> supportedLevelTypes):
			target(target), watcher(watcher),
			supportedLevelTypes(supportedLevelTypes) {
		ostringstream s;
		this->dialog = CreateDialog(pluginHInstance,
			MAKEINTRESOURCE(ID_PEAK_WATCHER_DLG), GetForegroundWindow(),
			Dialog::dialogProc);
		SetWindowLongPtr(this->dialog, GWLP_USERDATA, (LONG_PTR)this);
		translateDialog(this->dialog);
		s << translate_ctxt("Peak Watcher", "Peak Watcher") << ": ";
		describeTarget(target, s);
		SetWindowText(this->dialog, s.str().c_str());
		s.str("");

		HWND follow = GetDlgItem(this->dialog, ID_PEAK_FOLLOW);
		EnableWindow(follow, true);
		bool followChecked = watcher.follow;
		if (!holds_alternative<MediaTrack*>(target)) {
			// Target doesn't support following.
			EnableWindow(follow, false);
			followChecked = false;
		}
		CheckDlgButton(this->dialog, ID_PEAK_FOLLOW, followChecked ?
			BST_CHECKED : BST_UNCHECKED);

		HWND typeCombo = GetDlgItem(this->dialog, ID_PEAK_TYPE);
		WDL_UTF8_HookComboBox(typeCombo);
		assert(!holds_alternative<NoTarget>(target));
		// Select the first type if no supported type was selected.
		int typeSel = 0;
		for (unsigned int i = 0; i < supportedLevelTypes.size(); ++i) {
			unsigned int t = supportedLevelTypes[i];
			const LevelType& type = LEVEL_TYPES[t];
			ComboBox_AddString(typeCombo, translate(type.name));
			if ( watcher.levelType == t) {
				typeSel = i;
			}
		}
		ComboBox_SetCurSel(typeCombo, typeSel);

		for (int c = 0; c < NUM_CHANNELS; ++c) {
			CheckDlgButton(this->dialog, ID_PEAK_CHAN1 + c, watcher.channels[c].notify
				? BST_CHECKED : BST_UNCHECKED);
		}

		HWND level = GetDlgItem(this->dialog, ID_PEAK_LEVEL);
#ifdef _WIN32
		SendMessage(level, EM_SETLIMITTEXT, 6, 0);
#endif
		s << fixed << setprecision(2);
		s << watcher.notifyLevel;
		SetWindowText(level, s.str().c_str());
		s.str("");

		HWND holdTime = GetDlgItem(this->dialog, ID_PEAK_HOLD_TIME);
#ifdef _WIN32
		SendMessage(holdTime, EM_SETLIMITTEXT, 5, 0);
#endif
		int id;
		if (watcher.hold == -1) {
			id = ID_PEAK_HOLD_DISABLED;
		} else if (watcher.hold == 0) {
			id = ID_PEAK_HOLD_FOREVER;
		} else {
			id = ID_PEAK_HOLD_FOR;
			s << watcher.hold;
			SetWindowText(holdTime, s.str().c_str());
		}
		CheckDlgButton(this->dialog, id, BST_CHECKED);
		EnableWindow(holdTime, watcher.hold > 0);

		ShowWindow(this->dialog, SW_SHOWNORMAL);
	}
};

void report(int watcherIndex, int channel) {
	assert(watcherIndex < NUM_WATCHERS);
	assert(channel < NUM_CHANNELS);
	Watcher& watcher = watchers[watcherIndex];
	if (watcher.isDisabled()) {
		// Translators: Reported when the user tries to report a Peak Watcher
		// channel, but the Peak Watcher value is disabled.
		outputMessage(translate("watcher disabled"));
		return;
	}
	if (!timer) {
		// Translators: Reported when the user tries to report a Peak Watcher
		// channel, but the Peak Watcher is paused.
		outputMessage(translate("Peak Watcher paused"));
		return;
	}
	ostringstream s;
	s << fixed << setprecision(1);
	s << watcher.channels[channel].peak;
	outputMessage(s);
}

} // namespace peakWatcher

void cmdPeakWatcher(Command* command) {
	peakWatcher::Target target = peakWatcher::getFocusedTarget();
	if (holds_alternative<peakWatcher::NoTarget>(target)) {
		outputMessage(translate("Peak Watcher does not support the current focus"));
		return;
	}

	auto types = peakWatcher::getSupportedLevelTypes(target);
	if (types.empty()) {
		outputMessage(translate("Peak Watcher does not support the current focus"));
		return;
	}

	// Ask which watcher to configure.
	HMENU menu = CreatePopupMenu();
	MENUITEMINFO itemInfo;
	itemInfo.cbSize = sizeof(MENUITEMINFO);
	// MIIM_TYPE is deprecated, but win32_utf8 still relies on it.
	itemInfo.fMask = MIIM_TYPE | MIIM_ID;
	itemInfo.fType = MFT_STRING;
	for (int w = 0; w < peakWatcher::NUM_WATCHERS; ++w) {
		itemInfo.wID = w + 1;
		ostringstream s;
		// Translators: Used when asking which Peak Watcher value to configure.
		// {} will be replaced with the value number; e.g. "Value &2".
		// After this, information about the existing configuration for the value
		// will be appended.
		s << translate(peakWatcher::WATCHER_NAMES[w]) << ", ";
		peakWatcher::watchers[w].description(s);
		// Make sure this stays around until the InsertMenuItem call.
		string str = s.str();
		itemInfo.dwTypeData = (char*)str.c_str();
		itemInfo.cch = (int)s.tellp();
		InsertMenuItem(menu, w, true, &itemInfo);
	}
	int w = TrackPopupMenu(menu, TPM_NONOTIFY | TPM_RETURNCMD, 0, 0, 0,
		mainHwnd, nullptr) - 1;
	DestroyMenu(menu);
	if (w == -1) {
		return;
	}
	peakWatcher::Watcher& watcher = peakWatcher::watchers[w];

	new peakWatcher::Dialog(target, watcher, types);
}

void cmdReportPeakWatcherW1C1(Command* command) {
	peakWatcher::report(0, 0);
}

void cmdReportPeakWatcherW1C2(Command* command) {
	peakWatcher::report(0, 1);
}

void cmdReportPeakWatcherW2C1(Command* command) {
	peakWatcher::report(1, 0);
}

void cmdReportPeakWatcherW2C2(Command* command) {
	peakWatcher::report(1, 1);
}

void cmdResetPeakWatcherW1(Command* command) {
	peakWatcher::resetWatcher(0, true);
}

void cmdResetPeakWatcherW2(Command* command) {
	peakWatcher::resetWatcher(1, true);
}

void cmdPausePeakWatcher(Command* command) {
	if (peakWatcher::timer) {
		// Running.
		peakWatcher::stop();
		outputMessage(translate("paused Peak Watcher"));
	} else if (peakWatcher::isWatchingAnything()) {
		// Paused.
		peakWatcher::start();
		outputMessage(translate("resumed Peak Watcher"));
	} else {
		// Disabled.
		outputMessage(translate("Peak Watcher not enabled"));
	}
}
