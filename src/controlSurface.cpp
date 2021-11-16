/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Control surface code
 * Copyright 2019-2020 NV Access Limited, James Teh, Leonard de Ruijter
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <WDL/db2val.h>
#include <cstdint>
#include "osara.h"
#include "paramsUi.h"
#include "midiEditorCommands.h"
#include "translation.h"

using namespace std;

bool shouldReportSurfaceChanges = true;
bool shouldReportMarkersWhilePlaying = false;

// REAPER often notifies us about track states even if they haven't changed.
// It also notifies us about these states when a track is first created.
// We only want to report changes. So, we maintain a cache.
// We don't want to report the first value we're notified about. That means
// we need to know not just enabled/disabled, but whether the value has been
// cached yet. So, we have two flags for each state: one for enabled, one for
// disabled. If neither is set, that means uncached. We could have used a
// struct with std::optional<bool>s, but that wastes a lot of memory. This way,
// we can cache all of the states in 1 byte.
const uint8_t TC_MUTED = 1 << 0;
const uint8_t TC_UNMUTED = 1 << 1;
const uint8_t TC_SOLOED = 1 << 2;
const uint8_t TC_UNSOLOED = 1 << 3;
const uint8_t TC_ARMED = 1 << 4;
const uint8_t TC_UNARMED = 1 << 5;

// Make it easier to manipulate these cached states.
template<uint8_t enableFlag, uint8_t disableFlag>
class TrackCacheState {
	public:
	TrackCacheState(uint8_t& value): value(value) {}

	// Check if a supplied new state has changed from the cached state.
	bool hasChanged(bool isEnabled) {
		if (!(this->value & (enableFlag | disableFlag))) {
			// Not cached yet, so we don't consider it changed.
			return false;
		}
		bool wasEnabled = this->value & enableFlag;
		return isEnabled != wasEnabled;
	}

	// Update the cached state.
	void update(bool enabled) {
		// Clear both the enable and disable flags for this state. In practice, only
		// one of them should be set.
		this->value &= ~(enableFlag | disableFlag);
		// Set only the flag for the new state.
		this->value |= enabled ? enableFlag : disableFlag;
	}

	private:
	uint8_t& value;
};

/*** A control surface to obtain certain info that can only be retrieved that way.
 */
class Surface: public IReaperControlSurface {
	public:
	virtual const char* GetTypeString() override {
		return "OSARA";
	}

	virtual const char* GetDescString() override {
		return "OSARA";
	}

	virtual const char* GetConfigString() override {
		return "";
	}

	virtual void Run() override {
		if (shouldReportMarkersWhilePlaying && GetPlayState() & 1) {
			double playPos = GetPlayPosition();
			if (playPos == this->lastPlayPos) {
				return;
			}
			this->lastPlayPos = playPos;
			this->reportMarker(playPos);
		}
	}

	virtual void SetPlayState(bool play, bool pause, bool rec) override {
		if (play) {
			cancelPendingMidiPreviewNotesOff();
		}
		if (!shouldReportSurfaceChanges || this->wasCausedByCommand()) {
			return;
		}
		// Calculate integer based transport state
		int TransportState = (int)play | ((int)pause << 1) | ((int)rec << 2);
		reportTransportState(TransportState);
	}

	virtual void SetRepeatState(bool repeat) override {
		if (!shouldReportSurfaceChanges || this->wasCausedByCommand()) {
			return;
		}
		reportRepeat(repeat);
	}

	virtual void SetSurfaceVolume(MediaTrack* track, double volume) override {
		if (isParamsDialogOpen || !this->shouldHandleParamChange()) {
			return;
		}
		ostringstream s;
		bool different = this->reportTrackIfDifferent(track, s);
		different |= this->lastParam != PARAM_VOLUME;
		if (different) {
			s << translate("volume") << " ";
			this->lastParam = PARAM_VOLUME;
		}
		s << fixed << setprecision(2);
		s << VAL2DB(volume);
		outputMessage(s);
	}

	virtual void SetSurfacePan(MediaTrack* track, double pan) override {
		if (isParamsDialogOpen || !this->shouldHandleParamChange()) {
			return;
		}
		ostringstream s;
		bool different = this->reportTrackIfDifferent(track, s);
		different |= this->lastParam != PARAM_PAN;
		if (different) {
			s << translate("pan") << " ";
			this->lastParam = PARAM_PAN;
		}
		formatPan(pan, s);
		outputMessage(s);
	}

	virtual void SetSurfaceMute(MediaTrack* track, bool mute) override {
		if (!shouldReportSurfaceChanges) {
			return;
		}
		auto cache = this->cachedTrackState<TC_MUTED, TC_UNMUTED>(track);
		if (!isParamsDialogOpen && !this->wasCausedByCommand() &&
				cache.hasChanged(mute)) {
			ostringstream s;
			this->reportTrackIfDifferent(track, s);
			s << (mute ? translate("muted") : translate("unmuted"));
			outputMessage(s);
		}
		cache.update(mute);
	}

	virtual void SetSurfaceSolo(MediaTrack* track, bool solo) override {
		if (!shouldReportSurfaceChanges) {
			return;
		}
		if (track == GetMasterTrack(nullptr) && !(GetMasterMuteSoloFlags() & 2)) {
			// REAPER reports that the master track is soloed when you solo a track,
			// but it isn't from the user perspective.
			return;
		}
		auto cache = this->cachedTrackState<TC_SOLOED, TC_UNSOLOED>(track);
		if (!isParamsDialogOpen && !this->wasCausedByCommand() &&
				cache.hasChanged(solo)) {
			ostringstream s;
			this->reportTrackIfDifferent(track, s);
			s << (solo ? translate("soloed") : translate("unsoloed"));
			outputMessage(s);
		}
		cache.update(solo);
	}

	virtual void SetSurfaceRecArm(MediaTrack* track, bool arm) override {
		if (!shouldReportSurfaceChanges) {
			return;
		}
		auto cache = this->cachedTrackState<TC_ARMED, TC_UNARMED>(track);
		if (!isParamsDialogOpen && !this->wasCausedByCommand() &&
				cache.hasChanged(arm)) {
			ostringstream s;
			this->reportTrackIfDifferent(track, s);
			s << (arm ? translate("armed") : translate("unarmed"));
			outputMessage(s);
		}
		cache.update(arm);
		// REAPER calls SetSurfaceVolume after arming a track. Ensure we don't
		// report that, since nothing has really changed.
		this->lastParamChangeTime = GetTickCount();
	}

	virtual void SetSurfaceSelected(MediaTrack* track, bool selected) override {
		if (!selected || !shouldReportSurfaceChanges ||
			// REAPER calls this a *lot*, even if the track was already selected; e.g.
			// for mute, arm, solo, etc. Ignore this if we were already told about
			// this track being selected.
			track == lastSelectedTrack
		) {
			return;
		}
		// Cache the track even if we're handling a command because that command
		// might be navigating tracks.
		this->lastSelectedTrack = this->lastChangedTrack = track;
		if (this->wasCausedByCommand()) {
			return;
		}
		// The last touched track won't be updated yet, so we pass the track
		// explicitly.
		postGoToTrack(0, track);
	}

	virtual int Extended(int call, void* parm1, void* parm2, void* parm3) override {
		if (call == CSURF_EXT_SETFXPARAM) {
			if (!this->shouldHandleParamChange()) {
				return 0; // Unsupported.
			}
			auto track = (MediaTrack*)parm1;
			int fx = *(int*)parm2 >> 16;
			// Don't report parameter changes where they might already be reported by
			// the UI.
			if (isParamsDialogOpen || TrackFX_GetChainVisible(track) == fx) {
				return 0; // Unsupported.
			}
			int param = *(int*)parm2 & 0xFFFF;
			double normVal = *(double*)parm3;
			ostringstream s;
			char chunk[256];
			bool different = this->reportTrackIfDifferent(track, s);
			different |= fx != this->lastFx;
			if (different) {
				TrackFX_GetFXName(track, fx, chunk, sizeof(chunk));
				s << chunk << " ";
			}
			this->lastFx = fx;
			different |= param != this->lastParam;
			if (different) {
				TrackFX_GetParamName(track, fx, param, chunk, sizeof(chunk));
				s << chunk << " ";
			}
			this->lastParam = param;
			TrackFX_FormatParamValueNormalized(track, fx, param, normVal, chunk,
				sizeof(chunk));
			if (chunk[0]) {
				s << chunk;
			} else {
				s << normVal;
			}
			outputMessage(s);
		}
		return 0; // Unsupported.
	}

	private:
	bool wasCausedByCommand() {
		return isHandlingCommand ||
			// Sometimes, REAPER updates control surfaces after a command rather than
			// during. If the last command OSARA handled was <= 50 ms ago, we assume
			// this update was caused by that command.
			GetTickCount() - lastCommandTime <= 50;
	}

	// Used for parameters we don't cache such as volume, pan and FX parameters.
	// We cache states such as mute, solo and arm, so they don't use this.
	bool shouldHandleParamChange() {
		if (!shouldReportSurfaceChanges || this->wasCausedByCommand()) {
			return false;
		}
		DWORD now = GetTickCount();
		DWORD prevChangeTime = this->lastParamChangeTime;
		this->lastParamChangeTime = now;
		// Only handle param changes if the last change was 100ms or more ago.
		return now - prevChangeTime >= 100;
	}
	DWORD lastParamChangeTime = 0;

	bool reportTrackIfDifferent(MediaTrack* track, ostringstream& output) {
		bool different = track != this->lastChangedTrack;
		if (different) {
			this->lastChangedTrack = track;
			int trackNum = (int)(size_t)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER",
				nullptr);
			if (trackNum <= 0) {
				output << translate("master");
			} else {
				output << trackNum;
				char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", nullptr);
				if (trackName) {
					output << " " << trackName;
				}
			}
			output << " ";
		}
		return different;
	}

	template<uint8_t enableFlag, uint8_t disableFlag>
	TrackCacheState<enableFlag, disableFlag> cachedTrackState(MediaTrack* track) {
		uint8_t& value = this->trackCache[track];
		return TrackCacheState<enableFlag, disableFlag>(value);
	}

	void reportMarker(double playPos) {
		int marker, region;
		double markerPos;
		GetLastMarkerAndCurRegion(0, playPos, &marker, &region);
		const char* name;
		int number;
		ostringstream s;
		if (marker >= 0 && marker != this->lastMarker) {
			EnumProjectMarkers(marker, nullptr, &markerPos, nullptr, &name, &number);
			// Allow the cursor to be within 100ms, since this method is called
			// periodically.
			if (markerPos >= playPos - 0.1) {
				if (name[0]) {
					s << format(translate("{} marker"), name) << " ";
				} else {
					s << format(translate("marker {}"), number) << " ";
				}
			}
		}
		this->lastMarker = marker;
		if (region >= 0 && region != this->lastRegion) {
			EnumProjectMarkers(region, nullptr, nullptr, nullptr, &name, &number);
			if (name[0]) {
				// Translators: Reported when playback reaches a named region. {} will
				// be replaced with the region's name; e.g. "intro region".
				s << format(translate("{} region"), name) << " ";
			} else {
				// Translators: Reported when playback reaches an unnamed region. {}
				// will be replaced with the region's number; e.g. "region 2".
				s << format(translate("region {}"), number) << " ";
			}
		}
		this->lastRegion = region;
		if (s.tellp() > 0) {
			outputMessage(s, /* interrupt */ false);
		}
	}

	MediaTrack* lastSelectedTrack = nullptr;
	MediaTrack* lastChangedTrack = nullptr;
	int lastFx = 0;
	const int PARAM_NONE = -1;
	const int PARAM_VOLUME = -2;
	const int PARAM_PAN = -3;
	int lastParam = PARAM_NONE;
	map<MediaTrack*, uint8_t> trackCache;
	double lastPlayPos = 0;
	int lastMarker = -1;
	int lastRegion = -1;
};

IReaperControlSurface* createSurface() {
	return new Surface;
}
