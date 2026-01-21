# Apple Developer Certificate Setup for OSARA

This guide covers obtaining and configuring Apple Developer certificates for signing and notarizing OSARA builds.

## Prerequisites

- Apple Developer Program membership ($99/year)
- macOS development machine with Xcode installed

## Step 1: Obtain Developer ID Application Certificate

1. **Log into Apple Developer Portal**
   - Go to https://developer.apple.com/account
   - Sign in with your Apple ID

2. **Create Certificate Signing Request (CSR)**
   - Open Keychain Access on your Mac
   - Go to Keychain Access → Certificate Assistant → Request a Certificate from a Certificate Authority
   - Enter your email address and name
   - Select "Saved to disk" and "Let me specify key pair information"
   - Click Continue and save the CSR file

3. **Generate Certificate**
   - In Apple Developer Portal, go to Certificates, Identifiers & Profiles
   - Click the "+" button to create a new certificate
   - Select "Developer ID Application" under "Production"
   - Upload your CSR file
   - Download the generated certificate (.cer file)

4. **Install Certificate**
   - Double-click the downloaded .cer file to install it in Keychain Access
   - The certificate should appear in your "login" keychain

## Step 2: Export Certificate for CI

1. **Export as P12**
   - In Keychain Access, find your "Developer ID Application" certificate
   - Right-click and select "Export"
   - Choose "Personal Information Exchange (.p12)" format
   - Set a strong password and save the file

2. **Convert to Base64**
   ```bash
   base64 -i your_certificate.p12 | pbcopy
   ```
   This copies the base64-encoded certificate to your clipboard

## Step 3: Set Up App-Specific Password

1. **Generate App-Specific Password**
   - Go to https://appleid.apple.com
   - Sign in and go to Security section
   - Under "App-Specific Passwords", click "Generate Password"
   - Enter a label like "OSARA CI Notarization"
   - Save the generated password securely

## Step 4: Configure GitHub Secrets

Add these secrets to your GitHub repository:

- `APPLE_ID`: Your Apple ID email address
- `APPLE_ID_PASSWORD`: The app-specific password from Step 3
- `TEAM_ID`: Your Apple Developer Team ID (found in Apple Developer Portal)
- `APPLE_CERTIFICATE_P12`: The base64-encoded certificate from Step 2
- `APPLE_CERTIFICATE_PASSWORD`: The password you set when exporting the P12

## Step 5: Find Your Team ID

1. Go to https://developer.apple.com/account
2. Look for "Team ID" in the membership section
3. It's a 10-character alphanumeric string (e.g., "ABCD123456")

## Security Notes

- Never commit certificates or passwords to version control
- Use app-specific passwords, not your main Apple ID password
- Store the P12 certificate securely as a backup
- Rotate app-specific passwords periodically

## Troubleshooting

**Certificate not found during signing:**
- Verify the certificate is properly imported in Keychain Access
- Check that it's a "Developer ID Application" certificate, not "Mac Developer"

**Notarization fails:**
- Ensure you're using an app-specific password, not your regular Apple ID password
- Verify your Team ID is correct
- Check that the app is signed with a Developer ID certificate

**"Developer ID Application" not available:**
- Ensure you have a paid Apple Developer Program membership
- Individual accounts can create Developer ID certificates
- Organization accounts may need admin approval

## Local vs CI Builds

**Local Development:**
- Local builds now use ad-hoc signing only
- No production certificates needed for local testing
- Apps will show security warnings but can be opened with right-click → Open

**CI/Production Builds:**
- CI automatically signs with Developer ID Application certificate
- Apps are notarized and ready for distribution
- No security warnings for end users

## Testing the Setup

After configuring the GitHub secrets, push a commit to the master branch and check the GitHub Actions workflow. The Mac signing step should:

1. Import the certificate successfully
2. Sign the app bundle with your Developer ID
3. Submit for notarization
4. Staple the notarization ticket
5. Create the final signed ZIP file

If any step fails, check the GitHub Actions logs for specific error messages and verify your secrets are configured correctly.
