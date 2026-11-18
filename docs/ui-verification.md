# UI verification / 界面验证

## English

Use the [documentation index](README.md) for navigation and the [current R4 report](reports/20261118-ui-visual-r4.md) for the identified local source, tests, captures and native observations. This page describes how evidence is read; it does not repeat changing pass totals or turn historical observations into current acceptance.

### Verification procedure

1. Bind the run to its exact source manifest, executable, compiled translation catalogue, isolated settings and capture method. Keep distributed package identity separate from a local real-main probe.
2. Run applicable safe tests through CTest. Keep `SYNERGY_ENABLE_INTERACTIVE_TESTS=OFF` on the everyday host; `OSXClipboardTests` and `OSXKeyStateTests` are Disabled / Not Run, separate from passes. Direct invocation must not bypass that guard.
3. Inspect individual original images for both languages and required widths, every page/tab/action type and affected state successors. Preserve filenames, hashes, findings and coverage limits. A Qt case count or `QWidget::grab` is not compositor or human acceptance.
4. Check Save/Cancel, complete stored maps, typed selections, recording restoration and unchanged core grammar independently from appearance. Native pointer/keyboard observations and physical hardware/permission acceptance need their own evidence.
5. Report fresh execution, explicitly reused results, disabled targets, failures and unavailable raw separately. Apply package dependency/architecture and isolated-startup verification to the actual distribution payload before claiming package acceptance.

Reproduction entry points and host prerequisites remain in [the build guide](build.md). Repository layout and interactive/package constraints remain in [AGENTS.md](../AGENTS.md). Documentation consolidation itself does not build or launch an application.

### Current limits and history

R4 establishes bounded local macOS verification. Native drag has no observed successful Drop and remains unverified. Windows/Linux runtime, installed GitHub package/default startup, complete package dependencies, actual core/input/clipboard sharing and OS permissions require separate evidence. Synthetic messages establish component geometry only. Probe observations do not identify the shipped binary.

The [original verification document](archive/ui-20261117/ui-verification-before-20261118.md) is preserved byte-identically. Stage reports, including final tester closeout and image ledgers, are located through the [migration map](archive/ui-20261117/migration-map.json). The old `build/ui-visual-r4-evidence` raw directory was already unavailable before recorded cleanup; its historical hashes and reports do not recreate missing files.

Final probe cleanup receipts are `build/ui-current-evidence-20261118/final-probe-cleanup-before.json` and `build/ui-current-evidence-20261118/final-probe-cleanup-after.json`; use these exact paths ahead of the older bare path in the frozen total report. Local ignored build receipts are not part of a public documentation upload.

## 中文

从[文档索引](README.md)进入指南；当前本地源码、测试、截图及原生观察的精确身份见 [R4 总报告](reports/20261118-ui-visual-r4.md)。本页说明证据如何读取，不复制持续变化的通过数量，也不把历史观察改称当前验收。

### 验证流程

1. 将运行绑定到准确的源码清单、可执行文件、编译翻译目录、隔离设置及抓图方法。分发包身份与本地真实 main 探针分别记录。
2. 适用的安全测试通过 CTest 执行。日常宿主保持 `SYNERGY_ENABLE_INTERACTIVE_TESTS=OFF`；`OSXClipboardTests`、`OSXKeyStateTests` 单列为 Disabled / Not Run，不计通过，不直接执行程序绕过护栏。
3. 逐张查看两语及要求宽度的完整原图，覆盖每页、每个标签、动作类型及受影响后继状态；保留文件名、摘要、发现和覆盖限制。Qt 案例数量或 `QWidget::grab` 不等于 compositor 或人工验收。
4. 保存/取消、完整存储图、typed 选项、录制恢复与核心语法保持，应独立于外观检查。原生鼠标/键盘观察、物理硬件及系统权限验收分别需要证据。
5. 新执行、明确复用、禁用目标、失败和缺失原件分别报告。只有对真实分发载荷完成依赖、架构和隔离启动验证后，才能声称包验收。

复现入口和宿主前置条件继续见[构建指南](build.md)，目录及交互/打包约束见 [AGENTS.md](../AGENTS.md)。本次文档整理不构建或启动应用。

### 当前限制与历史

R4 证明的是本机 macOS 有界验证。原生拖拽尚无成功 Drop 观察，保持未验；Windows/Linux 运行、GitHub 安装包默认启动、包依赖完整性、真实 core/键鼠/剪贴板共享及系统权限仍需独立证据。合成消息只证明组件几何，探针观察不代表正式二进制身份。

[原验证说明](archive/ui-20261117/ui-verification-before-20261118.md)已原字节保存。阶段报告，包括最终测试收尾和读图台账，通过[集中迁移映射](archive/ui-20261117/migration-map.json)定位。旧 `build/ui-visual-r4-evidence` 原始目录在记录的清理前已经不可用；历史摘要及报告不能重造缺失文件。

最后探针清理回执的准确位置是 `build/ui-current-evidence-20261118/final-probe-cleanup-before.json` 与 `build/ui-current-evidence-20261118/final-probe-cleanup-after.json`，优先采用这两条，不沿用冻结总报告中的较早裸路径。本地被忽略的 build 回执不属于公开文档上传内容。
