# OSARA: Open Source Accessibility for the REAPER Application

- Author: James Teh &lt;jamie@jantrid.net&gt; & other contributors
- Copyright: 2014-2019 NV Access Limited, James Teh & other contributors
- License: GNU General Public License version 2.0

OSARA is a [REAPER](http://www.reaper.fm/) extension which aims to make REAPER accessible to screen reader users.
It was heavily inspired by and based on the concepts of the ReaAccess extension, but was created as a potential replacement because ReaAccess seems to have been abandoned and was not developed openly.
It runs on both Windows and Mac, though it is currently very experimental on Mac ad is not yet fully functional.

Features:

- Reports information about tracks when you navigate to them
- Reports information concerning track folders
- Reports adjustments to track mute, solo, arm, input monitor, phase and volume
- Reports information about items when you navigate to them
- Reports the edit cursor position when you move by pixel, measure, beat or to the start or end of the project
- Provides access to various context menus for tracks, items and the time ruler
- Reports track envelope selection
- Reports markers when you navigate to them
- Facility to adjust of automatable FX parameters
- Ability to watch and report track peak meters
- Noncontiguous selection of tracks/items
- Navigation and selection of chords and notes in the MIDI Editor

## Requirements
OSARA requires REAPER 5.92 or later.
The [SWS/S&M EXTENSION](http://www.sws-extension.org/) is highly recommended and OSARA supports several useful actions from this extension.

It has only been tested with the NVDA and VoiceOver screen readers.
However, on Windows, OSARA uses Microsoft Active Accessibility (MSAA) to communicate information, so it should work with any screen reader which supports this correctly.

## Download and Installation
You can download the latest OSARA installer from the [OSARA Development Snapshots](https://osara.reaperaccessibility.com/snapshots/) page.

### Windows
Once you have downloaded the installer, simply run it and follow the instructions.

Note that if you previously copied the OSARA extension into REAPER's program directory manually (before the installer became available), you must remove this first.
The installer installs the extension into your user configuration, not the program directory.

By default, the OSARA key map will be installed, completely replacing your existing key map.
If you do not wish this to occur, you can uncheck the "Replace existing key map with OSARA key map" option.

### Mac
Because OSARA is an extension (not a standalone application) and also needs to install a key map, the installation process is a little different to most Mac applications.
Please follow these instructions to install it:

1. Open the OSARA disk image file you downloaded.
2. Open the "Install OSARA extension.command" file.
 This will open a terminal window.
 Wait a few seconds and then press command+q to close the terminal window.
3. If you wish to replace your existing key map with the OSARA key map (which is recommended), open the "Replace existing key map with OSARA key map.command" file.
 This will open another terminal window.
 Wait a few seconds and then press command+q to close the terminal window.
4. Press command+e to eject the disk image.

### Key Map
Even if you chose not to replace your existing key map with the OSARA key map, the OSARA key map will be copied into your REAPER "KeyMaps" folder so you can import it manually from the Actions dialog later if you wish.
This is particularly useful if you wish to merge the key map with your existing key map, rather than replacing it.
Note that any keyboard commands described in this document assume you are using the OSARA key map.

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
Most of these are actions built into REAPER, but a few are very useful actions from the SWS extension.

#### Track Navigation/Management
- Track: Go to next track: DownArrow
- Track: Go to previous track: UpArrow
- Track: Go to next track (leaving other tracks selected): Shift+DownArrow
- Track: Go to previous track (leaving other tracks selected): Shift+UpArrow
- Track: Insert new track
- Track: Cycle track folder state: Shift+Enter
- Track: Cycle track folder collapsed state: Enter
- Track: Remove tracks

#### Adjusting Track Parameters
- Track: Mute/unmute tracks: F5
- Track: Solo/unsolo tracks: F6
- Toggle record arming for current (last touched) track: F7
- Track: Cycle track record monitor:F8
- Track: Invert track phase: F9
- Track: Toggle FX bypass for current track: B
- Track: Toggle FX bypass for master track: Shift+B
- Track: toggle FX bypass on all tracks: Control+B
- Track: Nudge track volume up: Alt+UpArrow
- Track: Nudge track volume down: Alt+DownArrow
- (SWS extension) Xenakios/SWS: Nudge volume of selected tracks up: Control+Shift+UpArrow
- (SWS extension) Xenakios/SWS: Nudge volume of selected tracks down: Control+Shift+DownArrow
- Track: Nudge master track volume up: Alt+Shift+UpArrow
- Track: Nudge master track volume down: Alt+Shift+DownArrow
- Track: Nudge track pan left: Alt+Leftarrow
- Track: Nudge track pan right: Alt+Rightarrow
- Master track: Toggle stereo/mono (L+R): shift+f9
- Monitoring FX: Toggle bypass: Control+Shift+B

#### Edit Cursor Movement
- View: Move cursor left one pixel: Leftarrow
- View: Move cursor right one pixel: Rightarrow
- Transport: Go to start of project: Control+Home
- Transport: Go to end of project: Control+End
- Go forward one measure: PageDown
- Go back one measure: PageUp
- Go forward one beat: Control+PageDown
- Go back one beat: Control+PageUp
- View: Move cursor right to grid division: Alt+Shift+Rightarrow
- View: Move cursor left to grid division: Alt+Shift+Leftarrow

#### Items
- Item navigation: Select and move to previous item: control+leftArrow
- Item navigation: Select and move to next item: control+rightArrow
- Item: Split items at edit or play cursor: S
- Item: Split items at time selection: Shift+S
- Item: Remove items
- Item edit: Move items/envelope points right: . or NumPad6
- Item edit: Move items/envelope points left: , or NumPad4
- Item edit: Grow left edge of items: Control+, or Control+NumPad4
- Item edit: Shrink left edge of items: Control+. or Control+NumPad6
- Item edit: Shrink right edge of items: Alt+, or Alt+NumPad4
- Item edit: Grow right edge of items: Alt+. or Alt+NumPad6
- Item: go to next stretch marker: Shift+'
- Item: go to previous stretch marker: Shift+;
- Item: remove stretch marker at current position
- Item navigation: Move cursor left to edge of item: Control+Shift+,
- Item navigation: Move cursor right to edge of item: Control+Shift+.
- Item: Select all items in track: Control+Alt+A
- Item: Select all items on selected tracks in current time selection: Alt+Shift+A
- Item grouping: Select all items in groups: Shift+G
- Item: Select all items in current time selection: Control+Shift+A
- (SWS extension) Xenakios/SWS: Select items under edit cursor on selected tracks: Shift+A
- Item properties: Toggle mute: Alt+F5
- Item: Open associated project in new tab

#### Takes
- Take: Switch items to next take: T
- Take: Switch items to previous take: Shift+T

#### Markers and Regions
- Markers: Go to previous marker/project start: ;
- Markers: Go to next marker/project end: '
- Markers: Delete marker near cursor: 
- Markers: Delete region near cursor
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

#### Time Signature/Tempo Markers
- Move edit cursor to previous tempo or time signature change: Control+;
- Move edit cursor to next tempo or time signature change: Control+'
- Markers: Delete time signature marker near cursor

#### Envelopes
- Item edit: Move items/envelope points up one track/a bit: NumPad8
- Item edit: Move items/envelope points down one track/a bit: NumPad2
- (SWS extension) SWS/FNG: Move selected envelope points up: Alt+NumPad8
- (SWS extension) SWS/FNG: Move selected envelope points down: Alt+NumPad2
- Envelope: Delete all selected points: Control+Shift+Delete
- Envelope: Delete all points in time selection: Alt+Shift+Delete
- Envelope: Insert new point at current position: Shift+E

#### Zoom
- Zoom out horizontal: - or NumPad-
- Zoom in horizontal: = or NumPad+

#### Options
- Options: Cycle ripple editing mode: Alt+P
- Options: Toggle metronome: Control+Shift+M
- Options: Toggle auto-crossfade on/off: alt+x
- Options: Toggle locking: l
- (SWS extension) SWS/BR: Options - Cycle through record modes: Alt+\
- Options: Solo in front: Control+Alt+F6

#### Undo
- Edit: Undo: Control+Z
- Edit: Redo: Control+Shift+Z

#### Transport
- Transport: Toggle repeat: Control+R
- Transport: Increase playrate by ~6% (one semitone): Control+Shift+=
- Transport: Decrease playrate by ~6% (one semitone): Control+Shift+-
- Transport: Increase playrate by ~0.6% (10 cents): Shift+=
- Transport: Decrease playrate by ~0.6% (10 cents): Shift+-

#### Selection
- Time selection: Set start point: [
- Time selection: Set end point: ]
- Loop points: Set start point: Alt+Shift+[
- Loop points: Set end point: Alt+Shift+]
- Time selection: Remove contents of time selection (moving later items)
- Time selection: Remove time selection and loop point selection: Escape
- Unselect all tracks/items/envelope points: Shift+Escape

#### Clipboard
- Edit: Cut items/tracks/envelope points (depending on focus) ignoring time selection: Control+X
- Edit: Cut items/tracks/envelope points (depending on focus) within time selection, if any (smart cut): Control+Shift+X
- Edit: Copy items/tracks/envelope points (depending on focus) ignoring time selection: Control+C
- Edit: Copy items/tracks/envelope points (depending on focus) within time selection, if any (smart copy): Control+Shift+C
- Item: Paste items/tracks: Control+V

#### View
- View: Toggle master track visible: Control+Alt+M

#### Grid
- Grid: Set to 1: control+shift+1
- Grid: Set to 1/2: control+shift+2
- Grid: Set to 1/32: control+shift+3
- Grid: Set to 1/4: control+shift+4
- Grid: Set to 1/6 (1/4 triplet): control+shift+5
- Grid: Set to 1/16: control+shift+6
- Grid: Set to 1/24 (1/16 triplet): control+shift+7
- Grid: Set to 1/8: control+shift+8
- Grid: Set to 1/12 (1/8 triplet): control+shift+9

#### Project Tabs
- Close current project tab: control+f4
- New project tab: alt+shift+n
- New project tab (ignore default template)
- Next project tab: control+tab
- Previous project tab: control+shift+tab

#### MIDI Editor
- View: Go to start of file: Control+Home
- View: Go to end of file: Control+End
- Edit: Move edit cursor right by grid: Alt+Shift+Leftarrow
- Edit: Move edit cursor left by grid: Alt+Shift+Rightarrow
- Navigate: Move edit cursor right one measure: PageDown
- Navigate: Move edit cursor left one measure: PageUp
- Edit: Increase pitch cursor one semitone: Alt+UpArrow or NumPad8
- Edit: Decrease pitch cursor one semitone: Alt+DownArrow or NumPad2
- Edit: Delete events: Delete
- Edit: Insert note at edit cursor: I

### Context Menus
There are several context menus in REAPER, but some of them are difficult to access or not accessible at all from the keyboard.
OSARA enables keyboard access for the track input, track area, track routing, item, ruler, envelope point and automation item context menus.

To open the context menu for the element you are working with, press the applications key on Windows or Control+1 on Mac.
For example, if you have just moved to a track, it will open the track input context menu for that track.
If you have just moved the edit cursor, it will open the context menu for the ruler.

For tracks, there are three context menus:

1. Track input: Allows you to set the input to use when recording, etc.
 You access this by just pressing the applications key on Windows or Control+1 on Mac.
2. Track area: Provides options for inserting, duplicating and removing tracks, etc.
 You access this by pressing control+applications on Windows or Control+2 on Mac.
3. Routing: Allows you to quickly add and remove sends, receives and outputs without opening the I/O window.
 You access this by pressing alt+applications on Windows or Control+3 on Mac.

### Parameter Lists
OSARA can display a list of parameters for various elements such as tracks, items and effects.
You can then check and change the values of these parameters.
This is useful for parameters which are tedious or impossible to access otherwise.

#### Track/Item Parameters
To access the parameter list for a track or an item, select the track or item you wish to work with.
Then, press control+shift+p (OSARA: View parameters for current track/item).

#### FX Parameters
Many effects are unfortunately either partially or completely inaccessible.
However, most effects make their parameters available for automation in a standard way.
This can also be used to make them at least partially accessible.
Thus, the FX parameter list is particular useful and is the only way to access some effects.

To access it:

1. Select a track or item with at least one effect.
 Then, press p (OSARA: View FX parameters for current track/take).
2. Alternatively, to access FX parameters for the master track, press shift+p (OSARA: View FX parameters for master track).
3. If there is more than one effect on the track, select the desired effect from the menu.
 If there are input or monitoring FX on the track, these will be included in the menu as well with an appropriate suffix.

Note that only some effects expose easily readable values, while others expose only percentages.
Even for effects that do expose easily readable values, the editable text is an internal number and probably won't correspond to the readable value on the slider.

#### Using Parameter Lists
Once you have opened a parameter list dialog, you can select a parameter from the Parameter combo box and check or adjust its value using the Value slider.
For parameters which support it, there is also an editable text field which allows you to edit the value textually.

The Filter field allows you to narrow the list to only contain parameters which include the entered text.
For example, if the full list contained "Volume" and "Pan" parameters and you type "vol" in the Filter field, the list will be narrowed to only show "Volume".
Clearing the text in the Filter field shows the entire list.

When you are done working with parameters, press the Close button.
Alternatively, you can press enter or escape.

### Reading Current Peaks
OSARA allows you to read the current audio peak for channels 1 and 2 of either the current or master tracks.
You do this using the following actions:

- OSARA: Report current peak for channel 1 of current track: j
- OSARA: Report current peak for channel 2 of current track: k
- OSARA: Report current peak for channel 1 of master track: shift+j
- OSARA: Report current peak for channel 2 of master track: shift+k

### Peak Watcher
In addition to reading current peaks, You can also be notified automatically when the volume exceeds a specified maximum level using Peak Watcher.
This can be done for one or two tracks.

To use Peak Watcher:

1. Press control+shift+w (OSARA: View Peak Watcher).
2. From the First track combo box, select one of the following:
 - None: Select this if you do not wish to monitor a track.
 - Follow current track: Select this if you want to watch peaks for whatever track you move to in your project.
 - Master: This watches peaks for the master track.
 - Otherwise, you can choose any track in your project.
3. If you wish to monitor a second track, you can choose another track from the Second track combo box.
4. If you want to be notified when the level of channels exceeds a certain level, in the "Notify automatically for channels:" grouping, check the options for the desired channels and enter the desired level (in dB).
5. The Hold peaks grouping allows you to specify whether the highest peak remains as the reported peak level and for how long.
 Holding peaks gives you time to examine the peak level, even if the audio level dropped immediately after the peak occurred.
 There are three options:
 - disabled: Don't hold peaks at all.
 - until reset: Hold peaks until the Peak Watcher is reset.
 - for (ms): Allows you to specify a time in milliseconds for which peaks will be held.
6. Press the Reset button to reset the reported peak levels if they are being held.
7. When you are done, press the OK button to accept any changes or the Cancel button to discard them.

At any time, you can report or reset the peak levels for either of the tracks being watched using the following actions:

- OSARA: Report Peak Watcher value for channel 1 of first track: alt+f9
- OSARA: Report Peak Watcher value for channel 2 of first track: alt+f10
- OSARA: Report Peak Watcher value for channel 1 of second track: alt+shift+f9
- OSARA: Report Peak Watcher value for channel 2 of second track: alt+shift+f10
- OSARA: Reset Peak Watcher for first track: alt+f8
- OSARA: Reset Peak Watcher for second track: alt+shift+f8

### Shortcut Help
It is possible to have REAPER list all shortcuts and to search for individual shortcuts in the Action List.
However, it is sometimes convenient to be able to simply press a shortcut and immediately learn what action it will perform.
This is especially useful if you have forgotten an exact shortcut but do have some idea of what it might be.
You can achieve this using OSARA's shortcut help mode.

You can turn shortcut help on and off by pressing f12 (OSARA: Toggle shortcut help).
While shortcut help is enabled, pressing any shortcut will report the action associated with that shortcut, but the action itself will not be run.

### Noncontiguous Selection
Usually, selection is done contiguously; e.g. you might select tracks 1 through 4.
Sometimes, it is desirable to select noncontiguously; e.g. you might want to select tracks 1, 3 and 5.

You can do this as follows:

1. Move to the track or item you want to start with.
2. Optionally, select some other contiguous tracks or items.
3. Press shift+space (OSARA: Enable noncontiguous selection/toggle selection of current track/item) to switch to noncontiguous selection.
4. Move through tracks/items leaving other tracks/items selected; e.g. using shift+downArrow and shift+upArrow to move through tracks.
 These tracks/items will not be selected, but any previously selected tracks/items will remain selected.
5. When you reach a track/item you want to select, press shift+space (OSARA: Enable noncontiguous selection/toggle selection of current track/item).
 You can also use this if you want to unselect a previously selected track/item.

Selection will revert to contiguous selection the next time you move to a track/item without leaving other tracks/items selected.

If you want to select noncontiguous items on several different tracks, the procedure is exactly the same.
However, it's important to remember that you must move between tracks without affecting the selection; i.e. using shift+downArrow and shift+upArrow.
Otherwise, selection will revert to contiguous selection.

### Accessing Controls for Sends/Receives/Outputs in the Track I/O Window
In the Track I/O window, there are various controls for each send, receive or hardware output.
Unfortunately, these controls cannot be reached with the tab key and it is tedious at best to access these with screen reader review functions.

When you tab to the Delete button for a send/receive/output, the name of the send/receive/output will first be reported.
You can then press the Applications key to access a menu of additional options.

### Manually Moving Stretch Markers
REAPER includes actions to snap stretch markers to the grid.
However, sometimes, this is not sufficient and it is useful to be able to manually move stretch markers to a specific position.

To do this:

1. Select the desired item.
2. Go to a stretch marker; e.g. using shift+apostrophe (Item: go to next stretch marker).
 Ensure that OSARA reports the stretch marker.
3. Move the edit cursor to the position to which you wish to move the stretch marker.
4. Press alt+m (OSARA: Move last focused stretch marker to current edit cursor position).

### Accessing FX Plug-in Windows
Some FX plug-ins can be controlled with keyboard commands, but you can't reach them by tabbing through the FX Chain dialog.
In these cases, you can press f6 to have OSARA attempt to focus the plug-in window.

### Automation Items
An envelope can contain one or more automation items positioned along the timeline.
With OSARA, you move to automation items as follows:

1. Select an envelope using control+l or control+shift+l (OSARA: Select next track/take envelope (depending on focus)).
2. Now, use the normal item navigation commands; i.e. control+rightArrow and control+leftArrow (Item navigation: Select and move to next item, Item navigation: Select and move to previous item).
 Multiple selection is also possible using control+shift+rightArrow and control+shift+leftArrow (OSARA: Move to next item (leaving other items selected), OSARA: Move to previous item (leaving other items selected).
 Noncontiguous selection is done the same way described above for tracks and items.
3. The item navigation commands will revert back to moving through media items (instead of automation items) when focus is moved away from the envelope.
 For example, moving to another track and back again will again allow you to move through the media items on the track.

Once you move to an automation item, the commands to move between envelope points such as alt+k and alt+j (OSARA: Move to next envelope point, OSARA: Move to previous envelope point) move between the points in the automation item.
The points within an automation item can only be accessed after moving to that automation item; they cannot be accessed from the underlying envelope.
To return to the points in the underlying envelope, simply move focus back to the envelope by selecting it again with control+l and control+shift+l (OSARA: Select next track/take envelope (depending on focus), OSARA: Select previous track/take envelope (depending on focus)).

### Notes and Chords in the MIDI Editor
In the MIDI Editor, OSARA enables you to move between chords and to move to individual notes in a chord.
In this context, a chord is any number of notes that are placed at the exact same position.
If there is only one note at a given position, it will be treated as a chord.

You move between chords using the left and right arrow keys (OSARA: Move to previous chord, OSARA: Move to next chord).
When you move to a chord, the edit cursor will be placed at the chord and the new position will be reported.
The notes of the chord will be played and the number of notes will be reported.
The notes in the chord are also selected so you can manipulate the entire chord.
For example, pressing delete will delete the chord.

Once you have moved to a chord, you can move to individual notes using the up and down arrow keys (OSARA: Move to previous note in chord, OSARA: Move to next note in chord).
When you move to a note, that note will be played and its name will be reported.
The single note will also be selected so you can manipulate just that note.
For example, pressing delete will delete only that note.

You can select multiple chords or multiple notes in a chord.
To do this, first move to the first chord or note you want to select.
Then, use shift plus the arrow keys to add the next or previous chord or note to the selection.
For example, if you've moved to a chord and also want to add the next chord to the selection, you would press shift+rightArrow.

Noncontiguous selection is also possible.
You do this in the same way described above for tracks and items.
That is, press shift+space (OSARA: Enable noncontiguous selection/toggle selection of current chord/note) to switch to noncontiguous selection, move to other chords/notes with shift plus the arrow keys and press shift+space to select/unselect the current chord/note.

### Configuration
OSARA includes a Configuration dialog to adjust various settings.
You open this dialog by pressing control+alt+shift+p (OSARA: Configuration).

The dialog contains the following options:

- Report position when scrubbing: When disabled, OSARA will not report the cursor position when using the scrubbing actions (View: Move cursor left/right one pixel).
- Report FX when moving to tracks/takes: When enabled, OSARA will report the names of any effects on a track or take when you move to it.
- Report transport state (play, record, etc.): When enabled, OSARA will report the transport state when you change it; e.g. if you begin playing or recording.

When you are done, press the OK button to accept any changes or the Cancel button to discard them.

### Miscellaneous Actions
OSARA also includes some other miscellaneous actions.

#### Main
- OSARA: Move to next item (leaving other items selected): control+shift+rightArrow
- OSARA: Move to previous item (leaving other items selected): control+shift+leftArrow
- OSARA: View I/O for master track: shift+i
- OSARA: Report ripple editing mode: alt+shift+p
- OSARA: Report muted tracks: control+shift+f5
- OSARA: Report soloed tracks: control+shift+f6
- OSARA: Report record armed tracks: control+shift+f7
- OSARA: Report tracks with record monitor on: control+shift+f8
- OSARA: Report tracks with phase inverted: control+shift+f9
- OSARA: Report track/item/time selection (depending on focus): control+shift+space
- OSARA: Remove items/tracks/contents of time selection/markers/envelope points (depending on focus): delete
- OSARA: Report edit/play cursor position: control+shift+j
 - If the ruler unit is set to Measures.Beats / Minutes:Seconds, Pressing this once will report the time in measures.beats, while pressing it twice will report the time in minutes:seconds .
- OSARA: Delete all time signature markers: alt+win+delete
- OSARA: Select next track/take envelope (depending on focus): control+l
- OSARA: Select previous track/take envelope (depending on focus): control+shift+l
- OSARA: Move to next envelope point: alt+k
- OSARA: Move to previous envelope point: alt+j
- OSARA: Move to next envelope point (leaving other points selected): alt+shift+k
- OSARA: Move to previous envelope point (leaving other points selected): alt+shift+j
- OSARA: Move to next transient: tab
- OSARA: Move to previous transient: shift+tab

#### MIDI Event List Editor
- OSARA: Focus event nearest edit cursor: control+f

## Support
If you need help, please subscribe to the [Reapers Without Peepers discussion group](https://groups.io/g/rwp) and ask your questions there.

## Reporting Issues
Issues should be reported [on GitHub](https://github.com/jcsteh/osara/issues).

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

- Windows only: Microsoft Visual Studio 2015 (Express for Desktop, or Community with VC++ and Windows SDK 7.1A support):
 - [Download for Visual Studio 2015 Express for Desktop](https://go.microsoft.com/fwlink/?LinkId=691984&clcid=0x409)
- Mac only: Either the [command line developer tools](https://developer.apple.com/library/ios/technotes/tn2339/_index.html) or [Xcode](https://developer.apple.com/xcode/download/)
- [SCons](http://www.scons.org/), version 2.3.2 or later

### How to Build
To build OSARA, from a command prompt, simply change to the OSARA checkout directory and run scons.
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
