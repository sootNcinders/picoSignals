Notarization and Codesigning for PicoSignals Config Editor

This document explains the GitHub Actions CI steps added to codesign and notarize macOS `.app` bundles, and the exact secrets you must add to your repository.

Secrets required (names used in the workflow):

- `APPLE_API_KEY` — Base64-encoded contents of your App Store Connect API key `.p8` file (AuthKey_XXXX.p8).
- `APPLE_KEY_ID` — The Key ID shown in App Store Connect for the API key (e.g., `ABC123XYZ`).
- `APPLE_ISSUER_ID` — The Issuer ID (your Team ID) from App Store Connect.
- `APPLE_P12` — Base64-encoded P12 (Personal Information Exchange) developer certificate used for Developer ID Application signing (if you prefer P12 approach).
- `APPLE_P12_PASSWORD` — Password for the P12 file.
- `CODESIGN_IDENTITY` — The identity string for codesign (e.g., `Developer ID Application: Your Name (TEAMID)`).

Notes on which secrets are required:
- For notarization using `notarytool` with the App Store Connect API key you need: `APPLE_API_KEY`, `APPLE_KEY_ID`, `APPLE_ISSUER_ID`.
- For codesigning (installing a P12 into the runner keychain and signing) you need: `APPLE_P12`, `APPLE_P12_PASSWORD`, and `CODESIGN_IDENTITY`.

How to create the App Store Connect API key (.p8):
1. Go to App Store Connect → Users and Access → Keys.
2. Click the "+" to create a new key, give it a name, select the role (Developer or App Manager), and enable the key. Download the `.p8` file.
3. Base64-encode the file and store as a secret:

   macOS / Linux:
   ```bash
   base64 AuthKey_ABC123.p8 > authkey.p8.base64
   # then copy/paste the contents of authkey.p8.base64 into the GitHub secret named APPLE_API_KEY
   ```

How to create a P12 (Developer ID Application) certificate (optional):
1. On a Mac with Keychain Access, export your Developer ID Application certificate and private key as a `.p12` (File → Export Items...).
2. Base64-encode the resulting `.p12` and save to GitHub secret `APPLE_P12`.

   macOS / Linux:
   ```bash
   base64 developer_identity.p12 > developer.p12.base64
   # paste contents into APPLE_P12 secret
   ```

3. Store the P12 password in the `APPLE_P12_PASSWORD` secret.

Setting up GitHub repository secrets:
- Go to your repository → Settings → Secrets and variables → Actions → New repository secret.
- Create secrets with the exact names used above.

How the workflow uses these secrets:
- `APPLE_API_KEY` is decoded on the runner to `/tmp/AuthKey.p8` and passed to `xcrun notarytool submit` along with `APPLE_KEY_ID` and `APPLE_ISSUER_ID`.
- If `APPLE_P12` and `APPLE_P12_PASSWORD` and `CODESIGN_IDENTITY` are provided, the workflow imports the P12 into a temporary keychain and runs `codesign` on the built `.app` prior to notarization.

Local testing tips:
- To mimic CI locally, export the encoded secrets into environment variables and run the same `ditto`, `codesign`, and `xcrun notarytool` commands described in the workflow.
- To base64-encode files for local testing, use `base64` as shown above.

Security notes:
- Keep keys and certificates secret. Use repository-level or organization-level secrets carefully.
- Prefer using App Store Connect API keys for notarization as they don’t require storing a long-lived machine cert with a private key.

If you want, I can also add a separate minimal workflow that only performs notarization (for re-submitting existing builds) or extend the current workflow to upload both signed and unsigned zips for debugging.