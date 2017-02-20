# OSARA: Open Source Accessibility for the REAPER Application

- Author: James Teh &lt;jamie@nvaccess.org&gt; & other contributors
- Copyright: 2014-2016 NV Access Limited
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

## Requirements
OSARA requires REAPER 5.16 or later.
The [SWS/S&M EXTENSION](http://www.sws-extension.org/) is highly recommended and OSARA supports several useful actions from this extension.

It has only been tested with the NVDA and VoiceOver screen readers.
However, on Windows, OSARA uses Microsoft Active Accessibility (MSAA) to communicate information, so it should work with any screen reader which supports this correctly.

## Download and Installation
You can download the latest OSARA installer from the [OSARA Development Snapshots](http://www.nvaccess.org/files/osara/snapshots.html) page.

### Windows
Once yu have downloaded the installer, simply run it and follow the instructions.

Note that if you previously copied the OSARA extension into REAPER's program directory manually (before the installer became available), you must remove this first.
The installer installs the extension into your user configuration, not the program directory.

By default, the OSARA key map will be installed, completely replacing your existing key map.
If yu do not wish this to occur, you can uncheck the "Replace existing key map with OSARA key map" option.

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

## Usage

### Supported REAPER and Extension Actions
OSARA supports reporting of information for the following actions.
Most of these are actions built into REAPER, but a few are very useful actions from the SWS extension.

#### Track Navigation/Management
- Track: Go to next track
- Track: Go to previous track
- Track: Go to next track (leaving other tracks selected)
- Track: Go to previous track (leaving other tracks selected)
- Track: Insert new track
- Track: Cycle track folder state
- Track: Cycle track folder collapsed state
- Track: Remove tracks

#### Adjusting Track Parameters
- Track: Mute/unmute tracks
- Track: Solo/unsolo tracks
- Toggle record arming for current (last touched) track
- Track: Cycle track record monitor
- Track: Invert track phase
- Track: Toggle FX bypass for current track
- Track: Toggle FX bypass for master track
- Track: toggle FX bypass on all tracks
- Track: Nudge track volume up
- Track: Nudge track volume down
- (SWS extension) Xenakios/SWS: Nudge volume of selected tracks up
- (SWS extension) Xenakios/SWS: Nudge volume of selected tracks down
- Track: Nudge master track volume up
- Track: Nudge master track volume down
- Track: Nudge track pan left
- Track: Nudge track pan right

#### Edit Cursor Movement
- View: Move cursor left one pixel
- View: Move cursor right one pixel
- Transport: Go to start of project
- Transport: Go to end of project
- Go forward one measure
- Go back one measure
- Go forward one beat
- Go back one beat
- View: Move cursor right to grid division
- View: Move cursor left to grid division

#### Items
- Item navigation: Select and move to previous item
- Item navigation: Select and move to next item
- Item: Split items at edit or play cursor
- Item: Split items at time selection
- Item: Remove items
- Item edit: Move items/envelope points right
- Item edit: Move items/envelope points left
- Item edit: Grow left edge of items
- Item edit: Shrink left edge of items
- Item edit: Shrink right edge of items
- Item edit: Grow right edge of items
- Item: go to next stretch marker
- Item: go to previous stretch marker
- Item: remove stretch marker at current position
- Item navigation: Move cursor left to edge of item
- Item navigation: Move cursor right to edge of item
- Item: Select all items in track
- Item: Select all items on selected tracks in current time selection
- Item grouping: Select all items in groups
- Item properties: Toggle mute

#### Takes
- Take: Switch items to next take
- Take: Switch items to previous take

#### Markers and Regions
- Markers: Go to previous marker/project start
- Markers: Go to next marker/project end
- Markers: Delete marker near cursor
- Markers: Delete region near cursor
- Markers: Go to marker 01
- Markers: Go to marker 02
- Markers: Go to marker 03
- Markers: Go to marker 04
- Markers: Go to marker 05
- Markers: Go to marker 06
- Markers: Go to marker 07
- Markers: Go to marker 08
- Markers: Go to marker 09
- Markers: Go to marker 10
- Regions: Go to region 01 after current region finishes playing (smooth seek)
- Regions: Go to region 02 after current region finishes playing (smooth seek)
- Regions: Go to region 03 after current region finishes playing (smooth seek)
- Regions: Go to region 04 after current region finishes playing (smooth seek)
- Regions: Go to region 05 after current region finishes playing (smooth seek)
- Regions: Go to region 06 after current region finishes playing (smooth seek)
- Regions: Go to region 07 after current region finishes playing (smooth seek)
- Regions: Go to region 08 after current region finishes playing (smooth seek)
- Regions: Go to region 09 after current region finishes playing (smooth seek)
- Regions: Go to region 10 after current region finishes playing (smooth seek)

#### Time Signature/Tempo Markers
- Move edit cursor to previous tempo or time signature change
- Move edit cursor to next tempo or time signature change
- Markers: Delete time signature marker near cursor

#### Envelopes
- Item edit: Move items/envelope points up one track/a bit
- Item edit: Move items/envelope points down one track/a bit
- (SWS extension) SWS/FNG: Move selected envelope points up
- (SWS extension) SWS/FNG: Move selected envelope points down
- Envelope: Delete all selected points
- Envelope: Delete all points in time selection

#### Zoom
- Zoom out horizontal
- Zoom in horizontal

#### Options
- Options: Cycle ripple editing mode
- Options: Toggle metronome

#### Undo
- Edit: Undo
- Edit: Redo

#### Transport
- Transport: Toggle repeat

#### Selection
- Time selection: Set start point
- Time selection: Set end point
- Loop points: Set start point
- Loop points: Set end point
- Time selection: Remove contents of time selection (moving later items)
- Time selection: Remove time selection and loop point selection
- Unselect all tracks/items/envelope points

#### Clipboard
- Edit: Cut items/tracks/envelope points (depending on focus) ignoring time selection
- Edit: Cut items/tracks/envelope points (depending on focus) within time selection, if any (smart cut)
- Edit: Copy items/tracks/envelope points (depending on focus) ignoring time selection
- Edit: Copy items/tracks/envelope points (depending on focus) within time selection, if any (smart copy)
- Item: Paste items/tracks

#### View
- View: Toggle master track visible

#### MIDI Editor
- View: Go to start of file
- View: Go to end of file
- Edit: Move edit cursor right by grid
- Edit: Move edit cursor left by grid
- Edit: Move edit cursor right one measure
- Edit: Move edit cursor left one measure
- Edit: Increase pitch cursor one semitone
- Edit: Decrease pitch cursor one semitone
- Edit: Delete events

### Context Menus
There are several context menus in REAPER, but some of them are difficult to access or not accessible at all from the keyboard.
OSARA enables keyboard access for the track input, track area, item and ruler context menus.

Pressing the applications key will open the context menu for the element you are working with.
For example, if you have just moved to a track, it will open the track input context menu for that track.
If you have just moved the edit cursor, it will open the context menu for the ruler.

For tracks, there are three context menus:

1. Track input: Allows you to set the input to use when recording, etc.
 You access this by just pressing the applications key.
2. Track area: Provides options for inserting, duplicating and removing tracks, etc.
 You access this by pressing control+applications.
3. Routing: Allows you to quickly add and remove sends, receives and outputs without opening the I/O window.
 You access this by pressing alt+applications.

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
Sometimes, it is desirable to select noncontiguously; e.g. you might want too select tracks 1, 3 and 5.

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

### Notes and Chords in the MIDI Editor
In the MIDI Editor, OSARA enables you to move between chords and to move to individual notes in a chord.

You move between chords using the left and right arrow keys (OSARA: Move to previous chord, OSARA: Move to next chord).
When you move to a chord, the edit cursor will be placed at the chord and the new position will be reported.
The notes of the chord will be played and the number of notes will be reported.
The notes in the chord are also selected so you can manipulate the entire chord.
For example, pressing delete will delete the chord.

Once you have moved to a chord, you can move to individual notes using the up and down arrow keys (OSARA: Move to previous note in chord, OSARA: Move to next note in chord).
When you move to a note, that note will be played and its name will be reported.
The single note will also be selected so you can manipulate just that note.
For example, pressing delete will delete only that note.

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

#### MIDI Event List Editor
- OSARA: Focus event nearest edit cursor: control+f

## Support
If you need help, please subscribe to the [Reapers Without Peepers mailing list](http://bluegrasspals.com/mailman/listinfo/rwp) and ask your questions there.

## Reporting Issues
Issues should be reported [on GitHub](https://github.com/nvaccess/osara/issues).

## Donations
If you find OSARA useful and want it to continue to improve, please consider [donating to NV Access](http://www.nvaccess.org/donate/).

## Building
This section is for those interested in building OSARA from source code.

You will need:

- Windows only: Microsoft Visual Studio 2015 (Express for Desktop, or Community with VC++ and Windows SDK 7.1A support):
 - [Download for Visual Studio 2015 Express for Desktop](https://go.microsoft.com/fwlink/?LinkId=691984&clcid=0x409)
- Mac only: Either the [command line developer tools](https://developer.apple.com/library/ios/technotes/tn2339/_index.html) or [Xcode](https://developer.apple.com/xcode/download/)
- [SCons](http://www.scons.org/), version 2.3.2 or later

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
