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
#include "osara.h"
#ifdef _WIN32
#include <Commctrl.h>
#endif

using namespace std;

typedef struct {
	int channel;
	int pitch;
	int velocity;
	int index;
} MidiNote;
vector<MidiNote> previewingNotes; // Notes currently being previewed.
UINT_PTR previewDoneTimer = 0;
const UINT PREVIEW_LENGTH = 300;
const int MIDI_NOTE_ON = 0x90;
const int MIDI_NOTE_OFF = 0x80;

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
	// Schedule note off messages.
	previewDoneTimer = SetTimer(NULL, NULL, PREVIEW_LENGTH, previewDone);
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

// Used to order notes in a chord by pitch.
bool compareNotes(const MidiNote& note1, const MidiNote& note2) {
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
		int chan, pitch, vel;
		MIDI_GetNote(take, note, NULL, NULL, NULL, NULL, &chan, &pitch, &vel);
		notes.push_back({chan, pitch, vel, note});
	}
	sort(notes.begin(), notes.end(), compareNotes);
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

void cmdMidiToggleSelection(Command* command) {
	if (isSelectionContiguous) {
		isSelectionContiguous = false;
		outputMessage("noncontiguous selection");
		return;
	}
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	bool select;
	if (curNoteInChord != -1) {
		// Note in chord.
		MidiNote note = findNoteInChord(take, 0);
		if (note.channel == -1)
			return;
		select = !isNoteSelected(take, note.index);
		selectNote(take, note.index, select);
	} else {
		// Chord.
		auto chord = findChord(take, 0);
		if (chord.first == -1)
			return;
		select = !isNoteSelected(take, chord.first);
		for (int note = chord.first; note <= chord.second; ++note)
			selectNote(take, note, select);
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
		double start = 0;
		int chan, pitch, vel;
		MIDI_GetNote(take, note, NULL, NULL, &start, NULL, &chan, &pitch, &vel);
		if (!cursorSet && direction != 0) {
			start = MIDI_GetProjTimeFromPPQPos(take, start);
			SetEditCurPos(start, true, false);
			cursorSet = true;
		}
		if (select)
			selectNote(take, note);
		notes.push_back({chan, pitch, vel});
	}
	previewNotes(take, notes);
	ostringstream s;
	s << formatCursorPosition(TF_MEASURE) << " ";
	if (!select && !isNoteSelected(take, chord.first))
		s << "unselected" << " ";
	int count = chord.second - chord.first + 1;
	s << count << (count == 1 ? " note" : " notes");
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
	ostringstream s;
	s << getMidiNoteName(take, note.pitch, note.channel);
	if (!select && !isNoteSelected(take, note.index))
		s << " unselected ";
	else
		s << ", ";
	double start, end;
	MIDI_GetNote(take, note.index, NULL, NULL, &start, &end, NULL, NULL, NULL);
	start = MIDI_GetProjTimeFromPPQPos(take, start);
	end = MIDI_GetProjTimeFromPPQPos(take, end);
	double length = end - start;
	s << formatTime(length, TF_MEASURE, true, false, false);
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
	outputMessage(getMidiNoteName(take, pitch, chan));
}

void cmdMidiInsertNote(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	int oldCount = MIDI_CountEvts(take, NULL, NULL, NULL);
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	if (MIDI_CountEvts(take, NULL, NULL, NULL) <= oldCount)
		return; // Not inserted.
	// Play the inserted note.
	int pitch = MIDIEditor_GetSetting_int(editor, "active_note_row");
	int chan = MIDIEditor_GetSetting_int(editor, "default_note_chan");
	int vel = MIDIEditor_GetSetting_int(editor, "default_note_vel");
	previewNotes(take, {{chan, pitch, vel}});
	outputMessage(formatCursorPosition(TF_MEASURE));
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
	int evtx=0;
	int count=0;
	for(;;){
		evtx = MIDI_EnumSelEvts(take, evtx);
		if (evtx == -1)
			break;
		++count;
	}
	ostringstream s;
	s << count << " event" << ((count == 1) ? "" : "s") << " selected";
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
