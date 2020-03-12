/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * MIDI Editor commands code
 * Author: James Teh <jamie@jantrid.net>
 * Copyright 2015-2017 NV Access Limited
 * License: GNU General Public License version 2.0
 */

#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <map>
#include "osara.h"
#ifdef _WIN32
#include <Commctrl.h>
#endif

using namespace std;

typedef struct {
	int channel;
	int index;
	int control;
	int value;
	double position;
} MidiControlChange;

const double DEFAULT_PREVIEW_LENGTH = 0.3;
extern FakeFocus fakeFocus;

typedef struct {
	int channel;
	int pitch;
	int velocity;
	int index;
	double start;
	double end;
	double getLength() const {
		return max (0, (end - start));
	}
} MidiNote;
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
// This function doesn't begin sending the events.
void previewNotesOff() {
	for (auto note = previewingNotes.cbegin(); note != previewingNotes.cend(); ++note) {
		MIDI_event_t event = {0, 3, {
			(unsigned char)(MIDI_NOTE_OFF | note->channel),
			(unsigned char)note->pitch, (unsigned char)note->velocity}};
		previewSource.events.push_back(event);
	}
}

// Called after the preview length elapses to turn off notes currently being previewed.
void CALLBACK previewDone(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
	if (event != previewDoneTimer)
		return; // Cancelled.
	previewNotesOff();
	previewingNotes.clear();
	// Send the events.
	previewReg.curpos = 0.0;
	PlayTrackPreview(&previewReg);
	previewDoneTimer = 0;
}

// Used to find out the minimum note length.
bool compareNotesByLength(const MidiNote& note1, const MidiNote& note2) {
	return note1.getLength() < note2.getLength();
}

void previewNotes(MediaItem_Take* take, const vector<MidiNote>& notes) {
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
	if (previewDoneTimer) {
		// Notes are currently being previewed. Interrupt them.
		// We want to turn off these notes immediately.
		KillTimer(NULL, previewDoneTimer);
		previewDoneTimer = 0;
		previewNotesOff();
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
	previewDoneTimer = SetTimer(NULL, NULL, max(DEFAULT_PREVIEW_LENGTH, (minLength * 1000)), previewDone);
}

void cmdMidiMoveCursor(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	ostringstream s;
	s << formatCursorPosition(TF_MEASURE);
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
		fakeFocus = FOCUS_NOTE;
		s << " " << count << (count == 1 ? " note" : " notes");
		outputMessage(s);
}

const string getMidiNoteName(MediaItem_Take *take, int pitch, int channel) {
	static char* names[] = {"c", "c sharp", "d", "d sharp", "e", "f",
		"f sharp", "g", "g sharp", "a", "a sharp", "b"};
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
	int count;
	MIDI_CountEvts(take, &count, NULL, NULL);
	double now = GetCursorPosition();
	// Find the first note of the chord.
	int firstNote = direction == -1 ? count - 1 : 0;
	int movement = direction == 0 ? 1 : direction;
	bool found = false;
	int nowNote = -1;
	double firstStart;
	for (; 0 <= firstNote && firstNote < count; firstNote += movement) {
		MIDI_GetNote(take, firstNote, NULL, NULL, &firstStart, NULL, NULL, NULL, NULL);
		firstStart = MIDI_GetProjTimeFromPPQPos(take, firstStart);
		if (firstStart == now)
			nowNote = firstNote;
		if ((direction == 0 && firstStart == now)
			|| (direction == 1 && firstStart > now)
			|| (direction == -1 && firstStart < now)
		) {
			found = true;
			break;
		}
	}
	if (!found) {
		if (nowNote != -1) {
			// No chord in the requested direction, so use the chord at the cursor.
			firstNote = nowNote;
			firstStart = now;
			// Reverse the direction to find the remaining notes in the chord.
			movement = direction == 1 ? -1 : 1;
		} else
			return {-1, -1};
	}
	// Find the last note of the chord.
	int lastNote = firstNote;
	for (int note = lastNote + movement; 0 <= note && note < count; note += movement) {
		double start;
		MIDI_GetNote(take, note, NULL, NULL, &start, NULL, NULL, NULL, NULL);
		start = MIDI_GetProjTimeFromPPQPos(take, start);
		if (start != firstStart)
			break;
		lastNote = note;
	}
	return {min(firstNote, lastNote), max(lastNote, firstNote)};
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
	const int lastNoteIndex = notes.size() - 1;
	// Work out which note to move to.
	if (direction != 0 && 0 <= curNoteInChord && curNoteInChord <= lastNoteIndex) {
		// Already on a note within the chord. Move to the next/previous note.
		curNoteInChord += direction;
		// If we were already on the first/last note, stay there.
		if (curNoteInChord < 0 || curNoteInChord > lastNoteIndex)
			curNoteInChord -= direction;
	} else if (direction != 0) {
		// We're moving into a new chord. Move to the first/last note.
		curNoteInChord = direction == 1 ? 0 : lastNoteIndex;
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
	for(;;){
		ccIndex = MIDI_EnumSelCC(take, ccIndex);
		if (ccIndex == -1) {
			break;
		}
		double position;
		int chan, control, value;
		MIDI_GetCC(take, ccIndex, NULL, NULL, &position, NULL, &chan, &control, &value);
		position = MIDI_GetProjTimeFromPPQPos(take, position);
		ccs.push_back({chan, ccIndex, control, value, position});
	}
	return ccs;
}

void cmdMidiToggleSelection(Command* command) {
	if (isSelectionContiguous) {
		isSelectionContiguous = false;
		outputMessage("noncontiguous selection");
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
	}
	outputMessage(select ? "selected" : "unselected");
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
	if (!select && !isNoteSelected(take, chord.first))
		s << "unselected" << " ";
	if (shouldReportNotes) {
		int count = chord.second - chord.first + 1;
		s << count << (count == 1 ? " note" : " notes");
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
	if (select)
		selectNote(take, note.index);
	previewNotes(take, {note});
	fakeFocus = FOCUS_NOTE;
	ostringstream s;
	if (shouldReportNotes) {
		s << getMidiNoteName(take, note.pitch, note.channel);
	}
	if (!select && !isNoteSelected(take, note.index)) {
		s << " unselected ";
	} else if (shouldReportNotes) {
		s << ", ";
	}
	if (shouldReportNotes) {
		s << formatTime(note.getLength(), TF_MEASURE, true, false, false);
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

void cmdMidiMovePitchCursor(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
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
	int oldCount = MIDI_CountEvts(take, NULL, NULL, NULL);
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	if (MIDI_CountEvts(take, NULL, NULL, NULL) <= oldCount)
		return; // Not inserted.
	int pitch = MIDIEditor_GetSetting_int(editor, "active_note_row");
	// Get selected notes.
	vector<MidiNote> selectedNotes = getSelectedNotes(take);
	// Find the just inserted note based on its pitch, as that makes it unique.
	auto note = *(find_if(
		selectedNotes.begin(), selectedNotes.end(),
		[pitch](MidiNote n) { return n.pitch == pitch; })
	);
	// Play the inserted note.
	previewNotes(take, {note});
	fakeFocus = FOCUS_NOTE;
	ostringstream s;
	if (shouldReportNotes) {
		s << getMidiNoteName(take, note.pitch, note.channel) << " ";
		s << formatTime(note.getLength(), TF_MEASURE, true, false, false);
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
	ostringstream s;
	s << removed << (removed == 1 ? " event" : " events") << " removed";
	outputMessage(s);
}

void cmdMidiSelectNotes(Command* command) {
	int noteCount;
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
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
	ostringstream s;
	s << count << " note" << ((count == 1) ? "" : "s") << " selected";
	outputMessage(s);
}

const string getMidiControlName(MediaItem_Take *take, int control, int channel) {
	static map<int, string> names = {
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
	};
	MediaTrack* track = GetMediaItemTake_Track(take);
	int tracknumber = static_cast<int> (GetMediaTrackInfo_Value(track, "IP_TRACKNUMBER")); // one based
	const char* controlName = GetTrackMIDINoteName(tracknumber - 1, 128 + control, channel); // track number is zero based, controls start at 128
	ostringstream s;
	s << control;
	if (controlName) {
		s << " (" << controlName << ")";
	} else {
		auto it = names.find(control);
		if (it != names.end()) {
			s << " (" << it->second << ")";
		}
	}
	return s.str();
}

// We cache the last reported control so we can report just the components which have changed.
int oldControl = -1;

void moveToCC(int direction, bool clearSelection=true, bool select=true, bool useCache=true) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int count;
	MIDI_CountEvts(take, NULL, &count, NULL);
	double cursor = GetCursorPosition();
	double position;
	int start = direction == -1 ? count - 1 : 0;
	if (fakeFocus == FOCUS_CC // If not, currentCC is invalid.
		&& currentCC != -1)
	{
		MIDI_GetCC(take, currentCC, NULL, NULL, &position, NULL, NULL, NULL, NULL);
		position = MIDI_GetProjTimeFromPPQPos(take, position);
		if (direction == 1 ? position <= cursor : position >= cursor) {
			// The cursor is right at or has moved past the CC to which the user last moved.
			// Therefore, start at the adjacent CC.
			start = currentCC + direction;
			if (start < 0 || start >= count) {
				// There's no adja	cent item in this direction,
				// so move to the current one again.
				start = currentCC;
			}
		}
	} else {
		currentCC = -1;  // Invalid.
	}
	for (int index = start; 0 <= index && index < count; index += direction) {
		int chan, control, value;
		MIDI_GetCC(take, index, NULL, NULL, &position, NULL, &chan, &control, &value);
		position = MIDI_GetProjTimeFromPPQPos(take, position);
		if (direction == 1 ? position < cursor : position > cursor) {
			continue; // Not the right direction.
		}
		currentCC = index;
		if (clearSelection || select) {
			Undo_BeginBlock();
		}
		if (clearSelection) {
			MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
			isSelectionContiguous = true;
		}
		if (select) {
			selectCC(take, index);
		}
		if (clearSelection || select) {
			Undo_EndBlock("Change CC Selection", 0);
		}
		SetEditCurPos(position, true, false);
		fakeFocus = FOCUS_CC;
		ostringstream s;
		s << formatCursorPosition(TF_MEASURE) << " ";
		if (!useCache || control != oldControl) {
			s << getMidiControlName(take, control, chan) << ", ";
			oldControl = control;
		}
		s << value;
		if (!select && !isCCSelected(take, index)) {
			s << "unselected" << " ";
		}
		outputMessage(s);
		return;
	}
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
	s << " item " << itemNum << " " << GetTakeName(take);
	outputMessage(s);
}

#ifdef _WIN32
void cmdFocusNearestMidiEvent(Command* command) {
	HWND focus = GetFocus();
	if (!focus)
		return;
	double cursorPos = GetCursorPosition();
	for (int i = 0; i < ListView_GetItemCount(focus); ++i) {
		char text[50];
		// Get the text from the position column (1).
		ListView_GetItemText(focus, i, 1, text, sizeof(text));
		// Convert this to project time. text is always in measures.beats.
		double eventPos = parse_timestr_pos(text, 2);
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

#endif // _WIN32
