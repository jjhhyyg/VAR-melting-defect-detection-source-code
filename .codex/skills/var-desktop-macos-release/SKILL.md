---
name: var-desktop-macos-release
description: Use when working in this repository on macOS desktop packaging, codesigning, notarization, stapling, or release troubleshooting for the Tauri frontend app. Covers fast local ad-hoc builds, local Developer ID signed verification builds, public release builds, notarization status checks, and common pitfalls such as tauri build inheriting Apple env vars too early or missing nested Mach-O binaries under runtime resources.
---

# VAR Desktop macOS Release

This skill is for the Tauri desktop app in `frontend/`.

Use it when the task involves:

- macOS app packaging
- Developer ID codesign
- notarization
- stapling and Gatekeeper validation
- release failure diagnosis

## Working Directory

Run release commands from:

```bash
cd /Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend
```

## Responsibility Layers

Treat the desktop workflow as three separate layers. Do not collapse them into one mental model.

### Layer 1: `tauri dev`

Use for interactive local development only.

- Starts the frontend dev flow through Tauri dev mode.
- Uses the Tauri dev configuration path, not the release bundle path.
- Does not produce a distributable `.app` or `.dmg`.
- Does not perform Developer ID release signing, notarization, stapling, or Gatekeeper validation.

Typical command:

```bash
npm run desktop:dev
```

### Layer 2: raw `tauri build`

Use for raw build-and-bundle work, not as the final public release command in this repository.

- Runs `beforeBuildCommand`, which in this project means `npm run generate && npm run desktop:build-worker`.
- Builds the Rust Tauri app.
- Bundles macOS artifacts such as `.app` and `.dmg`.
- Copies configured resources such as `resources/models/**/*` and `resources/runtime/**/*` into the app bundle.
- May perform Tauri-managed signing behavior if Apple env vars are visible, which is exactly why this layer is dangerous here.

Typical command:

```bash
npm run desktop:build
```

### Layer 3: `scripts/macos-release.mjs`

This is the repository-specific release orchestrator and the canonical entrypoint for real macOS release work.

- Wraps `tauri build` instead of replacing it.
- Clears Apple notarization env vars before invoking the raw build so Tauri does not notarize too early.
- Re-signs every nested Mach-O under the bundled runtime resources.
- Runs stricter local verification than raw `tauri build`.
- Optionally notarizes, then staples and validates release artifacts.

Use this layer for anything release-like:

```bash
npm run desktop:macos:ad-hoc
npm run desktop:macos:release-local
npm run desktop:macos:release-public
```

## Release Lanes

Use the cheapest lane that matches the goal.

- Fast local smoke build:

```bash
npm run desktop:macos:ad-hoc
```

- Local release-like build with Developer ID signing but without notarization:

```bash
npm run desktop:macos:release-local
```

- Public release build with notarization:

```bash
npm run desktop:macos:preflight
npm run desktop:macos:release-public
```

## Command Selection

- If the goal is to debug UI or application behavior locally, use Layer 1.
- If the goal is to inspect whether the project can build and bundle at all, use Layer 2.
- If the goal is to produce anything another machine should install or Gatekeeper should trust, use Layer 3.
- In this repository, asking whether raw `tauri build` is "enough for release" usually means the wrong layer is being used.

## Dependency Installation

Use the dependency install command that matches the situation.

- Use `npm install` during local development, especially when adding, removing, or changing dependencies.
- Use `npm ci` for clean, reproducible build environments where `package-lock.json` must be treated as the source of truth.
- If `package.json` and `package-lock.json` disagree, `npm ci` should fail. Treat that as a real lockfile problem, not a reason to fall back blindly to `npm install`.
- Do not confuse `npm ci` with a CI pipeline. It is an npm install mode, not automation by itself.

## Guardrails

- Do not run raw `tauri build` with `APPLE_SIGNING_IDENTITY`, `APPLE_API_KEY`, `APPLE_API_ISSUER`, or `APPLE_API_KEY_PATH` in scope.
- `frontend/scripts/macos-release.mjs` is the canonical entrypoint for public macOS release work.
- That script intentionally clears Apple notarization env vars during `tauri build`, then re-signs every nested Mach-O under `src-tauri/resources/runtime`, then submits notarization.
- Before notarization, `spctl` showing `Unnotarized Developer ID` is expected. It means the app is Developer ID signed but not yet notarized.

## Status And Recovery

- If the release script is still running, prefer waiting for it instead of polling in parallel.
- If the local wait process was interrupted, check status with `notarytool info` and the submission id printed by the release script.
- If the submission is `Invalid`, fetch the notarization log immediately.
- If the submission is `Accepted` but the local process already died, manually `staple` and `validate` the `.app` and `.dmg`.

For concrete commands and failure patterns, read `references/notarization.md`.
