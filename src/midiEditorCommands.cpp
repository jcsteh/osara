/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * MIDI Editor commands code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2022 NV Access Limited, James Teh
 * License: GNU General Public License version 2.0
 */

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <map>
#include <cassert>
#include "midiEditorCommands.h"
#include "osara.h"
#include "translation.h"
#ifdef _WIN32
#include <Commctrl.h>
#endif

using namespace std;
using namespace fmt::literals;

// Note: while the below struct is called MidiControlChange in line with naming in Reaper,
// It is also used for other MIDI messages.
typedef struct {
	int channel;
	int index;
	int message1;
	int message2;
	int message3;
	double position;
} MidiControlChange;

const UINT DEFAULT_PREVIEW_LENGTH = 300; // ms

struct MidiNote {
	int channel;
	int pitch;
	int velocity;
	int index;
	double start;
	double end;

	double getLength() const {
		return max (0, (end - start));
	}
};

vector<MidiNote> previewingNotes; // Notes currently being previewed.
UINT_PTR previewDoneTimer = 0;
const int MIDI_NOTE_ON = 0x90;
const int MIDI_NOTE_OFF = 0x80;
bool shouldReportNotes = true;

// A minimal PCM_source to send MIDI events for preview.
class PreviewSource : public PCM_source {
	public:
	
	PreviewSource() {
	}

	virtual ~PreviewSource() {
	}

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
		block->samples_out=0;
		if (block->midi_events) {
			for (auto event = this->events.begin(); event != this->events.end(); ++event)
				block->midi_events->AddItem(&*event);
			this->events.clear();
		}
	}

	void GetPeakInfo(PCM_source_peaktransfer_t* block) {
		block->peaks_out=0;
	}

	void SaveState(ProjectStateContext* ctx) {
	}

	int LoadState(char* firstLine, ProjectStateContext* ctx) {
		return -1;
	}

	void Peaks_Clear(bool deleteFile) {
	}

	int PeaksBuild_Begin() {
		return 0;
	}

	int PeaksBuild_Run() {
		return 0;
	}

	void PeaksBuild_Finish() {
	}

};

PreviewSource previewSource;
preview_register_t previewReg = {0};

// Queue note off events for the notes currently being previewed.
// when sendNoteOff is true, this function  also sends the events.
void previewNotesOff(bool sendNoteOff) {
	for (auto note = previewingNotes.cbegin(); note != previewingNotes.cend(); ++note) {
		MIDI_event_t event = {0, 3, {
			(unsigned char)(MIDI_NOTE_OFF | note->channel),
			(unsigned char)note->pitch, (unsigned char)note->velocity}};
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
	if (!GetToggleCommandState2(SectionFromUniqueID(MIDI_EDITOR_SECTION), 40041)) {  // Options: Preview notes when inserting or editing
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
	for (auto note = notes.cbegin(); note != notes.cend(); ++note) {
		MIDI_event_t event = {0, 3, {
			(unsigned char)(MIDI_NOTE_ON | note->channel),
			(unsigned char)note->pitch, (unsigned char)note->velocity}};
		previewSource.events.push_back(event);
	}
	// Save the notes being previewed so we can turn them off later (previewNotesOff).
	previewingNotes = notes;
	// Send the events.
	void* track = GetSetMediaItemTakeInfo(take, "P_TRACK", NULL);
	previewReg.preview_track = track;
	previewReg.curpos = 0.0;
	PlayTrackPreview(&previewReg);
	// Calculate the minimum note length.
	double minLength = min_element(notes.begin(), notes.end(), compareNotesByLength)->getLength();
	// Schedule note off messages.
	previewDoneTimer = SetTimer(nullptr, 0,
		(UINT)(minLength ? minLength * 1000 : DEFAULT_PREVIEW_LENGTH), previewDone);
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
// The template parameter is arbitrary and is only used to specialise the
// template for different event types (note, CC, etc.). We do this instead of
// subclassing because iterators need to return their exact type, not a base
// class.
template<auto eventType>
class MidiEventIterator {
	public:
	using difference_type = int;
	using value_type = const double;
	using pointer = void;
	using reference = void;
	using iterator_category = random_access_iterator_tag;

	MidiEventIterator(MediaItem_Take* take): take(take) {
		this->count = this->getCount();
		this->index = 0;
	}

	bool operator==(const MidiEventIterator& other) const {
		return this->take == other.take && this->index == other.index;
	}

	bool operator!=(const MidiEventIterator& other) const {
		return !(*this == other);
	}

	bool operator<(const MidiEventIterator& other) const {
		return this->index < other.index;
	}

	bool operator<=(const MidiEventIterator& other) const {
		return this->index <= other.index;
	}

	const double operator[](const int index) const {
		double pos = this->getEvent(this->index + index);
		return MIDI_GetProjTimeFromPPQPos(take, pos);
	}

	const double operator*() const {
		return (*this)[0];
	}

	MidiEventIterator& operator++() {
		++this->index;
		return *this;
	}

	MidiEventIterator& operator--() {
		--this->index;
		return *this;
	}

	MidiEventIterator& operator+=(const int increment) {
		this->index += increment;
		return *this;
	}

	MidiEventIterator& operator-=(const int decrement) {
		this->index -= decrement;
		return *this;
	}

	int operator-(const MidiEventIterator& other) {
		return this->index - other.index;
	}

	void moveToEnd() {
		this->index = count;
	}

	int getIndex() const {
		return this->index;
	}

	protected:
	int getCount() const;
	double getEvent(int index) const;
	MediaItem_Take* take;

	private:
	int count;
	int index;
};

using MidiNoteIterator = MidiEventIterator<1>;

template<>
int MidiNoteIterator::getCount() const {
	int count;
	MIDI_CountEvts(this->take, &count, nullptr, nullptr);
	return count;
}

template<>
double MidiNoteIterator::getEvent(int index) const {
	double start;
	MIDI_GetNote(take, index, nullptr, nullptr, &start, nullptr, nullptr,
		nullptr, nullptr);
	return start;
}

void cmdMidiMoveCursor(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	ostringstream s;
	s << formatCursorPosition(TF_MEASURE);
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int noteCount;
	MIDI_CountEvts(take, &noteCount, NULL, NULL);
	double now = GetCursorPosition();
	// todo: Optimise; perhaps a binary search?
	vector<MidiNote> notes;
	for (int n = 0; n < noteCount; ++n) {
		double start, end;
		int chan, pitch, vel;
		MIDI_GetNote(take, n, NULL, NULL, &start, &end, &chan, &pitch, &vel);
		start = MIDI_GetProjTimeFromPPQPos(take, start);
		if (start > now) {
			break;
		}
		if (start == now) {
			end = MIDI_GetProjTimeFromPPQPos(take, end);
			notes.push_back({chan, pitch, vel, 0, start, end});
		}
	}
	int count = static_cast<int>(notes.size());
	if (count > 0) {
		previewNotes(take, notes);
		fakeFocus = FOCUS_NOTE;
		s << " " << format(
			translate_plural("{} note", "{} notes", count), count);
	}
	outputMessage(s);
}

const string getMidiNoteName(MediaItem_Take *take, int pitch, int channel) {
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
	int tracknumber = static_cast<int> (GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER")); // one based
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

// Returns the indexes of the first and last notes in a chord in a given direction.
pair<int, int> findChord(MediaItem_Take* take, int direction) {
	double now = GetCursorPosition();
	MidiNoteIterator begin(take);
	MidiNoteIterator end = begin;
	end.moveToEnd();
	if (begin == end) {
		// No notes.
		return {-1, -1};
	}
	auto range = equal_range(begin, end, now);
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
		return {range.first.getIndex(), range.second.getIndex() - 1};
	} else {
		// Nothing in the requested direction or at the cursor.
		return {-1, -1};
	}
	// Find the last note of the chord.
	double firstStart = *firstNote;
	MidiNoteIterator lastNote = firstNote;
	MidiNoteIterator note = firstNote;
	for (note += direction; begin <= note && note < end; note += direction) {
		if (*note != firstStart) {
			break;
		}
		lastNote = note;
	}
	return {min(firstNote.getIndex(), lastNote.getIndex()),
		max(lastNote.getIndex(), firstNote.getIndex())};
}

// Keeps track of the note to which the user last moved in a chord.
// This is the number of the note in the chord; e.g. 0 is the first note.
// It is not a REAPER note index!
// -1 means not in a chord.
int curNoteInChord = -1;
// Keeps track of the CC to which the user last moved.
// This is a REAPER CC index.
// -1 means no CC.
int currentCC = -1;

// Used to order notes in a chord by pitch.
bool compareNotesByPitch(const MidiNote& note1, const MidiNote& note2) {
	return note1.pitch < note2.pitch;
}

// Finds a single note in the chord at the cursor in a given direction and returns its info.
// This updates curNoteInChord.
MidiNote findNoteInChord(MediaItem_Take* take, int direction) {
	auto chord = findChord(take, 0);
	if (chord.first == -1)
		return {-1};
	// Notes at the same position are ordered arbitrarily.
	// This is not intuitive, so sort them.
	vector<MidiNote> notes;
	for (int note = chord.first; note <= chord.second; ++note) {
		double start, end;
		int chan, pitch, vel;
		MIDI_GetNote(take, note, NULL, NULL, &start, &end, &chan, &pitch, &vel);
		start = MIDI_GetProjTimeFromPPQPos(take, start);
		end = MIDI_GetProjTimeFromPPQPos(take, end);
		notes.push_back({chan, pitch, vel, note, start, end});
	}
	sort(notes.begin(), notes.end(), compareNotesByPitch);
	const int lastNoteIndex = (int)notes.size() - 1;
	// Work out which note to move to.
	if (direction != 0 && 0 <= curNoteInChord &&
			curNoteInChord <= (int)lastNoteIndex) {
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

void selectNote(MediaItem_Take* take, const int note, bool select=true) {
	MIDI_SetNote(take, note, &select, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

bool isNoteSelected(MediaItem_Take* take, const int note) {
	bool sel;
	MIDI_GetNote(take, note, &sel, NULL, NULL, NULL, NULL, NULL, NULL);
	return sel;
}

int countSelectedNotes(MediaItem_Take* take, int offset=-1) {
	int noteIndex = offset;
	int count = 0;
	for(;;){
		noteIndex = MIDI_EnumSelNotes(take, noteIndex);
		if (noteIndex == -1) {
			break;
		}
		++count;
	}
	return count;
}

vector<MidiNote> getSelectedNotes(MediaItem_Take* take, int offset=-1) {
	int noteIndex = offset;
	vector<MidiNote> notes;
	for(;;){
		noteIndex = MIDI_EnumSelNotes(take, noteIndex);
		if (noteIndex == -1) {
			break;
		}
		double start, end;
		int chan, pitch, vel;
		MIDI_GetNote(take, noteIndex, NULL, NULL, &start, &end, &chan, &pitch, &vel);
		start = MIDI_GetProjTimeFromPPQPos(take, start);
		end = MIDI_GetProjTimeFromPPQPos(take, end);
		notes.push_back({chan, pitch, vel, noteIndex, start, end});
	}
	return notes;
}

// Finds a single CC at the cursor in a given direction and returns its info.
// This updates currentCC.
MidiControlChange findCC(MediaItem_Take* take, int direction) {
	int count;
	MIDI_CountEvts(take, NULL, &count, NULL);
	double cursor = GetCursorPosition();
	double position;
	int start = direction == -1 ? count - 1 : 0;
	if (currentCC != -1) {
		MIDI_GetCC(take, currentCC, NULL, NULL, &position, NULL, NULL, NULL, NULL);
		position = MIDI_GetProjTimeFromPPQPos(take, position);
		if (direction == 0 && position == cursor) {
			start = currentCC;
		} else if (direction == 1 ? position <= cursor : position >= cursor) {
			// The cursor is right at or has moved past the CC to which the user last moved.
			// Therefore, start at the adjacent CC.
			start = currentCC + direction;
			if (start < 0 || start >= count) {
				// There's no adjacent item in this direction,
				// so move to the current one again.
				start = currentCC;
			}
		}
	}
	int movement = direction == 0 ? 1 : direction;
	bool found = false;
	int chan, msg1, msg2, msg3;
	for (; 0 <= start && start < count; start += movement) {
		MIDI_GetCC(take, start, NULL, NULL, &position, &msg1, &chan, &msg2, &msg3);
		position = MIDI_GetProjTimeFromPPQPos(take, position);
		if (movement == -1 ? position <= cursor : position >= cursor) {
			currentCC = start;
			found = true;
			break;
		}
	}
	if (!found) {
		return {-1};
	}
	return {chan, currentCC, msg1, msg2, msg3, position};
}

void selectCC(MediaItem_Take* take, const int cc, bool select=true) {
	MIDI_SetCC(take, cc, &select, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

bool isCCSelected(MediaItem_Take* take, const int cc) {
	bool sel;
	MIDI_GetCC(take, cc, &sel, NULL, NULL, NULL, NULL, NULL, NULL);
	return sel;
}

vector<MidiControlChange> getSelectedCCs(MediaItem_Take* take, int offset=-1) {
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
				select = !isNoteSelected(take, note.index);
				selectNote(take, note.index, select);
			} else {
				// Chord.
				auto chord = findChord(take, 0);
				if (chord.first == -1) {
					return;
				}
				select = !isNoteSelected(take, chord.first);
				for (int note = chord.first; note <= chord.second; ++note) {
					selectNote(take, note, select);
				}
			}
			break;
		}
		case FOCUS_CC: {
			if (currentCC == -1) {
				return;
			}
			select = !isCCSelected(take, currentCC);
			selectCC(take, currentCC, select);
			break;
		}
		default:
			return;
	}
	outputMessage(select ? translate("selected") : translate("unselected"));
}

void moveToChord(int direction, bool clearSelection=true, bool select=true) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	auto chord = findChord(take, direction);
	if (chord.first == -1)
		return;
	curNoteInChord = -1;
	if (clearSelection) {
		MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
		isSelectionContiguous = true;
	}
	// Move the edit cursor to this chord, select it and play it.
	bool cursorSet = false;
	vector<MidiNote> notes;
	for (int note = chord.first; note <= chord.second; ++note) {
		double start, end;
		int chan, pitch, vel;
		MIDI_GetNote(take, note, NULL, NULL, &start, &end, &chan, &pitch, &vel);
		start = MIDI_GetProjTimeFromPPQPos(take, start);
		end = MIDI_GetProjTimeFromPPQPos(take, end);
		if (!cursorSet && direction != 0) {
			SetEditCurPos(start, true, false);
			cursorSet = true;
		}
		if (select)
			selectNote(take, note);
		notes.push_back({chan, pitch, vel, 0, start, end});
	}
	previewNotes(take, notes);
	fakeFocus = FOCUS_NOTE;
	ostringstream s;
	s << formatCursorPosition(TF_MEASURE) << " ";
	if (!select && !isNoteSelected(take, chord.first)) {
		s << translate("unselected") << " ";
	}
	if (shouldReportNotes) {
		int count = chord.second - chord.first + 1;
		// Translators: used when reporting the number of notes in a chord.
		// {} will be replaced by the number of notes. E.g. "3 notes"
		s << format(
			translate_plural("{} note", "{} notes", count), count);
	}
	outputMessage(s);
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

void moveToNoteInChord(int direction, bool clearSelection=true, bool select=true) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	MidiNote note = findNoteInChord(take, direction);
	if (note.channel == -1)
		return;
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
	if (shouldReportNotes) {
		s << getMidiNoteName(take, note.pitch, note.channel);
	}
	if (!select && !isNoteSelected(take, note.index)) {
		s << " " << translate("unselected") << " ";
	} else if (shouldReportNotes) {
		s << ", ";
	}
	if (shouldReportNotes) {
		s << formatNoteLength(note.start, note.end);
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
	previewNotes(take, {{chan, pitch, vel}});
	if (shouldReportNotes) {
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
	auto it = find_if(
		selectedNotes.begin(), selectedNotes.end(),
		[pitch](MidiNote n) { return n.pitch == pitch; }
	);
	if (it == selectedNotes.end()) {
		return;
	}
	auto& note = *it;
	// Play the inserted note when preview is enabled.
	previewNotes(take, {note});
	fakeFocus = FOCUS_NOTE;
	ostringstream s;
	if (shouldReportNotes) {
		s << getMidiNoteName(take, note.pitch, note.channel) << " ";
		s << formatNoteLength(note.start, note.end);
		s << ", ";
	}
	s << formatCursorPosition(TF_MEASURE);
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
	outputMessage(format(
		translate_plural("{} event removed", "{} events removed", removed), removed));
}

void postMidiSelectNotes(int command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int noteIndex=-1;
	int count=0;
	for(;;){
		noteIndex = MIDI_EnumSelNotes(take, noteIndex);
		if (noteIndex == -1)
			break;
		++count;
	}
	fakeFocus = FOCUS_NOTE;
	// Translators: used when notes are selected in the MIDI editor.
	// {} is replaced by the number of notes. E.g. "4 notes selected"
	outputMessage(format(
		translate_plural("{} note selected", "{} notes selected", count ),
		count ));
}

void postMidiSelectEvents(int command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int evtIndex=-1;
	int count=0;
	for(;;){
		evtIndex = MIDI_EnumSelEvts(take, evtIndex);
		if (evtIndex == -1) {
			break;
		}
		unsigned char msg[3] = "\0";
		int size = sizeof(msg);
		MIDI_GetEvt(take, evtIndex, /* selectedOut */ nullptr, /* mutedOut */ nullptr,
			/* ppqposOut */ nullptr, (char*)msg, &size);
		if (0x80 <= msg[0] && msg[0] <= 0x8F) {
			continue; // Don't count note off messages.
		}
		++count;
	}
	if (fakeFocus != FOCUS_NOTE && fakeFocus != FOCUS_CC) {
		fakeFocus = FOCUS_NOTE;
	}
	// Translators: Reported when selecting events in the MIDI editor. {} will be replaced with
	// the number of events; e.g. "2 events selected".
	outputMessage(format(
		translate_plural("{} event selected", "{} events selected", count),
		count));
}

const string getMidiControlName(MediaItem_Take *take, int control, int channel) {
	static map<int, string> names = {
		// translate firstString begin
		{0, "Bank Select MSB"},
		{1, "Mod Wheel MSB"},
		{2, "Breath MSB"},
		{4, "Foot Pedal MSB"},
		{5, "Portamento MSB"},
		{6, "Data Entry MSB"},
		{7, "Volume MSB"},
		{8, "Balance MSB"},
		{10, "Pan Position MSB"},
		{11, "Expression MSB"},
		{12, "Control 1 MSB"},
		{13, "Control 2 MSB"},
		{16, "GP Slider 1"},
		{17, "GP Slider 2"},
		{18, "GP Slider 3"},
		{19, "GP Slider 4"},
		{32, "Bank Select LSB"},
		{33, "Mod Wheel LSB"},
		{34, "Breath LSB"},
		{36, "Foot Pedal LSB"},
		{37, "Portamento LSB"},
		{38, "Data Entry LSB"},
		{39, "Volume LSB"},
		{40, "Balance LSB"},
		{42, "Pan Position LSB"},
		{43, "Expression LSB"},
		{44, "Control 1 LSB"},
		{45, "Control 2 LSB"},
		{64, "Hold Pedal (on/off)"},
		{65, "Portamento (on/off)"},
		{66, "Sostenuto (on/off)"},
		{67, "Soft Pedal (on/off)"},
		{68, "Legato Pedal (on/off)"},
		{69, "Hold 2 Pedal (on/off)"},
		{70, "Sound Variation"},
		{71, "Timbre/Resonance"},
		{72, "Sound Release"},
		{73, "Sound Attack"},
		{74, "Brightness/Cutoff Freq"},
		{75, "Sound Control 6"},
		{76, "Sound Control 7"},
		{77, "Sound Controll 8"},
		{78, "Sound Control 9"},
		{79, "Sound Control 10"},
		{80, "GP Button 1 (on/off)"},
		{81, "GP Button 2 (on/off)"},
		{82, "GP Button 3 (on/off)"},
		{83, "GP Button 4 (on/off)"},
		{91, "Effects Level"},
		{92, "Tremolo Level"},
		{93, "Chorus Level"},
		{94, "Celeste Level"},
		{95, "Phaser Level"},
		{96, "Data Button Inc"},
		{97, "Data Button Dec"},
		{98, "Non-Reg Parm LSB"},
		{99, "Non-Reg Parm MSB"},
		{100, "Reg Parm LSB"},
		{101, "Reg Parm MSB"},
		{120, "All Sound Off"},
		{121, "Reset"},
		{122, "Local"},
		{123, "All Notes Off"},
		{124, "Omni On"},
		{125, "Omni Off"},
		{126, "Mono On"},
		{127, "Poly On"}
		// translate firstString end
	};
	MediaTrack* track = GetMediaItemTake_Track(take);
	int tracknumber = static_cast<int> (GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER")); // one based
	ostringstream s;
	const char* controlName = GetTrackMIDINoteName(tracknumber - 1, 128 + control, channel); // track number is zero based, controls start at 128
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

void moveToCC(int direction, bool clearSelection=true, bool select=true) {
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
		Undo_EndBlock("Change CC Selection", 0);
	}
	SetEditCurPos(cc.position, true, false);
	fakeFocus = FOCUS_CC;
	ostringstream s;
	s << formatCursorPosition(TF_MEASURE) << " ";
	if (cc.message1 == 0xA0) {
		// Translators: MIDI poly aftertouch. {note} will be replaced with the note
		// name and {value} will be replaced with the aftertouch value; e.g.
		// "Poly Aftertouch c sharp 4  96"
		s << format(translate("Poly Aftertouch {note}  {value}"),
			"note"_a=getMidiNoteName(take, cc.message2, cc.channel),
			"value"_a=cc.message3);
	} else if (cc.message1 == 0xB0) {
		// Translators: A MIDI CC. {control} will be replaced with the control number and name. {value} will be replaced with the value of the control; e.g. "control 70 (Sound Variation), 64"
		s << format(translate("Control {control}, {value}"),
		"control"_a=getMidiControlName(take, cc.message2, cc.channel),
		"value"_a=cc.message3);
	} else if (cc.message1 == 0xC0) {
		//Translators: a MIDI program number.  {} will be replaced with the program number; e.g. "Program 5"
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
	MIDIEditor_OnCommand(editor, ((direction==1)?40798:40797)); // Contents: Activate next/previous MIDI media item on this track, clearing the editor first
	MIDIEditor_OnCommand(editor, 40036); // View: Go to start of file
	int cmd = NamedCommandLookup("_FNG_ME_SELECT_NOTES_NEAR_EDIT_CURSOR");
	if(cmd>0)
		MIDIEditor_OnCommand(editor, cmd); // SWS/FNG: Select notes nearest edit cursor
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	MediaItem* item = GetMediaItemTake_Item(take);
	MediaTrack* track = GetMediaItem_Track(item);
	int count = CountTrackMediaItems(track);
	int itemNum;
	for (int i=0; i<count; ++i) {
		MediaItem* itemTmp = GetTrackMediaItem(track, i);
		if (itemTmp == item) {
			itemNum = i+1;
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
	Undo_EndBlock("OSARA: Move to next midi item on track", 0);
}

void cmdMidiMoveToPrevItem(Command* command) {
	Undo_BeginBlock();
	midiMoveToItem(-1);
	Undo_EndBlock("OSARA: Move to previous midi item on track", 0);
}

void cmdMidiMoveToTrack(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	MediaItem* item = GetMediaItemTake_Item(take);
	MediaTrack* track = GetMediaItem_Track(item);
	int count = CountTrackMediaItems(track);
	int itemNum;
	for (int i=0; i<count; ++i) {
		MediaItem* itemTmp = GetTrackMediaItem(track, i);
		if (itemTmp == item) {
			itemNum = i+1;
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
	s << " " << format(
		translate("item {num} {name}"),
		"num"_a=itemNum, "name"_a=GetTakeName(take));
	outputMessage(s);
}

void cmdMidiSelectSamePitchStartingInTimeSelection(Command* command) {
	double tsStart,tsEnd;
	GetSet_LoopTimeRange(false, false, &tsStart, &tsEnd, false);
	if(tsStart == tsEnd) {
		outputMessage(translate("no time selection"));
		return;
	}
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int selNote = MIDI_EnumSelNotes(take, -1);
	if(selNote==-1) {
		outputMessage(translate("no notes selected"));
		return;
	}
	int selPitch;
	MIDI_GetNote(take, selNote, nullptr, nullptr, nullptr, nullptr, nullptr, &selPitch, nullptr);
	Undo_BeginBlock();
	MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
	int noteCount {0}, selectCount {0};
	MIDI_CountEvts(take, &noteCount, nullptr, nullptr);
	for(int i=0; i<noteCount; i++) {
		double startPPQPos;
		int pitch;
		MIDI_GetNote(take, i, nullptr, nullptr, &startPPQPos, nullptr, nullptr, &pitch, nullptr);
		double startTime = MIDI_GetProjTimeFromPPQPos(take, startPPQPos);
		if(tsStart<=startTime && startTime<tsEnd && pitch==selPitch) {
			selectNote(take, i);
			selectCount++;
		}
	}
	Undo_EndBlock("OSARA: Select all notes with the same pitch within time selection",0);
	// Translators: used when notes are selected in the MIDI editor.
	// {} is replaced by the number of notes. E.g. "4 notes selected"
	outputMessage(format(
		translate_plural("{} note selected", "{} notes selected", selectCount), selectCount ));
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
				"oldCount"_a=oldCount, "newCount"_a=newCount));
			break;
		case 40456:
			// Translators: used when joining notes in the midi editor.
			// {oldCount} is replaced by the number of notes that were selected
			// before the command.  This is the number the plural form is based
			// on. {newCount} is replaced by the number of selected notes after
			// the command. E.g. "2 notes joined into 1"
			outputMessage(format(
				translate_plural("{oldCount} note joined into {newCount}", "{oldCount} notes joined into {newCount}", oldCount),
				"oldCount"_a=oldCount, "newCount"_a=newCount));
			break;
		default:
			break;
	}
}

#ifdef _WIN32
map<string, string> parseEventData(string const& source) {
	map<string, string> m;
	string key, val;
	istringstream s(source);
	while(getline(getline(s, key, '='), val, ' ')) {
		m[key] = val;
	}
	return m;
}

void cmdFocusNearestMidiEvent(Command* command) {
	HWND focus = GetFocus();
	if (!focus)
		return;
	double cursorPos = GetCursorPosition();
	HWND editor = MIDIEditor_GetActive();
	assert(editor == GetParent(focus));
	auto listCount = MIDIEditor_GetSetting_int(editor, "list_cnt");
	for (int i = 0; i < listCount; ++i) {
		auto setting = format("list_{}", i);
		char eventData[255] = "\0";
		if (!MIDIEditor_GetSetting_str(editor, setting.c_str(), eventData, sizeof(eventData))) {
			continue;
		}
		auto eventValueMap = parseEventData(eventData);
		// Check whether this event has a position
		auto posIt = eventValueMap.find("pos");
		if (posIt == eventValueMap.end()) {
			// no position
			continue;
		}
		auto eventPosQn = stof((*posIt).second);
		auto eventPos = TimeMap2_QNToTime(nullptr, eventPosQn);
		if (eventPos >= cursorPos) {
			// This item is at or just after the cursor.
			int oldFocus = ListView_GetNextItem(focus, -1, LVNI_FOCUSED);
			// Focus and select this item.
			ListView_SetItemState(focus, i,
				LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
			ListView_EnsureVisible (focus, i, false);
			if (oldFocus != -1 && oldFocus != i) {
				// Unselect the previously focused item.
				ListView_SetItemState(focus, oldFocus,
					0, LVIS_SELECTED);
			}
			break;
		}
	}
}

void cmdMidiFilterWindow(Command *command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	// TODO: we could also check the command state was "off", to skip searching otherwise
	HWND filter = FindWindow(WC_DIALOG, "Filter Events");
	if (filter && (filter != GetFocus())) {
		SetFocus(filter); // focus the window
	}
}

void maybePreviewCurrentNoteInEventList(HWND hwnd) {
	if (!GetToggleCommandState2(SectionFromUniqueID(MIDI_EVENT_LIST_SECTION), 40041)) {  // Options: Preview notes when inserting or editing
		return;
	}
	auto focused = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
	HWND editor = MIDIEditor_GetActive();
	assert(editor == GetParent(hwnd));
	auto setting = format("list_{}", focused);
	char eventData[255] = "\0";
	if (!MIDIEditor_GetSetting_str(editor, setting.c_str(), eventData, sizeof(eventData))) {
		return;
	}
	auto eventValueMap = parseEventData(eventData);
	// Check whether this is a note
	auto lenIt = eventValueMap.find("len");
	if (lenIt == eventValueMap.end()) {
		// No Note
		return;
	}
	MidiNote note;
	auto lengthQn = stof((*lenIt).second);
	auto startQn = stof(eventValueMap.at("pos"));
	auto endQn = startQn + lengthQn;
	note.start = TimeMap2_QNToTime(nullptr, startQn);
	note.end = TimeMap2_QNToTime(nullptr, endQn);
	auto msg = stoi(eventValueMap.at("msg"), nullptr, 16);
	note.pitch = (msg >> 8) & 0xff;
	note.channel = (msg >> 16) &0xf;
	note.velocity = msg & 0xff;
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	previewNotes(take, {note});
}

#endif // _WIN32

void postMidiChangeVelocity(int command) {
	if (!shouldReportNotes) {
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
		if (chord.first == -1) {
			generalize = true;
		} else {
			generalize = !(all_of(
				selectedNotes.begin(), selectedNotes.end(),
				[chord](MidiNote n) { return chord.first <= n.index && n.index <= chord.second; }
			));
		}
	}
	// The Reaper action takes care of note preview.
	ostringstream s;
	if (generalize) {
		int count = static_cast<int>(selectedNotes.size());
		s << format(
			translate_plural("{} note", "{} notes", count), count) << " ";
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
	} else{
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
		if (chord.first == -1) {
			generalize = true;
		} else {
			generalize = !(all_of(
				selectedNotes.begin(), selectedNotes.end(),
				[chord](MidiNote n) { return chord.first <= n.index && n.index <= chord.second; }
			));
		}
	}
	if (!generalize) {
		previewNotes(take, selectedNotes);
	}
	if (shouldReportNotes) {
		ostringstream s;
		if (generalize) {
			int count = static_cast<int>(selectedNotes.size());
			switch (command) {
				case 40444:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g. "3
					// notes lengthened pixel"
					s << format(
						translate_plural("{} note lengthened pixel", "{} notes lengthened pixel", count), count);
					break;
				case 40445:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g. "3
					// notes shortened pixel"
					s << format(
						translate_plural("{} note shortened pixel", "{} notes shortened pixel", count), count);
					break;
				case 40446:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes lengthened grid unit"
					s << format(
						translate_plural("{} note lengthened grid unit", "{} notes lengthened grid unit", count), count);
					break;
				case 40447:
										// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes shortened grid unit"
					s << format(
						translate_plural("{} note shortened grid unit", "{} notes shortened grid unit", count), count);
					break;
				case 40633:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes set length to grid size"
					s << format(
						translate_plural("{} note set length to grid size", "{} notes length set to grid size", count), count);
					break;
				case 40765:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes made legato"
					s << format(
						translate_plural("{} note made legato", "{} notes made legato", count), count);
					break;
				default:
					// Translators: Used when changing note length in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes length changed"
					s << format(
						translate_plural("{} note length changed", "{} notes length changed", count), count);
					break;
			}
		} else{ 
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
	if (!shouldReportNotes) {
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
		if (chord.first == -1) {
			generalize = true;
		} else {
			generalize = !(all_of(
				selectedNotes.begin(), selectedNotes.end(),
				[chord](MidiNote n) { return chord.first <= n.index && n.index <= chord.second; }
			));
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
				s << format(
					translate_plural("{} note semitone up", "{} notes semitone up", count), count);
				break;
			case 40178:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g. 
				// "3 notes semitone down"
				s << format(
					translate_plural("{} note semitone down", "{} notes semitone down", count), count);
				break;
			case 40179:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g. 
				// "3 notes octave up"
				s << format(
					translate_plural("{} note octave up", "{} notes octave up", count), count);
				break;
			case 40180:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g. 
				// "3 notes octave down"
				s << format(
					translate_plural("{} note octave down", "{} notes octave down", count), count);
				break;
			case 41026:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g. 
				// "3 notes semitone up ignoring scale"
				s << format(
					translate_plural("{} note semitone up ignoring scale", "{} notes semitone up ignoring scale", count), count);
				break;
			case 41027:
				// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g. 
				// "3 notes semitone down ignoring scale"
				s << format(
					translate_plural("{} note semitone down ignoring scale", "{} notes semitone down ignoring scale", count), count);
				break;
			default:
			// Translators: Used when changing note pitch in the MIDI
				// editor. {} is replaced by the number of notes, e.g. 
				// "3 notes pitch changed"
				s << format(
					translate_plural("{} note pitch changed", "{} notes pitch changed", count), count);
				break;
		}
	} else{ 
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
	bool generalize = count >= 8 || any_of(
		selectedNotes.begin(), selectedNotes.end(),
		[firstStart](MidiNote n) { return firstStart != n.start; }
		);
	if (!generalize) {
		previewNotes(take, selectedNotes);
	}
	if (shouldReportNotes) {
		ostringstream s;
		if (generalize) {
			switch (command) {
				case 40181:
					// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes pixel left"
					s << format(
						translate_plural("{} note pixel left", "{} notes pixel left", count), count);
					break;
				case 40182:
				// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes pixel right"
					s << format(
						translate_plural("{} note pixel right", "{} notes pixel right", count), count);
					break;
				case 40183:
					// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes grid unit left"
					s << format(
						translate_plural("{} note grid unit left", "{} notes grid unit left", count), count);
					break;
				case 40184:
					// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes grid unit right"
					s << format(
						translate_plural("{} note grid unit right", "{} notes grid unit right", count), count);
					break;				
				default:
					// Translators: Used when moving notes in the MIDI
					// editor. {} is replaced by the number of notes, e.g. 
					// "3 notes pixel left"
					s << format(
						translate_plural("{} note start moved", "{} notes start moved", count), count);
					break;
			}
		} else{ 
			for (auto note = selectedNotes.cbegin(); note != selectedNotes.cend(); ++note) {
				if (note == selectedNotes.cbegin()) {
					s << formatTime(note->start, TF_MEASURE) << " ";
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
				s << format(
					translate_plural("{} value increase", "{} values increased", count), count);
				break;
			}
			case 40677: {
				// Translators: Used when MIDI CCs change. {} is replaced by the
				// number of values changed. E.g. "2 values decreased"
				s << format(
					translate_plural("{} value decreased", "{} values decreased", count), count);
				break;
			}
			default: {
				// Translators: Used when MIDI CCs change. {} is replaced by the
				// number of values changed. E.g. "2 values changed"
				s << format(
					translate_plural("{} value changed", "{} values changed", count), count);
				break;
			}
		}
	} else{ 
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
	if(GetToggleCommandState2(SectionFromUniqueID(MIDI_EDITOR_SECTION), command)) {
		outputMessage(translate("Enabled  f1-f12 as step input"));
	} else {
		outputMessage(translate("Disabled  f1-f12 as step input"));
	}
}
