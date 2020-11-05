/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Control surface code
 * Copyright 2019-2020 NV Access Limited, James Teh, Leonard de Ruijter
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <sstream>
#include <iomanip>
#include <WDL/db2val.h>
#include "osara.h"
#include "paramsUi.h"

using namespace std;

bool shouldReportSurfaceChanges = true;

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

	virtual void SetPlayState(bool play, bool pause, bool rec) override {
		if (!this->shouldHandleChange()) {
			return;
		}
		// Calculate integer based transport state
		int TransportState = (int)play | ((int)pause << 1) | ((int)rec << 2);
		reportTransportState(TransportState);
	}

	virtual void SetRepeatState(bool repeat) override {
		if (!this->shouldHandleChange()) {
			return;
		}
		reportRepeat(repeat);
	}

	virtual void SetSurfaceVolume(MediaTrack* track, double volume) override {
		if (!this->shouldHandleChange()) {
			return;
		}
		ostringstream s;
		bool different = this->reportTrackIfDifferent(track, s);
		different |= this->lastParam != PARAM_VOLUME;
		if (different) {
			s << "volume ";
			this->lastParam = PARAM_VOLUME;
		}
		s << fixed << setprecision(2);
		s << VAL2DB(volume);
		outputMessage(s);
	}

	virtual void SetSurfacePan(MediaTrack* track, double pan) override {
		if (!this->shouldHandleChange()) {
			return;
		}
		ostringstream s;
		bool different = this->reportTrackIfDifferent(track, s);
		different |= this->lastParam != PARAM_PAN;
		if (different) {
			s << "pan ";
			this->lastParam = PARAM_PAN;
		}
		formatPan(pan, s);
		outputMessage(s);
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
		if (isHandlingCommand) {
			return;
		}
		// The last touched track won't be updated yet, so we pass the track
		// explicitly.
		postGoToTrack(0, track);
	}

	virtual int Extended(int call, void* parm1, void* parm2, void* parm3) override {
		if (call == CSURF_EXT_SETFXPARAM) {
			if (!this->shouldHandleChange()) {
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
			// Don't report the effect name if we're changing the same effect.
			bool different = this->reportTrackIfDifferent(track, s);
			different |= fx != this->lastFx;
			if (different) {
				TrackFX_GetFXName(track, fx, chunk, sizeof(chunk));
				s << chunk << " ";
			}
			this->lastChangedTrack = track;
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
	bool shouldHandleChange() {
		if (!shouldReportSurfaceChanges) {
			return false;
		}
		DWORD now = GetTickCount();
		DWORD prevChangeTime = lastChangeTime;
		lastChangeTime = now;
		if (!isHandlingCommand &&
			// Only handle surface changes if the last change is 100ms or more ago.
			now - prevChangeTime >= 100
		) {
			return true;
		}
		return false;
	}
	DWORD lastChangeTime = 0;

	bool reportTrackIfDifferent(MediaTrack* track, ostringstream& output) {
		bool different = track != this->lastChangedTrack;
		if (different) {
			this->lastChangedTrack = track;
			int trackNum = (int)(size_t)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER",
				nullptr);
			if (trackNum <= 0) {
				output << "master";
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

	MediaTrack* lastSelectedTrack = nullptr;
	MediaTrack* lastChangedTrack = nullptr;
	int lastFx = 0;
	const int PARAM_NONE = -1;
	const int PARAM_VOLUME = -2;
	const int PARAM_PAN = -3;
	int lastParam = PARAM_NONE;
};

IReaperControlSurface* createSurface() {
	return new Surface;
}
