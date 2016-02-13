# OSARA: Open Source Accessibility for the REAPER Application

- Author: James Teh &lt;jamie@nvaccess.org&gt; & other contributors
- Copyright: 2014-2015 NV Access Limited
- License: GNU General Public License version 2.0

OSARA is a [REAPER](http://www.reaper.fm/) extension which aims to make REAPER accessible to screen reader users.
It was heavily inspired by and based on the concepts of the ReaAccess extension, but was created as a potential replacement because ReaAccess seems to have been abandoned and was not developed openly.
Currently, it runs only on Windows.

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
OSARA requires REAPER 5.0 or later.

It has only been tested with the NVDA screen reader.
However, OSARA uses Microsoft Active Accessibility (MSAA) to communicate information, so it should work with any screen reader which supports this correctly.

## Downloading
For now, here are links to the latest versions of the plug-in files:

- [32 bit dll](http://www.nvaccess.org/files/osara/reaper_osara32.dll)
- [64 bit dll](http://www.nvaccess.org/files/osara/reaper_osara64.dll)

## Installation
- If you are using a 32 bit version of REAPER, the file you need is reaper_osara32.dll.
- If you are using a 64 bit version of REAPER, the file you need is reaper_osara64.dll.
- Copy this file into the Plugins folder inside your REAPER program folder; e.g. C:\Program Files\REAPER\Plugins.
 Alternatively, you can copy it into the UserPlugins folder inside your REAPER user configuration; e.g. %appdata%\REAPER\UserPlugins.

## A Note for ReaAccess Users
Some of the actions in ReaAccess are specific to ReaAccess and will not work with OSARA.
In particular, this includes arming tracks and item navigation.
Please see below for a list of the exact actions supported.
You will need to use these actions in order for OSARA to provide feedback.

## Key Map
OSARA does not yet provide its own key map.
You may wish to use [this key map created by Derek Lane and Gianluca Apollaro](http://dl.dropbox.com/u/5126017/DGKeyMap03-10-2015.ReaperKeyMap?dl=1).
You can import it from the Actions dialog.
This key map is similar to that provided by ReaAccess, though there are some differences.
You can see the full key map by selecting Key bindings and mouse modifiers from the Help menu.

## Usage

### Supported REAPER and Extension Actions
OSARA supports reporting of information for the following actions.
Most of these are actions built into REAPER, but a few are very useful actions from the popular SWS extension.
If keyboard shortcuts aren't bound to them already, you will need to do this yourself.

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
- Track: Select previous envelope
- Track: Select next envelope
- (SWS extension) SWS/BR: Move edit cursor to previous envelope point
- (SWS extension) SWS/BR: Move edit cursor to next envelope point
- (SWS extension) SWS/BR: Move edit cursor to previous envelope point and select it
- (SWS extension) SWS/BR: Move edit cursor to next envelope point and select it
- (SWS extension) SWS/BR: Move edit cursor to previous envelope point and add to selection
- (SWS extension) SWS/BR: Move edit cursor to next envelope point and add to selection

#### Zoom
- Zoom out horizontal
- Zoom in horizontal

#### Options
- Options: Cycle ripple editing mode

#### Undo
- Edit: Undo
- Edit: Redo

#### Transport
- Transport: Toggle repeat

#### Time Selection
- Time selection: Remove contents of time selection (moving later items)

#### Clipboard
- Edit: Cut items/tracks/envelope points (depending on focus) ignoring time selection
- Edit: Cut items/tracks/envelope points (depending on focus) within time selection, if any (smart cut)
- Edit: Copy items/tracks/envelope points (depending on focus) ignoring time selection
- Edit: Copy items/tracks/envelope points (depending on focus) within time selection, if any (smart copy)
- Item: Paste items/tracks

### Context Menus
There are several context menus in REAPER, but some of them are difficult to access or not accessible at all from the keyboard.
OSARA enables keyboard access for the track, track area, item and ruler context menus.

Pressing the applications key will open the context menu for the element you are working with.
For example, if you have just moved to a track, it will open the context menu for the track.
If you have just moved the edit cursor, it will open the context menu for the ruler.

For tracks, there are two context menus.
You access the second by pressing control+applications.

### Parameter Lists
OSARA can display a list of parameters for various elements such as tracks, items and effects.
You can then check and change the values of these parameters.
This is useful for parameters which are tedious or impossible to access otherwise.

#### Track/Item Parameters
To access the parameter list for a track or an item, select the track or item you wish to work with.
Then, run the "OSARA: View parameters for current track/item (depending on focus)" action.
You will probably want to add a keyboard shortcut for this action so you can access it quickly.

#### FX Parameters
Many effects are unfortunately either partially or completely inaccessible.
However, most effects make their parameters available for automation in a standard way.
This can also be used to make them at least partially accessible.
Thus, the FX parameter list is particular useful and is the only way to access some effects.

To access it:

1. Select a track with at least one effect. Then, run the "OSARA: View FX parameters for current track" action.
 You will probably want to add a keyboard shortcut for this action so you can access it quickly.
2. Alternatively, run the "OSARA: View FX parameters for master track" action.
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

### Peak Watcher
Peak watcher allows you to read the level of audio peaks in a specified track.
You can also be notified automatically when the volume exceeds a specified maximum level.

To use Peak Watcher:

1. Move to a track.
 If you want to report the peaks for a specific track, this is the track you should select.
2. Run the "OSARA: View Peak Watcher" action.
3. From the Track combo box, select one of the following:
 - Disabled: Select this to disable Peak Watcher.
 - Follow current track: Select this if you want to watch peaks for whatever track you move to.
 - Master: This watches peaks for the master track.
 - You can choose the track which was current when the dialog was opened to always watch this specific track.
 - If you were previously watching a specific track, this track will also be listed.
4. If you want to be notified when the level of channels exceeds a certain level, in the "Notify automatically for channels:" grouping, check the options for the desired channels and enter the desired level (in dB).
5. The Hold peaks option allows you to specify whether the highest peak remains as the reported peak level and for how long.
 Holding peaks gives you time to examine the peak level, even if the audio level dropped immediately after the peak occurred.
 Specify -1 to disable holding of peaks or 0 to hold peaks forever.
6. Press the Reset button to reset the reported peak levels if they are being held.
7. When you are done, press the OK button to accept any changes or the Cancel button to discard them.

At any time, you can report the current peak level for the track being watched by running the "OSARA: Report Peak Watcher peaks" action.
You will probably want to add a keyboard shortcut for this action so you can access it quickly.

### Shortcut Help
It is possible to have REAPER list all shortcuts and to search for individual shortcuts in the Action List.
However, it is sometimes convenient to be able to simply press a shortcut and immediately learn what action it will perform.
This is especially useful if you have forgotten an exact shortcut but do have some idea of what it might be.
You can achieve this using OSARA's shortcut help mode.

You can turn shortcut help on and off using the "OSARA: Toggle shortcut help" action.
While shortcut help is enabled, pressing any shortcut will report the action associated with that shortcut, but the action itself will not be run.

### Noncontiguous Selection
Usually, selection is done contiguously; e.g. you might select tracks 1 through 4.
Sometimes, it is desirable to select noncontiguously; e.g. you might want too select tracks 1, 3 and 5.

You can do this as follows:

1. Move to the track or item you want to start with.
2. Optionally, select some other contiguous tracks or items.
3. Run the "OSARA: Enable noncontiguous selection/toggle selection of current track/item (depending on focus)" action to switch to noncontiguous selection.
4. Move through tracks/items leaving other tracks/items selected; e.g. using the "Track: Go to next track (leaving other tracks selected)" action.
 These tracks/items will not be selected, but any previously selected tracks/items will remain selected.
5. When you reach a track you want to select, run the "OSARA: Enable noncontiguous selection/toggle selection of current track" action.
 You can also use this if you want to unselect a previously selected track/item.

Selection will revert to contiguous selection the next time you move to a track/item without leaving other tracks/items selected.

If you want to select noncontiguous items on several different tracks, the procedure is exactly the same.
However, it's important to remember that you must move between tracks without affecting the selection; e.g. using the "Track: Go to next track (leaving other tracks selected)" action.
Otherwise, selection will revert to contiguous selection.

### Accessing Controls for Sends/Receives/Outputs in the Track I/O Window
In the Track I/O window, there are various controls for each send, receive or hardware output.
Unfortunately, these controls cannot be reached with the tab key and it is tedious at best to access these with screen raeder review functions.

When you tab to the Delete button for a send/receive/output, the name of the send/receive/output will first be reported.
You can then press the Applications key to access a menu of additional options.

### Manually Moving Stretch Markers
REAPER includes actions to snap stretch markers to the grid.
However, sometimes, this is not sufficient and it is useful to be able to manually move stretch markers to a specific position.

To do this:

1. Select the desired item.
2. Go to a stretch marker; e.g. using the"Item: go to next stretch marker" action.
 Ensure that OSARA reports the stretch marker.
3. Move the edit cursor to the position to which you wish to move the stretch marker.
4. Run the "OSARA: Move last focused stretch marker to current edit cursor position" action.

### Configuration
OSARA includes a Configuration dialog to adjust various settings.
You open this dialog using the "OSARA: Configuration" action.

The dialog contains the following options:

- Report position when scrubbing: When disabled, OSARA will not report the cursor position when using the scrubbing actions (View: Move cursor left/right one pixel).
- Report FX when moving to tracks: When enabled, OSARA will report the names of any effects on a track when you move to it.

When you are done, press the OK button to accept any changes or the Cancel button to discard them.

### Miscellaneous Actions
OSARA also includes some other miscellaneous actions.

#### Main
- OSARA: Move to next item (leaving other items selected)
- OSARA: Move to previous item (leaving other items selected)
- OSARA: View I/O for master track
- OSARA: Report ripple editing mode
- OSARA: Report muted tracks
- OSARA: Report soloed tracks
- OSARA: Report record armed tracks
- OSARA: Report tracks with record monitor on
- OSARA: Report tracks with phase inverted
- OSARA: Report track/item/time selection (depending on focus)
- OSARA: Remove items/tracks/contents of time selection/markers (depending on focus)
- OSARA: Report edit/play cursor position
 - If the ruler unit is set to Measures.Beats / Minutes:Seconds, Pressing this once will report the time in measures.beats, while pressing it twice will report the time in minutes:seconds .

#### MIDI Event List Editor
- OSARA: Focus event nearest edit cursor

## Support
If you need help, please subscribe to the [Reapers Without Peepers mailing list](http://bluegrasspals.com/mailman/listinfo/rwp) and ask your questions there.

## Reporting Issues
Issues should be reported [on GitHub](https://github.com/nvaccess/osara/issues).

## Donations
If you find OSARA useful and want it to continue to improve, please consider [donating to NV Access](http://www.nvaccess.org/donate/).

## Building
This section is for those interested in building OSARA from source code.

You will need:

- Microsoft Visual Studio 2012 or later for Windows Desktop.
 The Express edition is fine.
- [SCons](http://www.scons.org/), version 2.3.2 or later

To build OSARA, from a command prompt, simply change to the OSARA checkout directory and run scons.
The resulting dll files can be found at build\x86\reaper_osara32.dll and build\x86_64\reaper_osara64.dll.

## Contributors
- NV Access Limited
- James Teh
- Victor Tsaran
