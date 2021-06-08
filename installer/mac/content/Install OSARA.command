#!/bin/bash
osascript -l JavaScript - "`dirname \"$0\"`/.data/" << 'EOF'
"use strict";
var app = Application.currentApplication();
app.includeStandardAdditions = true;
var finder = Application("Finder");

function isPortableReaper ( dir ) {
	return (
		finder.exists(Path(dir + "/REAPER.app")) ||
		finder.exists(Path(dir + "/REAPER-ARM.app")) ||
		finder.exists(Path(dir + "/REAPER64.app")) ||
		finder.exists(Path(dir + "REAPER32.app")) )
}

function run(argv) {
	var source = argv[0];
	 var res = app.displayDialog("Choose weather to install Osara into a standard or a portable Reaper", {
		buttons: ["Standard Reaper Installation", "Portable Reaper Installation", "Cancel"],
		defaultButton: "Standard Reaper Installation",
		cancelButton: "Cancel"
	});
	var portable = (res.buttonReturned === "Portable Reaper Installation");
	var target;
	if (portable === true) {
		while(true) {
			target = app.chooseFolder({
				withPrompt:"Choose the folder containing your portable Reaper installation:"
			});
			if(isPortableReaper(target)) {
				break;
			} // user chose a folder that doesn't contain Reaper.
			app.displayDialog("The folder you chose does not contain an installation of Reaper.");
		}
	} else {
		target = app.pathTo("home folder") + "/Library/Application Support/REAPER"
	}
	target = target.toString();

	var s = app.doShellScript;
	try{
		s(`mkdir -p '${target}/UserPlugins'`);
	} catch (ignore){}// directory probably already exists
	s(`cat '${source}/reaper_osara.dylib' > '${target}/UserPlugins/reaper_osara.dylib'`);
	try{
		s(`mkdir '${target}/KeyMaps'`);
	} catch(ignore) {} // directory probably already exists
	s(`cp '${source}/OSARA.ReaperKeyMap' '${target}/KeyMaps/'`);
	s(`mkdir -p '${target}/osara/locale'`);
	s(`cp '${source}/locale/'* '${target}/osara/locale/'`);
	var res = app.displayDialog(
		"Do you want to replace the existing keymap with the Osara keymap?", {
		buttons: ["Yes", "No"],
		defaultButton: "Yes"});
	if(res.buttonReturned==="Yes") {
		try{
			s(`cp '${target}/reaper-kb.ini' '${target}/KeyMaps/backup.ReaperKeyMap'`);
		} catch(ignore) {} // there might not be a keymap to backup
		s(`cp '${target}/KeyMaps/OSARA.ReaperKeyMap' '${target}/reaper-kb.ini'`);
	}
}
EOF
