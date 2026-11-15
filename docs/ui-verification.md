# UI verification / 界面验证

## English

### Scope

The previous dialog stylesheet layer (`DialogStyle`) was deleted; every page now derives its spacing, radii and control heights from the single design-token stylesheet. Dialog `.ui` files are rebuilt as card + form-row structures instead of carrying legacy grid layouts. A shared design-token stylesheet defines the palette, radii (11/8) and control heights (32/30/26); the main window is now a declarative page bar, scrollable workspace and status bar. The log is an embedded card with toolbar, filter chips, records and footer instead of a dock widget.

The UI refactor preserves the separate GUI, core and Windows daemon processes. `PageHost` embeds existing editor transactions in the main window, replacing blocking editor calls with accepted/rejected callbacks. Native file pickers and OS permission dialogs remain native.

The home page contains selectable server/client cards, editable computer identity, connection controls and an address/port row. Server, client and preference layouts retain their existing configuration fields. Screen aliases are validated before replacement; cancelling the server editor restores its protocol. Explicit interface bindings are not replaced by automatic selection.

Update notes come from release metadata, not sample text. Download progress includes bytes, speed, percentage and estimated remaining time when a total is available. Each update page uses an independent temporary directory. A verified download requires a second explicit install action. Leaving the page aborts the download.

### Verification evidence

Local macOS arm64 static-Qt build verification on 2026-11-15:

- Full CMake build passed.
- CTest from `build/src/unittests`: 65 enabled targets passed. `OSXClipboardTests` and `OSXKeyStateTests` were Disabled / Not Run, not passes.
- `PageHostTests`: embedded acceptance, back cancellation, deferred destruction and parent/child dismissal.
- `EditorTransactionTests`: client save/cancel, alias replacement, stored/default protocol conversion, protocol cancellation, log filtering/counting/history cap and long-record height cap.
- Encoding check passed; C++ formatting uses clang-format 20.1.0.
- Actual GUI interaction used disposable settings: home, about, preferences (general/network/advanced/scope), server configuration (all four tabs), client configuration, screen settings, hotkey page and local fingerprint page were rendered and captured inside the single application window; the core was never started from that GUI.
- `DialogRenderTests` renders the hotkey, action, update and fingerprint pages offscreen and writes PNGs to `$TMPDIR/tupig-dialog-render`, because macOS blocks synthetic key recording without granting Accessibility permission (which this project must not do automatically).
- Fixed crash found by that run: `PageHost` looked up its page stack in the constructor, before `uic` created the child widgets, and reparenting cleared the editor title. Both are now handled (lazy lookup, captured title).
- Package verification checked arm64 Mach-O dependency closure, clean-environment Cocoa startup and a TLS 1.3 loopback handshake with a disposable trusted identity. No Synergy input-sharing greeting was sent.

### Platform scope

Visual acceptance targets macOS and Windows only. Linux builds still run in CI to keep the shared code compiling, but no Linux visual or behavioural acceptance is claimed.

### Not established

- The main window, log card, about page and preferences page were inspected against the design and match its structure; the remaining configuration pages still need the same per-page comparison.
- Native Windows/Linux rendering, DPI and hardware acceptance require their own environments. A macOS build is not proof of those platforms.
- No second-machine input sharing, live clipboard replacement, secure-desktop/UAC acceptance, OS permission grant or installer launch was tested.
- Hotkey recording and the update download/install flow were not exercised end to end: the first needs real key capture, the second needs a published release.
- Core diagnostic messages retain their original text; translating panel labels does not translate every core log message.
- Remote CI results must be checked against the pushed commit, not a previous green run.

### Reproduction

```sh
cmake --build build -j 4
ctest --test-dir build/src/unittests --output-on-failure -j 4
python3 -I scripts/ci/check-encoding.py --fail
scripts/ci/verify-package-macos.sh --expected-arch arm64 'build/bin/TuPig Synergy.app'
```

Keep `SYNERGY_ENABLE_INTERACTIVE_TESTS=OFF` on the everyday host. Do not bypass CTest by invoking interactive test executables directly.

## 中文

### 范围

界面重构保留 GUI、core 和 Windows daemon 三个独立进程。`PageHost` 将原编辑器事务嵌入主窗口，以 accepted/rejected 回调替代阻塞式编辑器调用。原生文件选择器和系统权限窗口继续由系统提供。

主页包含服务端/客户端模式卡片、可编辑计算机名称、连接控制及地址/端口同行。服务端、客户端和首选项保留原配置字段。计算机别名先完整校验再替换；取消服务端编辑恢复原协议。自动选择接口不覆盖明确设置的监听地址。

更新说明来自真实发布元数据，而非示例文字。总大小已知时，下载显示数据量、网速、百分比及预计剩余时间。每个更新页使用独立临时目录，校验完成后仍需明确点击安装，离开页面立即中止下载。

### 验证证据

2026年11月15日本机 macOS arm64 静态 Qt 构建验证：

- 完整 CMake 构建通过。
- 从 `build/src/unittests` 运行 CTest：65 个启用目标通过。`OSXClipboardTests` 和 `OSXKeyStateTests` 为 Disabled / Not Run，不计入通过。
- `PageHostTests`：嵌入页接受、返回取消、延迟销毁及父子页关闭。
- `EditorTransactionTests`：客户端保存/取消、别名替换、持久化/默认协议转换、取消协议、日志筛选/统计/历史上限及长记录高度上限。
- 编码检查通过；C++ 使用 clang-format 20.1.0 格式化。
- 使用一次性设置实际操作 GUI：主页→客户端配置→返回、首选项及网络页、服务端配置→新增计算机→返回，切换过程中保持一个应用窗口。未从该 GUI 启动 core。
- 包验证覆盖 arm64 Mach-O 依赖闭包、干净环境 Cocoa 启动，以及使用临时可信身份的 TLS 1.3 回环握手。没有发送启用输入共享的 Synergy greeting。

### 平台范围

界面验收只针对 macOS 与 Windows。Linux 构建仍在 CI 中运行以保持共享代码可编译，但不声称任何 Linux 视觉或行为验收。

### 尚未证实

- 尚未证明与批准的 HTML 设计稿逐像素一致。本机已生成截图，但本会话图像工具输出未提供可靠的视觉审阅条件。
- Windows/Linux 原生渲染、DPI 和硬件验收需要对应环境，macOS 构建不能作为这些平台的证明。
- 未测试真实双机输入共享、实时系统剪贴板替换、安全桌面/UAC、授予系统权限或启动安装器。
- core 诊断日志保留原文；面板标签翻译不代表所有 core 日志均已翻译。
- 远端 CI 必须核对本次推送的提交，不能沿用旧提交的绿灯。

### 复现

使用上方 English 小节中的命令。本机日常环境保持 `SYNERGY_ENABLE_INTERACTIVE_TESTS=OFF`，不得直接执行交互测试程序绕过 CTest 护栏。
