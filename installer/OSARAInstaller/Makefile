# Makefile for new OSARA Mac Installer

# Configuration
TARGET = OSARAInstaller
SWELL_DIR = ../../include/WDL/WDL/swell
BUILD_DIR = ../build
OBJ_DIR = $(BUILD_DIR)/objects
GEN_DIR = $(BUILD_DIR)/generated
RESOURCES_DIR = $(BUILD_DIR)/Resources

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++20 -O2 -I$(SWELL_DIR) -I. -I$(GEN_DIR) -I../../include -I../../include/tinygettext/include -I../../include/fmt/include
LDFLAGS = -framework Cocoa -framework Carbon

# Source files
SOURCES = main.mm \
          installer_app.cpp \
          dialog_procs.mm \
          translation.cpp

# External library sources
TINYGETTEXT_SOURCES = ../../include/tinygettext/src/dictionary.cpp \
                      ../../include/tinygettext/src/language.cpp \
                      ../../include/tinygettext/src/log.cpp \
                      ../../include/tinygettext/src/plural_forms.cpp \
                      ../../include/tinygettext/src/po_parser.cpp \
                      ../../include/tinygettext/src/tinygettext.cpp

FMT_SOURCES = ../../include/fmt/src/format.cc \
              ../../include/fmt/src/os.cc

# SWELL source files (complete set for standalone app)
SWELL_SOURCES = $(SWELL_DIR)/swell-modstub.mm \
                $(SWELL_DIR)/swell-wnd.mm \
                $(SWELL_DIR)/swell-menu.mm \
                $(SWELL_DIR)/swell-dlg.mm \
                $(SWELL_DIR)/swell-gdi.mm \
                $(SWELL_DIR)/swell-misc.mm \
                $(SWELL_DIR)/swell-miscdlg.mm \
                $(SWELL_DIR)/swell-kb.mm \
                $(SWELL_DIR)/swell-ini.cpp \
                $(SWELL_DIR)/swell.cpp

# Object files
OBJECTS = $(addprefix $(OBJ_DIR)/, $(SOURCES:.cpp=.o))
OBJECTS := $(OBJECTS:.mm=.o)
SWELL_OBJECTS = $(addprefix $(OBJ_DIR)/, $(notdir $(SWELL_SOURCES:.cpp=.o)))
SWELL_OBJECTS := $(SWELL_OBJECTS:.mm=.o)
TINYGETTEXT_OBJECTS = $(addprefix $(OBJ_DIR)/, $(notdir $(TINYGETTEXT_SOURCES:.cpp=.o)))
FMT_OBJECTS = $(addprefix $(OBJ_DIR)/, $(notdir $(FMT_SOURCES:.cc=.o)))

.PHONY: all clean resources app-bundle sign-app

all: resources $(TARGET)

# Generate SWELL resources
$(GEN_DIR)/installer.rc_mac_dlg $(GEN_DIR)/installer.rc_mac_menu: installer.rc
	@echo "Generating SWELL resources..."
	@mkdir -p $(GEN_DIR)
	@/opt/homebrew/bin/php $(SWELL_DIR)/swell_resgen.php installer.rc || php $(SWELL_DIR)/swell_resgen.php installer.rc
	@mv installer.rc_mac_dlg $(GEN_DIR)/ 2>/dev/null || true
	@mv installer.rc_mac_menu $(GEN_DIR)/ 2>/dev/null || true

resources: $(GEN_DIR)/installer.rc_mac_dlg $(GEN_DIR)/installer.rc_mac_menu

# Build the executable
$(TARGET): $(OBJECTS) $(SWELL_OBJECTS) $(TINYGETTEXT_OBJECTS) $(FMT_OBJECTS)
	@echo "Linking $(TARGET)..."
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(OBJECTS) $(SWELL_OBJECTS) $(TINYGETTEXT_OBJECTS) $(FMT_OBJECTS) $(LDFLAGS) -o $(BUILD_DIR)/$(TARGET)

# Compile C++ source files
$(OBJ_DIR)/%.o: %.cpp
	@echo "Compiling $<..."
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile Objective-C++ source files
$(OBJ_DIR)/%.o: %.mm
	@echo "Compiling $<..."
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# main.mm depends on generated resources
$(OBJ_DIR)/main.o: $(GEN_DIR)/installer.rc_mac_dlg $(GEN_DIR)/installer.rc_mac_menu

# Compile SWELL source files
$(OBJ_DIR)/%.o: $(SWELL_DIR)/%.cpp
	@echo "Compiling $<..."
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SWELL_DIR)/%.mm
	@echo "Compiling $<..."
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile tinygettext source files
$(OBJ_DIR)/%.o: ../../include/tinygettext/src/%.cpp
	@echo "Compiling $<..."
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile fmt source files
$(OBJ_DIR)/%.o: ../../include/fmt/src/%.cc
	@echo "Compiling $<..."
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create app bundle
app-bundle: $(TARGET)
	@echo "Creating app bundle..."
	@mkdir -p $(BUILD_DIR)/$(TARGET).app/Contents/MacOS
	@mkdir -p $(BUILD_DIR)/$(TARGET).app/Contents/Resources
	@mkdir -p $(BUILD_DIR)/$(TARGET).app/Contents/Resources/locale
	@cp $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(TARGET).app/Contents/MacOS/
	@cp Info.plist $(BUILD_DIR)/$(TARGET).app/Contents/
	@if [ -d "$(RESOURCES_DIR)" ]; then cp -r $(RESOURCES_DIR)/* $(BUILD_DIR)/$(TARGET).app/Contents/Resources/; fi
	@echo "Copying locale files..."
	@cp -r ../../locale/*.po $(BUILD_DIR)/$(TARGET).app/Contents/Resources/locale/ 2>/dev/null || echo "Warning: No locale files found"
	@echo "Copying license file..."
	@cp ../../copying.txt $(BUILD_DIR)/$(TARGET).app/Contents/Resources/ 2>/dev/null || echo "Warning: License file not found"
	@echo "App bundle created: $(BUILD_DIR)/$(TARGET).app"
	@echo "Code signing application..."
	@$(MAKE) sign-app

# Clean build files
clean:
	@echo "Cleaning..."
	@rm -rf $(BUILD_DIR)

# Code signing
sign-app:
ifeq ($(CI),true)
	@echo "CI environment detected, skipping code signing (will be signed later)"
	@echo "App bundle created: $(BUILD_DIR)/$(TARGET).app"
else
	@echo "Local build - using ad-hoc signing for testing..."
	@echo "Signing app bundle with entitlements..."
	@codesign --force --sign - --entitlements OSARAInstaller.entitlements $(BUILD_DIR)/$(TARGET).app/Contents/MacOS/$(TARGET)
	@codesign --force --sign - --entitlements OSARAInstaller.entitlements $(BUILD_DIR)/$(TARGET).app
	@echo "Verifying code signature..."
	@codesign --verify --verbose=2 $(BUILD_DIR)/$(TARGET).app
	@echo "Checking entitlements..."
	@codesign -d --entitlements - $(BUILD_DIR)/$(TARGET).app
	@if [ $$? -eq 0 ]; then \
		echo "✅ Code signing with entitlements successful!"; \
		echo ""; \
		echo "App bundle is ready: $(BUILD_DIR)/$(TARGET).app"; \
		echo ""; \
		echo "⚠️  Note: This app is ad-hoc signed for local testing only."; \
	else \
		echo "❌ Code signing failed!"; \
		exit 1; \
	fi
endif

# Help
help:
	@echo "Available targets:"
	@echo "  all        - Build the installer (default)"
	@echo "  resources  - Generate SWELL resources only"
	@echo "  app-bundle - Create macOS app bundle with code signing"
	@echo "  sign-app   - Code sign the app bundle"
	@echo "  clean      - Clean build files"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  make                    # Build installer"
	@echo "  make app-bundle         # Build and create signed app bundle"
	@echo "  make clean              # Clean build files"
