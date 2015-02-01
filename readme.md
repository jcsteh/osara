# OSARA: Open Source Accessibility for the REAPER Application

- Author: James Teh &lt;jamie@jantrid.net&gt;
- Copyright: 2014-2015 James Teh
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

## Requirements
OSARA requires REAPER 4.71 or later.

It has only been tested with the NVDA screen reader.
However, OSARA uses Microsoft Active Accessibility (MSAA) to communicate information, so it should work with any screen reader which supports this correctly.

## Downloading
For now, here are links to the latest versions of the plug-in files:

- [32 bit dll](https://dl.dropboxusercontent.com/u/28976681/reaper_osara32.dll?dl=1)
- [64 bit dll](https://dl.dropboxusercontent.com/u/28976681/reaper_osara64.dll?dl=1)

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

## Usage

### Supported REAPER and Extension Actions
OSARA supports reporting of information for the following actions.
Most of these are actions built into REAPER, but a few are very useful actions from the popular SWS extension.
If keyboard shortcuts aren't bound to them already, you will need to do this yourself.

#### Track Navigation/Management
- Track: Go to next track
- Track: Go to previous track
- Track: Insert new track
- Track: Cycle track folder state
- Track: Cycle track folder collapsed state

#### Adjusting Track Parameters
- Track: Mute/unmute tracks
- Track: Solo/unsolo tracks
- Toggle record arming for current (last touched) track
- Track: Cycle track record monitor
- Track: Invert track phase
- Track: Nudge track volume up
- Track: Nudge track volume down
- (SWS extension) Xenakios/SWS: Nudge volume of selected tracks up
- (SWS extension) Xenakios/SWS: Nudge volume of selected tracks down
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

#### Markers and Regions
- Markers: Go to previous marker/project start
- Markers: Go to next marker/project end
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

#### Envelopes
- Track: Select previous envelope
- Track: Select next envelope

#### Zoom
- Zoom out horizontal
- Zoom in horizontal

### FX Parameter List
Many effects are unfortunately either partially or completely inaccessible.
However, most effects make their parameters available for automation in a standard way.
This can also be used to make them at least partially accessible.
OSARA can display a list of parameters for an effect and allow you to check and change the values of these parameters.

To do this:

1. Select a track with at least one effect.
2. Run the "OSARA: View FX parameters for current track" action.
 You will probably want to add a keyboard shortcut for this action so you can access it quickly.
3. If there is more than one effect on the track, select the desired effect from the menu.
4. Once the FX Parameters dialog appears, you can select a parameter from the Parameter combo box and check or adjust its value using the Value slider.
5. When you are done, press the Close button.
 Alternatively, you can press enter or escape.

Note that only some effects expose easily readable values, while others expose only percentages.

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

## Reporting Issues
Issues should be reported [on GitHub](https://github.com/jcsteh/osara/issues).

## Building
This section is for those interested in building OSARA from source code.

You will need:

- Microsoft Visual Studio 2012 or later for Windows Desktop.
 The Express edition is fine.
- [SCons](http://www.scons.org/), version 2.3.2 or later

To build OSARA, from a command prompt, simply change to the OSARA checkout directory and run scons.
The resulting dll files can be found at build\x86\reaper_osara32.dll and build\x86_64\reaper_osara64.dll.
