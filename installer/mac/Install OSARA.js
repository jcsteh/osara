"use strict";
var app = Application.currentApplication();
app.includeStandardAdditions = true;
var finder = Application("Finder");

function getResourcesDir() {
	var bundleURL = $.NSBundle.mainBundle.bundleURL;
	if (bundleURL) {
		var bundlePath = bundleURL.path.js;
		return bundlePath + "/Contents/Resources";
	}
	
	var processPath = $.NSProcessInfo.processInfo.processName.js;
	var argCount = $.NSProcessInfo.processInfo.arguments.count;
	throw new Error("Could not determine Resources directory. Process: " + processPath + 
					", Args count: " + argCount + 
					", Bundle path: " + (bundleURL ? bundleURL.path.js : "null"));
}

function isPortableReaper ( dir ) {
	return (
		finder.exists(Path(dir + "/REAPER.app")) ||
		finder.exists(Path(dir + "/REAPER-ARM.app")) ||
		finder.exists(Path(dir + "/REAPER64.app")) ||
		finder.exists(Path(dir + "REAPER32.app")) )
}

function run(argv) {
	var source = getResourcesDir();
	 var res = app.displayDialog("Choose whether to install OSARA into a standard or a portable REAPER", {
		buttons: ["Standard REAPER Installation", "Portable REAPER Installation", "Cancel"],
		defaultButton: "Standard REAPER Installation",
		cancelButton: "Cancel"
	});
	var portable = (res.buttonReturned === "Portable Reaper Installation");
	var target;
	if (portable === true) {
		while(true) {
			target = app.chooseFolder({
				withPrompt:"Choose the folder containing your portable REAPER installation:"
			});
			if(isPortableReaper(target)) {
				break;
			} // user chose a folder that doesn't contain Reaper.
			app.displayDialog("The folder you chose does not contain an installation of REAPER.");
		}
	} else {
		target = app.pathTo("home folder") + "/Library/Application Support/REAPER"
	}
	target = target.toString();

	var s = app.doShellScript;
	try{
		s(`mkdir -p '${target}'`);
		s(`mkdir -p '${target}/UserPlugins'`);
	} catch (ignore){}// directory probably already exists
	s(`cp -f '${source}/reaper_osara.dylib' '${target}/UserPlugins/reaper_osara.dylib'`);
	try{
		s(`mkdir -p '${target}/KeyMaps'`);
	} catch(ignore) {} // directory probably already exists
	s(`cp -f '${source}/OSARA.ReaperKeyMap' '${target}/KeyMaps/'`);
	s(`mkdir -p '${target}/osara/locale'`);
	s(`cp -f '${source}/locale/'* '${target}/osara/locale/'`);
	var res = app.displayDialog(
		"Do you want to replace the existing keymap with the OSARA keymap?", {
		buttons: ["Yes", "No"],
		defaultButton: "Yes"});
	if(res.buttonReturned==="Yes") {
		try{
			s(`cp '${target}/reaper-kb.ini' '${target}/KeyMaps/backup.ReaperKeyMap'`);
		} catch(ignore) {} // there might not be a keymap to backup
		s(`cp '${target}/KeyMaps/OSARA.ReaperKeyMap' '${target}/reaper-kb.ini'`);
	}
	app.displayDialog("Installation Complete", {
		buttons: ["OK"],
		defaultButton: "OK"
	});
}
