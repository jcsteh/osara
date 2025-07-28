#!/bin/bash
set -e

# This script builds the new native Mac OSARA installer
version=$1
if [ -z "$version" ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 2024.1"
    exit 1
fi

# Set some directory variables for shorthand
cd "`dirname \"$0\"`"
cd ..
INSTALLER_DIR=$PWD
RESOURCES_DIR="$INSTALLER_DIR/build/Resources"
cd ..
PROJECT_ROOT=$PWD

echo "Building OSARA native Mac Installer version $version..."

# Ensure we have the latest plugin build
if [ ! -f "build/reaper_osara.dylib" ]; then
    echo "Error: reaper_osara.dylib not found. Build OSARA first."
    echo "Run: scons from the project root directory"
    exit 1
fi

# Create Resources directory for installer
mkdir -p "$RESOURCES_DIR"

# Copy latest plugin to installer resources
cp "build/reaper_osara.dylib" "$RESOURCES_DIR/"

# Copy keymap file
if [ -f "config/mac/reaper-kb.ini" ]; then
    cp "config/mac/reaper-kb.ini" "$RESOURCES_DIR/OSARA.ReaperKeyMap"
else
    echo "Warning: OSARA keymap not found at $PROJECT_ROOT/config/mac/reaper-kb.ini"
fi

# Copy license file
if [ -f "copying.txt" ]; then
    cp "copying.txt" "$RESOURCES_DIR/"
else
    echo "Warning: License file not found at $PROJECT_ROOT/copying.txt"
fi

# Copy locale files
if [ -d "locale" ]; then
    cp -r "locale" "$RESOURCES_DIR/"
else
    echo "Warning: Locale directory not found at $PROJECT_ROOT/locale"
fi

# Copy any additional documentation
if [ -f "readme.md" ]; then
    cp "readme.md" "$RESOURCES_DIR/"
fi

echo "Resources copied successfully."

# Build using Makefile
echo "Building installer executable..."
cd $INSTALLER_DIR/OSARAInstaller
make app-bundle

echo "Build complete: $INSTALLER_DIR/build/OSARAInstaller.app"
echo ""
echo "Checking code signature status..."
if codesign --verify --verbose=2 "$INSTALLER_DIR/build/OSARAInstaller.app" 2>/dev/null; then
    echo "App is properly code signed"
    
    # Check what type of signature it has
    if codesign -dv "$INSTALLER_DIR/build/OSARAInstaller.app" 2>&1 | grep -q "Developer ID Application"; then
        echo "Signed with Developer ID Application certificate - ready for distribution!"
    elif codesign -dv "$INSTALLER_DIR/build/OSARAInstaller.app" 2>&1 | grep -q "Mac Developer\|Apple Development"; then
        echo "Signed with development certificate - good for testing"
    else
        echo "Warning: App is ad-hoc signed - self-signed"
    fi
else
    echo "Warning: App signature verification failed"
fi

# Create versioned output files
if [ "$CI" = "true" ]; then
    echo "CI build complete. Unsigned app bundle ready at: $INSTALLER_DIR/build/OSARAInstaller.app"
    echo "App will be signed and notarized by CI pipeline."
else
    # Create versioned zip file from the app bundle for local use
    if [ -d "$INSTALLER_DIR/build/OSARAInstaller.app" ]; then
        echo "Creating local distribution package..."
        cd "$INSTALLER_DIR"
        if [ -f "osara_${version}.zip" ]; then
            rm "osara_${version}.zip"
        fi
        cd build
        zip -r "../osara_${version}.zip" "OSARAInstaller.app"
        echo "Created: $INSTALLER_DIR/osara_${version}.zip"
        cd - > /dev/null
    fi
fi

echo ""
echo "OSARA Installer build complete!"
echo ""

if [ "$CI" = "true" ]; then
    echo "CI build ready for signing and notarization pipeline."
else
    echo "Local build files are located in: $INSTALLER_DIR/build"
fi
