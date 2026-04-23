# macOS 桌面端发布指南

本文档只覆盖 `frontend` 子项目里的 Tauri macOS 桌面端发布流程。  
服务器端 Docker 部署仍以 [`项目接手、开发测试与部署指南.md`](./项目接手、开发测试与部署指南.md) 为准。

如果你只是改页面、调接口或验证业务行为，不要跑正式发布流程。这个项目的桌面端包体很重，运行时资源里包含大量嵌套 Mach-O 二进制，发布成本明显高于普通前端构建。

## 1. 工作目录

所有桌面端命令都从 `frontend/` 目录运行：

```bash
cd /Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend
```

## 2. 三层职责边界

桌面端流程分三层。不要把“开发”“构建”“发布”混成一个动作。

### 2.1 本地开发层：`tauri dev`

用途：交互式本地开发。

```bash
npm run desktop:dev
```

这一层只负责开发调试：

- 启动 Tauri dev mode
- 使用前端开发服务
- 不产出可分发 `.app` 或 `.dmg`
- 不做 Developer ID release 签名
- 不做 notarization、staple、Gatekeeper 验证

### 2.2 原始构建层：raw `tauri build`

用途：验证项目能否完成原始构建和打包。

```bash
npm run desktop:build
```

这一层会做：

- 执行 `beforeBuildCommand`
- 运行 `npm run generate`
- 运行 `npm run desktop:build-worker`
- 编译 Rust Tauri app
- 生成 macOS `.app` 和 `.dmg`
- 把 `resources/models/**/*` 和 `resources/runtime/**/*` 复制进 app bundle

这一层不是本仓库的正式发布入口。尤其不要在 `APPLE_SIGNING_IDENTITY`、`APPLE_API_KEY`、`APPLE_API_ISSUER` 或 `APPLE_API_KEY_PATH` 已导出的 shell 里直接跑裸 `tauri build`，否则 Tauri 可能在 runtime 嵌套二进制重签名前抢先发起 notarization。

### 2.3 发布编排层：`scripts/macos-release.mjs`

用途：生成真正可以对外分发的 macOS 产物。

这一层是本仓库唯一可信的 macOS 发布入口：

```bash
npm run desktop:macos:ad-hoc
npm run desktop:macos:release-local
npm run desktop:macos:release-public
```

它会包装 raw `tauri build`，并补上本项目必须做的发布动作：

- 构建前避免 Tauri 过早继承 Apple notarization 环境变量
- 构建后重签 runtime 里的嵌套 Mach-O
- 严格校验 `.app`、`.dmg` 和嵌套二进制签名
- 正式发布时提交 notarization
- notarization 通过后执行 `staple` 和 `validate`

## 3. 发布通道

用最便宜的通道解决当前目标，不要动不动就跑完整 notarization。

### 3.1 本地快速冒烟

```bash
npm run desktop:macos:ad-hoc
```

用于快速检查桌面端能否打出本机可运行包。不适合发给别人安装。

### 3.2 本地 release-like 验证

```bash
npm run desktop:macos:release-local
```

用于验证 Developer ID 签名、runtime 重签和本地校验逻辑，但不提交 notarization。

### 3.3 对外发布

```bash
npm run desktop:macos:preflight
npm run desktop:macos:release-public
```

只有准备给其他机器安装时才走这条。notarization 绑定具体产物，每次正式发布构建都要重新提交。

## 4. Apple 凭据

正式发布需要在当前 shell 中提供 Apple Developer ID 和 notarytool 凭据：

```bash
export APPLE_SIGNING_IDENTITY="Developer ID Application: YANGYANG HOU (JU68L3U235)"
export APPLE_API_KEY="9SY55VDLYX"
export APPLE_API_ISSUER="6d7d24bb-2a58-4120-8943-0ba2ffd38d42"
export APPLE_API_KEY_PATH="/Users/erikssonhou/Documents/Team API Key/AuthKey_9SY55VDLYX.p8"
```

不要把 `.p8` 私钥提交进仓库。更稳的长期做法是把凭据存进 notarytool keychain profile，但不要把个人机器路径写死到自动化配置里。

## 5. 产物位置

常见产物路径：

```text
/Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend/src-tauri/target/release/bundle/macos/VAR Desktop.app
/Users/erikssonhou/Projects/VAR熔池挑战/codes/frontend/src-tauri/target/release/bundle/dmg/VAR Desktop_0.1.0_aarch64.dmg
```

版本号或架构变化时，DMG 文件名可能变化。不要把一次构建的文件名当成永久常量。

## 6. notarization 状态判断

查询状态：

```bash
xcrun notarytool info <submission-id> \
  --key "$APPLE_API_KEY_PATH" \
  --key-id "$APPLE_API_KEY" \
  --issuer "$APPLE_API_ISSUER"
```

只看 `status` 字段：

- `In Progress`：Apple 还在处理，不代表成功也不代表失败
- `Accepted`：notarization 通过，可以 staple
- `Invalid`：notarization 失败，立刻拉日志

失败日志：

```bash
xcrun notarytool log <submission-id> \
  --key "$APPLE_API_KEY_PATH" \
  --key-id "$APPLE_API_KEY" \
  --issuer "$APPLE_API_ISSUER"
```

如果日志里出现 `not signed with a valid Developer ID certificate`、`signature does not include a secure timestamp` 或 `hardened runtime disabled`，优先检查失败路径是否是 `resources/runtime` 下漏签的 `.dylib`、`.so`、`ffmpeg`、`ffprobe`、`desktop_worker` 或 `torch/bin/*`。

## 7. 强制红线

- 不要把 raw `tauri build` 当正式发布命令。
- 不要在 Apple 凭据已导出的 shell 里直接跑裸 `tauri build`。
- 不要把 `Successfully received submission info` 当成 notarization 成功，它只表示查询成功。
- 不要在 `In Progress` 阶段反复重新提交同一个包。
- 不要只看主 `.app` 的 `codesign --deep --verify`，这个项目必须关注 runtime 里的嵌套 Mach-O。

## 8. 相关项目知识

给 Codex 或其他 coding agent 处理这个发布流程时，优先让它读取：

```text
/Users/erikssonhou/Projects/VAR熔池挑战/codes/.codex/skills/var-desktop-macos-release/SKILL.md
```

更细的 notarytool 命令和常见失败路径在：

```text
/Users/erikssonhou/Projects/VAR熔池挑战/codes/.codex/skills/var-desktop-macos-release/references/notarization.md
```
