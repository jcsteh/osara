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
		finder.exists(Path(dir + "/REAPER32.app")) )
}

function run(argv) {
	var source = getResourcesDir();
	var s = app.doShellScript;
	var running = false;
	try {
		var code = s("pgrep -x 'REAPER|REAPER64|REAPER-ARM' >/dev/null 2>&1; echo $?");
		running = (code.trim() === "0");
	} catch (ignore) {}
	if (running) {
		app.displayDialog("OSARA cannot be installed while REAPER is running. Please close REAPER then run this installer again.", {
			buttons: ["OK"],
			defaultButton: "OK"
		});
		return;
	}
	 var res = app.displayDialog("Choose whether to install OSARA into a standard or a portable REAPER", {
		buttons: ["Standard REAPER Installation", "Portable REAPER Installation", "Cancel"],
		defaultButton: "Standard REAPER Installation",
		cancelButton: "Cancel"
	});
	var portable = (res.buttonReturned === "Portable REAPER Installation");
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

	try{
		s(`mkdir -p '${target}/UserPlugins'`);
	} catch (ignore){}// directory probably already exists
	s(`cat '${source}/reaper_osara.dylib' > '${target}/UserPlugins/reaper_osara.dylib'`);
	try{
		s(`mkdir -p '${target}/KeyMaps'`);
	} catch(ignore) {} // directory probably already exists
	s(`cp -f '${source}/OSARA.ReaperKeyMap' '${target}/KeyMaps/'`);
	s(`mkdir -p '${target}/osara/locale'`);
	s(`cp -f '${source}/locale/'* '${target}/osara/locale/'`);
}
