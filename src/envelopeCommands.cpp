/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * Envelope commands code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2023 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <sstream>
#include <tuple>
#include <regex>
#include <functional>
#include <set>
#include <algorithm>
#include <optional>
#include "osara.h"
#include "translation.h"

using namespace std;
using namespace fmt::literals;

bool selectedEnvelopeIsTake = false;
int currentAutomationItem = -1;

int getEnvelopePointByProjectTime(double time) {
	TrackEnvelope* envelope = GetSelectedEnvelope(nullptr);
	if (!envelope) {
		return -1;
	}
	double offset = 0.0;
	double rate = 1.0;
	if (selectedEnvelopeIsTake) {
		MediaItem* item = GetSelectedMediaItem(0, 0);
		if (!item) {
			return -1;
		}
		offset = GetMediaItemInfo_Value(item, "D_POSITION");
		MediaItem_Take* take = GetActiveTake(item);
		if (!take) {
			return -1;
		}
		rate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
	}
	// GetEnvelopePointByTime often returns the point before instead of right at the position.
	// Increment the cursor position a bit to work around this.
	return GetEnvelopePointByTimeEx(envelope, currentAutomationItem, (time + 0.0001 - offset) * rate);
}

int getEnvelopePointAtCursor() {
	return getEnvelopePointByProjectTime(GetCursorPosition());
}

double envelopeTimeToProjectTime(double time) {
	TrackEnvelope* envelope = GetSelectedEnvelope(nullptr);
	if (!envelope) {
		return time;
	}
	double offset = 0.0;
	double rate = 1.0;
	if (selectedEnvelopeIsTake) {
		MediaItem* item = GetSelectedMediaItem(0, 0);
		if (!item) {
			return time;
		}
		offset = GetMediaItemInfo_Value(item, "D_POSITION");
		MediaItem_Take* take = GetActiveTake(item);
		if (!take) {
			return time + offset;
		}
		rate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
	}
	return time / rate + offset;
}

void postMoveEnvelopePoint(int command) {
	TrackEnvelope* envelope = GetSelectedEnvelope(nullptr);
	if (!envelope) {
		return;
	}
	fakeFocus = FOCUS_ENVELOPE;
	int point = getEnvelopePointAtCursor();
	if (point < 0)
		return;
	double value;
	bool selected;
	GetEnvelopePointEx(envelope, currentAutomationItem, point, NULL, &value, NULL, NULL, &selected);
	if (!selected)
		return; // Not moved.
	char out[64];
	Envelope_FormatValue(envelope, value, out, sizeof(out));
	outputMessage(out);
}

int countEnvelopePointsIncludingAutoItems(TrackEnvelope* envelope) {
	// First, count the points in the envelope itself.
	int count = CountEnvelopePoints(envelope);
	// Now, add the points in each automation item.
	int itemCount = CountAutomationItems(envelope);
	for (int item = 0; item < itemCount; ++item) {
		count += CountEnvelopePointsEx(envelope, item);
	}
	return count;
}

void cmdhDeleteEnvelopePointsOrAutoItems(int command, bool checkPoints, bool checkItems) {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope)
		return;
	int oldPoints;
	int oldItems;
	if (checkPoints) {
		oldPoints = countEnvelopePointsIncludingAutoItems(envelope);
	}
	if (checkItems) {
		oldItems = CountAutomationItems(envelope);
	}
	Main_OnCommand(command, 0);
	int removed;
	// Check items first, since deleting an item might also implicitly remove
	// points.
	if (checkItems) {
		removed = oldItems - CountAutomationItems(envelope);
		// If no items wer removed, fall through to the points check below unless
		// we're not checking points, in which case report 0 items.
		if (removed > 0 || !checkPoints) {
			// Translators: Reported when removing automation items. {} will be
			// replaced with the number of items; e.g. "2 automation items removed".
			outputMessage(
					format(translate_plural("{} automation item removed", "{} automation items removed", removed), removed)
			);
			return;
		}
	}
	if (checkPoints) {
		removed = oldPoints - countEnvelopePointsIncludingAutoItems(envelope);
		// Translators: Reported when removing envelope points. {} will be
		// replaced with the number of points; e.g. "2 points removed".
		outputMessage(format(translate_plural("{} point removed", "{} points removed", removed), removed));
	}
}

void cmdDeleteEnvelopePoints(Command* command) {
	cmdhDeleteEnvelopePointsOrAutoItems(command->gaccel.accel.cmd, true, false);
}

// For each selected envelope point, Call func(autoItemIndex, pointIndex) .
// func should return true to continue iterating, false to stop.
void forEachSelectedEnvelopePoint(TrackEnvelope* envelope, auto func) {
	// Iterate the points in the envelope itself and each automation item. -1
	// means the envelope itself.
	int itemCount = CountAutomationItems(envelope);
	for (int item = -1; item < itemCount; ++item) {
		int pointCount = CountEnvelopePointsEx(envelope, item);
		for (int point = 0; point < pointCount; ++point) {
			bool selected;
			GetEnvelopePointEx(envelope, item, point, nullptr, nullptr, nullptr, nullptr, &selected);
			if (selected && !func(item, point)) {
				return;
			}
		}
	}
}

// If max2 is true, this only counts to 2;
// i.e. 2 or more selected envelope points returns 2.
int countSelectedEnvelopePoints(TrackEnvelope* envelope, bool max2 = false) {
	int numSel = 0;
	forEachSelectedEnvelopePoint(envelope, [max2, &numSel](int item, int point) {
		++numSel;
		return !(max2 && numSel == 2);
	});
	return numSel;
}

optional<int> currentEnvelopePoint{};

const char* getEnvelopeShapeName(int shape) {
	static const char* names[] = {
			// Translators: A shape for an envelope point.
			translate("linear"),
			// Translators: A shape for an envelope point.
			translate("square"),
			// Translators: A shape for an envelope point.
			translate("slow start/end"),
			// Translators: A shape for an envelope point.
			translate("fast start"),
			// Translators: A shape for an envelope point.
			translate("fast end"),
			// Translators: A shape for an envelope point.
			translate("bezier"),
	};
	return names[shape];
}

void moveToEnvelopePoint(int direction, bool clearSelection = true, bool select = true) {
	TrackEnvelope* envelope = GetSelectedEnvelope(nullptr);
	if (!envelope) {
		return;
	}
	int count = CountEnvelopePointsEx(envelope, currentAutomationItem);
	if (count == 0) {
		return;
	}
	double now = GetCursorPosition();
	int point = getEnvelopePointAtCursor();
	if (point < 0) {
		if (direction != 1)
			return;
		++point;
	}
	double time, value;
	int shape;
	bool selected;
	GetEnvelopePointEx(envelope, currentAutomationItem, point, &time, &value, &shape, nullptr, &selected);
	time = envelopeTimeToProjectTime(time);
	if ((direction == 1 && time < now)
			// If this point is at the cursor, skip it only if it's the current point.
			// This allows you to easily get to a point at the cursor
			// while still allowing you to move beyond it once you do.
			|| (direction == 1 && point == currentEnvelopePoint && time == now)
			// Moving backward should skip the point at the cursor.
			|| (direction == -1 && time >= now)) {
		// This isn't the point we want. Try the next.
		int newPoint = point + direction;
		if (0 <= newPoint && newPoint < count) {
			point = newPoint;
			GetEnvelopePointEx(envelope, currentAutomationItem, point, &time, &value, &shape, nullptr, &selected);
			time = envelopeTimeToProjectTime(time);
		}
	}
	if (direction != 0 && direction == 1 ? time < now : time > now)
		return; // No point in this direction.
	fakeFocus = FOCUS_ENVELOPE;
	currentEnvelopePoint.emplace(point);
	if (clearSelection) {
		Main_OnCommand(40331, 0); // Envelope: Unselect all points
		isSelectionContiguous = true;
	}
	if (select)
		SetEnvelopePointEx(envelope, currentAutomationItem, point, NULL, NULL, NULL, NULL, &bTrue, &bTrue);
	if (direction != 0)
		SetEditCurPos(time, true, true);
	char out[64];
	Envelope_FormatValue(envelope, value, out, sizeof(out));
	ostringstream s;
	// Translators: Reported when moving to an envelope point. {point} will be
	// replaced with the number of the point. {value} will be replaced with its
	// value. {shape} will be replaced with its shape.
	// For example: "point 1 value 0.00 dB linear".
	s << format(
			translate("point {point} value {value} {shape}"), "point"_a = point, "value"_a = out,
			"shape"_a = getEnvelopeShapeName(shape)
	);
	bool isSelected;
	GetEnvelopePointEx(envelope, currentAutomationItem, point, NULL, NULL, NULL, NULL, &isSelected);
	if (isSelected) {
		int numSel = countSelectedEnvelopePoints(envelope, true);
		// One selected point is the norm, so don't report selected in this case.
		if (numSel > 1) {
			s << " " << translate("selected");
		}
	} else {
		s << " " << translate("unselected");
	}
	s << " " << formatCursorPosition();
	outputMessage(s);
}

optional<bool> toggleCurrentEnvelopePointSelection() {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope || !currentEnvelopePoint)
		return nullopt;
	bool isSelected;
	if (!GetEnvelopePointEx(envelope, currentAutomationItem, *currentEnvelopePoint, NULL, NULL, NULL, NULL, &isSelected))
		return nullopt;
	isSelected = !isSelected;
	SetEnvelopePointEx(
			envelope, currentAutomationItem, *currentEnvelopePoint, NULL, NULL, NULL, NULL, &isSelected, &bTrue
	);
	return {isSelected};
}

void cmdInsertEnvelopePoint(Command* command) {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope)
		return;
	int oldCount = CountEnvelopePoints(envelope);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	if (CountEnvelopePoints(envelope) <= oldCount)
		return;
	moveToEnvelopePoint(0); // Select and report inserted point.
}

const regex RE_ENVELOPE_STATE("<(AUX|HW)?(\\S+)[^]*?\\sACT (0|1)[^]*?\\sVIS (0|1)[^]*?\\sARM (0|1)");

void cmdhSelectEnvelope(int direction) {
	MediaTrack* track = NULL;
	int count;
	function<TrackEnvelope*(int)> getEnvelope;
	// selectedEnvelopeIsTake is set when focus changes to track or item.
	if (selectedEnvelopeIsTake) {
		MediaItem* item = GetSelectedMediaItem(0, 0);
		if (!item)
			return;
		MediaItem_Take* take = GetActiveTake(item);
		if (!take)
			return;
		count = CountTakeEnvelopes(take);
		getEnvelope = [take](int index) { return GetTakeEnvelope(take, index); };
	} else {
		track = GetLastTouchedTrack();
		if (!track)
			return;
		count = CountTrackEnvelopes(track);
		getEnvelope = [track](int index) { return GetTrackEnvelope(track, index); };
	}
	if (count == 0) {
		outputMessage(selectedEnvelopeIsTake ? translate("no take envelopes") : translate("no track envelopes"));
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
	for (;;) {
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
		char state[200];
		GetEnvelopeStateChunk(env, state, sizeof(state), false);
		regex_search(state, m, RE_ENVELOPE_STATE);
		bool invisible = !m.empty() && m.str(4)[0] == '0';
		if (env == origEnv) {
			// We're back where we started. Don't try to go any further.
			if (invisible) {
				// This envelope is now invisible, so don't report it.
				env = nullptr;
			}
			break;
		}
		if (invisible)
			continue; // Invisible, so skip.
		break; // We found our envelope!
	}
	if (!env) {
		outputMessage(translate("no visible envelopes"));
		return;
	}

	SetCursorContext(2, env);
	currentAutomationItem = -1;
	currentEnvelopePoint.reset();
	fakeFocus = FOCUS_ENVELOPE;
	shouldMoveToAutoItem = true;
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
	// Translators: Reported when selecting an envelope. {} will be replaced
	// with the name of the envelope; e.g. "volume envelope".
	s << format(translate("{} envelope"), name);
	if (!m.empty()) {
		if (m.str(3)[0] == '0') {
			s << " " << translate("bypassed");
		}
		if (m.str(5)[0] == '1') {
			s << " " << translate("armed");
		}
	}
	outputMessage(s);
}

void cmdSelectNextEnvelope(Command* command) {
	cmdhSelectEnvelope(1);
}

void cmdSelectPreviousEnvelope(Command* command) {
	cmdhSelectEnvelope(-1);
}

void cmdMoveToNextEnvelopePoint(Command* command) {
	moveToEnvelopePoint(1, true, true);
}

void cmdMoveToPrevEnvelopePoint(Command* command) {
	moveToEnvelopePoint(-1, true, true);
}

void cmdMoveToNextEnvelopePointKeepSel(Command* command) {
	moveToEnvelopePoint(1, false, isSelectionContiguous);
}

void cmdMoveToPrevEnvelopePointKeepSel(Command* command) {
	moveToEnvelopePoint(-1, false, isSelectionContiguous);
}

void selectAutomationItem(TrackEnvelope* envelope, int index, bool select = true) {
	GetSetAutomationItemInfo(envelope, index, "D_UISEL", select, true);
}

void unselectAllAutomationItems(TrackEnvelope* envelope) {
	int count = CountAutomationItems(envelope);
	for (int i = 0; i < count; ++i) {
		selectAutomationItem(envelope, i, false);
	}
}

bool isAutomationItemSelected(TrackEnvelope* envelope, int index) {
	return GetSetAutomationItemInfo(envelope, index, "D_UISEL", 0, false);
}

// If max2 is true, this only counts to 2;
// i.e. 2 or more selected automation items returns 2.
int countSelectedAutomationItems(TrackEnvelope* envelope, bool max2 = false) {
	int count = CountAutomationItems(envelope);
	int sel = 0;
	for (int i = 0; i < count; ++i) {
		if (isAutomationItemSelected(envelope, i)) {
			++sel;
		}
		if (max2 && sel == 2) {
			// optimisation: We don't care beyond 2.
			break;
		}
	}
	return sel;
}

void moveToAutomationItem(int direction, bool clearSelection = true, bool select = true) {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope) {
		return;
	}
	int count = CountAutomationItems(envelope);
	if (!count) {
		return;
	}
	double cursor = GetCursorPosition();
	double pos;
	int start = direction == 1 ? 0 : count - 1;
	if (0 <= currentAutomationItem && currentAutomationItem < count) {
		pos = GetSetAutomationItemInfo(envelope, currentAutomationItem, "D_POSITION", 0, false);
		if (direction == 1 ? pos <= cursor : pos >= cursor) {
			// The cursor is right at or has moved past the automation item to which the user last moved.
			// Therefore, start at the adjacent automation item.
			// This is faster and also allows the user to move to automation items which start at the same position.
			start = currentAutomationItem + direction;
			if (start < 0 || start >= count) {
				// There's no adjacent automation item in this direction,
				// so move to the current one again.
				start -= direction;
			}
		}
	} else {
		currentAutomationItem = -1; // Invalid.
	}

	for (int i = start; 0 <= i && i < count; i += direction) {
		pos = GetSetAutomationItemInfo(envelope, i, "D_POSITION", 0, false);
		if (direction == 1 ? pos < cursor : pos > cursor) {
			continue; // Not the right direction.
		}
		currentAutomationItem = i;
		if (clearSelection || select) {
			Undo_BeginBlock();
		}
		if (clearSelection) {
			unselectAllAutomationItems(envelope);
			isSelectionContiguous = true;
		}
		if (select) {
			selectAutomationItem(envelope, i);
		}
		if (clearSelection || select) {
			Undo_EndBlock(translate("Change Automation Item Selection"), 0);
		}
		SetEditCurPos(pos, true, true); // Seek playback.

		// Report the automation item.
		fakeFocus = FOCUS_AUTOMATIONITEM;
		ostringstream s;
		char name[500];
		GetSetAutomationItemInfo_String(envelope, i, "P_POOL_NAME", name, false);
		if (name[0]) {
			// Translators: Reported when moving to an automation item. {} will be
			// replaced with the name or number of the automation item; e.g. "auto 2".
			s << format(translate("auto {}"), name);
		} else {
			s << format(translate("auto {}"), i + 1);
		}
		if (isAutomationItemSelected(envelope, i)) {
			// One selected item is the norm, so don't report selected in this case.
			if (countSelectedAutomationItems(envelope, true) > 1) {
				s << " " << translate("selected");
			}
		} else {
			s << " " << translate("unselected");
		}
		s << " " << formatCursorPosition();
		outputMessage(s);
		return;
	}
}

bool toggleCurrentAutomationItemSelection() {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope || currentAutomationItem == -1) {
		// We really shouldn't get called if this happens, but just in case...
		return false;
	}
	bool select = !isAutomationItemSelected(envelope, currentAutomationItem);
	selectAutomationItem(envelope, currentAutomationItem, select);
	return select;
}

void reportCopiedEnvelopePointsOrAutoItems() {
	TrackEnvelope* envelope = GetSelectedEnvelope(0);
	if (!envelope) {
		return;
	}
	int count;
	if ((count = countSelectedAutomationItems(envelope))) {
		// Translators: Reported when copying automation items. {} will be replaced
		// with the number of items; e.g. "2 automation items copied".
		outputMessage(format(translate_plural("{} automation item copied", "{} automation items copied", count), count));
	} else {
		count = countSelectedEnvelopePoints(envelope);
		// Translators: Reported when copying envelope points. {} will be replaced
		// with the number of points; e.g. "2 envelope points copied".
		outputMessage(format(translate_plural("{} envelope point copied", "{} envelope points copied", count), count));
	}
}

bool isEnvelopeVisible(TrackEnvelope* envelope) {
	char state[200];
	GetEnvelopeStateChunk(envelope, state, sizeof(state), false);
	cmatch m;
	regex_search(state, m, RE_ENVELOPE_STATE);
	return !m.empty() && m.str(4)[0] == '1';
}

set<TrackEnvelope*> getVisibleEnvelopes(auto obj, auto countFunc, auto getFunc) {
	set<TrackEnvelope*> envelopes;
	int count = countFunc(obj);
	for (int i = 0; i < count; ++i) {
		TrackEnvelope* env = getFunc(obj, i);
		if (isEnvelopeVisible(env)) {
			envelopes.emplace(env);
		}
	}
	return envelopes;
}

void cmdhToggleEnvelope(
		int command, auto obj, auto countFunc, auto getFunc, const char* showedMsg, const char* hidMsg
) {
	set<TrackEnvelope*> before = getVisibleEnvelopes(obj, countFunc, getFunc);
	Main_OnCommand(command, 0);
	set<TrackEnvelope*> after = getVisibleEnvelopes(obj, countFunc, getFunc);
	if (after.size() == before.size()) {
		outputMessage(translate("no envelopes toggled"));
		return;
	}
	set<TrackEnvelope*> difference;
	set_symmetric_difference(
			before.begin(), before.end(), after.begin(), after.end(), inserter(difference, difference.end())
	);
	TrackEnvelope* envelope = *difference.begin();
	char name[50];
	GetEnvelopeName(envelope, name, sizeof(name));
	if (after.size() > before.size()) {
		outputMessage(format(showedMsg, name));
	} else {
		outputMessage(format(hidMsg, name));
	}
}

void cmdhToggleTrackEnvelope(int command) {
	MediaTrack* track = GetLastTouchedTrack();
	if (!track) {
		return;
	}
	cmdhToggleEnvelope(
			command, track, CountTrackEnvelopes, GetTrackEnvelope, translate("showed track {} envelope"),
			translate("hid track {} envelope")
	);
}

void cmdToggleTrackEnvelope(Command* command) {
	cmdhToggleTrackEnvelope(command->gaccel.accel.cmd);
}

void cmdhToggleTakeEnvelope(int command) {
	MediaItem* item = getItemWithFocus();
	if (!item) {
		return;
	}
	MediaItem_Take* take = GetActiveTake(item);
	if (!take) {
		return;
	}
	cmdhToggleEnvelope(
			command, take, CountTakeEnvelopes, GetTakeEnvelope, translate("showed take {} envelope"),
			translate("hid take {} envelope")
	);
}

void cmdToggleTakeEnvelope(Command* command) {
	cmdhToggleTakeEnvelope(command->gaccel.accel.cmd);
}

void postSelectMultipleEnvelopePoints(int command) {
	TrackEnvelope* envelope = GetSelectedEnvelope(nullptr);
	if (!envelope) {
		return;
	}
	int count = countSelectedEnvelopePoints(envelope);
	// Translators: Reported when selecting envelope points. {} will be replaced
	// with the number of points; e.g. "2 points selected".
	outputMessage(format(translate_plural("{} point selected", "{} points selected", count), count));
}

void cmdMoveSelEnvelopePoints(Command* command) {
	TrackEnvelope* envelope = GetSelectedEnvelope(nullptr);
	if (!envelope || !currentEnvelopePoint || !shouldReportTimeMovement()) {
		Main_OnCommand(command->gaccel.accel.cmd, 0);
		return;
	}
	double oldPos{0.0};
	GetEnvelopePointEx(
			envelope, currentAutomationItem, *currentEnvelopePoint, &oldPos, nullptr, nullptr, nullptr, nullptr
	);
	Main_OnCommand(command->gaccel.accel.cmd, 0);
	double newPos{0.0};
	GetEnvelopePointEx(
			envelope, currentAutomationItem, *currentEnvelopePoint, &newPos, nullptr, nullptr, nullptr, nullptr
	);
	ostringstream s;
	auto cache = FT_USE_CACHE;
	if (lastCommand != command->gaccel.accel.cmd) {
		s << getActionName(command->gaccel.accel.cmd) << " ";
	}
	if (oldPos == newPos) {
		s << translate("no change");
	} else {
		s << formatTime(envelopeTimeToProjectTime(newPos), TF_RULER, false, cache);
	}
	outputMessage(s);
}

void cmdCycleEnvelopePointShape(Command* command) {
	TrackEnvelope* envelope = GetSelectedEnvelope(nullptr);
	if (!envelope) {
		return;
	}
	int shape = -1;
	forEachSelectedEnvelopePoint(envelope, [envelope, &shape](int item, int point) {
		if (shape == -1) {
			// Get the shape of the first selected point. (The shape variable will
			// only be -1 for the first point we encounter.)
			GetEnvelopePointEx(envelope, item, point, nullptr, nullptr, &shape, nullptr, nullptr);
			// Adjust the shape. This will be set for all selected points.
			++shape;
			if (shape > 4) {
				// 4 is fast end. Don't support setting bezier (5) here, as that requires
				// a tension value.
				shape = 0; // Linear
			}
		}
		SetEnvelopePointEx(envelope, item, point, nullptr, nullptr, &shape, nullptr, nullptr, nullptr);
		return true;
	});
	if (shape == -1) {
		return; // No selected points.
	}
	outputMessage(getEnvelopeShapeName(shape));
}

void cmdToggleVolumeEnvelope(Command* command) {
	if (fakeFocus == FOCUS_ITEM) {
		cmdhToggleTakeEnvelope(40693); // Take: Toggle take volume envelope
	} else {
		cmdhToggleTrackEnvelope(40406); // Track: Toggle track volume envelope visible
	}
}

void cmdTogglePanEnvelope(Command* command) {
	if (fakeFocus == FOCUS_ITEM) {
		cmdhToggleTakeEnvelope(40694); // Take: Toggle take pan envelope
	} else {
		cmdhToggleTrackEnvelope(40407); // Track: Toggle track pan envelope visible
	}
}

void cmdToggleMuteEnvelope(Command* command) {
	if (fakeFocus == FOCUS_ITEM) {
		cmdhToggleTakeEnvelope(40695); // Take: Toggle take mute envelope
	} else {
		cmdhToggleTrackEnvelope(40867); // Track: Toggle track mute envelope visible
	}
}

void cmdTogglePreFXPanOrTakePitchEnvelope(Command* command) {
	if (fakeFocus == FOCUS_ITEM) {
		cmdhToggleTakeEnvelope(41612); // Take: Toggle take pitch envelope
	} else {
		cmdhToggleTrackEnvelope(40409); // Track: Toggle track pre-FX pan envelope visible
	}
}
