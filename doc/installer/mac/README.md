# Native Mac Installer

OSARA uses a native Mac installer built as an app bundle that can be signed and notarized for distribution.

## SCons Build Options

### Local Development
```bash
# Build with ad-hoc signing (default - for local testing)
scons

# Build without signing (for CI or when codesign is not available)
scons mac_signing_mode=none
```

### Signing Modes

The `mac_signing_mode` variable controls code signing behavior:

- `mac_signing_mode=ad-hoc` (default) → Ad-hoc signs for local testing
- `mac_signing_mode=none` → No signing (used by CI)

## CI/CD Pipeline

The GitHub Actions workflow automatically:

1. **PR Builds**: Build with `mac_signing_mode=none` (no signing)
2. **Push Builds**: Build with `mac_signing_mode=none`, then CI handles proper Developer ID signing and notarization

This approach separates build concerns from signing concerns, making the build system simpler and more reliable.

## Apple Developer Requirements

For production releases, the CI pipeline requires:
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

## Build Process

1. **SCons builds** the app bundle and handles local ad-hoc signing
2. **CI signing step** (push builds only) handles proper Developer ID signing and notarization
3. **Distribution zip** is created containing the signed app bundle

This approach ensures:
- Local builds work out of the box with ad-hoc signing
- PR builds don't require certificates
- Production builds are properly signed and notarized
