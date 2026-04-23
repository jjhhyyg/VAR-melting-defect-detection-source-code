# Notarization Notes

## Canonical Files

- Release driver:
  `/Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend/scripts/macos-release.mjs`
- App bundle:
  `/Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend/src-tauri/target/release/bundle/macos/VAR Desktop.app`
- DMG:
  `/Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend/src-tauri/target/release/bundle/dmg/VAR Desktop_0.1.0_aarch64.dmg`

## Manual Status Check

Replace `<submission-id>` with the id printed by `notarytool submit`.

```bash
xcrun notarytool info <submission-id> \
  --key "$APPLE_API_KEY_PATH" \
  --key-id "$APPLE_API_KEY" \
  --issuer "$APPLE_API_ISSUER"
```

Polling loop:

```bash
while true; do
  clear
  date '+%Y-%m-%d %H:%M:%S %Z'
  xcrun notarytool info <submission-id> \
    --key "$APPLE_API_KEY_PATH" \
    --key-id "$APPLE_API_KEY" \
    --issuer "$APPLE_API_ISSUER" \
  | rg 'status:|id:|createdDate:'
  sleep 20
done
```

## On Invalid

```bash
xcrun notarytool log <submission-id> \
  --key "$APPLE_API_KEY_PATH" \
  --key-id "$APPLE_API_KEY" \
  --issuer "$APPLE_API_ISSUER"
```

Focus on whether the rejected path is:

- a runtime tool under `resources/runtime/.../tools/`
- a worker executable under `resources/runtime/.../worker/desktop_worker/`
- a binary under `torch/bin/`
- a `.dylib` or `.so` nested under the Python runtime tree

If Apple says `not signed with a valid Developer ID certificate`, `missing secure timestamp`, or `hardened runtime disabled`, inspect whether the nested Mach-O collector in `macos-release.mjs` still catches that path class.

## On Accepted After Local Interruption

```bash
xcrun stapler staple '/Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend/src-tauri/target/release/bundle/macos/VAR Desktop.app'
xcrun stapler validate '/Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend/src-tauri/target/release/bundle/macos/VAR Desktop.app'
xcrun stapler staple '/Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend/src-tauri/target/release/bundle/dmg/VAR Desktop_0.1.0_aarch64.dmg'
xcrun stapler validate '/Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend/src-tauri/target/release/bundle/dmg/VAR Desktop_0.1.0_aarch64.dmg'
```

## Common Pitfalls

- `APPLE_API_ISSUER` is empty:
  `notarytool info` fails with `must be a valid UUID`.
- `tauri build` sees `APPLE_*` variables:
  Tauri may submit notarization too early, before runtime nested binaries are re-signed.
- `spctl` says `Unnotarized Developer ID` before submission:
  expected, not a release failure.
- Long `In Progress`:
  usually Apple queueing or scanning time, especially for this repository because the runtime bundle is large and contains many nested binaries.
