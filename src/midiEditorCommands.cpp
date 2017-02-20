/*
 * OSARA: Open Source Accessibility for the REAPER Application
 * MIDI Editor commands code
 * Author: James Teh <jamie@nvaccess.org>
 * Copyright 2015-2017 NV Access Limited
 * License: GNU General Public License version 2.0
 */

#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include "osara.h"
#ifdef _WIN32
#include <Commctrl.h>
#endif

using namespace std;

typedef struct {
	int channel;
	int pitch;
	int velocity;
} MidiNote;
vector<MidiNote> previewingNotes; // Notes currently being previewed.
UINT_PTR previewDoneTimer = 0;
const int PREVIEW_LENGTH = 300;
const int MIDI_NOTE_ON = 0x90;
const int MIDI_NOTE_OFF = 0x80;

// Called to turn off notes currently being previewed,
// either by a timer once the preview length is reached
// or directly if interrupted by another preview.
VOID CALLBACK previewDone(HWND hwnd, UINT msg, UINT event, DWORD time) {
	if (event != previewDoneTimer)
		return; // Cancelled.
	// Send note off messages for the notes just previewed.
	for (auto note = previewingNotes.cbegin(); note != previewingNotes.cend(); ++note)
		StuffMIDIMessage(0, MIDI_NOTE_OFF | note->channel, note->pitch, note->velocity);
	previewingNotes.clear();
	previewDoneTimer = 0;
}

void previewNotes(const vector<MidiNote>& notes) {
	if (previewDoneTimer) {
		// Notes are currently being previewed. Interrupt them.
		// We want to turn off these notes immediately.
		KillTimer(NULL, previewDoneTimer);
		previewDone(NULL, NULL, previewDoneTimer, 0);
	}
	// Send note on messages.
	for (auto note = notes.cbegin(); note != notes.cend(); ++note)
		StuffMIDIMessage(0, MIDI_NOTE_ON | note->channel, note->pitch, note->velocity);
	previewingNotes = notes;
	// Schedule note off messages.
	previewDoneTimer = SetTimer(NULL, NULL, PREVIEW_LENGTH, previewDone);
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
int currentNote = -1;

const bool bTrue = true;
void moveToChord(int direction) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	auto chord = findChord(take, direction);
	if (chord.first == -1)
		return;
	currentNote = -1;
	MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
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
		// Select this note.
		MIDI_SetNote(take, note, &bTrue, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		notes.push_back({chan, pitch, vel});
	}
	previewNotes(notes);
	ostringstream s;
	s << formatCursorPosition() << " ";
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

void moveToNoteInChord(int direction) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	auto chord = findChord(take, 0);
	if (chord.first == -1)
		return;
	if (chord.first <= currentNote && currentNote <= chord.second) {
		// Already on a note within the chord. Move to the next/previous note.
		currentNote += direction;
		// If we were already on the first/last note, stay there.
		if (currentNote < chord.first || currentNote > chord.second)
			currentNote -= direction;
	} else {
		// We're moving into a new chord. Move to the first/last note.
		currentNote = direction == 1 ? chord.first : chord.second;
	}
	// Select this note.
	MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
	MIDI_SetNote(take, currentNote, &bTrue, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	int chan, pitch, vel;
	MIDI_GetNote(take, currentNote, NULL, NULL, NULL, NULL, &chan, &pitch, &vel);
	previewNotes({{chan, pitch, vel}});
	outputMessage(getMidiNoteName(pitch));
}

void cmdMidiMoveToNextNoteInChord(Command* command) {
	moveToNoteInChord(1);
}

void cmdMidiMoveToPreviousNoteInChord(Command* command) {
	moveToNoteInChord(-1);
}

void cmdMidiMovePitchCursor(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	int pitch = MIDIEditor_GetSetting_int(editor, "active_note_row");
	int chan = MIDIEditor_GetSetting_int(editor, "default_note_chan");
	int vel = MIDIEditor_GetSetting_int(editor, "default_note_vel");
	previewNotes({{chan, pitch, vel}});
	outputMessage(getMidiNoteName(pitch));
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
	previewNotes({{chan, pitch, vel}});
	outputMessage(formatCursorPosition());
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
#endif // _WIN32
