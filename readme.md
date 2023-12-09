# OSARA: Open Source Accessibility for the REAPER Application

- Author: James Teh &lt;jamie@jantrid.net&gt; & other contributors
- Copyright: 2014-2023 NV Access Limited, James Teh & other contributors
- License: GNU General Public License version 2.0

OSARA is a [REAPER](http://www.reaper.fm/) extension which aims to make REAPER accessible to screen reader users.
It was heavily inspired by and based on the concepts of the abandoned ReaAccess extension.
It runs on both Windows and Mac.

Features:

- Reports information about tracks when you navigate to them
- Reports information concerning track folders
- Reports adjustments to track mute, solo, arm, input monitor, phase and volume
- Reports information about items when you navigate to them
- Reports the edit cursor position when you move by measure, beat, pixel, grid unit, note or to the start or end of the project
- Provides access to various context menus for tracks, items and the time ruler
- Reports track and take envelope selection
- Reports markers when you navigate to them, and optionally reports during project playback
- Facility to adjust automatable FX parameters
- Ability to watch and report track peak meters, and gain reduction from supported FX
- Noncontiguous selection of tracks/items
- Navigation and selection of chords and notes in the MIDI Editor

## Requirements
OSARA requires REAPER 6.44 or later.
The latest stable release of the [SWS/S&M EXTENSION](http://www.sws-extension.org/) is highly recommended, as OSARA supports several useful actions from this extension.

OSARA is tested with NVDA and VoiceOver screen readers during development.
However, on Windows, OSARA uses Microsoft Active Accessibility (MSAA) and UI Automation (UIA) to communicate information, so it should work with any screen reader which supports this correctly.

## Download and Installation
You can download the latest OSARA installer from the [OSARA Development Snapshots](https://osara.reaperaccessibility.com/snapshots/) page.

### Windows
Once you have downloaded the installer, simply run it and follow the instructions.

Note that if you previously copied the OSARA extension into REAPER's program directory manually (before the installer became available), you must remove this first.
The installer installs the extension into your user configuration, not the program directory.

If you wish to completely replace your existing key map with the OSARA key map (recommended to make sure you have the most recent mappings), answer yes when prompted.
If the installer does replace the key map, a safety backup of your existing key map will be made in Reaper's KeyMaps folder.

### Mac
Because OSARA is an extension (not a standalone application) and also needs to install a key map, the installation process is a little different to most Mac applications.
Please follow these instructions to install it:

1. Open the OSARA disk image file you downloaded.
2. Open the "Install OSARA.command" file.
 On macOS Catalina and later, you have to choose open from the context menu, accessed with VO+Shift+M.
3. Follow the instructions.
 If you wish to replace the existing key map with the OSARA key map (which is recommended), answer yes when prompted.
  A backup of your existing key map will be made in Reaper's KeyMaps folder. 
4. The installer leaves a terminal window open.
 It can be closed with Command+Q.
5. Press Command+E to eject the disk image.

### Key Map
Even if you chose not to replace your existing key map with the OSARA key map, the OSARA key map will be copied into your REAPER "KeyMaps" folder so you can import it manually from the Actions dialog later if you wish.
This is particularly useful if you wish to merge new additions with your existing key map, rather than replacing it.
Note that all keyboard commands described in this document assume you are using the OSARA key map.

For users who previously used ReaAccess, the OSARA key map is similar to that provided by ReaAccess, though there are some differences.
You can see the full key map by selecting Key bindings and mouse modifiers from the Help menu.

## Additional Documentation
The sections below document all functionality specific to OSARA.
However, they do not attempt to explain how a screen reader user can best understand and use REAPER.
REAPER is a fairly complex application and most of the existing documentation is very much targeted at sighted users.
For more complete, in-depth documentation and guides on using REAPER with OSARA, please consult the [REAPER Accessibility Wiki](https://reaperaccessibility.com/).

## Usage

### Supported REAPER and Extension Actions
OSARA supports reporting of information for the following actions.
Most of these are actions built into REAPER, but a few are useful actions from the SWS extension.

#### Track Navigation/Management
- Track: Go to next track: DownArrow
- Track: Go to previous track: UpArrow
- Track: Go to next track (leaving other tracks selected): Shift+DownArrow
- Track: Go to previous track (leaving other tracks selected): Shift+UpArrow
- Track: Cycle track folder state: Shift+Enter
- Track: Cycle track folder collapsed state: Enter
- SWS: Select nearest next folder: Alt+PageDown
- SWS: Select nearest previous folder: Alt+PageUp
- Track properties: Toggle free item positioning: Alt+Shift+F2
- Track: Duplicate tracks: Alt+D

#### Adjusting Track Parameters
- Track: Mute/unmute tracks: F5
- Track: Toggle mute for master track: Shift+F5
- Track: Solo/unsolo tracks: F6
- Toggle record arming for current (last touched) track: F7
- Track: Cycle track record monitor: F8
- Track: Invert track phase: F9
- Track: Toggle FX bypass for current track: B
- Track: Toggle FX bypass for master track: Shift+B
- Track: toggle FX bypass on all tracks: Alt+Shift+B
- Track: Nudge track volume up: Alt+UpArrow
- Track: Nudge track volume down: Alt+DownArrow
- (SWS extension) Xenakios/SWS: Nudge volume of selected tracks up: Alt+Shift+UpArrow
- (SWS extension) Xenakios/SWS: Nudge volume of selected tracks down: Alt+Shift+DownArrow
- Track: Nudge master track volume up: Shift+F12
- Track: Nudge master track volume down: Shift+F11
- Track: Nudge track pan left: Alt+LeftArrow
- Track: Nudge track pan right: Alt+RightArrow
- Master track: Toggle stereo/mono (L+R): shift+F9
- Monitoring FX: Toggle bypass: Control+Shift+B
- Track: Unmute all tracks: alt+F5
- Track: Unsolo all tracks: alt+F6
- Track: Unarm all tracks for recording: alt+F7
- Track: Toggle track solo defeat: Control+Win+F6

#### Edit Cursor Movement
- View: Move cursor left one pixel: LeftArrow
- View: Move cursor right one pixel: RightArrow
- Transport: Go to start of project: Control+Home
- Transport: Go to end of project: Control+End
- Move edit cursor to start of next measure: Alt+End
- Move edit cursor to start of current measure: Alt+Home
- Go forward one measure: PageDown
- Go back one measure: PageUp
- Go forward one beat: Control+PageDown
- Go back one beat: Control+PageUp
- View: Move cursor right to grid division: Alt+Shift+RightArrow
- View: Move cursor left to grid division: Alt+Shift+LeftArrow

#### Items
- Item navigation: Select and move to previous item: Control+LeftArrow
- Item navigation: Select and move to next item: Control+RightArrow
- Item: Split items at edit or play cursor: S
- Item: Split items at time selection: Shift+S
- Item edit: Move items/envelope points right: . or NumPad6
- Item edit: Move items/envelope points left: , or NumPad4
- Item edit: Move items/envelope points right by grid size: Alt+Shift+. or Control+Alt+NumPad6
- Item edit: Move items/envelope points left by grid size: Alt+Shift+, or Control+Alt+NumPad4
- Item edit: Grow left edge of items: Control+, or Control+NumPad4
- Item edit: Shrink left edge of items: Control+. or Control+NumPad6
- Item edit: Shrink right edge of items: Alt+, or Alt+NumPad4
- Item edit: Grow right edge of items: Alt+. or Alt+NumPad6
- Item: go to next stretch marker: Control+'
- Item: go to previous stretch marker: Control+;
- Item navigation: Move cursor left to edge of item: Control+Shift+,
- Item navigation: Move cursor right to edge of item: Control+Shift+.
- Item: Select all items in track: Control+Alt+A
- Item: Select all items on selected tracks in current time selection: Alt+Shift+A
- Item grouping: Group items: Control+G
- Item grouping: Remove items from group: Control+Shift+G
- Item grouping: Select all items in groups: Shift+G
- Item: Select all items in current time selection: Control+Shift+A
- (SWS extension) Xenakios/SWS: Select items under edit cursor on selected tracks: Shift+A
- Item properties: Toggle mute: Control+F5
- Item properties: Toggle solo exclusive: Control+F6
- Item properties: Normalize items: Control+Shift+N
- Item properties: Normalize multiple items to common gain: Shift+N
- Item: Open associated project in new tab: Alt+Shift+O
- Xenakios/SWS: Nudge item volume down: Control+Shift+DownArrow
- Xenakios/SWS: Nudge item volume up: Control+Shift+UpArrow
- Item: Set item end to source media end: Control+L
- Item: Remove selected area of items: Control+Win+Delete

#### Takes
- Take: Switch items to next take: T
- Take: Switch items to previous take: Shift+T
- Item properties: Set take channel mode to normal: Control+F7
- Item properties: Set take channel mode to mono (downmix): Control+F8
- Item properties: Set take channel mode to mono (left): Control+F9
- Item properties: Set take channel mode to mono (right): Control+F10
- Xenakios/SWS: Nudge active take volume up: Control+UpArrow
- Xenakios/SWS: Nudge active take volume down: Control+DownArrow
- Item properties: Decrease item rate by ~0.6% (10 cents): Shift+1
- Item properties: Decrease item rate by ~0.6% (10 cents), clear 'preserve pitch': Shift+3
- Item properties: Decrease item rate by ~6% (one semitone), clear 'preserve pitch': Shift+5
- Item properties: Increase item rate by ~0.6% (10 cents): Shift+2
- Item properties: Increase item rate by ~0.6% (10 cents), clear 'preserve pitch': shift+4
- Item properties: Increase item rate by ~6% (one semitone), clear 'preserve pitch': shift+6
- Item properties: Set item rate to 1.0: Control+Alt+Backspace
- Item properties: Pitch item down one cent: Shift+7
- Item properties: Pitch item up one cent: Shift+8
- Item properties: Pitch item down one semitone: shift+9
- Item properties: Pitch item up one semitone: Shift+0
- Item properties: Reset item pitch: Control+Backspace

#### FX
- FX: Toggle delta solo for last focused FX: Shift+F6
- FX: Clear delta solo for all project FX: Alt+Shift+F6
- FX: Show/hide track envelope for last touched FX parameter: Control+Alt+L

#### Markers and Regions
- Markers: Go to previous marker/project start: ;
- Markers: Go to next marker/project end: '
- Markers: Go to marker 01: 1
- Markers: Go to marker 02: 2
- Markers: Go to marker 03: 3
- Markers: Go to marker 04: 4
- Markers: Go to marker 05: 5 
- Markers: Go to marker 06:6
- Markers: Go to marker 07: 7
- Markers: Go to marker 08: 8
- Markers: Go to marker 09: 9
- Markers: Go to marker 10: 0
- Markers: Insert marker at current position: M
- Regions: Go to region 01 after current region finishes playing (smooth seek): Alt+1
- Regions: Go to region 02 after current region finishes playing (smooth seek): Alt+2
- Regions: Go to region 03 after current region finishes playing (smooth seek): Alt+3
- Regions: Go to region 04 after current region finishes playing (smooth seek): Alt+4
- Regions: Go to region 05 after current region finishes playing (smooth seek): Alt+5
- Regions: Go to region 06 after current region finishes playing (smooth seek): Alt+6
- Regions: Go to region 07 after current region finishes playing (smooth seek): Alt+7
- Regions: Go to region 08 after current region finishes playing (smooth seek): Alt+8
- Regions: Go to region 09 after current region finishes playing (smooth seek): Alt+9
- Regions: Go to region 10 after current region finishes playing (smooth seek): Alt+0
- Markers: Insert region from time selection: Shift+R

#### Time Signature/Tempo Markers
- Move edit cursor to previous tempo or time signature change: Shift+;
- Move edit cursor to next tempo or time signature change: Shift+'

#### Automation
- Item edit: Move items/envelope points up one track/a bit: NumPad8
- Item edit: Move items/envelope points down one track/a bit: NumPad2
- (SWS extension) SWS/FNG: Move selected envelope points up: Alt+NumPad8
- (SWS extension) SWS/FNG: Move selected envelope points down: Alt+NumPad2
- Envelope: Delete all selected points: Control+Shift+Delete
- Envelope: Delete all points in time selection: Alt+Shift+Delete
- Envelope: Insert new point at current position (remove nearby points): Shift+E

#### Zoom
- View: Zoom out horizontal: - or NumPad-
- View: Zoom in horizontal: = or NumPad+
- View: Zoom out vertical
- View: Zoom in vertical
- View: Toggle track zoom to minimum height
- View: Toggle track zoom to maximum height

#### Options
- Options: Cycle ripple editing mode: Alt+Shift+P
- Options: Toggle metronome: Control+Shift+M
- Options: Toggle auto-crossfade on/off: Alt+Shift+X
- Options: Toggle locking: L
- (SWS extension) SWS/BR: Options - Cycle through record modes: Alt+\
- Options: Solo in front: Control+Alt+F6
- Pre-roll: Toggle pre-roll on record: Shift+`
- SWS/AW: Toggle count-in before recording: Control+Shift+`
- Transient detection sensitivity: Increase: Alt+Shift+PageUp
- Transient detection sensitivity: Decrease: Alt+Shift+PageDown
- Transient detection threshold: Increase: Control+Shift+PageUp
- Transient detection threshold: Decrease: Control+Shift+PageDown
- Options: Toggle snapping: Alt+N

#### Undo
- Edit: Undo: Control+Z
- Edit: Redo: Control+Shift+Z

#### Transport
- Transport: Toggle repeat: Control+R
- Transport: Increase playrate by ~6% (one semitone): Control+Shift+=
- Transport: Decrease playrate by ~6% (one semitone): Control+Shift+-
- Transport: Increase playrate by ~0.6% (10 cents): Shift+=
- Transport: Decrease playrate by ~0.6% (10 cents): Shift+-
- Transport: Set playrate to 1.0: Control+Shift+Backspace

#### Selection
- Time selection: Set start point: [
- Time selection: Set end point: ]
- Loop points: Set start point: Alt+Shift+[
- Loop points: Set end point: Alt+Shift+]
- Go to start of loop: Alt+Shift+Home
- Go to end of loop: Alt+Shift+End
- Time selection: Remove time selection and loop point selection: Escape
- Unselect all tracks/items/envelope points: Shift+Escape
- Time selection: Nudge left edge left: Control+[
- Time selection: Nudge left edge right: Control+]
- Time selection: Nudge right edge left: Alt+[
- Time selection: Nudge right edge right: Alt+]
- Time selection: Nudge left: Control+Alt+[
- Time selection: Nudge right: Control+Alt+]
- Time selection: Shift left (by time selection length): Shift+[
- Time selection: Shift right (by time selection length): Shift+]
- Go to start of time selection: Home
- Go to end of time selection: End
- Time selection: Swap left edge of time selection to next transient in items: Control+Shift+[
- Time selection: Extend time selection to next transient in items: Control+Shift+]

#### Clipboard
- Edit: Cut items/tracks/envelope points (depending on focus) ignoring time selection: Control+X
- Edit: Cut items/tracks/envelope points (depending on focus) within time selection, if any (smart cut): Control+Shift+X
- Item: Copy selected area of items: Control+Win+C
- Item: Cut selected area of items: Control+Win+X
- Edit: Copy items/tracks/envelope points (depending on focus) ignoring time selection: Control+C
- Edit: Copy items/tracks/envelope points (depending on focus) within time selection, if any (smart copy): Control+Shift+C
- Item: Paste items/tracks: Control+V
- Select all items/tracks/envelope points (depending on focus): Control+A

#### View
- View: Toggle master track visible: Control+Alt+Shift+M
- Toggle fullscreen: F11

#### Grid
- Grid: Set to 1: Control+Shift+1
- Grid: Set to 1/2: Control+Shift+2
- Grid: Set to 1/32: Control+Shift+3
- Grid: Set to 1/4: Control+Shift+4
- Grid: Set to 1/6 (1/4 triplet): Control+Shift+5
- Grid: Set to 1/16: Control+Shift+6
- Grid: Set to 1/24 (1/16 triplet): Control+Shift+7
- Grid: Set to 1/8: Control+Shift+8
- Grid: Set to 1/12 (1/8 triplet): Control+Shift+9
- Grid: Set to 1/48 (1/32 triplet): Control+Shift+0

#### Project Tabs
- Close current project tab: Control+F4
- New project tab: Alt+Shift+N
- Next project tab: Control+Tab
- Previous project tab: Control+Shift+Tab

#### Tempo
- Tempo: Decrease current project tempo 01 BPM: Alt+-
- Tempo: Increase current project tempo 01 BPM: alt+=
- SWS/BR: Options - Toggle "Playback position follows project timebase when changing tempo": Alt+Backspace

#### Alternative Key Map Layers (requires Reaper 7.0 or newer)
- Main action section: Set override to default
- Main action section: Toggle override to recording
- Main action section: Toggle override to alt-1
- Main action section: Toggle override to alt-2
- Main action section: Toggle override to alt-3
- Main action section: Toggle override to alt-4
- Main action section: Toggle override to alt-5
- Main action section: Toggle override to alt-6
- Main action section: Toggle override to alt-7
- Main action section: Toggle override to alt-8
- Main action section: Toggle override to alt-9
- Main action section: Toggle override to alt-10
- Main action section: Toggle override to alt-11
- Main action section: Toggle override to alt-12
- Main action section: Toggle override to alt-13
- Main action section: Toggle override to alt-14
- Main action section: Toggle override to alt-15
- Main action section: Toggle override to alt-16
- Main action section: Momentarily set override to default
- Main action section: Momentarily set override to recording
- Main action section: Momentarily set override to alt-1
- Main action section: Momentarily set override to alt-2
- Main action section: Momentarily set override to alt-3
- Main action section: Momentarily set override to alt-4
- Main action section: Momentarily set override to alt-5
- Main action section: Momentarily set override to alt-6
- Main action section: Momentarily set override to alt-7
- Main action section: Momentarily set override to alt-8
- Main action section: Momentarily set override to alt-19
- Main action section: Momentarily set override to alt-10
- Main action section: Momentarily set override to alt-11
- Main action section: Momentarily set override to alt-12
- Main action section: Momentarily set override to alt-13
- Main action section: Momentarily set override to alt-14
- Main action section: Momentarily set override to alt-15
- Main action section: Momentarily set override to alt-16

#### MIDI Editor
- View: Go to start of file: Control+Home
- View: Go to end of file: Control+End
- Edit: Move edit cursor right one pixel: Alt+RightArrow or Control+Alt+Numpad6
- Edit: Move edit cursor left one pixel: Alt+LeftArrow or Control+Alt+Numpad4
- Navigate: Move edit cursor right by grid: Alt+Shift+RightArrow or Control+Numpad6
- Navigate: Move edit cursor left by grid: Alt+Shift+LeftArrow or Control+Numpad4
- Navigate: Move edit cursor right one measure: PageDown
- Navigate: Move edit cursor left one measure: PageUp
- Edit: Increase pitch cursor one semitone: Alt+UpArrow or Ctrl+NumPad8
- Edit: Decrease pitch cursor one semitone: Alt+DownArrow or Ctrl+NumPad2
- Edit: Increase pitch cursor one octave: Alt+Shift+UpArrow
- Edit: Decrease pitch cursor one octave: Alt+Shift+DownArrow
- Edit: Delete events: Delete
- Edit: Insert note at edit cursor: I or NumPad5
- Edit: Insert note at edit cursor (no advance edit cursor): Shift+I or Shift+NumPad5
- Edit: Select all events: Control+A
- Edit: Select all notes starting in time selection: Control+Shift+A
- Invert selection: Control+I
- Select all notes with the same pitch: Shift+A
- Grid: Set to 1: 1, Control+Shift+1
- Grid: Set to 1/2: 2, Control+Shift+2
- Grid: Set to 1/32: 3, Control+Shift+3
- Grid: Set to 1/4: 4, Control+Shift+4
- Grid: Set to 1/6 (1/4 triplet): 5, Control+Shift+5
- Grid: Set to 1/16: 6, Control+Shift+6
- Grid: Set to 1/24 (1/16 triplet): 7, Control+Shift+7
- Grid: Set to 1/8: 8, Control+Shift+8
- Grid: Set to 1/12 (1/8 triplet): 9, Control+Shift+9
- Grid: Set to 1/48 (1/32 triplet): 0
- Grid: Set to 1/64
- Grid: Set to 1/128
- Set length for next inserted note: 1: Control+1
- Set length for next inserted note: 1/2: Control+2
- Set length for next inserted note: 1/32: Control+3
- Set length for next inserted note: 1/4: Control+4
- Set length for next inserted note: 1/4T: Control+5
- Set length for next inserted note: 1/16: Control+6
- Set length for next inserted note: 1/16T: Control+7
- Set length for next inserted note: 1/8: Control+8
- Set length for next inserted note: 1/8T: Control+9
- Set length for next inserted note: 1/32T: Control+0
- Set length for next inserted note: 1/64
- Set length for next inserted note: 1/128
- Activate next MIDI track: Ctrl+DownArrow
- Activate previous MIDI track: Ctrl+UpArrow
- Edit: Move notes up one semitone: NumPad8 or N
- Edit: Move notes down one semitone: NumPad2 or Shift+N
- Edit: Move notes up one semitone ignoring scale/key: Ctrl+Alt+NumPad8
- Edit: Move notes down one semitone ignoring scale/key: Ctrl+Alt+NumPad2
- Edit: Move notes up one octave: Alt+NumPad8 or Alt+N
- Edit: Move notes down one octave: Alt+NumPad2 or Alt+Shift+N
- Edit: Lengthen notes one pixel: Alt+NumPad3 or Alt+L
- Edit: Shorten notes one pixel: Alt+NumPad1 or Alt+Shift+L
- Edit: Lengthen notes one grid unit: NumPad3 or L
- Edit: Shorten notes one grid unit: NumPad1 or Shift+L
- Edit: Set note lengths to grid size: Control+Shift+L
- Edit: Make notes legato, preserving note start times: G
- Edit: Move notes left one pixel: Alt+NumPad4 or Alt+Shift+P
- Edit: Move notes right one pixel: Alt+NumPad6 or Alt+P
- Edit: Move notes left one grid unit: NumPad4 or Shift+P
- Edit: Move notes right one grid unit: NumPad6 or P
- Edit: Note velocity +01: NumPad9 or V
- Edit: Note velocity +10: Alt+NumPad9 or Alt+V
- Edit: Note velocity -01: NumPad7 or Shift+V
- Edit: Note velocity -10: Alt+NumPad7 or Alt+Shift+V
- Edit: Join notes: J
- Edit: Split notes: S
- Edit: Increase value a little bit for CC events: =
- Edit: Decrease value a little bit for CC events: -
- CC: Next CC lane: Control+Alt+=
- CC: Previous CC lane: Control+Alt+-
- Edit: Toggle selection of all CC events under selected notes: C
- Navigate: Move edit cursor to start of selected events: Control+Shift+Home
- Navigate: Move edit cursor to end of selected events: Control+Shift+End
- View: Toggle snap to grid: Alt+S
- View: Zoom in horizontally: NumPad+, Shift+=
- View: Zoom out horizontally: NumPad-, Shift+-

#### Media Explorer
- Preview: increase volume by 1 dB: Alt+Shift+UpArrow
- Preview: decrease volume by 1 dB: Alt+Shift+DownArrow
- Autoplay: Toggle on/off: Alt+A
- Start on bar: Toggle on/off: Alt+S
- Preview: Toggle repeat on/off: Control+R
- Options: Preserve pitch when tempo-matching or changing play rate: Alt+P
- Preview: reset pitch: Control+Backspace
- Tempo match: Toggle on/off: Alt+T
- Tempo match: /2: Alt+/
- Tempo match: x2: Alt+2
- Dock Media Explorer in Docker: Shift+D
(Note: although the action name doesn't make it clear, this is a toggle. For best screen reader accessibility, you should run this action once to remove Media Explorer from the Docker)

### Context Menus
There are several context menus in REAPER, but some of them are difficult to access or not accessible at all from the keyboard.
OSARA enables keyboard access for the track input, track area, track routing, item, ruler, envelope point and automation item context menus.

To open the context menu for the element you are working with, press the Applications key or Shift+F10 on Windows, or Control+1 on Mac.
For example, if you have just moved to a track, it will open the track input context menu for that track.
If you have just moved the edit cursor, it will open the context menu for the ruler.

For tracks, there are three context menus:

1. Track input: Allows you to set the input to use when recording, etc.
 You access this by pressing the Applications key or Shift+F10 on Windows, or Control+1 on Mac.
2. Track area: Provides options for inserting, duplicating and removing tracks, etc.
 You access this by pressing Control+Applications or Control+Shift+F10 on Windows, or Control+2 on Mac.
3. Routing: Allows you to quickly add and remove sends, receives and outputs without opening the I/O window.
 You access this by pressing Alt+Applications or Control+Alt+Shift+F10 on Windows, or Control+3 on Mac.

### Parameter Lists
OSARA can display a list of parameters for various elements such as tracks, items and effects.
You can then check and change the values of these parameters.
This is useful for parameters which are tedious or impossible to access otherwise.

#### Track/Item Parameters
To access the parameter list for a track or an item, select the track or item you wish to work with.
Then, press Alt+p (OSARA: View parameters for current track/item/FX (depending on focus)).

#### FX Parameters
Many effects are unfortunately either partially or completely inaccessible.
However, most effects make their parameters available for automation in a standard way.
This can also be used to make them at least partially accessible.
Thus, the FX parameter list is particular useful and is the only way to access some effects.

To access it:

1. Select a track or item that has at least one effect instantiated.
 Then, press P (OSARA: View FX parameters for current track/take).
2. Alternatively, to access FX parameters for the master track, press Shift+P (OSARA: View FX parameters for master track).
3. If there is more than one effect instantiated on the track or item, OSARA will present choices in a menu that you can navigate with Up and Down arrows. Hit Enter on the desired effect to reach its parameters.
 If there are input or monitoring FX instantiated on the track, these will also be included in the menu with an appropriate suffix.

You can also access the FX parameter list from the FX chain dialog.
To do this, select the effect in the FX chain list box and then press Alt+P (OSARA: View parameters for current track/item/FX (depending on focus)).

Note that some effects expose easily readable values, while others expose only percentages.
 When effects do expose easily readable values, these will be reported when focus is on the slider. The editable text is an internal number and probably won't correspond to the readable value on the slider.

#### Using Parameter Lists
Once you have opened a parameter list dialog, you can select a parameter from the Parameter combo box and check or adjust its value using the Value slider.
For parameters which support it, there is also an editable text field which allows you to edit the value textually.

The Filter field allows you to narrow the list to only contain parameters which include the entered text.
For example, if the full list contained "Volume" and "Pan" parameters and you type "vol" in the Filter field, the list will be narrowed to only show "Volume".
Clearing the text in the Filter field shows the entire list.

As an alternative to using the Parameter combo box, you can press control+tab or control+shift+tab anywhere in the dialog to move to the next or previous parameter, respectively.

Some effects expose a lot of unnamed parameters which can make finding useful parameters challenging.
The Include unnamed parameters check box may help with this.
When unchecked, unnamed parameters are excluded from the parameter list.
Currently, this means parameters with completely empty names, the single character "-", a name like #001 or a name like P001.

When you are done working with parameters, press the Close button.
Alternatively, you can press enter or escape.

### Reading Current Peaks
OSARA allows you to read the current audio peak for channels 1 and 2 of either the current or master tracks.
You do this using the following actions:

- OSARA: Report level in peak dB at play cursor for channel 1 of current track (reports input level instead when track is armed): J
- OSARA: Report level in peak dB at play cursor for channel 2 of current track (reports input level instead when track is armed): K
- OSARA: Report level in peak dB at play cursor for channel 1 of master track: Shift+J
- OSARA: Report level in peak dB at play cursor for channel 2 of master track: Shift+K

### Peak Watcher
Peak Watcher allows you to be notified automatically when a level exceeds a specified value.
It can also hold the level until it is manually reset or for a specified time, allowing you to catch peaks that might otherwise be missed when manually checking the current peak.
Two "watchers" are provided, enabling you to watch two different levels and configure settings independently.
Beyond simple peak levels, various types of levels are supported for tracks and track effects, including LUFS, RMS and gain reduction.

To use Peak Watcher:

1. Navigate to the track or track effect you want to watch.
 To watch a track effect, open the FX chain for the track and select the desired effect.
2. Press Alt+w (OSARA: Configure Peak Watcher for current track/track FX (depending on focus)).
3. From the context menu, choose which of the two watchers you want to configure.
 If a watcher is already configured, information about the configuration will be included in the menu.
 Choosing a watcher which is already configured will reconfigure the watcher for the track or effect you focused in step 1.
4. From the Level type combo box, select the type of level you want to use: peak dB, several LUFS options, loudness range LU, several RMS options, true peak dBTP or gain reduction dB.
 - Peak dB is measured post-fader.
 - The LUFS, RMS and true peak options use the JS: Loudness Meter Peak/RMS/LUFS (Cockos) effect, which is included with REAPER.
  OSARA will add the effect to tracks automatically and remove it when it is no longer required.
  These levels are measured pre-fader due to the reliance on the JS effect.
 - Gain reduction is only supported for track effects which expose this information.
5. If you are watching a track, you can check the Follow when last touch track changes option to watch whatever track you move to in your project.
6. If you want to be notified when the level of channels exceeds a certain level, in the "Notify automatically for channels:" grouping, check the options for the desired channels and enter the desired level.
7. The Hold level grouping allows you to specify whether the highest level (or lowest level for some level types) remains as the reported level and for how long.
 Holding the highest/lowest level gives you time to examine the level, even if the audio level changed immediately after the highest/lowest level occurred.
 There are three options:
 - disabled: Don't hold the level at all.
 - until reset: Hold the level until the Peak Watcher is reset.
 - for (ms): Allows you to specify a time in milliseconds for which the level will be held.
8. Press the Reset button to reset the reported peak levels if they are being held.
9. When you are done, press the OK button to accept any changes or the Cancel button to discard them.
10. Alternatively, you can press the Disable button to disable this watcher.
 If you have configured another watcher, that watcher will continue to watch levels.

At any time, you can report or reset the levels for either of the watchers using the following actions:

- OSARA: Report Peak Watcher value for first watcher first channel: Alt+F11
- OSARA: Report Peak Watcher value for first watcher second channel: Alt+F12
- OSARA: Report Peak Watcher value for second watcher first channel: Alt+Shift+F11
- OSARA: Report Peak Watcher value for second watcher second channel: Alt+Shift+F12
- OSARA: Reset Peak Watcher first watcher: Alt+F10
- OSARA: Reset Peak Watcher second watcher: Alt+Shift+F10

You can also quickly pause Peak Watcher using OSARA: Pause/resume Peak Watcher.
While paused, Peak Watcher won't notify you of any level changes.
You can later use the same action again to resume automatic reporting.

Peak Watcher settings are saved with the project.
To configure default settings to use for new projects, you can save them in a project template and configure this template to be used as the template for new projects in the Project section of REAPER Preferences.

### Shortcut Help
It is possible to have REAPER list all shortcuts and to search for individual shortcuts in the Action List.
However, it is sometimes convenient to be able to simply press a shortcut and immediately learn what action it will perform.
This is especially useful if you have forgotten an exact shortcut but do have some idea of what it might be.
You can achieve this using OSARA's shortcut help mode.

You can turn shortcut help on and off by pressing F12 (OSARA: Toggle shortcut help).
While shortcut help is enabled, pressing any shortcut will report the action associated with that shortcut, but the action itself will not be run.

### Noncontiguous Selection
Usually, selection is done contiguously; e.g. you might select tracks 1 through 4.
Sometimes, it is desirable to select noncontiguously; e.g. you might want to select tracks 1, 3 and 5.

You can do this as follows:

1. Move to the track or item you want to start with.
2. Optionally, select some other contiguous tracks or items.
3. Press Shift+Space (OSARA: Enable noncontiguous selection/toggle selection of current track/item) to switch to noncontiguous selection.
4. Move through tracks/items leaving other tracks/items selected; e.g. using Shift+DownArrow and Shift+UpArrow to move through tracks.
 These tracks/items will not be selected automatically as you move over them, but any previously selected tracks/items will remain selected.
5. When you reach a track/item you want to select, press Shift+Space (OSARA: Enable noncontiguous selection/toggle selection of current track/item).
 You can also use this if you want to unselect a previously selected track/item.

Selection will automatically revert to contiguous selection the next time you move to a track/item without leaving other tracks/items selected.

If you want to select noncontiguous items on several different tracks, the procedure is exactly the same.
However, it's important to remember that you must move between tracks without affecting the selection; i.e. using Shift+DownArrow and Shift+UpArrow.
Otherwise, selection will revert to contiguous selection.

### Accessing Controls for Sends/Receives/Outputs in the Track I/O Window
In the Track I/O window, there are various controls for each send, receive or hardware output.
Unfortunately, these controls cannot be reached with the Tab key and it is tedious at best to access these with screen reader review functions.

When you tab to the Delete button for a send/receive/output, the name of the send/receive/output will first be reported.
You can then press the Applications key or Shift+F10 to access a menu of additional options.

### Manually Moving Stretch Markers
REAPER includes actions to snap stretch markers to the grid.
However, sometimes, this is not sufficient and it is useful to be able to manually move stretch markers to a specific position.

To do this:

1. Select the desired item.
2. Go to a stretch marker; e.g. using Control+' (Item: go to next stretch marker).
 Ensure that OSARA reports the stretch marker.
3. Move the edit cursor to the position to which you wish to move the stretch marker.
4. Press Control+Alt+M (OSARA: Move last focused stretch marker to current edit cursor position).

### Accessing FX Plug-in Windows
Some FX plug-ins can be controlled with keyboard commands, but you can't reach them by tabbing through the FX Chain dialog.
In these cases, you can press F6 when focus is in the FX chain list to have OSARA attempt to focus the plug-in window.

### Automation Items
An envelope can contain one or more automation items positioned along the timeline.
With OSARA, you move to automation items as follows:

1. Select an envelope using Alt+L or Alt+Shift+L (OSARA: Select next track/take envelope (depending on focus)).
2. Now, use the normal item navigation commands; i.e. Control+RightArrow and Control+LeftArrow (Item navigation: Select and move to next item, Item navigation: Select and move to previous item).
 Multiple selection is also possible using Control+Shift+RightArrow and Control+Shift+LeftArrow (OSARA: Move to next item (leaving other items selected), OSARA: Move to previous item (leaving other items selected).
 Noncontiguous selection is done the same way described above for tracks and items.
3. The item navigation commands will revert back to moving through media items (instead of automation items) when focus is moved away from the envelope.
 For example, moving to another track and back again will again allow you to move through the media items on the track.

Once you move to an automation item, the commands to move between envelope points such as Alt+K and Alt+J (OSARA: Move to next envelope point, OSARA: Move to previous envelope point) move between the points in the automation item.
The points within an automation item can only be accessed after moving to that automation item; they cannot be accessed from the underlying envelope.
To return to the points in the underlying envelope, simply move focus back to the envelope by selecting it again with Alt+L or Alt+Shift+L (OSARA: Select next track/take envelope (depending on focus), OSARA: Select previous track/take envelope (depending on focus)).

### Notes and Chords in the MIDI Editor
In the MIDI Editor, OSARA enables you to move between chords and to move to individual notes in a chord.
In this context, a chord is any number of notes that are placed at the exact same position.
If there is only one note at a given position, it will be treated as a chord.

You move between chords using the Left and Right arrow keys (OSARA: Move to previous chord, OSARA: Move to next chord).
When you move to a chord, the edit cursor will be placed at the chord and the new position will be reported.
The notes of the chord will be previewed and the number of notes will be reported.
The notes in the chord are also selected so you can manipulate the entire chord.
For example, pressing Delete will delete the chord.

Once you have moved to a chord, you can move to individual notes using the Up and Down arrow keys (OSARA: Move to previous note in chord, OSARA: Move to next note in chord).
When you move to a note, that note will be previewed and its name will be reported.
The single note will also be selected so you can manipulate just that note.
For example, pressing Delete will delete only that note.

When a chord or a note within a chord is being previewed, you can cancel the note preview by pressing the Control key.

You can select multiple chords or multiple notes in a chord.
To do this, first move to the first chord or note you want to select.
Then, use Shift+DownArrow or Shift+UpArrow to add the next or previous chord or note to the selection.
For example, if you've moved to a chord and also want to add the next chord to the selection, you would press Shift+RightArrow.

Noncontiguous selection is also possible.
You do this in the same way described above for tracks and items.
That is, press Shift+Space (OSARA: Enable noncontiguous selection/toggle selection of current chord/note) to switch to noncontiguous selection, move to other chords/notes with Shift+Arrows and press Shift+Space to select/unselect the current chord/note.

### Note Preview in the MIDI Event List (Windows Only)
When the MIDI Editor is set to Event List mode, REAPER presents a list with all the events in the current MIDI item.
When a note gets focus in the list, OSARA will play a preview of the focused note.
To cancel a note preview, press the Control key.

### Navigating FX Presets Without Activating Them (Windows Only)
REAPER's FX preset combo box doesn't allow keyboard users to move through presets without activating them.
Sometimes, you need to be able to examine or search the available presets without activating each one.
OSARA provides a dialog to facilitate this.

You activate this dialog by pressing Alt+DownArrow when focused on REAPER's FX preset combo box.
The dialog displays the preset list and allows you to navigate without presets being automatically activated.
The Filter field allows you to filter the list to show only presets containing the entered text.
Pressing the OK button activates the preset that's currently selected in the list.

### Configuration
OSARA provides a Configuration dialog to adjust various settings.
You can open this dialog by pressing Control+F12 or Control+Alt+Shift+P (OSARA: Configuration).

The dialog contains the following options:

- Report position when scrubbing: When disabled, OSARA will not report the cursor position when using the scrubbing actions (View: Move cursor left/right one pixel).
 The cursor position also won't be reported when moving to chords in the MIDI editor (OSARA: Move to previous/next chord).
- Report time movement during playback/recording: When disabled, OSARA will not report actions during playback/recording which move the cursor or adjust positions or lengths.
 This includes item navigation.
 Although not strictly time movement, this also includes inserting markers or regions.
- Report full time for time movement commands: When enabled, time movement commands such as moving by bar/beat will report the full time, even if a portion of that report has not changed since the last movement.
 For example, if you are at bar 1 beat 1 0% and you move one beat forward, OSARA will report "bar 1 beat 2 0%", even though only the beat changed.
 When this is disabled, OSARA will only report the part of the time that changed.
 Using the same example, OSARA would report only "beat 2".
- Move relative to the play cursor for time movement commands during playback: When enabled, time movement commands such as scrubbing or moving by bar/beat will move from where you are currently playing, rather than relative to the edit cursor.
- Report markers and regions during playback: When enabled, project markers and regions will be reported during playback as the cursor passes them.
- Report transport state (play, record, etc.): When enabled, OSARA will report the transport state when you change it; e.g. if you begin playing or recording.
- Report track numbers: When enabled, OSARA will report track numbers as well as track names.
 When disabled, track numbers will not be reported except for unnamed tracks.
- Report FX when moving to tracks/takes: When enabled, OSARA will report the names of any effects on a track or take when you move to it.
- Report position when navigating chords in MIDI editor: When enabled, OSARA will report the cursor position as you move through chords in the piano roll.
- Report MIDI notes in MIDI editor: When enabled, OSARA will report the names of individual MIDI notes and the number of notes in a chord.
- Report changes made via control surfaces: When enabled, OSARA will report track selection changes, parameter changes, etc. made using a control surface.

When you are done, press the OK button to accept any changes or the Cancel button to discard them.

There is also an action to toggle each setting; e.g. OSARA: Toggle Report position when scrubbing.
These do not have keyboard shortcuts mapped by default, but you can add shortcuts in the Actions dialog (hit F4 to go there).

### Miscellaneous Actions
OSARA also includes some other miscellaneous actions.

#### Main section of actions list
- OSARA: go to first track: Control+Alt+Home
- OSARA: go to last track: Control+Alt+End
- OSARA: Move to next item (leaving other items selected): Control+Shift+RightArrow
- OSARA: Move to previous item (leaving other items selected): Control+Shift+LeftArrow
- OSARA: View properties for current media item/take/automation item (depending on focus): Shift+F2
- OSARA: Report ripple editing mode: Control+Shift+P
- OSARA: Report muted tracks: Alt+Shift+F5
 - Pressing this twice will display the information in a dialog with a text box for easy review.
- OSARA: Report soloed tracks: Alt+Shift+F6
 - Pressing this twice will display the information in a dialog with a text box for easy review.
- OSARA: Report record armed tracks: Alt+Shift+F7
 - Pressing this twice will display the information in a dialog with a text box for easy review.
- OSARA: Report tracks with record monitor on: Control+Shift+F8
 - Pressing this twice will display the information in a dialog with a text box for easy review.
- OSARA: Unmonitor all tracks: Alt+F8
- OSARA: Report tracks with phase inverted: Alt+Shift+F9
 - Pressing this twice will display the information in a dialog with a text box for easy review.
- OSARA: Set phase normal for all tracks: Alt+F9
- OSARA: Report track/item/time selection (depending on focus): Control+Shift+Space
 - Pressing this twice will display the information for all selections (not just the focus) in a dialog with a text box for easy review.
- OSARA: Select from cursor to start of project: Shift+Home
- OSARA: Select from cursor to end of project: Shift+End
- OSARA: Remove items/tracks/contents of time selection/markers/envelope points (depending on focus): Delete
- OSARA: Report edit/play cursor position and transport state: Control+Shift+J
 - If the ruler unit is set to Measures.Beats / Minutes:Seconds, Pressing this once will report the time in measures.beats, while pressing it twice will report the time in minutes:seconds .
- OSARA: Delete all time signature markers: Alt+Win+Delete
- OSARA: Toggle track/take volume envelope visibility (depending on focus): Control+Alt+V
- OSARA: Toggle track/take pan envelope visibility (depending on focus): Control+Alt+P
- OSARA: Toggle track/take mute envelope visibility (depending on focus): Control+Alt+F5
- OSARA: Toggle track pre-FX pan or take pitch envelope visibility (depending on focus): Control+Alt+Shift+P
- OSARA: Select next track/take envelope (depending on focus): Alt+L
- OSARA: Select previous track/take envelope (depending on focus): Alt+Shift+L
- OSARA: Move to next envelope point: Alt+K
- OSARA: Move to previous envelope point: Alt+J
- OSARA: Move to next envelope point (leaving other points selected): Alt+Shift+K
- OSARA: Move to previous envelope point (leaving other points selected): Alt+Shift+J
- OSARA: Cycle shape of selected envelope points: Control+Alt+J
- OSARA: Move to next transient: Tab
- OSARA: Move to previous transient: Shift+Tab
- OSARA: Cycle automation mode of selected tracks: Control+Shift+\
- OSARA: Report global / Track Automation Mode: Control+Shift+L
- OSARA: Toggle global automation override between latch preview and off: Control+Alt+Shift+L
- OSARA: Cycle through midi recording modes of selected tracks: Alt+Shift+\
- OSARA: Report groups for current track
- OSARA: Report regions, last project marker and items on selected tracks at current position: Control+Shift+R
 - Pressing this twice will display the information in a dialog with a text box for easy review.
- OSARA: About: Control+F1

#### Midi Editor section of actions list
- OSARA: Move to next midi item on track: Control+RightArrow
- OSARA: Move to previous midi item on track: Control+LeftArrow
- OSARA: Move to next CC: Control+=
- OSARA: Move to previous CC: Control+-
- OSARA: Move to next CC and add to selection: Control+Shift+=
- OSARA: Move to previous CC and add to selection: Control+Shift+-
- OSARA: Select all notes with the same pitch starting in time selection: Alt+Shift+A

#### MIDI Event List Editor section of actions list
- OSARA: Focus event nearest edit cursor: Control+F

### Actions that report, but are not directly mapped to keyboard shortcuts
OSARA will report any native REAPER or SWS toggle action that exposes its state with a generic reporting style.
The actions listed in this section are ones where some effort has been made for reports to be more dynamic, or productivity has been refined in some way.
While these actions aren't directly on the key map, typically they're used as part of providing context sensitive workflows, custom or cycle actions instead.
This list is worth referencing when making your own key map additions, assigning actions to control surfaces where reporting is desirable or assembling custom actions, as everything included here should report productively.

#### Unmapped in Main section
- Envelope: Cut points within time selection
- Envelope: Insert new point at current position (do not remove nearby points)
- Envelope: Select all points
- Global automation override: All automation in latch mode
- Global automation override: All automation in latch preview mode
- Global automation override: All automation in read mode
- Global automation override: All automation in touch mode
- Global automation override: All automation in trim/read mode
- Global automation override: All automation in write mode
- Global automation override: Bypass all automation
- Global automation override: No override (set automation modes per track)
- Grid: Set to 1/64
- Grid: Set to 1/128
- Item: Remove items
- Item: Copy loop of selected area of audio items
- Edit: Cut items
- Item properties: Pitch item down one octave
- Item properties: Pitch item up one octave
- Item properties: Toggle take preserve pitch
- Item properties: Clear take preserve pitch
- Item properties: Set take preserve pitch
- Item properties: Decrease item rate by ~6% (one semitone)
- Item properties: Increase item rate by ~6% (one semitone)
- Item properties: Set item rate from user-supplied source media tempo/bpm...
- Take: Nudge active takes volume +1dB
- Take: Nudge active takes volume -1dB
- Item: Nudge items volume +1dB
- Item: Nudge items volume -1dB
- Item: remove stretch marker at current position
- Item: Set cursor to next take marker in selected items
- Item: Set cursor to previous take marker in selected items
- Item: Delete take marker at cursor
- Item: Delete all take markers
- Markers: Delete time signature marker near cursor
- Markers: Delete marker near cursor
- Markers: Delete region near cursor
- New project tab (ignore default template)
- Options: Envelope points move with media items
- Tempo: Decrease current project tempo 0.1 BPM
- Tempo: Decrease current project tempo 10 BPM
- Tempo: Decrease current project tempo 10 percent
- Tempo: Decrease current project tempo 50 percent (half)
- Tempo: Increase current project tempo 0.1 BPM
- Tempo: Increase current project tempo 10 BPM
- Tempo: Increase current project tempo 10 percent
- Tempo: Increase current project tempo 100 percent (double)
- Time selection: Remove contents of time selection (moving later items)
- Track: Insert new track
- Track: Select all tracks
- Track: Toggle mute for selected tracks
- Track: Toggle record arm for selected tracks
- Track: Remove tracks
- Track: Cut tracks
- Xenakios/SWS: Nudge master volume 1 dB up
- Xenakios/SWS: Nudge master volume 1 dB down
- Xenakios/SWS: Set master volume to 0 dB
-- Track: Toggle track volume envelope visible
- Track: Toggle track pre-FX volume envelope visible
- Track: Toggle track pan envelope visible
- Track: Toggle track pre-FX pan envelope visible
- Track: Toggle track mute envelope visible
- Transport: Toggle preserve pitch in audio items when changing master playrate
- Transport: Play/stop (move edit cursor on stop)
- View: Set horizontal zoom to default project setting
- View: Time unit for ruler: Absolute frames
- View: Time unit for ruler: Hours:Minutes:Seconds:Frames
- View: Time unit for ruler: Measures.Beats
- View: Time unit for ruler: Measures.Beats / minutes:Seconds
- View: Time unit for ruler: Minutes:Seconds
- View: Time unit for ruler: Samples
- View: Time unit for ruler: Seconds
- View: Secondary time unit for ruler: Absolute frames
- View: Secondary time unit for ruler: Hours:Minutes:Seconds:Frames
- View: Secondary time unit for ruler: Minutes:Seconds
- View: Secondary time unit for ruler: None
- View: Secondary time unit for ruler: Samples
- View: Secondary time unit for ruler: Seconds

#### Unmapped in MIDI Editor section
- Edit: Select all notes in time selection
- Options: MIDI inputs as step input mode
- Options: F1-F12 as step input mode

#### Unmapped OSARA actions
- OSARA: Pause/resume Peak Watcher
- OSARA: Toggle Move relative to the play cursor for time movement commands during playback
- OSARA: Toggle Report FX when moving to tracks/takes
- OSARA: Toggle Report MIDI notes in MIDI editor
- OSARA: Toggle Report changes made via control surfaces
- OSARA: Toggle Report full time for time movement commands
- OSARA: Toggle Report markers during playback
- OSARA: Toggle Report position when navigating chords in MIDI editor
- OSARA: Toggle Report position when scrubbing
- OSARA: Toggle Report time movement during playback/recording
- OSARA: Toggle Report track numbers
- OSARA: Toggle Report transport state (play, record, etc.)
- OSARA: Select from cursor to start of project

### Muting OSARA Messages in Custom/Cycle Actions
The action "OSARA: Mute next message from OSARA" can be used in custom/cycle actions to mute OSARA feedback for the next action.
It should be placed before each action to be muted.
If using this as part of custom actions that you'll be distributing, please test thoroughly to ensure that its placement won't suppress important reports.

## Support
If you need help, please subscribe to the [Reapers Without Peepers discussion group](https://groups.io/g/rwp) and ask your questions there.

## Reporting Issues
Issues should be reported [on GitHub](https://github.com/jcsteh/osara/issues).

## Translating
This section is for those interested in translating OSARA into their language.

OSARA can be translated using gettext PO files.
If a REAPER language pack is installed, OSARA will attempt to load a translation based on the name of the REAPER language pack.
OSARA can only load PO files in the UTF-8 encoding.

Translations are managed on [Crowdin](https://crowdin.com/project/osara).
With Crowdin, you can translate entirely online on the web, upload and download translations as .po files or directly sync your translations using Poedit.

If you'd like to translate OSARA, you will first need to [sign up for a Crowdin account](https://accounts.crowdin.com/register).
Then, please [file a GitHub issue](https://github.com/jcsteh/osara/issues/new) and include these details:

- Your Crowdin username;
- The language you'd like to translate to; and
- The file name of the REAPER language pack (or packs) for your language.

If you'd like to test your translation before it is included in OSARA, you can copy it into the osara/locale folder inside your REAPER resource folder .
If you don't know where the REAPER resource folder is, use the Show REAPER resource path action in REAPER.
The file should be named with the same name as the REAPER language pack you are using, but with a .po extension.
For example, if you have installed German.ReaperLangpack, rename the file to German.po.
You will need to restart REAPER for any translation changes to take effect.

## Building
This section is for those interested in building OSARA from source code.

### Getting the Source Code
The OSARA Git repository is located at https://github.com/jcsteh/osara.git.
You can clone it with the following command, which will place files in a directory named osara:

```
git clone --recursive https://github.com/jcsteh/osara.git
```

The `--recursive` option is needed to retrieve Git submodules we use.
If you didn't pass this option to `git clone`, you will need to run `git submodule update --init`.
Whenever a required submodule commit changes (e.g. after git pull), you will need to run `git submodule update`.
If you aren't sure, run `git submodule update` after every git pull, merge or checkout.

### Dependencies
To build OSARA, you will need:

- Several git submodules used by OSARA.
	See the note about submodules in the previous section.

#### Windows
- [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)

	Visual Studio 2022 Community/Professional/Enterprise is also supported.
	However, Preview versions of Visual Studio will not be detected and cannot be used.

	Whether installing Build Tools or Visual Studio, you must enable the following:

	* In the list on the Workloads tab, in the Windows grouping: Desktop development with C++
	* Then in the Installation details tree view, under Desktop development with C++ > Optional:
		- C++ ATL for latest v143 build tools (x86 & x64)
		- C++ Clang tools for Windows
		- Windows 11 SDK (10.0.22000.0)

- [Python](https://www.python.org/downloads/), version 3.7 or later:
- [SCons](https://www.scons.org/), version 3.0.4 or later:
	* Once Python is installed, you should be able to install SCons by running this at the command line:

	`py -3 -m pip install scons`

- [NSIS](https://nsis.sourceforge.io/Download), version 3.03 or later

#### Mac OS
- Xcode 13: download from the [Mac App Store](https://apps.apple.com/us/app/xcode/id497799835?ls=1&mt=12).
	* Please run `xcode` at least once to make sure the latest command line tools are installed on your system.
- Homebrew: download and install using the instructions at the [Homebrew website](http://brew.sh)
	* Verify the installation with the `brew doctor` and `brew update` commands.
- download and install `python`, `scons` and `php` using the `brew install` command.
	* Note that `php` may already be installed on versions of Mac OS below 12.x, in which case you may not need to install `php` from the Homebrew repository.

### How to Build
To build OSARA, from a command prompt, simply change to the OSARA checkout directory and run `scons`.
The resulting installer can be found in the installer directory.

## Contributors
- NV Access Limited
- James Teh
- Victor Tsaran
- Scott Chesworth
- Derek Lane
- Gianluca Apollaro
- Marc Mulcahy
- Garth Humphreys
- Lars Sönnebo
- Alexey Zhelezov
- Leonard de Ruijter
- Robbie Murray
- Glen Gordon
- James Scholes
- Christian Fillion
- Jenny K Brennan
