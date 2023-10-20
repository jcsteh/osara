/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * MIDI Editor commands code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2023 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <map>
#include <cassert>
#include <functional>
#include <float.h>
#include <compare>
#include <regex>
#include <string_view>
#include "midiEditorCommands.h"
#include "osara.h"
#include "config.h"
#include "translation.h"
#ifdef _WIN32
#include <Commctrl.h>
#endif

using namespace std;
using namespace fmt::literals;

// returns the pulses per quarter note of the first midi source in the item.
int getItemPPQ(MediaItem* item) {
	const int defaultPPQ = 960;
	static MediaItem* cachedItem;
	static int cachedPPQ;
	if (item == cachedItem) {
		return cachedPPQ;
	}
	char buff[4096] = "";
	if (!GetItemStateChunk(item, buff, sizeof(buff), false)) {
		return defaultPPQ;
	}
	static const regex re("^\\s*HASDATA [0-9]+ ([0-9]+) ");
	cmatch match;
	if (!regex_search(buff, match, re)) {
		return defaultPPQ;
	}
	int ppq = stoi(match.str(1));
	cachedPPQ = ppq;
	cachedItem = item;
	return ppq;
}

struct FreeReaperPtr {
	void operator()(void* p) {
		FreeHeapPtr(p);
	}
};

// return the midi editor zoom ratio of the take
double getMidiZoomRatio(MediaItem_Take* take) {
	static const regex re("CFGEDITVIEW -?[0-9.]+ ([0-9.]+) ");
	char guid[40];
	GetSetMediaItemTakeInfo_String(take, "GUID", guid, false);
	MediaItem* item = GetMediaItemTake_Item(take);
	unique_ptr<char, FreeReaperPtr> state(GetSetObjectState(item, ""));
	if (!state) {
		return -1;
	}
	auto stateSV = string_view(state.get());
	size_t takePos = stateSV.find(guid);
	if (takePos == string::npos) {
		return -1;
	}
	match_results<string_view::const_iterator> match;
	if (!regex_search(stateSV.cbegin() + takePos, stateSV.cend(), match, re)) {
		return -1;
	}
	return stod(match.str(1));
}

// Note: while the below struct is called MidiControlChange in line with naming in Reaper,
// It is also used for other MIDI messages.
struct MidiControlChange {
	int channel = -1;
	int index = -1;
	int message1 = -1;
	int message2 = -1;
	int message3 = -1;
	double position = -1.0;
	bool selected;
	bool muted;

	struct ReqParams {
		bool position = true;
		bool message1 = false;
		bool channel = false;
		bool message2 = false;
		bool message3 = false;
		bool selected = false;
		bool muted = false;
	};

	// Used to compare a position with the position of a CC.
	struct CompareByPosition {
		bool operator()(const MidiControlChange& cc, double pos) const {
			return cc.position < pos;
		}

		bool operator()(double pos, const MidiControlChange& cc) const {
			return pos < cc.position;
		}
	};

	static bool compareForSortAtPosition(const MidiControlChange& cc1, const MidiControlChange& cc2) {
		if (cc1.message1 < cc2.message1) {
			return true;
		}
		if (cc1.message1 > cc2.message1) {
			return false;
		}
		if (cc1.channel < cc2.channel) {
			return true;
		}
		if (cc1.channel > cc2.channel) {
			return false;
		}
		if (cc1.message2 < cc2.message2) {
			return true;
		}
		if (cc1.message2 > cc2.message2) {
			return false;
		}
		return cc1.message3 < cc2.message3;
	}

	static const MidiControlChange get(MediaItem_Take* take, int index, ReqParams params) {
		MidiControlChange cc;
		double position;
		if (MIDI_GetCC(
						take, index, params.selected ? &cc.selected : nullptr, params.muted ? &cc.muted : nullptr,
						params.position ? &position : nullptr, params.message1 ? &cc.message1 : nullptr,
						params.channel ? &cc.channel : nullptr, params.message2 ? &cc.message2 : nullptr,
						params.message3 ? &cc.message3 : nullptr
				)) {
			if (params.position) {
				position = MIDI_GetProjTimeFromPPQPos(take, position);
				cc.position = position;
			}
			cc.index = index;
		} else {
			cc.position = DBL_MAX;
		}
		return cc;
	}

	static const int getCount(MediaItem_Take* take) {
		int count = 0;
		while (MIDI_GetCC(take, count, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
			++count;
		}
		return count;
	}
};

const UINT DEFAULT_PREVIEW_LENGTH = 300; // ms

struct MidiNote {
	int channel = -1;
	int pitch = -1;
	int velocity = -1;
	int index = -1;
	double start = -1.0;
	double end = -1.0;
	bool selected;
	bool muted;

	double getLength() const {
		return max(0, (this->end - this->start));
	}

	struct ReqParams {
		bool start = true;
		bool end = false;
		bool channel = false;
		bool pitch = false;
		bool velocity = false;
		bool selected = false;
		bool muted = false;
	};

	// Used to compare a position with the start of a note.
	struct CompareByStart {
		bool operator()(const MidiNote& note, double pos) const {
			return note.start < pos;
		}

		bool operator()(double pos, const MidiNote& note) const {
			return pos < note.start;
		}
	};

	// Used to order notes in a chord by pitch.
	static bool compareByPitch(const MidiNote& note1, const MidiNote& note2) {
		return note1.pitch < note2.pitch;
	}

	static const MidiNote get(MediaItem_Take* take, int index, ReqParams params) {
		MidiNote note;
		double start, end;
		if (MIDI_GetNote(
						take, index, params.selected ? &note.selected : nullptr, params.muted ? &note.muted : nullptr,
						params.start ? &start : nullptr, params.end ? &end : nullptr, params.channel ? &note.channel : nullptr,
						params.pitch ? &note.pitch : nullptr, params.velocity ? &note.velocity : nullptr
				)) {
			if (params.start) {
				start = MIDI_GetProjTimeFromPPQPos(take, start);
				note.start = start;
			}
			if (params.end) {
				end = MIDI_GetProjTimeFromPPQPos(take, end);
				note.end = end;
			}
			note.index = index;
		} else {
			note.start = note.end = DBL_MAX;
		}
		return note;
	}

	static const int getCount(MediaItem_Take* take) {
		int count = 0;
		while (MIDI_GetNote(take, count, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
			++count;
		}
		return count;
	}
};

struct MidiEventListData {
	int index = -1;
	double position = -1.0;
	string message;
	int offVel = -1;
	double length = -1.0;
	bool selected;

	struct ReqParams {};

	// Used to compare a position with the position of a MIDI event list item.
	struct CompareByPosition {
		bool operator()(const MidiEventListData& data, double pos) const {
			return data.position < pos;
		}

		bool operator()(double pos, const MidiEventListData& data) const {
			return pos < data.position;
		}
	};

	static const MidiEventListData get(HWND editor, int index, ReqParams params = {}) {
		MidiEventListData data{index};
		auto setting = format("list_{}", index);
		char eventData[255] = "\0";
		if (MIDIEditor_GetSetting_str(editor, setting.c_str(), eventData, sizeof(eventData))) {
			MediaItem_Take* take = MIDIEditor_GetTake(editor);
			MediaItem* item = GetMediaItemTake_Item(take);
			int ppq = getItemPPQ(item);
			string key, val;
			istringstream s(eventData);
			double eventPosPpq = -1.0;
			double lengthPpq = -1.0;
			while (getline(getline(s, key, '='), val, ' ')) {
				if (key == "pos") {
					eventPosPpq = stof(val) * ppq;
					data.position = MIDI_GetProjTimeFromPPQPos(take, eventPosPpq);
				} else if (key == "len") {
					lengthPpq = stof(val) * ppq;
				} else if (key == "msg") {
					data.message = val;
				} else if (key == "offvel") {
					data.offVel = stoi(val);
				} else if (key == "sel") {
					data.selected = val == "1" ? true : false;
				}
			}
			if (lengthPpq >= 0.0) {
				double endPos = MIDI_GetProjTimeFromPPQPos(take, lengthPpq + eventPosPpq);
				data.length = endPos - data.position;
			}
		} else {
			data.position = DBL_MAX;
		}
		return data;
	}

	static const int getCount(HWND midiEditor) {
		return MIDIEditor_GetSetting_int(midiEditor, "list_cnt");
	}

	const MidiNote toMidiNote() {
		int msg = stoi(this->message, nullptr, 16);
		MidiNote note{
				(msg >> 16) & 0xf, // channel
				(msg >> 8) & 0x7f, // pitch
				msg & 0x7f, // velocity
				-1, // index
				this->position, // start
				this->position + this->length, // end
		};
		return note;
	}
};

vector<MidiNote> previewingNotes; // Notes currently being previewed.
UINT_PTR previewDoneTimer = 0;
const int MIDI_NOTE_ON = 0x90;
const int MIDI_NOTE_OFF = 0x80;

// A minimal PCM_source to send MIDI events for preview.
class PreviewSource : public PCM_source {
	public:
	PreviewSource() {}

	virtual ~PreviewSource() {}

	// The events to send.
	// These will be consumed (and the vector cleared) soon after PlayTrackPreview is called.
	vector<MIDI_event_t> events;

	bool SetFileName(const char* fn) {
		return false;
	}

	PCM_source* Duplicate() {
		return new PreviewSource();
	}

	bool IsAvailable() {
		return true;
	}

	const char* GetType() {
		return "OsaraMIDIPreview";
	};

	int GetNumChannels() {
		return 1;
	}

	double GetSampleRate() {
		return 0.0;
	}

	double GetLength() {
		// This only needs to be long enough to send MIDI events immediately.
		// Once we send note on events, the notes stay on, even though the preview stops.
		// We then play another preview with more events to turn the notes off.
		return 0.001;
	}

	int PropertiesWindow(HWND parent) {
		return -1;
	}

	void GetSamples(PCM_source_transfer_t* block) {
		block->samples_out = 0;
		if (block->midi_events) {
			for (auto event = this->events.begin(); event != this->events.end(); ++event)
				block->midi_events->AddItem(&*event);
			this->events.clear();
		}
	}

	void GetPeakInfo(PCM_source_peaktransfer_t* block) {
		block->peaks_out = 0;
	}

	void SaveState(ProjectStateContext* ctx) {}

	int LoadState(char* firstLine, ProjectStateContext* ctx) {
		return -1;
	}

	void Peaks_Clear(bool deleteFile) {}

	int PeaksBuild_Begin() {
		return 0;
	}

	int PeaksBuild_Run() {
		return 0;
	}

	void PeaksBuild_Finish() {}
};

PreviewSource previewSource;
preview_register_t previewReg = {0};

// Queue note off events for the notes currently being previewed.
// when sendNoteOff is true, this function  also sends the events.
void previewNotesOff(bool sendNoteOff) {
	for (auto note = previewingNotes.cbegin(); note != previewingNotes.cend(); ++note) {
		MIDI_event_t event = {
				0,
				3,
				{(unsigned char)(MIDI_NOTE_OFF | note->channel), (unsigned char)note->pitch, (unsigned char)note->velocity}
		};
		previewSource.events.push_back(event);
	}
	if (sendNoteOff) {
		// Send the events.
		previewReg.curpos = 0.0;
		PlayTrackPreview(&previewReg);
	}
	previewingNotes.clear();
}

// Called after the preview length elapses to turn off notes currently being previewed.
void CALLBACK previewDone(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	if (event != previewDoneTimer) {
		return; // Cancelled.
	}
	bool canceled = cancelPendingMidiPreviewNotesOff();
	assert(canceled);
	previewNotesOff(true);
}

// Used to find out the minimum note length.
bool compareNotesByLength(const MidiNote& note1, const MidiNote& note2) {
	return note1.getLength() < note2.getLength();
}

void previewNotes(MediaItem_Take* take, const vector<MidiNote>& notes) {
	if (!GetToggleCommandState2(
					SectionFromUniqueID(MIDI_EDITOR_SECTION), 40041
			)) { // Options: Preview notes when inserting or editing
		return;
	}
	if (!previewReg.src) {
		// Initialise preview.
#ifdef _WIN32
		InitializeCriticalSection(&previewReg.cs);
#else
		pthread_mutex_init(&previewReg.mutex, NULL);
#endif
		previewReg.src = &previewSource;
		previewReg.m_out_chan = -1; // Use .preview_track.
	}
	// Stop the current preview.
	if (cancelPendingMidiPreviewNotesOff()) {
		previewNotesOff(false);
	}
	// Queue note on events for the new notes.
	for (auto const& note : notes) {
		if (note.muted) {
			continue;
		}
		MIDI_event_t event = {
				0, 3, {(unsigned char)(MIDI_NOTE_ON | note.channel), (unsigned char)note.pitch, (unsigned char)note.velocity}
		};
		previewSource.events.push_back(event);
		// Save the note being previewed so we can turn it off later (previewNotesOff).
		previewingNotes.push_back(note);
	}
	// Send the events.
	void* track = GetSetMediaItemTakeInfo(take, "P_TRACK", NULL);
	previewReg.preview_track = track;
	previewReg.curpos = 0.0;
	PlayTrackPreview(&previewReg);
	// Calculate the minimum note length.
	double minLength = min_element(previewingNotes.cbegin(), previewingNotes.cend(), compareNotesByLength)->getLength();
	// Schedule note off messages.
	previewDoneTimer = SetTimer(nullptr, 0, (UINT)(minLength ? minLength * 1000 : DEFAULT_PREVIEW_LENGTH), previewDone);
}

bool cancelPendingMidiPreviewNotesOff() {
	if (previewDoneTimer) {
		KillTimer(nullptr, previewDoneTimer);
		previewDoneTimer = 0;
		return true;
	}
	return false;
}

// A random access iterator for MIDI events.
// The EventType template parameter is used to specialise the
// template for different event types (note, CC, etc.).
// The SourceType template parameter is used to define the source of events (MediaItem_Take*, HWND for event list
// window, etc.)
template<typename EventType, typename SourceType> class MidiEventIterator {
	public:
	using difference_type = int;
	using value_type = const EventType;
	using pointer = value_type*;
	using reference = value_type;
	using iterator_category = random_access_iterator_tag;

	MidiEventIterator(SourceType source, typename EventType::ReqParams ReqParams = {}, difference_type index = 0)
		: source(source), reqParams(ReqParams), index(index) {
		this->count = EventType::getCount(this->source);
	}

	bool operator==(const MidiEventIterator& other) const {
		return this->source == other.source && this->index == other.index;
	}

	bool operator!=(const MidiEventIterator& other) const {
		return !(*this == other);
	}

	auto operator<=>(const MidiEventIterator& other) const {
		return this->index <=> other.index;
	}

	value_type operator[](const difference_type index) const {
		return this->getEvent(this->index + index);
	}

	reference operator*() const {
		return (*this)[0];
	}

	pointer operator->() const {
		this->currentValue = (*this)[0];
		return &(this->currentValue);
	}

	MidiEventIterator& operator++() {
		++this->index;
		return *this;
	}

	MidiEventIterator& operator--() {
		--this->index;
		return *this;
	}

	MidiEventIterator& operator+=(const difference_type increment) {
		this->index += increment;
		return *this;
	}

	MidiEventIterator& operator-=(const difference_type decrement) {
		this->index -= decrement;
		return *this;
	}

	MidiEventIterator operator+(const difference_type increment) {
		auto tmpIt = *this;
		tmpIt += increment;
		return tmpIt;
	}

	MidiEventIterator operator-(const difference_type decrement) {
		auto tmpIt = *this;
		tmpIt -= decrement;
		return tmpIt;
	}

	difference_type operator-(const MidiEventIterator& other) {
		return this->index - other.index;
	}

	void moveToEnd() {
		this->index = count;
	}

	difference_type getIndex() const {
		return this->index;
	}

	protected:
	value_type getEvent(difference_type index) const {
		return EventType::get(this->source, index, this->reqParams);
	}

	SourceType source;

	private:
	int count;
	typename EventType::ReqParams reqParams;
	difference_type index;
	mutable EventType currentValue;
};

using MidiNoteIterator = MidiEventIterator<MidiNote, MediaItem_Take*>;

const string getMidiNoteName(MediaItem_Take* take, int pitch, int channel) {
	static const char* names[] = {
			// Translators: The name of a musical note.
			translate("c"),
			// Translators: The name of a musical note.
			translate("c sharp"),
			// Translators: The name of a musical note.
			translate("d"),
			// Translators: The name of a musical note.
			translate("d sharp"),
			// Translators: The name of a musical note.
			translate("e"),
			// Translators: The name of a musical note.
			translate("f"),
			// Translators: The name of a musical note.
			translate("f sharp"),
			// Translators: The name of a musical note.
			translate("g"),
			// Translators: The name of a musical note.
			translate("g sharp"),
			// Translators: The name of a musical note.
			translate("a"),
			// Translators: The name of a musical note.
			translate("a sharp"),
			// Translators: The name of a musical note.
			translate("b")
	};
	MediaTrack* track = GetMediaItemTake_Track(take);
	int tracknumber = static_cast<int>(GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER")); // one based
	const char* noteName = GetTrackMIDINoteName(tracknumber - 1, pitch, channel); // track number is zero based
	ostringstream s;
	if (noteName) {
		s << noteName;
	} else {
		int octave = pitch / 12 - 1;
		int szOut = 0;
		int* octaveOffset = (int*)get_config_var("midioctoffs", &szOut);
		if (octaveOffset && (szOut == sizeof(int))) {
			octave += *octaveOffset - 1; // REAPER offset "0" is saved as "1" in the preferences file.
		}
		pitch %= 12;
		s << names[pitch] << " " << octave;
	}
	return s.str();
}

// Returns iterators to the first and exclusive last notes in a chord in a given direction.
pair<MidiNoteIterator, MidiNoteIterator> findChord(
		MediaItem_Take* take, int direction, MidiNote::ReqParams reqParams = {}
) {
	// Ensure we always collect the start of the note since we need it to find chords.
	reqParams.start = true;
	double now = GetCursorPosition();
	MidiNoteIterator begin(take, reqParams);
	MidiNoteIterator end = begin;
	end.moveToEnd();
	if (begin == end) {
		// No notes.
		return {begin, end};
	}
	auto range = equal_range(begin, end, now, MidiNote::CompareByStart{});
	// Find the first note of the chord.
	MidiNoteIterator firstNote = end;
	if (direction == 1 && range.second != end) {
		// Return chord after the cursor.
		// range.second is the first note after now.
		firstNote = range.second;
	} else if (direction == -1 && range.first != begin) {
		// Return chord before the cursor.
		// range.first is the first note at or after now, so one before that is
		// the first note before now.
		firstNote = range.first;
		--firstNote;
	} else if (range.first != range.second) {
		// Return chord at the cursor.
		return range;
	} else {
		// Nothing in the requested direction or at the cursor.
		return {end, end};
	}
	// Find the last note of the chord.
	double firstStart = firstNote->start;
	MidiNoteIterator lastNote = firstNote;
	MidiNoteIterator note = firstNote;
	for (note += direction; begin <= note && note < end; note += direction) {
		if (note->start != firstStart) {
			break;
		}
		lastNote = note;
	}
	return {min(firstNote, lastNote), max(lastNote, firstNote) + 1};
}

// Keeps track of the note to which the user last moved in a chord.
// This is the number of the note in the chord; e.g. 0 is the first note.
// It is not a REAPER note index!
// -1 means not in a chord.
int curNoteInChord = -1;
// Keeps track of the CC to which the user last moved at a particular position.
// The first entry of the pair is the position.
// The second entry of the pair is the number of the CC at the position; e.g. 0 is the first CC.
// It is not a REAPER CC index.
// -1 means no position/CC.
pair<double, int> currentCC = {-1, -1};

// Finds a single note in the chord at the cursor in a given direction and returns its info.
// This updates curNoteInChord.
MidiNote findNoteInChord(MediaItem_Take* take, int direction) {
	auto chord = findChord(
			take, 0,
			{
					true, // start
					true, // end
					true, // channel
					true, // pitch
					true, // velocity
					true, // selected
					true // muted
			}
	);
	if (chord.first == chord.second) {
		return {-1};
	}
	// Notes at the same position are ordered arbitrarily.
	// This is not intuitive, so sort them.
	vector<MidiNote> notes(chord.first, chord.second);
	stable_sort(notes.begin(), notes.end(), MidiNote::compareByPitch);
	const int lastNoteIndex = (int)notes.size() - 1;
	// Work out which note to move to.
	if (direction != 0 && 0 <= curNoteInChord && curNoteInChord <= (int)lastNoteIndex) {
		// Already on a note within the chord. Move to the next/previous note.
		curNoteInChord += direction;
		// If we were already on the first/last note, stay there.
		if (curNoteInChord < 0 || curNoteInChord > lastNoteIndex)
			curNoteInChord -= direction;
	} else if (direction != 0) {
		// We're moving into a new chord. Move to the first/last note.
		curNoteInChord = direction == 1 ? 0 : (int)lastNoteIndex;
	}
	return notes[curNoteInChord];
}

void cmdMidiMoveCursor(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	ostringstream s;
	s << formatCursorPosition();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	auto chord = findChord(
			take, 0,
			{
					true, // start
					true, // end
					true, // channel
					true, // pitch
					true, // velocity
					false, // selected
					true // muted
			}
	);
	vector<MidiNote> notes(chord.first, chord.second);
	int count = static_cast<int>(notes.size());
	if (count > 0) {
		previewNotes(take, notes);
		fakeFocus = FOCUS_NOTE;
		s << " " << format(translate_plural("{} note", "{} notes", count), count);
		int mutedCount = count_if(notes.begin(), notes.end(), [](auto note) { return note.muted; });
		if (mutedCount > 0) {
			// Translators: used when reporting the number of muted notes in a chord.
			// {} will be replaced by the number of muted notes. E.g. "3 muted"
			s << format(translate_plural("{} muted", "{} muted", mutedCount), mutedCount);
		}
	}
	outputMessage(s);
}

void selectNote(MediaItem_Take* take, const int note, bool select = true) {
	MIDI_SetNote(take, note, &select, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

bool isNoteSelected(MediaItem_Take* take, const int note) {
	bool sel;
	MIDI_GetNote(take, note, &sel, NULL, NULL, NULL, NULL, NULL, NULL);
	return sel;
}

int countSelectedNotes(MediaItem_Take* take, int offset = -1) {
	int noteIndex = offset;
	int count = 0;
	for (;;) {
		noteIndex = MIDI_EnumSelNotes(take, noteIndex);
		if (noteIndex == -1) {
			break;
		}
		++count;
	}
	return count;
}

vector<MidiNote> getSelectedNotes(MediaItem_Take* take, int offset = -1) {
	int noteIndex = offset;
	vector<MidiNote> notes;
	for (;;) {
		noteIndex = MIDI_EnumSelNotes(take, noteIndex);
		if (noteIndex == -1) {
			break;
		}
		notes.push_back(MidiNote::get(
				take, noteIndex,
				{
						true, // start
						true, // end
						true, // channel
						true, // pitch
						true, // velocity
						false, // selected
						true // muted
				}
		));
	}
	return notes;
}

using MidiControlChangeIterator = MidiEventIterator<MidiControlChange, MediaItem_Take*>;

// Finds a single CC at the cursor in a given direction and returns its info.
// This updates currentCC.
MidiControlChange findCC(MediaItem_Take* take, int direction) {
	MidiControlChangeIterator begin(
			take,
			{
					true, // position
					true, // message1
					true, // channel
					true, // message2
					true, // message3,
					true, // selected
					true // muted
			}
	);
	MidiControlChangeIterator end = begin;
	end.moveToEnd();
	if (begin == end) {
		// No CCs.
		currentCC = {-1, -1};
		return {-1};
	}
	double now = GetCursorPosition();
	// Find all CCs at the current position.
	auto range = equal_range(begin, end, now, MidiControlChange::CompareByPosition{});
	int count = range.second - range.first;
	double position = currentCC.first;
	int curCC = currentCC.second;
	if (curCC != -1 && position == now && count > 0) {
		// In this case, we have a cached CC at the current position
		// and the range of CCs at the current position contains at least one CC.
		// We can simmply move to the adjacent CC in the range,
		// unless curCC is at the edge of the range.
		// In that case, we skip this block entirely and behave as if there was nothing cached.
		auto newCC = curCC + direction;
		if (direction == -1 ? newCC >= 0 : newCC < count) {
			// #434: CC events are ordered arbitrarily and, unlike notes, order can change when interacting with them.
			// Create a vector of CCs in order to sort them in a stable way.
			vector<MidiControlChange> ccs(range.first, range.second);
			stable_sort(ccs.begin(), ccs.end(), MidiControlChange::compareForSortAtPosition);
			currentCC.second = newCC;
			return ccs.at(newCC);
		}
	} else if (direction == 1) {
		// Direction is forward, but we want to ensure that focus lands on the first CC at or after the cursor.
		// Sticking to direction == 1 would mean we'd focus a CC after the cursor, even if there was a CC at the cursor.
		direction = 0;
	}
	MidiControlChangeIterator firstCC = end;
	if (direction == 1 && range.second != end) {
		// We want a CC after the cursor
		// range.second is the first CC after now.
		firstCC = range.second;
	} else if (direction == -1 && range.first != begin) {
		// We want a CC before the cursor
		// range.First is the first CC at or after now, so one before that is
		// the first CC before now.
		firstCC = range.first;
		--firstCC;
	} else if (direction == 0 && range.first != end) {
		// We want a CC at or after the cursor
		// range.First is the first CC at or after now.
		firstCC = range.first;
	} else {
		// Nothing in the requested direction or at the cursor.
		return {-1};
	}
	// Find the last CC at the position of the first.
	double firstPos = firstCC->position;
	MidiControlChangeIterator lastCC = firstCC;
	MidiControlChangeIterator cc = firstCC;
	int movement = direction != -1 ? 1 : -1;
	for (cc += movement; begin <= cc && cc < end; cc += movement) {
		if (cc->position != firstPos) {
			break;
		}
		lastCC = cc;
	}
	int index = 0;
	MidiControlChange ret;
	if (firstCC == lastCC) {
		// There is only one CC at this position.
		ret = *firstCC;
	} else {
		// #434: CC events are ordered arbitrarily.
		// To sort them in a stable way, create a vector containing all the CCs at the position we're navigating to.
		// Note that the constructor of vector expects an exclusive end, but our range is inclusive.
		vector<MidiControlChange> ccs(movement != -1 ? firstCC : lastCC, movement != -1 ? lastCC + 1 : firstCC + 1);
		stable_sort(ccs.begin(), ccs.end(), MidiControlChange::compareForSortAtPosition);
		// Focus the first or last note in the vector, depending on direction being forward or backward, respectively.
		index = movement != -1 ? 0 : (static_cast<int>(ccs.size()) - 1);
		ret = ccs.at(index);
	}
	// Save the new CC for future invocations of findCC.
	currentCC = {ret.position, index};
	return ret;
}

void selectCC(MediaItem_Take* take, const int cc, bool select = true) {
	MIDI_SetCC(take, cc, &select, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

bool isCCSelected(MediaItem_Take* take, const int cc) {
	bool sel;
	MIDI_GetCC(take, cc, &sel, NULL, NULL, NULL, NULL, NULL, NULL);
	return sel;
}

vector<MidiControlChange> getSelectedCCs(MediaItem_Take* take, int offset = -1) {
	int ccIndex = offset;
	vector<MidiControlChange> ccs;
	for (;;) {
		ccIndex = MIDI_EnumSelCC(take, ccIndex);
		if (ccIndex == -1) {
			break;
		}
		double position;
		int chan, msg1, msg2, msg3;
		MIDI_GetCC(take, ccIndex, NULL, NULL, &position, &msg1, &chan, &msg2, &msg3);
		position = MIDI_GetProjTimeFromPPQPos(take, position);
		ccs.push_back({chan, ccIndex, msg1, msg2, msg3, position});
	}
	return ccs;
}

void cmdMidiToggleSelection(Command* command) {
	if (isSelectionContiguous) {
		isSelectionContiguous = false;
		outputMessage(translate("noncontiguous selection"));
		return;
	}
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	bool select;
	switch (fakeFocus) {
		case FOCUS_NOTE: {
			if (curNoteInChord != -1) {
				// Note in chord.
				MidiNote note = findNoteInChord(take, 0);
				if (note.channel == -1) {
					return;
				}
				select = !note.selected;
				selectNote(take, note.index, select);
			} else {
				// Chord.
				auto chord = findChord(
						take, 0,
						{
								true, // start
								false, // end
								false, // channel
								false, // pitch
								false, // velocity
								true // selected
						}
				);
				if (chord.first == chord.second) {
					return;
				}
				select = !(chord.first->selected);
				for (auto note = chord.first; note < chord.second; ++note) {
					selectNote(take, note.getIndex(), select);
				}
			}
			break;
		}
		case FOCUS_CC: {
			if (currentCC.first == -1 || currentCC.second == -1) {
				return;
			}
			auto curCC = findCC(take, 0);
			select = !curCC.selected;
			selectCC(take, curCC.index, select);
			break;
		}
		default:
			return;
	}
	outputMessage(select ? translate("selected") : translate("unselected"));
}

void moveToChord(int direction, bool clearSelection = true, bool select = true) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	auto chord = findChord(
			take, direction,
			{
					true, // start
					true, // end
					true, // channel
					true, // pitch
					true, // velocity
					false, // selected
					true // muted
			}
	);
	if (chord.first == chord.second) {
		return;
	}
	curNoteInChord = -1;
	if (clearSelection) {
		MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
		isSelectionContiguous = true;
	}
	const double oldCursor = GetCursorPosition();
	// Move the edit cursor to this chord, select it and play it.
	bool cursorSet = false;
	vector<MidiNote> notes(chord.first, chord.second);
	for (auto const& note : notes) {
		if (!cursorSet && direction != 0) {
			SetEditCurPos(note.start, true, false);
			cursorSet = true;
		}
		if (select) {
			selectNote(take, note.index);
		}
	}
	const bool cursorMoved = oldCursor != GetCursorPosition();
	if (cursorMoved) {
		previewNotes(take, notes);
	}
	fakeFocus = FOCUS_NOTE;
	ostringstream s;
	if (settings::reportPositionMIDI) {
		s << formatCursorPosition();
		if (s.tellp() > 0) {
			s << " ";
		}
	}
	if (cursorMoved && !select && !isNoteSelected(take, chord.first.getIndex())) {
		s << translate("unselected") << " ";
	}
	if (cursorMoved && settings::reportNotes && settings::reportPositionMIDI) {
		int count = chord.second - chord.first;
		// Translators: used when reporting the number of notes in a chord.
		// {} will be replaced by the number of notes. E.g. "3 notes"
		s << format(translate_plural("{} note", "{} notes", count), count);
		int mutedCount = count_if(notes.begin(), notes.end(), [](auto note) { return note.muted; });
		if (mutedCount > 0) {
			// Translators: used when reporting the number of muted notes in a chord.
			// {} will be replaced by the number of muted notes. E.g. "3 muted"
			s << format(translate_plural("{} muted", "{} muted", mutedCount), mutedCount);
		}
	}
	if (s.tellp() > 0) {
		outputMessage(s);
	}
}

void cmdMidiMoveToNextChord(Command* command) {
	moveToChord(1);
}

void cmdMidiMoveToPreviousChord(Command* command) {
	moveToChord(-1);
}

void cmdMidiMoveToNextChordKeepSel(Command* command) {
	moveToChord(1, false, isSelectionContiguous);
}

void cmdMidiMoveToPreviousChordKeepSel(Command* command) {
	moveToChord(-1, false, isSelectionContiguous);
}

void moveToNoteInChord(int direction, bool clearSelection = true, bool select = true) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	MidiNote note = findNoteInChord(take, direction);
	if (note.channel == -1) {
		return;
	}
	if (clearSelection) {
		MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
		isSelectionContiguous = true;
	}
	if (select) {
		selectNote(take, note.index);
	}
	previewNotes(take, {note});
	fakeFocus = FOCUS_NOTE;
	ostringstream s;
	if (settings::reportNotes) {
		if (note.muted) {
			s << translate("muted") << " ";
		}
		s << getMidiNoteName(take, note.pitch, note.channel);
	}
	if (!select && !isNoteSelected(take, note.index)) {
		s << " " << translate("unselected") << " ";
	} else if (settings::reportNotes) {
		s << ", ";
	}
	if (settings::reportNotes) {
		s << formatNoteLength(note.start, note.end);
		if (GetToggleCommandState2(
						SectionFromUniqueID(MIDI_EDITOR_SECTION), 40632
				)) { // View: Show velocity numbers on notes
			s << ", " << note.velocity << " " << translate("velocity");
		}
	}
	outputMessage(s);
}

void cmdMidiMoveToNextNoteInChord(Command* command) {
	moveToNoteInChord(1);
}

void cmdMidiMoveToPreviousNoteInChord(Command* command) {
	moveToNoteInChord(-1);
}

void cmdMidiMoveToNextNoteInChordKeepSel(Command* command) {
	moveToNoteInChord(1, false, isSelectionContiguous);
}

void cmdMidiMoveToPreviousNoteInChordKeepSel(Command* command) {
	moveToNoteInChord(-1, false, isSelectionContiguous);
}

void postMidiMovePitchCursor(int command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int pitch = MIDIEditor_GetSetting_int(editor, "active_note_row");
	int chan = MIDIEditor_GetSetting_int(editor, "default_note_chan");
	int vel = MIDIEditor_GetSetting_int(editor, "default_note_vel");
	double start = GetCursorPosition();
	double startQn = TimeMap2_timeToQN(nullptr, start);
	double lenQn;
	double gridQn = MIDI_GetGrid(take, nullptr, &lenQn);
	if (lenQn == 0) {
		lenQn = gridQn;
	}
	double end = TimeMap2_QNToTime(nullptr, startQn + lenQn);
	previewNotes(take, {{chan, pitch, vel, -1, start, end}});
	if (settings::reportNotes) {
		outputMessage(getMidiNoteName(take, pitch, chan));
	}
}

void cmdMidiInsertNote(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int oldCount;
	MIDI_CountEvts(take, &oldCount, nullptr, nullptr);
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	int newCount;
	MIDI_CountEvts(take, &newCount, nullptr, nullptr);
	if (newCount <= oldCount) {
		return; // Not inserted.
	}
	int pitch = MIDIEditor_GetSetting_int(editor, "active_note_row");
	// Get selected notes.
	vector<MidiNote> selectedNotes = getSelectedNotes(take);
	// Find the just inserted note based on its pitch, as that makes it unique.
	auto it = find_if(selectedNotes.begin(), selectedNotes.end(), [pitch](MidiNote n) { return n.pitch == pitch; });
	if (it == selectedNotes.end()) {
		return;
	}
	auto& note = *it;
	// Play the inserted note when preview is enabled.
	previewNotes(take, {note});
	fakeFocus = FOCUS_NOTE;
	ostringstream s;
	// If we're advancing the cursor position, we should report the new position.
	const bool reportNewPos = command->gaccel.accel.cmd == 40051; // Edit: Insert note at edit cursor
	if (settings::reportNotes) {
		s << getMidiNoteName(take, note.pitch, note.channel) << " ";
		s << formatNoteLength(note.start, note.end);
		if (reportNewPos) {
			s << ", ";
		}
	}
	if (reportNewPos) {
		s << formatCursorPosition();
	}
	outputMessage(s);
}

void cmdMidiDeleteEvents(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int oldCount = MIDI_CountEvts(take, NULL, NULL, NULL);
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	int removed = oldCount - MIDI_CountEvts(take, NULL, NULL, NULL);
	// Translators: Used when events are deleted in the MIDI editor. {} is
	// replaced by the number of events. E.g. "3 events removed"
	outputMessage(format(translate_plural("{} event removed", "{} events removed", removed), removed));
}

void postMidiSelectNotes(int command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int count = countSelectedNotes(take);
	fakeFocus = FOCUS_NOTE;
	// Translators: used when notes are selected in the MIDI editor.
	// {} is replaced by the number of notes. E.g. "4 notes selected"
	outputMessage(format(translate_plural("{} note selected", "{} notes selected", count), count));
}

int countSelectedEvents(MediaItem_Take* take) {
	int evtIndex = -1;
	int count = 0;
	for (;;) {
		evtIndex = MIDI_EnumSelEvts(take, evtIndex);
		if (evtIndex == -1) {
			break;
		}
		unsigned char msg[3] = "\0";
		int size = sizeof(msg);
		MIDI_GetEvt(
				take, evtIndex, /* selectedOut */ nullptr, /* mutedOut */ nullptr,
				/* ppqposOut */ nullptr, (char*)msg, &size
		);
		if (0x80 <= msg[0] && msg[0] <= 0x8F) {
			continue; // Don't count note off messages.
		}
		++count;
	}
	return count;
}

void postMidiSelectEvents(int command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int count = countSelectedEvents(take);
	if (fakeFocus != FOCUS_NOTE && fakeFocus != FOCUS_CC) {
		fakeFocus = FOCUS_NOTE;
	}
	// Translators: Reported when selecting events in the MIDI editor. {} will be replaced with
	// the number of events; e.g. "2 events selected".
	outputMessage(format(translate_plural("{} event selected", "{} events selected", count), count));
}

void cmdMidiToggleSelCC(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int oldCount = countSelectedEvents(take);
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	int newCount = countSelectedEvents(take);
	int count = newCount - oldCount;
	if (count >= 0) {
		// Translators: Used in the MIDI editor when CC events are selected.  {}
		// is replaced by the number of events selected.
		outputMessage(format(translate_plural("{} CC event selected", "{} CC events selected", count), count));
	} else {
		// Translators: Used in the MIDI editor when CC events are unselected.  {}
		// is replaced by the number of events unselected.
		outputMessage(format(translate_plural("{} CC event unselected", "{} CC events unselected", -count), -count));
	}
}

const string getMidiControlName(MediaItem_Take* take, int control, int channel) {
	static map<int, string> names = {
			{0, _t("Bank Select MSB")},
			{1, _t("Mod Wheel MSB")},
			{2, _t("Breath MSB")},
			{4, _t("Foot Pedal MSB")},
			{5, _t("Portamento MSB")},
			{6, _t("Data Entry MSB")},
			{7, _t("Volume MSB")},
			{8, _t("Balance MSB")},
			{10, _t("Pan Position MSB")},
			{11, _t("Expression MSB")},
			{12, _t("Control 1 MSB")},
			{13, _t("Control 2 MSB")},
			{16, _t("GP Slider 1")},
			{17, _t("GP Slider 2")},
			{18, _t("GP Slider 3")},
			{19, _t("GP Slider 4")},
			{32, _t("Bank Select LSB")},
			{33, _t("Mod Wheel LSB")},
			{34, _t("Breath LSB")},
			{36, _t("Foot Pedal LSB")},
			{37, _t("Portamento LSB")},
			{38, _t("Data Entry LSB")},
			{39, _t("Volume LSB")},
			{40, _t("Balance LSB")},
			{42, _t("Pan Position LSB")},
			{43, _t("Expression LSB")},
			{44, _t("Control 1 LSB")},
			{45, _t("Control 2 LSB")},
			{64, _t("Hold Pedal (on/off)")},
			{65, _t("Portamento (on/off)")},
			{66, _t("Sostenuto (on/off)")},
			{67, _t("Soft Pedal (on/off)")},
			{68, _t("Legato Pedal (on/off)")},
			{69, _t("Hold 2 Pedal (on/off)")},
			{70, _t("Sound Variation")},
			{71, _t("Timbre/Resonance")},
			{72, _t("Sound Release")},
			{73, _t("Sound Attack")},
			{74, _t("Brightness/Cutoff Freq")},
			{75, _t("Sound Control 6")},
			{76, _t("Sound Control 7")},
			{77, _t("Sound Controll 8")},
			{78, _t("Sound Control 9")},
			{79, _t("Sound Control 10")},
			{80, _t("GP Button 1 (on/off)")},
			{81, _t("GP Button 2 (on/off)")},
			{82, _t("GP Button 3 (on/off)")},
			{83, _t("GP Button 4 (on/off)")},
			{91, _t("Effects Level")},
			{92, _t("Tremolo Level")},
			{93, _t("Chorus Level")},
			{94, _t("Celeste Level")},
			{95, _t("Phaser Level")},
			{96, _t("Data Button Inc")},
			{97, _t("Data Button Dec")},
			{98, _t("Non-Reg Parm LSB")},
			{99, _t("Non-Reg Parm MSB")},
			{100, _t("Reg Parm LSB")},
			{101, _t("Reg Parm MSB")},
			{120, _t("All Sound Off")},
			{121, _t("Reset")},
			{122, _t("Local")},
			{123, _t("All Notes Off")},
			{124, _t("Omni On")},
			{125, _t("Omni Off")},
			{126, _t("Mono On")},
			{127, _t("Poly On")}
	};
	MediaTrack* track = GetMediaItemTake_Track(take);
	int tracknumber = static_cast<int>(GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER")); // one based
	ostringstream s;
	const char* controlName = GetTrackMIDINoteName(
			tracknumber - 1, 128 + control, channel
	); // track number is zero based, controls start at 128
	s << control;
	if (controlName) {
		s << " (" << controlName << ")";
	} else {
		auto it = names.find(control);
		if (it != names.end()) {
			s << " (" << translate(it->second) << ")";
		}
	}
	return s.str();
}

void moveToCC(int direction, bool clearSelection = true, bool select = true) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	auto cc = findCC(take, direction);
	if (cc.channel == -1) {
		return;
	}
	if (clearSelection || select) {
		Undo_BeginBlock();
	}
	if (clearSelection) {
		MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
		isSelectionContiguous = true;
	}
	if (select) {
		selectCC(take, cc.index);
	}
	if (clearSelection || select) {
		Undo_EndBlock(translate("Change CC Selection"), 0);
	}
	SetEditCurPos(cc.position, true, false);
	fakeFocus = FOCUS_CC;
	ostringstream s;
	s << formatCursorPosition() << " ";
	if (cc.muted) {
		s << translate("muted") << " ";
	}
	if (cc.message1 == 0xA0) {
		// Translators: MIDI poly aftertouch. {note} will be replaced with the note
		// name and {value} will be replaced with the aftertouch value; e.g.
		// "Poly Aftertouch c sharp 4  96"
		s << format(
				translate("Poly Aftertouch {note}  {value}"), "note"_a = getMidiNoteName(take, cc.message2, cc.channel),
				"value"_a = cc.message3
		);
	} else if (cc.message1 == 0xB0) {
		// Translators: A MIDI CC. {control} will be replaced with the control number and name. {value} will be replaced
		// with the value of the control; e.g. "control 70 (Sound Variation), 64"
		s << format(
				translate("Control {control}, {value}"), "control"_a = getMidiControlName(take, cc.message2, cc.channel),
				"value"_a = cc.message3
		);
	} else if (cc.message1 == 0xC0) {
		// Translators: a MIDI program number.  {} will be replaced with the program number; e.g. "Program 5"
		s << format(translate("Program {}"), cc.message2);
	} else if (cc.message1 == 0xD0) {
		// Midi channel pressure. {} will be replaced with the pressure value; e.g. "Channel pressure 64"
		s << format(translate("Channel pressure {}"), cc.message2);
	} else if (cc.message1 == 0xE0) {
		auto pitchBendValue = (cc.message3 << 7) | cc.message2;
		// Translators: MIDI pitch bend.  {} will be replaced with the pitch bend value; e.g. "Pitch Bend 100"
		s << format(translate("Pitch Bend {}"), pitchBendValue);
	}
	if (!select && !isCCSelected(take, cc.index)) {
		s << " " << translate("unselected");
	}
	outputMessage(s);
}

void cmdMidiMoveToNextCC(Command* command) {
	moveToCC(1);
}

void cmdMidiMoveToPreviousCC(Command* command) {
	moveToCC(-1);
}

void cmdMidiMoveToNextCCKeepSel(Command* command) {
	moveToCC(1, false, isSelectionContiguous);
}

void cmdMidiMoveToPreviousCCKeepSel(Command* command) {
	moveToCC(-1, false, isSelectionContiguous);
}

void midiMoveToItem(int direction) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(
			editor, ((direction == 1) ? 40798 : 40797)
	); // Contents: Activate next/previous MIDI media item on this track, clearing the editor first
	MIDIEditor_OnCommand(editor, 40036); // View: Go to start of file
	int cmd = NamedCommandLookup("_FNG_ME_SELECT_NOTES_NEAR_EDIT_CURSOR");
	if (cmd > 0)
		MIDIEditor_OnCommand(editor, cmd); // SWS/FNG: Select notes nearest edit cursor
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	MediaItem* item = GetMediaItemTake_Item(take);
	MediaTrack* track = GetMediaItem_Track(item);
	int count = CountTrackMediaItems(track);
	int itemNum = 1;
	for (int i = 0; i < count; ++i) {
		MediaItem* itemTmp = GetTrackMediaItem(track, i);
		if (itemTmp == item) {
			itemNum = i + 1;
			break;
		}
	}
	fakeFocus = FOCUS_ITEM;
	ostringstream s;
	s << itemNum << " " << GetTakeName(take);
	s << " " << formatCursorPosition();
	outputMessage(s);
}

void cmdMidiMoveToNextItem(Command* command) {
	Undo_BeginBlock();
	midiMoveToItem(1);
	Undo_EndBlock(translate("OSARA: Move to next midi item on track"), 0);
}

void cmdMidiMoveToPrevItem(Command* command) {
	Undo_BeginBlock();
	midiMoveToItem(-1);
	Undo_EndBlock(translate("OSARA: Move to previous midi item on track"), 0);
}

void cmdMidiMoveToTrack(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	MediaItem* item = GetMediaItemTake_Item(take);
	MediaTrack* track = GetMediaItem_Track(item);
	int count = CountTrackMediaItems(track);
	int itemNum;
	for (int i = 0; i < count; ++i) {
		MediaItem* itemTmp = GetTrackMediaItem(track, i);
		if (itemTmp == item) {
			itemNum = i + 1;
			break;
		}
	}
	fakeFocus = FOCUS_TRACK;
	ostringstream s;
	int trackNum = (int)(size_t)GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", NULL);
	s << trackNum;
	char* trackName = (char*)GetSetMediaTrackInfo(track, "P_NAME", NULL);
	if (trackName)
		s << " " << trackName;
	// Translators: Used when reporting activation of the next/previous track in
	// the MIDI editor. {num} will be replaced with the item number. {name} will
	// be replaced with its name. For example: "item 2 chorus".
	s << " " << format(translate("item {num} {name}"), "num"_a = itemNum, "name"_a = GetTakeName(take));
	outputMessage(s);
}

void cmdMidiSelectSamePitchStartingInTimeSelection(Command* command) {
	double tsStart, tsEnd;
	GetSet_LoopTimeRange(false, false, &tsStart, &tsEnd, false);
	if (tsStart == tsEnd) {
		outputMessage(translate("no time selection"));
		return;
	}
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int selNote = MIDI_EnumSelNotes(take, -1);
	if (selNote == -1) {
		outputMessage(translate("no notes selected"));
		return;
	}
	int selPitch;
	MIDI_GetNote(take, selNote, nullptr, nullptr, nullptr, nullptr, nullptr, &selPitch, nullptr);
	Undo_BeginBlock();
	MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
	int noteCount{0}, selectCount{0};
	MIDI_CountEvts(take, &noteCount, nullptr, nullptr);
	for (int i = 0; i < noteCount; i++) {
		double startPPQPos;
		int pitch;
		MIDI_GetNote(take, i, nullptr, nullptr, &startPPQPos, nullptr, nullptr, &pitch, nullptr);
		double startTime = MIDI_GetProjTimeFromPPQPos(take, startPPQPos);
		if (tsStart <= startTime && startTime < tsEnd && pitch == selPitch) {
			selectNote(take, i);
			selectCount++;
		}
	}
	Undo_EndBlock(translate("OSARA: Select all notes with the same pitch within time selection"), 0);
	// Translators: used when notes are selected in the MIDI editor.
	// {} is replaced by the number of notes. E.g. "4 notes selected"
	outputMessage(format(translate_plural("{} note selected", "{} notes selected", selectCount), selectCount));
}

void cmdMidiNoteSplitOrJoin(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	// Get selected note count before action.
	auto oldCount = countSelectedNotes(take);
	auto cmdId = command->gaccel.accel.cmd;
	MIDIEditor_OnCommand(editor, cmdId);
	auto newCount = countSelectedNotes(take);
	if (oldCount == newCount) {
		return;
	}
	switch (cmdId) {
		case 40046:
			// Translators: used when splitting notes in the midi editor.
			// {oldCount} is replaced by the number of notes that were selected
			// before the command.  This is the number the plural form is based
			// on. {newCount} is replaced by the number of selected notes after
			// the command. E.g. "1 note split into 2"
			outputMessage(format(
					translate_plural("{oldCount} note split into {newCount}", "{oldCount} notes split into {newCount}", oldCount),
					"oldCount"_a = oldCount, "newCount"_a = newCount
			));
			break;
		case 40456:
			// Translators: used when joining notes in the midi editor.
			// {oldCount} is replaced by the number of notes that were selected
			// before the command.  This is the number the plural form is based
			// on. {newCount} is replaced by the number of selected notes after
			// the command. E.g. "2 notes joined into 1"
			outputMessage(format(
					translate_plural(
							"{oldCount} note joined into {newCount}", "{oldCount} notes joined into {newCount}", oldCount
					),
					"oldCount"_a = oldCount, "newCount"_a = newCount
			));
			break;
		default:
			break;
	}
}

#ifdef _WIN32
using MidiEventListDataIterator = MidiEventIterator<MidiEventListData, HWND>;

void focusNearestMidiEvent(HWND hwnd) {
	double cursorPos = GetCursorPosition();
	HWND editor = MIDIEditor_GetActive();
	assert(editor == GetParent(hwnd));
	auto begin = MidiEventListDataIterator(editor);
	auto end = begin;
	end.moveToEnd();
	if (begin == end) {
		// No events
		return;
	}
	auto range = equal_range(begin, end, cursorPos, MidiEventListData::CompareByPosition{});
	auto first = range.first;
	auto last = range.second - 1;
	if (first == end) {
		// Cursor is after all events.
		return;
	}
	const int curFocus = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
	auto firstIndex = first.getIndex();
	auto lastIndex = last.getIndex();
	if (curFocus != -1 && firstIndex <= curFocus && curFocus <= lastIndex) {
		// Current focus is within the range of events at the cursor.
		return;
	}
	const int lvBitMask = LVIS_FOCUSED | LVIS_SELECTED;
	// select and focus the first item
	ListView_SetItemState(hwnd, firstIndex, lvBitMask, lvBitMask);
	ListView_EnsureVisible(hwnd, firstIndex, false);
	if (curFocus != -1) {
		// Unselect the previously focused item.
		ListView_SetItemState(hwnd, curFocus, 0, LVIS_SELECTED);
	}
}

void cmdFocusNearestMidiEvent(Command* command) {
	HWND hwnd = GetFocus();
	if (!hwnd) {
		return;
	}
	focusNearestMidiEvent(hwnd);
}

void cmdMidiFilterWindow(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	// TODO: we could also check the command state was "off", to skip searching otherwise
	HWND filter = FindWindowW(L"#32770", widen(LocalizeString("Filter Events", "midi_DLG_128", 0)).c_str());
	if (filter && (filter != GetFocus())) {
		SetFocus(filter); // focus the window
	}
}

void maybeHandleEventListItemFocus(HWND hwnd, long childId) {
	if (childId == CHILDID_SELF) {
		// Focus is set to the list, not to an item within the list.
		// By default, REAPER doesn't focus any event in the event list when coming from outside.
		// Since the edit cursor follows the focused event in the event list, this is impractical,
		// as changing focus with the arrow keys means that the current edit cursor position will be lost.
		// Therefore, focus the nearest event in this case.
		focusNearestMidiEvent(hwnd);
		return;
	}
	bool shouldPreviewNotes = GetToggleCommandState2(
			SectionFromUniqueID(MIDI_EVENT_LIST_SECTION), 40041
	); // Options: Preview notes when inserting or editing
	if (!shouldPreviewNotes) {
		return;
	}
	HWND editor = MIDIEditor_GetActive();
	assert(editor == GetParent(hwnd));
	auto focused = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
	auto event = MidiEventListData::get(editor, focused);
	// Check whether this is a note
	if (event.length == -1) {
		// No Note
		return;
	}
	auto note = event.toMidiNote();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	previewNotes(take, {note});
}

#endif // _WIN32

void postMidiChangeVelocity(int command) {
	if (!settings::reportNotes) {
		return;
	}
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	// Get selected notes.
	vector<MidiNote> selectedNotes = getSelectedNotes(take);
	if (selectedNotes.size() == 0) {
		return;
	}
	bool generalize = false;
	if (selectedNotes.size() >= 8) {
		generalize = true;
	} else {
		// Get indexes for the current chord.
		auto chord = findChord(take, 0);
		if (chord.first == chord.second) {
			generalize = true;
		} else {
			generalize = !(all_of(selectedNotes.begin(), selectedNotes.end(), [chord](MidiNote n) {
				return chord.first.getIndex() <= n.index && n.index < chord.second.getIndex();
			}));
		}
	}
	// The Reaper action takes care of note preview.
	ostringstream s;
	if (generalize) {
		int count = static_cast<int>(selectedNotes.size());
		s << format(translate_plural("{} note", "{} notes", count), count) << " ";
		switch (command) {
			case 40462:
				s << translate("velocity +1");
				break;
			case 40463:
				s << translate("velocity +10");
				break;
			case 40464:
				s << translate("velocity -1");
				break;
			case 40465:
				s << translate("velocity -10");
				break;
			default:
				s << translate("velocity changed");
				break;
		}
	} else {
		for (auto note = selectedNotes.cbegin(); note != selectedNotes.cend(); ++note) {
			s << getMidiNoteName(take, note->pitch, note->channel) << "  " << note->velocity;
			if (note != selectedNotes.cend() - 1) {
				s << ", ";
			}
		}
	}
	outputMessage(s);
}

void postMidiChangeLength(int command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	// Get selected notes.
	vector<MidiNote> selectedNotes = getSelectedNotes(take);
	if (selectedNotes.size() == 0) {
		return;
	}
	if (command == 40765 && selectedNotes.size() == 1) {
		// Making notes legato doesn't do anything when only one note is selected.
		return;
	}
	bool generalize = false;
	if (selectedNotes.size() >= 8) {
		generalize = true;
	} else {
		// Get indexes for the current chord.
		auto chord = findChord(take, 0);
		if (chord.first == chord.second) {
			generalize = true;
		} else {
			generalize = !(all_of(selectedNotes.begin(), selectedNotes.end(), [chord](MidiNote n) {
				return chord.first.getIndex() <= n.index && n.index < chord.second.getIndex();
			}));
		}
	}
	if (!generalize) {
		previewNotes(take, selectedNotes);
	}
	if (settings::reportNotes) {
		ostringstream s;
		if (generalize) {
			int count = static_cast<int>(selectedNotes.size());
			switch (command) {
				case 40444:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g. "3
					// notes lengthened pixel"
					s << format(translate_plural("{} note lengthened pixel", "{} notes lengthened pixel", count), count);
					break;
				case 40445:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g. "3
					// notes shortened pixel"
					s << format(translate_plural("{} note shortened pixel", "{} notes shortened pixel", count), count);
					break;
				case 40446:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes lengthened grid unit"
					s << format(translate_plural("{} note lengthened grid unit", "{} notes lengthened grid unit", count), count);
					break;
				case 40447:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes shortened grid unit"
					s << format(translate_plural("{} note shortened grid unit", "{} notes shortened grid unit", count), count);
					break;
				case 40633:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes set length to grid size"
					s << format(
							translate_plural("{} note set length to grid size", "{} notes length set to grid size", count), count
					);
					break;
				case 40765:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes made legato"
					s << format(translate_plural("{} note made legato", "{} notes made legato", count), count);
					break;
				default:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes length changed"
					s << format(translate_plural("{} note length changed", "{} notes length changed", count), count);
					break;
			}
		} else {
			for (auto note = selectedNotes.cbegin(); note != selectedNotes.cend(); ++note) {
				s << getMidiNoteName(take, note->pitch, note->channel) << " ";
				s << formatNoteLength(note->start, note->end);
				if (note != selectedNotes.cend() - 1) {
					s << ", ";
				}
			}
		}
		outputMessage(s);
	}
}

void postMidiChangePitch(int command) {
	if (!settings::reportNotes) {
		return;
	}
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	// Get selected notes.
	vector<MidiNote> selectedNotes = getSelectedNotes(take);
	if (selectedNotes.size() == 0) {
		return;
	}
	bool generalize = false;
	if (selectedNotes.size() >= 8) {
		generalize = true;
	} else {
		// Get indexes for the current chord.
		auto chord = findChord(take, 0);
		if (chord.first == chord.second) {
			generalize = true;
		} else {
			generalize = !(all_of(selectedNotes.begin(), selectedNotes.end(), [chord](MidiNote n) {
				return chord.first.getIndex() <= n.index && n.index <= chord.second.getIndex();
			}));
		}
	}
	// The Reaper action takes care of note preview.
	ostringstream s;
	if (generalize) {
		int count = static_cast<int>(selectedNotes.size());
		switch (command) {
			case 40177:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g.
				// "3 notes semitone up"
				s << format(translate_plural("{} note semitone up", "{} notes semitone up", count), count);
				break;
			case 40178:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g.
				// "3 notes semitone down"
				s << format(translate_plural("{} note semitone down", "{} notes semitone down", count), count);
				break;
			case 40179:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g.
				// "3 notes octave up"
				s << format(translate_plural("{} note octave up", "{} notes octave up", count), count);
				break;
			case 40180:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g.
				// "3 notes octave down"
				s << format(translate_plural("{} note octave down", "{} notes octave down", count), count);
				break;
			case 41026:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g.
				// "3 notes semitone up ignoring scale"
				s << format(
						translate_plural("{} note semitone up ignoring scale", "{} notes semitone up ignoring scale", count), count
				);
				break;
			case 41027:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g.
				// "3 notes semitone down ignoring scale"
				s << format(
						translate_plural("{} note semitone down ignoring scale", "{} notes semitone down ignoring scale", count),
						count
				);
				break;
			default:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g.
				// "3 notes pitch changed"
				s << format(translate_plural("{} note pitch changed", "{} notes pitch changed", count), count);
				break;
		}
	} else {
		for (auto note = selectedNotes.cbegin(); note != selectedNotes.cend(); ++note) {
			s << getMidiNoteName(take, note->pitch, note->channel);
			if (note != selectedNotes.cend() - 1) {
				s << ", ";
			}
		}
	}
	outputMessage(s);
}

void postMidiMoveStart(int command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	// Get selected notes.
	vector<MidiNote> selectedNotes = getSelectedNotes(take);
	int count = static_cast<int>(selectedNotes.size());
	if (count == 0) {
		return;
	}
	auto firstStart = selectedNotes.cbegin()->start;
	bool generalize = count >= 8 ||
			any_of(selectedNotes.begin(), selectedNotes.end(), [firstStart](MidiNote n) { return firstStart != n.start; });
	if (!generalize) {
		previewNotes(take, selectedNotes);
	}
	if (settings::reportNotes) {
		ostringstream s;
		if (generalize) {
			switch (command) {
				case 40181:
					// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes pixel left"
					s << format(translate_plural("{} note pixel left", "{} notes pixel left", count), count);
					break;
				case 40182:
					// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes pixel right"
					s << format(translate_plural("{} note pixel right", "{} notes pixel right", count), count);
					break;
				case 40183:
					// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes grid unit left"
					s << format(translate_plural("{} note grid unit left", "{} notes grid unit left", count), count);
					break;
				case 40184:
					// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes grid unit right"
					s << format(translate_plural("{} note grid unit right", "{} notes grid unit right", count), count);
					break;
				default:
					// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g.
					// "3 notes pixel left"
					s << format(translate_plural("{} note start moved", "{} notes start moved", count), count);
					break;
			}
		} else {
			for (auto note = selectedNotes.cbegin(); note != selectedNotes.cend(); ++note) {
				if (note == selectedNotes.cbegin()) {
					s << formatTime(note->start) << " ";
				}
				s << getMidiNoteName(take, note->pitch, note->channel);
				if (note != selectedNotes.cend() - 1) {
					s << ", ";
				}
			}
		}
		outputMessage(s);
	}
}

void postMidiChangeCCValue(int command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	// Get selected CCs.
	vector<MidiControlChange> selectedCCs = getSelectedCCs(take);
	int count = static_cast<int>(selectedCCs.size());
	if (count == 0) {
		return;
	}
	ostringstream s;
	if (count > 1) {
		switch (command) {
			case 40676: {
				// Translators: Used when MIDI CCs change. {} is replaced by the
				// number of values changed. E.g. "2 values increased"
				s << format(translate_plural("{} value increase", "{} values increased", count), count);
				break;
			}
			case 40677: {
				// Translators: Used when MIDI CCs change. {} is replaced by the
				// number of values changed. E.g. "2 values decreased"
				s << format(translate_plural("{} value decreased", "{} values decreased", count), count);
				break;
			}
			default: {
				// Translators: Used when MIDI CCs change. {} is replaced by the
				// number of values changed. E.g. "2 values changed"
				s << format(translate_plural("{} value changed", "{} values changed", count), count);
				break;
			}
		}
	} else {
		auto cc = *selectedCCs.cbegin();
		if (cc.message1 == 0xA0) {
			// Note: separate the note and value with two spaces to avoid treatment as thausands separator.
			s << getMidiNoteName(take, cc.message2, cc.channel) << "  ";
			s << cc.message3;
		} else if (cc.message1 == 0xB0) {
			s << cc.message3;
		} else if (cc.message1 == 0xC0 || cc.message1 == 0xD0) {
			s << cc.message2;
		} else if (cc.message1 == 0xE0) {
			auto pitchBendValue = (cc.message3 << 7) | cc.message2;
			s << pitchBendValue;
		}
	}
	outputMessage(s);
}

void postMidiSwitchCCLane(int command) {
	HWND editor = MIDIEditor_GetActive();
	ostringstream s;
	int ccNum = MIDIEditor_GetSetting_int(editor, "last_clicked_cc_lane");
	if (ccNum < 128) {
		s << ccNum << " ";
	}
	const int BUFFER_LENGTH = 64;
	char textBuffer[BUFFER_LENGTH];
	MIDIEditor_GetSetting_str(editor, "last_clicked_cc_lane", textBuffer, BUFFER_LENGTH);
	s << textBuffer;
	outputMessage(s);
}

void postToggleMidiInputsAsStepInput(int command) {
	if (GetToggleCommandState2(SectionFromUniqueID(MIDI_EDITOR_SECTION), command)) {
		outputMessage(translate("Enabled MIDI inputs as step input"));
	} else {
		outputMessage(translate("Disabled MIDI inputs as step input"));
	}
}

void postToggleFunctionKeysAsStepInput(int command) {
	if (GetToggleCommandState2(SectionFromUniqueID(MIDI_EDITOR_SECTION), command)) {
		outputMessage(translate("Enabled  f1-f12 as step input"));
	} else {
		outputMessage(translate("Disabled  f1-f12 as step input"));
	}
}

void postMidiToggleSnap(int command) {
	if (GetToggleCommandState2(SectionFromUniqueID(MIDI_EDITOR_SECTION), command)) {
		outputMessage(translate("enabled snap to grid"));
	} else {
		outputMessage(translate("disabled snap to grid"));
	}
}

void postMidiChangeZoom(int command) {
	MediaItem_Take* take = MIDIEditor_GetTake(MIDIEditor_GetActive());
	if (!take) {
		return;
	}
	double zoom = getMidiZoomRatio(take);
	if (zoom < 0) {
		return;
	}
	// Translators: Reported when zooming in or out horizontally. {} will be
	// replaced with the number of pixels per second; e.g. 100 pixels/second.
	outputMessage(format(translate("{} pixels/second"), formatDouble(zoom, 1)));
}
