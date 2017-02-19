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
const int PREVIEW_LENGTH = 250;
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

void cmdMidiMoveToNote(Command* command) {
	HWND editor = MIDIEditor_GetActive();
	MediaItem_Take* take = MIDIEditor_GetTake(editor);
	double now = GetCursorPosition();

	bool selAtCur = false;
	int note = -1;
	double start;
	while ((note = MIDI_EnumSelNotes(take, note)) != -1) {
		MIDI_GetNote(take, note, NULL, NULL, &start, NULL, NULL, NULL, NULL);
		start = MIDI_GetProjTimeFromPPQPos(take, start);
		if (start == now) {
			selAtCur = true;
			break;
		}
	}

	if (!selAtCur) {
		// There are selected notes which aren't at the cursor.
		// In this case, these actions move relative to these selected notes,
		// but we want to move relative to the edit cursor.
		// Clear the selection to make these actions use the edit cursor.
		MIDIEditor_OnCommand(editor, 40214); // Edit: Unselect all
		// Move back a tiny bit so notes right at our start position are always treated as next.
		// SetEditCurPos isn't respected here for some reason.
		MIDIEditor_OnCommand(editor, 40185); // Edit: Move edit cursor left one pixel
	}

	MIDIEditor_OnCommand(editor, command->gaccel.accel.cmd);
	note = MIDI_EnumSelNotes(take, -1);
	if (note == -1) {
		// We might have moved the edit cursor.
		SetEditCurPos(now, true, false);
		return;
	}
	int pitch;
	MIDI_GetNote(take, note, NULL, NULL, &start, NULL, NULL, &pitch, NULL);
	start = MIDI_GetProjTimeFromPPQPos(take, start);
	SetEditCurPos(start, false, false);
	ostringstream s;
	s << getMidiNoteName(pitch) << " " << formatCursorPosition();
	outputMessage(s);
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