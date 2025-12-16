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

## Apple Developer Requirements

For production releases, the CI pipeline requires:
- Apple Developer Program membership
- Developer ID Application certificate
- App-specific password for notarization
- Team ID

### Required Secrets (for CI)
- `APPLE_ID`: Apple ID email
- `APPLE_ID_PASSWORD`: App-specific password
- `APPLE_TEAM_ID`: Apple Developer Team ID
- `APPLE_CERTIFICATE_P12`: Base64-encoded certificate
- `APPLE_CERTIFICATE_PASSWORD`: Certificate password
