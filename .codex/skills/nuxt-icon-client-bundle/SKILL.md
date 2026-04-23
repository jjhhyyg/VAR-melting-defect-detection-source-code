---
name: nuxt-icon-client-bundle
description: Use this skill when adding, changing, replacing, or reviewing Nuxt UI/Nuxt Icon icons in the frontend, including UIcon, UButton icon props, trailing-icon, leading-icon, dynamic icon strings, logo icons, or any i-lucide/i-simple-icons/i-carbon/i-heroicons/i-mdi usage. Ensures newly introduced icons are included in Nuxt Icon clientBundle for offline desktop use.
metadata:
  short-description: Verify Nuxt icons are bundled offline
---

# Nuxt Icon Client Bundle

Use this for this repository whenever touching Nuxt UI/Nuxt Icon usage.

## Rules

- Prefer static icon names such as `icon="i-lucide-refresh-cw"` or `name="i-lucide-video"` so `icon.clientBundle.scan` can include them automatically.
- Do not rely on dynamically constructed icon names such as `` `i-lucide-${name}` `` for desktop-critical UI. If dynamic names are unavoidable, add the resolved icons explicitly to `frontend/nuxt.config.ts` under `icon.clientBundle.icons`.
- Do not edit `.nuxt/nuxt-icon-client-bundle.mjs` directly. It is generated output.
- After adding or changing icons, regenerate Nuxt output with `npm run typecheck` or the relevant Nuxt dev/build command from `frontend/`.
- Verify the generated bundle contains the icons before finishing the task.

## Verification

From the repository root, run:

```bash
node .codex/skills/nuxt-icon-client-bundle/scripts/verify-nuxt-icon-client-bundle.mjs
```

The script scans static `i-lucide-*`, `i-simple-icons-*`, `i-carbon-*`, `i-heroicons-*`, and `i-mdi-*` usages under `frontend/app`, then checks `.nuxt/nuxt-icon-client-bundle.mjs`.

If it reports missing icons:

- Confirm the icon name exists in the installed `@iconify-json/*` package.
- Replace invalid icon names with valid ones when possible.
- For legitimate dynamic icons, add explicit `collection:name` entries to `icon.clientBundle.icons` in `frontend/nuxt.config.ts`, then regenerate and rerun verification.

## Completion Criteria

- `npm run typecheck` passes from `frontend/`, unless the user explicitly asks not to run it.
- The verification script reports all scanned static icons are present in the client bundle.
- The final response states which icons were added or changed and whether bundle verification passed.
