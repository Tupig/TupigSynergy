# R2 UI correction build package / R2界面修正构建包

## English

Immutable BP-UI-20261117-R2, created2026-11-17. Master spec spec.md revision UI-2026-11-17-R2 SHA-256 6c6aac1f73965cc4b496cf4e7ccccf0a774d0fe45e8ad9dcbb98d2afaf622a29. Predecessor build-package.md (R1) retained. User authority: full delegation plus explicit correction of whitespace, menus, subpages, UI/function tests. Standard risk; existing security/protocol/runtime-install boundaries unchanged. Covered UI-001..012, contract C-UI-R2 R2-V01..06.

W1 independent tester: review current root causes and contracts, then create actual R2 tests before product edits. Planned files src/unittests/gui/LayoutAcceptanceTests.cpp (new) and/or NativeDesignTests.cpp, GUI CMake. Natural fit API PageHost::preferredContentHeight(int width), contentSizeChanged, requestContentFit; menu helper API gui/AppMenus.* exposing MenuPlatform, existing QAction* bundle and populate main/tray menus; key widget recordingChanged(bool)/isRecording. PM must approve concrete tests. Exit: check adequacy recorded.
W2 shared-host/menu coder after W1: PageHost.*, UiMainWindow.*, MainWindow.*, mainwindow/MainWindowWindow.cpp and narrow presentation-state lines of MainWindowConnection.cpp; new AppMenus.* plus gui CMake. Active-only measurement, natural window height/cap, compact pagebar, shared action menu model/shortcuts/state. Preserve core calls, no page-design edits. Verify R2-V01..03 and old PageHost/state regression.
W3 page coder after W1/W2 interface freeze: dialogs/*Design.*, relevant dialog bindings, widgets/KeySequenceWidget.*, MessagePage/FingerprintPreview, update/UpdateDialog presentation, zh_CN/ja additions. All page composition and recording/recipient transactions per spec; no backend policy changes. Verify R2-V01/02/04/05.
W4 independent tester: build via project release entrypoint, direct synthetic check red/green where useful, full safe CTest with interactive OFF, all actual subpage screenshots and menu routing/live observation, bounded SecOps. Package under build/ui-delivery-r2, compare disposable extracted payload, dependency/architecture/signature/isolated loader audit. Preserve prior DMG.
W5 PM: inspect diffs/raw evidence, update issue#380, record exact identities and deliver local artifacts. No commit/push/install/release. Role models mapped to available gpt-6.1-sol high under existing full delegation; PM non-implementing. No further process approval questions for these authorized routine decisions.

## 中文

不可变构建包BP-UI-20261117-R2，绑定唯一主规格spec.md（R2）摘要6c6aac1f73965cc4b496cf4e7ccccf0a774d0fe45e8ad9dcbb98d2afaf622a29；前序build-package.md保留。用户已全权授权修正留白、菜单、所有子页并做界面/功能测试。覆盖UI-001..012，契约C-UI-R2。

顺序：W1独立角色先建立真实高度/按钮间距/菜单/录制/页面事务检查，PM审核充分性；W2重做共享宿主、窗口自适应及真实菜单；W3完整重排各子页，保留业务绑定；W4独立构建、全量安全CTest、逐页及菜单验收、解包依赖审查；W5 PM核对证据并同步#380、交付本机产物。英文明确每项文件、接口、前置及验收。旧包不覆盖，新包输出build/ui-delivery-r2。保留core/daemon、协议、安全策略及已安装环境。不提交、推送或正式发布。模型沿用已授权的可用映射。
