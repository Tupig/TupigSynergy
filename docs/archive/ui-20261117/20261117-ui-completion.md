# UI completion delivery / 界面完整实现交付

## English

Run: BP-UI-20261117-COMPLETE, 2026-11-17. The supplied design has been implemented in the existing native C++/Qt application and verified within the local implementation scope. Main and child pages retain real configuration bindings. This report records agent verification; physical tray/Dock, distributed binaries and hardware acceptance remain open.

### Delivered behavior

- Main shell, device identity, Server/Client cards, log, status and navigation use the compact design. Pages fit active content at 540/600/820 logical pixels; shorter tabs and nested Back shrink naturally.
- Server, client, preferences, computer, hotkey, action, fingerprint, update, message and About pages use consistent cards, independent pill tabs, bounded short fields and primary/neutral/secondary action tiers.
- Computer-name editing has explicit Save/Cancel; Enter saves, Escape cancels, focus loss does not save. Selected-computer deletion protects the primary/empty/external selection and remains transactional.
- Hotkey associated actions use a nested draft. Returning from accepted child edits refreshes the parent action list and clears stale selection. Action/direction/trigger/recipient/modifier/preferences selectors persist semantic IDs. Legacy and unknown actions remain read-only; invalid output is rejected before truncating an existing configuration.
- Preferences identify the saved certificate when displaying its complete 32-byte SHA-256, in two left-aligned 16-byte lines. Unsaved path edits survive nested Back and Cancel preserves persisted settings. Remote trust/reject defaults are retained.
- Update checking, failure, Retry, cancellation, measured rate/ETA and stalled-network waiting use real state. Retry ownership is exactly once; existing origin/hash/install policies remain unchanged.
- Menus retain native roles and shared command state. The tray Quit proxy delegates once with NoRole and no duplicate shortcut. About uses the compiled version and retains notice/contributor routes. Click, option and navigation motion and reduced-motion handling are covered by actual intermediate-frame tests.
- Live Chinese/English preview, Cancel, Save and reopen update the Scope controls, mode titles, hostname/port accessible names and command tooltips. The final main-window correction adds eight existing-context translation setters without changing configuration or mode state.

### Requirement coverage and evidence

| Requirements | Local result | Evidence |
|---|---|---|
| UI-001..004, UI-010..011, UI-016 | Compact composition, content fit and real draft flows implemented; settled bilingual matrix and representative native routes inspected | LayoutAcceptance, NativeDesign, MainWindowFlow, R3Visual; PM visual/native receipts |
| UI-005..007 | Complete saved-source fingerprint, real update state, log and About behavior implemented | Fingerprint, update/downloader/security/log suites; two-certificate and network flow cases; selected full-size images/native observations |
| UI-008, UI-012 | Pinned-manifest entrypoint and all enabled safe CTest targets pass | Current raw logs, JUnit, source/render indexes |
| UI-009 | Native application/File menus observed; shared menu routing/state tested | NativeDesign and tray model checks; CUA Quit followed by process exit |
| UI-013 | Production main/hooks pass isolated fresh/current/legacy fixtures | R3StartupContract; not reproduction of the historical unidentified crash or installed-default-app acceptance |
| UI-014..015 | Semantic options, serialization, transactions, actual popup closure and intermediate motion checked | R3Option, R3Serialization, R3Motion, MainWindowFlow |
| UI-019 | Source/menu-model correction verified; application Quit observed | Physical status-item and Dock context-menu verification remains pending |
| UI-017..018 | Outside this execution package | GitHub build/publication, exact downloaded package and installation remain separate work |

### Current verification

All current evidence is under `build/ui-completion-evidence/final-main-language/` unless stated otherwise.

| Check | Actual result |
|---|---|
| `scripts/build.sh release` | Exit 0; Release arm64, pinned vcpkg manifest, static Qt 6.11.2. Installed ports and existing build outputs reused; no cold dependency rebuild claim. `entrypoint-build-r2.log/.exit` |
| Complete safe CTest | 78 registered, 76 executed/passed, 0 failed, 2 Disabled/Not Run, 135.42 seconds, exit 0. `full-safe-r2.log/.xml/.exit` and console |
| Disabled tests | `OSXClipboardTests`, `OSXKeyStateTests`; excluded from pass counts and never invoked directly |
| Complete Qt rows | 75 raw text logs, 1389 passes including setup/cleanup, 0 failures/skips; independently reconciled with full verbose output. MainWindowFlow 44, Option 77, Visual 27, Native 73, Layout 87, Startup 5 |
| Render matrix | 511 PNGs: 347 endpoints + 164 R3 motion/visual. 60/60 base rows cover ten page kinds × two languages × three widths, with all actual tabs and eight Action states. PM independently rehashed all 511 images with 0 mismatch |
| Source freeze | 1089 build/runtime/test inputs, identical before/after and at PM closeout; no added/missing/changed source. Independent current enumeration and canonical compact-JSON digest match |
| Final preview | Fresh arm64 build exit 0; executable identity independently matches. Runtime settings/state/legacy/test stores are contained in one temporary root; auto-core/update disabled, unique IPC and blocked commands verified |

Source manifest SHA-256: `e8ddb505c19f360664da29a8ef81bd9ced18de1dabcd6f94f3c6f812aa883b37`. Relative to the Scope successor, the only two changed inputs are `src/lib/gui/MainWindowDesign.cpp` and `src/unittests/gui/MainWindowFlowTests.cpp`.

Final preview executable SHA-256: `8aa3d2a1fa693008c7974aa20fd911a447f0c19f6b82b8e774efd481dde485fa`. Preview source SHA-256: `3f3edb3b6ce455d07c3f7cc754f6f587ec309df544231600614cbe710b5eb2e0`. This diagnostic entry omits production entry hooks; the separate startup probe exercises the real main/hooks with isolated INI fixtures. Neither result certifies a shipped executable or macOS NativeFormat migration.

PM visual inspection covered all 17 contact sheets containing 162 page/tab/action images from the preceding source, selected full-size main/log/fingerprint/update images, and current main-language/Action-alignment successors. This is not a claim of 511 individual full-size inspections. Final native observation on the exact fresh executable confirms Scope refresh, Chinese Save to both mode cards/export/restart, translated client hostname/port accessibility with fixture values retained, native application Quit and a second inventory reporting the process stopped. Earlier native observations cover name transactions, selected deletion/Cancel, nested Hotkey/Action, recording Escape restoration, saved fingerprint/Back, Log and About.

Independent PE/SecOps review found no additional confirmed actionable source/security defect in the bounded affected code and reports. The scoped pattern/context PII review found synthetic documentation/loopback fixtures; it is not an assertion about unrelated settings, ignored build outputs or all possible personal data. PM raw/source reconciliation is recorded in `build/ui-completion-evidence/pm-final-verification.json`; native lineage is in `pm-native-observations.json` and `pm-native-final-main-language.json`.

### Evidence integrity and limits

Earlier failing baselines, six-row language/geometry REDs, compile failures and the interrupted `full-safe.exit=130` remain preserved. Only `full-safe-r2.exit=0` and its matching source are used for final counts. JUnit truncates long system-out; Qt totals therefore use complete raw text plus verbose logs. Recursive R3 image indexing corrected an initial aggregation undercount without changing product/checks or rerunning tests. An independent filesystem enumeration also encountered one ignored Python cache derivative; Git tracked plus non-ignored untracked inputs match the 1089-file manifest, and the derivative was retained.

CUA Action Direction selection returned `AXError.notImplemented`; native selection acceptance is not claimed. Real Qt mouse/keyboard checks exercise all eight rows with immediate exactly-once activation and observed asynchronous closure (93–108ms). Dock inventory timed out and the physical status item was not observed. UI-019 remains open for those native surfaces. No real core connection, host input/clipboard sharing, OS permission grant, Windows/Linux run, final package audit, installation, commit, push, merge or public release occurred in this execution.

The current spec and immutable execution package are unchanged: `spec.md` revision `UI-2026-11-17-R3-GH`, SHA-256 `46cbb513917138c8f65bf2ba89e9fa779441cc5fb9c63a01869ad5ef01597aa0`; package `docs/aegis/20261117-ui-completion-build-package.md`, SHA-256 `2b3a4e79973be2094114c5f894dabea896494a01047a0d96273b0ba98f58b531`; design SHA-256 `63895000d2dff328e6a075df18d2f61a2170023a748c19e12500e7f18047345f`. Existing branch `refactor/project-ui-cpp` and base HEAD `a2793eff1f9b8d51900f2b47270ab346f2fbab4d` are preserved. Issue #380 remains open for remaining delivery/native boundaries.

## 中文

本轮执行包：BP-UI-20261117-COMPLETE，日期 2026-11-17。已在现有 C++/Qt 原生应用中落实设计稿，并完成本轮本地实现范围的验证；主页及子页继续使用真实配置和业务绑定。此报告记录代理验收证据，实际托盘/Dock、发布二进制与硬件验收仍有明确边界。

### 已交付行为

- 主页、设备身份、服务端/客户端卡片、日志、状态栏和导航采用紧凑设计；540/600/820 逻辑像素宽度按活动内容自适应，短分页及嵌套返回会自然收缩。
- 服务端、客户端、首选项、计算机、快捷键、动作、指纹、更新、消息及关于页采用统一卡片、独立胶囊分页、短字段宽度约束和主操作/取消/次级操作层级。
- 名称编辑明确保存/取消；Enter 保存、Escape 取消，失焦不自动保存。选中计算机删除保护主机、空位及外部选择，编辑仍属于可取消草稿。
- 快捷键关联动作用独立嵌套草稿；子页接受后刷新父列表并清除旧选择。动作、方向、触发时机、接收者、修饰键和首选项使用稳定语义 ID。旧动作/未知动作只读；无效序列化在打开截断目标文件前拒绝。
- 指纹页明确显示已保存证书来源，完整 32 字节 SHA-256 按两行、每行 16 字节左对齐。嵌套返回保留未保存路径，取消不改变原设置；远端信任/拒绝默认行为保持。
- 更新检查、失败、重试、取消、真实测速/预计时间及网络停滞等待均按实际状态显示；重试只触发一次，既有来源、摘要与安装策略保持。
- 菜单保留原生角色与共享命令状态；托盘退出代理只委托一次、使用 NoRole、不重复绑定快捷键。关于页使用构建版本并保留许可及贡献者入口。点击、选项及导航动效和减少动态效果均有实际中间帧测试。
- 中英文实时预览、取消、保存和重开会更新范围控件、模式标题、主机名/端口可访问名称和命令提示。最后的主窗口修正仅补八项既有翻译设置，不改变配置及模式状态。

### 需求覆盖

| 需求 | 本轮本地结果 | 证据 |
|---|---|---|
| UI-001..004、010..011、016 | 紧凑结构、自然高度、真实草稿流程已落实；检查双语矩阵及代表性原生流程 | LayoutAcceptance、NativeDesign、MainWindowFlow、R3Visual 及 PM 视觉/原生记录 |
| UI-005..007 | 完整且来源清晰的指纹、真实更新状态、日志和关于页已落实 | 指纹/下载/安全/日志套件、双证书与网络流程、实尺寸图及原生观察 |
| UI-008、012 | 固定 manifest 项目入口与全部启用安全 CTest 通过 | 当前原始日志、JUnit、源码/图片清单 |
| UI-009 | 已看原生应用与 File 菜单，逻辑路由和状态有测试 | NativeDesign/托盘模型、CUA 退出及进程结束 |
| UI-013 | 真实 main/hooks 的隔离 fresh/current/legacy 夹具通过 | R3StartupContract；不等于复现历史未知崩溃或已安装默认入口验收 |
| UI-014..015 | 选项语义、序列化、事务、真实弹层关闭及中间动效已测试 | R3Option、R3Serialization、R3Motion、MainWindowFlow |
| UI-019 | 源码/菜单模型修正已验证，原生应用退出已观察 | 实际状态栏托盘与 Dock 右键菜单仍待验证 |
| UI-017..018 | 本执行包之外 | GitHub 出包、发布、精确下载包及安装另行完成 |

### 最新实际验证

当前证据目录为 `build/ui-completion-evidence/final-main-language/`，不是前序源码的测试结果。

| 检查 | 实际结果 |
|---|---|
| `scripts/build.sh release` | 退出 0；Release arm64、固定 vcpkg manifest、静态 Qt 6.11.2。复用已安装端口和已有构建产物，不冒称冷构建全部依赖。见 `entrypoint-build-r2.log/.exit` |
| 完整安全 CTest | 注册 78 项，实际执行 76 项全部通过，0 失败，2 项 Disabled/Not Run，135.42 秒，退出 0。见 `full-safe-r2` 日志/XML/exit 及 console |
| 禁用测试 | `OSXClipboardTests`、`OSXKeyStateTests`，不计入通过，也未直接调用 |
| 完整 Qt 行 | 75 份完整原始文本，1389 行通过（含准备/清理）、0 失败/跳过；PM 与完整 verbose 日志独立核对。MainWindowFlow 44、Option 77、Visual 27、Native 73、Layout 87、Startup 5 |
| 渲染矩阵 | 511 张 PNG：347 端点＋164 R3 动效/视觉；十类页×中英文×三宽度的 60/60 基础行齐全，含全部实际分页和八种动作。PM 重算全部 511 张哈希，0 不符 |
| 源码冻结 | 1089 份构建/运行/测试输入，测试前后及 PM 收口时一致；独立枚举无新增/缺失/改动，规范紧凑 JSON 摘要匹配 |
| 最终预览 | 新构建退出 0、arm64，执行文件摘要独立匹配。设置/状态/旧配置/测试存储均位于同一临时根，自动 core/更新关闭，唯一 IPC 及命令阻断已核对 |

源码清单 SHA-256：`e8ddb505c19f360664da29a8ef81bd9ced18de1dabcd6f94f3c6f812aa883b37`。相对范围后继仅两份输入变化：`src/lib/gui/MainWindowDesign.cpp`、`src/unittests/gui/MainWindowFlowTests.cpp`。

最终预览执行文件 SHA-256：`8aa3d2a1fa693008c7974aa20fd911a447f0c19f6b82b8e774efd481dde485fa`；预览源码 SHA-256：`3f3edb3b6ce455d07c3f7cc754f6f587ec309df544231600614cbe710b5eb2e0`。诊断预览省略生产入口钩子；另一启动 probe 通过临时 INI 夹具执行真实 main/hooks。两者均不等于交付程序或 macOS NativeFormat 迁移验收。

PM 已查看前序源码的全部 17 张联系表（162 张页面/分页/动作图），另看主页、日志、指纹、更新实尺寸图，以及当前主窗口语言和 Action 对齐后继原图；不冒称逐张实尺寸检查全部 511 图。最终精确预览的原生复核确认：Scope 同窗刷新、保存中文后两个模式卡/导出/重启更新、客户端主机名/端口可访问名称和说明更新且夹具值保留、原生应用退出后第二次清单显示进程结束。前一轮原生观察还覆盖名称事务、选中删除/取消、快捷键嵌套动作、录制 Escape 恢复、已保存指纹/返回、日志及关于页。

独立 PE/SecOps 在有界受影响源码和报告中未确认新的可操作源码/安全缺陷。有限模式及上下文隐私检查发现的是合成文档地址或 loopback 夹具，不代表检查了无关设置、忽略的构建输出或所有个人数据。PM 独立核对见 `build/ui-completion-evidence/pm-final-verification.json`；原生观察沿革见 `pm-native-observations.json` 和 `pm-native-final-main-language.json`。

### 证据完整性与未覆盖范围

早期失败基线、六组语言/对齐失败、编译失败及中断的 `full-safe.exit=130` 均保留。最终统计只使用 `full-safe-r2.exit=0` 及匹配源码。JUnit 的长 system-out 会截断，因此 Qt 行数由完整原始文本与 verbose 核对。递归索引 R3 子目录修正了初次图片少计，没有改产品/检查或重跑测试。PM 枚举另遇到一份忽略的 Python 缓存派生文件；按已跟踪加非忽略未跟踪输入核对仍是 1089 项，缓存保留在原处。

CUA 操作 Action Direction 返回 `AXError.notImplemented`，未声称原生选择验收；真实 Qt 鼠标/键盘测试覆盖八项，立即只激活一次，观察弹层异步关闭为 93–108ms。Dock 枚举超时，实际状态栏托盘未观察到，因此 UI-019 对这些系统表面仍开放。本轮未执行真实 core 连接、宿主键鼠/剪贴板共享、系统权限授予、Windows/Linux 运行、最终包审计、安装、提交、推送、合并或公开发布。

主规格、不可变执行包及设计稿摘要均未改变，完整身份见英文末段。既有 `refactor/project-ui-cpp` 分支和基线 `a2793eff1f9b8d51900f2b47270ab346f2fbab4d` 保留，所有继承未提交工作均保留。#380 继续开放，记录剩余发布及原生验收边界。
