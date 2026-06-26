#!/bin/bash
set -e
version=$1
zip_file=../osara_$version.zip
app_name="OSARAInstaller.app"
cd "`dirname \"$0\"`"

# Compile the script into the app bundle using JavaScript for Automation
osacompile -l JavaScript -o "$app_name" "Install OSARA.js"

# Update the Info.plist with our custom bundle information
cat > "$app_name/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>applet</string>
	<key>CFBundleIdentifier</key>
	<string>co.osara.installer</string>
	<key>CFBundleName</key>
	<string>Install OSARA</string>
	<key>CFBundleVersion</key>
	<string>$version</string>
	<key>CFBundleShortVersionString</key>
	<string>$version</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>LSMinimumSystemVersion</key>
	<string>10.9</string>
	<key>NSAppleEventsUsageDescription</key>
	<string>This app needs to use AppleEvents to install OSARA components.</string>
	<key>NSAppleScriptEnabled</key>
	<true/>
	<key>OSAScriptingDefinition</key>
	<string>AppleScriptKit</string>
</dict>
</plist>
EOF

# Copy resources into the app bundle
cp ../../build/reaper_osara.dylib "$app_name/Contents/Resources/"
cp ../../copying.txt "$app_name/Contents/Resources/"
cp ../../config/mac/reaper-kb.ini "$app_name/Contents/Resources/OSARA.ReaperKeyMap"
mkdir -p "$app_name/Contents/Resources/locale"
cp ../../locale/*.po "$app_name/Contents/Resources/locale/"

# Create the final zip file with the app bundle and license
rm -f $zip_file
zip -r $zip_file "$app_name"

echo "Created installer package: $zip_file"
