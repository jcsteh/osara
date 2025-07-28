# Experimental Native Mac Installer

In addition to the existing OSARA Mac installer, which uses a disk image, OSARA also now supports an experimental native Mac installer. This installer is built into an app bundle, and can eventually be sined and notarized.

## SCons Build Targets

### Local Development
```bash
# Build legacy DMG installer (default)
scons

# Build native app installer
scons mac-native-installer
```

### CI/CD Pipeline Control

The GitHub Actions workflow uses the `USE_MAC_NATIVE_INSTALLER` environment variable:

- `USE_NATIVE_INSTALLER: false` (default) → Builds legacy DMG
- `USE_MAC_NATIVE_INSTALLER: true` → Builds native Mac app installer with signing/notarization

## Migration Path

### Phase 1: Current State (Legacy Default)
- Default builds use legacy DMG installer
- Native installer available for testing via `scons mac-native-installer`

### Phase 2: Enable Native Installer
When Apple Developer account is ready:
1. Set `USE_MAC_NATIVE_INSTALLER: true` in `.github/workflows/build.yml`
2. Ensure Apple Developer secrets are configured
3. CI will build and sign native installer

## Apple Developer Requirements

The native installer requires:
- Apple Developer Program membership ($99/year)
- Developer ID Application certificate
- App-specific password for notarization
- Team ID

### Required Secrets (for CI)
- `APPLE_ID`: Apple ID email
- `APPLE_ID_PASSWORD`: App-specific password
- `TEAM_ID`: Apple Developer Team ID
- `APPLE_CERTIFICATE_P12`: Base64-encoded certificate
- `APPLE_CERTIFICATE_PASSWORD`: Certificate password
