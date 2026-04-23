# Project Instructions

- 对这个仓库里的 macOS 桌面打包、codesign、notarization、staple、Gatekeeper 校验问题，优先使用项目 skill：`.codex/skills/var-desktop-macos-release/SKILL.md`
- 对 `frontend/` 里的 Nuxt UI / Nuxt Icon 图标新增、替换、动态图标、logo icon 或离线图标缺失问题，优先使用项目 skill：`.codex/skills/nuxt-icon-client-bundle/SKILL.md`
- 所有桌面发布命令都从 `frontend/` 目录运行
- 桌面工作流分三层，不要混用：
  - `tauri dev` / `npm run desktop:dev`：只用于本地交互开发，不产出可分发包
  - raw `tauri build` / `npm run desktop:build`：只用于原始构建和打包，不是这个仓库的正式发布入口
  - `frontend/scripts/macos-release.mjs` / `npm run desktop:macos:*`：这个仓库唯一可信的 macOS 发布入口，负责 release 编排、嵌套 Mach-O 重签名、校验、notarization、staple
- 不要把 `APPLE_*` 签名或 notarization 环境变量直接传给裸 `tauri build`；统一走 `frontend/scripts/macos-release.mjs` 或对应的 npm scripts
