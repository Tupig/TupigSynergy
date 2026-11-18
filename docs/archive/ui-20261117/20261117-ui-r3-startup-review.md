# R3 startup investigation / R3 启动调查

## English

Live todo:
- [completed] Reconstructed authorized current R3 scope and inspected real startup, button-feedback lifetime and Action type dispatch without reading daily configuration or secrets.
- [completed] Reported evidenced candidates, unknowns and the smallest safe real-entry reproduction/check design; no source edits, build or CUA.

Boundary: source-only investigation. Do not launch production/preview, mutate IPC/user stores, grant permissions, or start core/daemon. The mounted user volume is reported to contain an older application identity; volume-guard behavior is not evidence of an R2 crash.

## 中文

实时 todo：
- [已完成] 核对现行 R3 范围，只读启动路径、按钮反馈生命周期与 Action 类型处理，不读取日常配置或密钥。
- [已完成] 报告有证据的候选、未知项和最小安全真实入口检查方案；不改源码、不构建、不操作 CUA。

边界：仅源码调查。不启动生产/预览、不改 IPC/用户存储、不授予权限、不启动 core/daemon。已知用户挂载卷属于旧应用身份，卷内启动保护不能作为 R2 崩溃证据。


## English

Source authority: current master spec remains UI-2026-11-17-R2; journal R3 intake records the user's startup/menu/animation correction and continued local delegation. This report is investigation evidence, not a new specification or product-edit authorization. PM-provided identity evidence says the mounted app is 2.1.6-dev+7216123c, not the R2 2.1.7 payload. No current TuPig crash IPS was identified by PM. The launch copy and actual failure mode remain unconfirmed here. The /Volumes guard in src/apps/synergy-gui/synergy-gui.cpp:156-162 displays installation advice then returns 1, but cannot currently be attributed to the user's R2 complaint.

R3-SRC-01 (confirmed source defect, runtime reproduction pending): ActionDialog conflates presentation indices and model IDs. Action.h has 11 legal enum/name entries, while the current Action combo contains eight. Constructor line46 sets currentIndex to the stored type; legal legacy values8..10 therefore become -1. canSave lines154-169 falls through true for that index, and accept line82 writes -1 into the draft. The parent accepted callback (ServerConfigDialog.cpp:348) immediately calls Action::text(), which indexes its name table with at(type()) at Action.cpp:16. This is a concrete invalid-index/assert/out-of-bounds path, independently confirmed against the tester candidate. It has not been reproduced by this reviewer and is not established as the reported startup cause.

Minimal GUI remedy candidate accepted: give the eight supported visual options stable enum itemData; load with findData and serialize/dispatch using the selected semantic ID, never the visual index. Preserve legacy/unknown values in a clear unsupported-current placeholder, disable Save until a deliberate supported selection, and leave Cancel/model data unchanged. Do not call Action::text blindly for legacy/unknown GUI list summaries; use a bounded safe label for inputs outside the supported presentation contract. Deliberate conversion to an existing supported action can be a PM-scoped UI decision; it must not add protocol types. Verify types8..10, negative/out-of-range types, all eight existing options, placeholder Cancel, explicit supported conversion and recipient/direction/cursor serialization before paired edits.

Startup reachability is conditional. Fresh settings default AutoStartCore=false (Settings.h:267), so invalid Action data does not explain every startup. A previously enabled server auto-start can reach serialization through MainWindow::open -> startCore -> CoreProcess::start/persistServerConfig -> ServerConfig::save -> Hotkey stream output -> Action::text, before executing the core. This chain is source evidence only. The GUI candidate prevents new poisoning; it does not by itself establish safe export of already-invalid persisted data. Keep that acceptance gap visible, and use a pure synthetic serialization/subprocess check rather than starting a real core to inspect it.

R3-SRC-02 (interaction coverage defect, not proven crash cause): installButtonFeedback runs at entry line81, before onPreInit84, I18N88 and MainWindow173. The feedback controller belongs to QApplication; overlays belong to real QPushButtons; the QVariantAnimation member removes itself from QObject ownership during member destruction before QWidget base teardown. No direct double-delete or startup failure has been established from these lifetimes. The filter lazily creates an overlay on Enter/mouse press/release/Leave/EnabledChange, but not on Resize alone. It has no equivalent pressed/released/clicked signal path for keyboard/native accessibility activation. PageHost has no page-switch animation. Thus the source does not support a claim of complete native activation/page-switch animation coverage. Potential reentrant construction or early-message teardown needs actual focused lifecycle evidence, not speculative product changes.

Smallest safe real-entry check design: build a test-only target using the same synergy-gui.cpp main path, actual onPreInit/onMainWindow/onAppStart and production migration algorithm, with an explicitly injected startup context before onPreInit. Do not reuse R2's no-op migration preview, HOME replacement or XDG-only isolation. Supply temporary user/system settings/state/extra paths; explicit temporary legacy-user/system QSettings inputs instead of the NativeFormat user/system stores; an exact temporary TestSettings path avoiding ancestor/home lookup; and one unique shared-memory/GUI socket identity used by both duplicate checking and MainWindow. Production absent-context defaults remain byte-for-byte semantically unchanged. Cover both migration extraFile helpers, SettingsScope paths and initial Settings construction, not only a late setSettingsFile call.

Use fresh/new-format/legacy fixtures with auto-core off and update checks off; label the fresh fixture's safety-control seeding honestly. TLS writes must stay under the temporary settings root. Do not confirm Scope/Clear/Install or activate clipboard controls. Run real style/feedback installation and real entry hooks, recording only phase markers (post-QApplication/style, pre-init complete, I18N complete, singleton complete, MainWindow constructed/open, first event-loop turn and clean quit). Test early-message/button feedback creation/destruction separately with synthetic hidden widgets and queued animation completion; no global input. Keep existing-source/identity hashes and exit/signal classification. Each case should prove no daily/native legacy stores were opened and no production shared memory/socket or core/daemon was contacted. This design is pending contract and actual-check adequacy, not permission to execute or change source.

Investigation SHA-256 checkpoint: synergy-gui.cpp c77331018634adf9e967a1af631c600f98d90cc273735857c3eb5567931529ae; AppStyle.cpp dfead373a7bb304ba8c8951b3f4eea348dd77e8b9d3828384ba926a100bd7d9a; ActionDialog.cpp e55c05e9028c372b0f6fd8f0172859096310f4b6cbc836cf68c496fba3c3b7d7; Action.cpp ec133f9a01138184b435b4e610f429e03bba9b3512ba67af3080a607c6187224; Action.h f095c930ac1d3407757597394094b16453e0c95790575c16348c77bdb6c46eff; SettingsMigration.cpp 7fb682a3a4c734495ccb2ce631c80fb2240bc5340cc7725abc9eff7957c0ef94.

## 中文

源码权威：当前主规格仍为 UI-2026-11-17-R2；journal 的 R3 受理记录用户启动/菜单/动画纠正及继续本地全权委托。本报告只是调查证据，不是新规格或产品修改授权。PM 提供的身份显示挂载应用为 2.1.6-dev+7216123c，并非 R2 的 2.1.7；PM 未找到本次 TuPig 崩溃 IPS。本审查尚不能确认用户实际启动副本和故障形态。真实入口第156-162行的 /Volumes 保护会显示安装提示并返回1，但目前不能把它归因于用户所说的 R2 崩溃。

R3-SRC-01（确认源码缺陷，待运行复现）：ActionDialog 混用界面索引与模型 ID。Action.h 有11项合法枚举/名称，界面只有8项。构造第46行直接把已有类型设为 currentIndex，使合法旧类型8..10变成-1；canSave第154-169行对此返回true，accept第82行写入-1。父页接受回调（ServerConfigDialog.cpp第348行）随即调用 Action::text，而 Action.cpp第16行用 at(type()) 取名称。这是独立核实的无效索引/断言/越界路径，但本审查未执行复现，也未证明它就是用户启动故障。

接受的最小 GUI 候选：8个支持选项使用稳定枚举 itemData，按 findData 加载，按语义 ID 保存和分派，不能再按界面索引写入。旧/未知值显示明确的“不支持当前值”占位，在用户明确选择支持项前禁用保存；取消精确保留原模型。GUI 列表摘要对超出呈现契约的输入显示安全说明，不盲调 Action::text。是否允许显式改选既有支持动作由 PM 确定，不能添加协议类型。配对修改前应检查8..10、负数/越界类型、原8项、占位取消、明确转换，以及接收对象/方向/锁定序列化。

启动可达性有条件：全新设置默认 AutoStartCore=false，不能据无效动作解释所有启动。若以前启用了服务端自动启动，可经 MainWindow::open、startCore、CoreProcess 的 persistServerConfig、ServerConfig::save、Hotkey 流输出进入 Action::text，且发生在 core 执行前。此为源码链，不是运行证据。GUI 候选可以防止继续写坏，但不能单独证明已有坏数据导出安全；保留验收缺口，用纯合成序列化/子进程检查，不启动真实 core。

R3-SRC-02（交互覆盖缺陷，并非已证启动原因）：真实入口第81行安装按钮反馈，早于 pre-init、第88行 I18N 和第173行 MainWindow。控制器归 QApplication，叠层归真实按钮；QVariantAnimation 成员析构时会先脱离父对象，之后才到 QWidget 基类清理，因此未发现直接双删证据。过滤器在 Enter/鼠标按下释放/Leave/EnabledChange 时才按需创建叠层，Resize 不单独创建；没有覆盖键盘或原生辅助功能激活的同等 pressed/released/clicked 信号入口。PageHost 无页面切换动画。因此源码不能支持“所有原生激活/页面切换都有动画”的结论。重入构造或早期提示页销毁风险仍需真实有界生命周期检查，不能猜测修改。

最小安全真实入口检查方案：仅测试构建使用同一 synergy-gui.cpp 主路径及实际 onPreInit/onMainWindow/onAppStart/迁移算法，在 pre-init 前注入显式启动上下文。不复用 R2 迁移空替身，不覆盖 HOME，也不能只设 XDG。上下文提供临时用户/系统配置、状态及 extra 路径；临时 legacy-user/system QSettings 输入以替代原生存储；精确临时 TestSettings 路径以避开父目录/用户目录搜索；并让共享内存查重与 MainWindow 使用同一唯一测试身份。无上下文时生产默认语义必须保持。须覆盖两个迁移 extraFile 辅助、SettingsScope 和首次 Settings 构造，不能只在后面调用 setSettingsFile。

使用全新/新格式/旧格式夹具，明确关闭自动 core 和更新检查，诚实注明新夹具的安全开关预置；TLS 仅写临时设置根。不能确认 Scope/Clear/Install 或激活复制。执行真实样式/反馈和入口 hooks，仅记录阶段标志：QApplication/样式后、pre-init完成、I18N完成、单实例完成、MainWindow创建/打开、首轮事件循环、正常退出。早期消息页/按钮反馈创建销毁另用隐藏合成控件与动画队列检查，不注入全局输入。保留源码身份和退出/信号分类，证明没有打开日常/原生旧存储、没有接触正式共享内存/socket或core/daemon。方案仍待契约和实际检查充分性，不代表可立即运行或改源码。

调查摘要见英文。本次只读源码与受权生命周期记录，没有读取日常配置/密钥、启动应用、构建、操作 CUA、触碰 IPC 或修改产品源码；R2 报告保持不变。

## English

R3 concrete design todo:
- [completed] Specified exact real-entry isolated context/files and production packaging boundary; reviewed atomic app-layer Action rejection.
- [completed] Returned PE decision and test-first prerequisites to PM; no product edit/build/CUA.

## 中文

R3 具体方案 todo：
- [已完成] 明确真实入口隔离上下文/文件及生产打包边界，复核应用层 Action 原子拒绝。
- [已完成] 向 PM 返回 PE 决策与先建检查前置；不改产品、不构建、不操作 CUA。


## English

Frozen R3 authority update: spec.md is now UI-2026-11-17-R3 SHA-256 a6472b99194b8a991dd9c63ea618d9b202502a4d597ef428fd5b883de47a1784; BP docs/aegis/20261117-ui-r3-build-package.md SHA-256 6bde14c1658d9cc587d5bd2d5149d5000bda7243c760f8938ffd97ec8aa35450. UI-013..015 authorize the bounded isolation/selectors/motion correction. The frozen UI-014 law supersedes the earlier conversion suggestion: legacy8..10/unknown Action is read-only with Save disabled and Cancel lossless; no implicit conversion.

PE concrete design decision: ACCEPT; PM has accepted this design and opened G4 for actual-check creation, but W2 product edits still wait for actual-check adequacy. Runtime and motion APIs below were sent to and ACKed by the independent tester. No product source or CMake was changed during this design work.

Runtime API (new common/RuntimeContext.h/.cpp, namespace synergy::runtime): IsolatedRuntimeContext contains root and instanceKey; configureIsolatedRuntime(context,error) configures once and only before first path/factory/identity use, which seals the context; isIsolatedRuntime() only reports mode; sealRuntimeContext() explicitly seals. runtimeStateFile(), runtimeTestSettingsFile(), runtimeInstanceKey(productionDefault), and legacySettings(QSettings::Scope) provide the bounded dependencies. Settings static paths become userDir()/systemDir()/userSettingsFile()/systemSettingsFile(). Production absent-context formulas, path selection and NativeFormat legacy construction remain unchanged. No environment or production command-line activation is added.

The isolated root is an existing canonical disposable child of canonical QDir::tempPath, not the temp root itself. Reject relative/empty/symlink roots, descendant symlinks/escapes, reconfiguration and late configuration. Derived paths are fixed: user/ and system/ with kAppName.conf and kAppName.extra.conf; state/app.state; legacy/user.ini and legacy/system.ini; test-settings.ini; logs/app.log. The instance key is short ASCII, no path/control characters, not a production identity; fixture uses a fresh UUID. Configuration performs no store writes and cannot fall back to daily stores on error. Legacy factory uses temporary IniFormat only in isolated context, with the actual migration algorithm; this remains explicitly different from macOS CFPreferences/NativeFormat migration acceptance.

Runtime files: add common/RuntimeContext.h/.cpp and common CMake registration; update Settings.h/.cpp and QSettingsProxy.cpp; update synergy/gui/SettingsMigration.cpp, SettingsScope.cpp, TestSettings.cpp, FeatureHandler.cpp and hooks/gui_hook.h callsites. Existing common SettingsTests.h uses the new accessor spellings. FeatureHandler.cpp remains W2's single-owned file: scope choices use findData/currentData bool and originalScope bool property, with switching only on accepted changed value; W3 owns corresponding SettingsDialog load/modification checks. Context overrides default log filename to the isolated logs path, while fixtures keep ToFile=false.

Real entry API: same src/apps/synergy-gui/synergy-gui.cpp compiles normally for production and with SYNERGY_STARTUP_PROBE only for the tester's EXCLUDE_FROM_ALL/no-install target. The test-only header/source R3StartupProbeBootstrap.h/.cpp provides configureStartupProbe(QStringList), startupProbeApplicationReady(QApplication*), startupProbePhase(const char*), afterStartupProbePreInit(), startupProbeWindowReady(MainWindow*) and startupProbeFinished(int). The probe injects context before QApplication, keeps all actual hooks, records the actual migrated auto-start result, then explicitly disables auto-core/update before constructing MainWindow. Both shared-memory duplicate handling and MainWindow use context identity; absent context preserves each original production default. Test bootstrap contains the only extra argument handling, lifecycle receipt and controlled quit code; the delivered target has no macro/helper source/test option or test resource. Tester owns test/bootstrap/CMake files, W2 owns product source callsites.

Exact phase names: context-ready, application-ready, style-ready, preinit-ready, i18n-ready, instance-ready, mainwindow-ready, appstart-ready, event-loop-ready, clean-exit. A test-only event filter installed at application-ready may dismiss only the expected real migration information notice, records its appearance, and verifies the actual notified marker; unexpected dialogs fail instead of being approved. This preserves the real showNoticeIfPending algorithm. The auto-core branch is deliberately excluded and documented, not claimed tested.

Motion API (new gui/Motion.h/.cpp): enum MotionMode { System, Reduced, Animated }; setMotionModeForTesting(mode), reducedMotion(), animateContentTransition(QWidget*). installButtonFeedback remains the existing application entry, now installs shared production observers for QAbstractButton pressed/released/clicked plus tab/combo selection. PageHost push/pop calls animateContentTransition. Button feedback lasts about110ms; page/tab/option drawing transition about160ms. Inspectable overlay names buttonFeedbackOverlay/contentTransitionOverlay; animation names buttonFeedbackAnimation/contentTransitionAnimation; motionOpacity property supplements actual pixel-frame checks. Layers are transparent to input, have no focus, never move layout or invoke/delay business actions. Rapid replacement cancels old animation and leaves only the latest state; reduced mode immediately clears/finishes all existing visual effects. Actual checks must include intermediate rendered frames, completion, exactly-once model action, cancel/delete/resize and rapid replacement.

Native reduced-motion source boundary: platform/ReducedMotion.h declares Unknown/Reduced/Full preference and readMotionPreference(); macos/ReducedMotion.mm reads NSWorkspace.accessibilityDisplayShouldReduceMotion, verified in the local SDK NSAccessibility.h:103 and [Apple primary documentation](https://developer.apple.com/documentation/appkit/nsworkspace/accessibilitydisplayshouldreducemotion). win32/ReducedMotion.cpp reads SPI_GETCLIENTAREAANIMATION with SystemParametersInfoW; TRUE means animations enabled, as specified by [Microsoft primary documentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfow). No preference setter or permission request is used. linux/ReducedMotion.cpp reports Unknown rather than inventing native support; central System policy may honor the available Qt UI effect setting but must disclose that it is not a general Linux OS accessibility preference. Deterministic test override is internal and has no production CLI/environment path. Native preference is queried for each transition; reducing while effects run must finish them through the central policy.

Motion files: new gui/Motion.h/.cpp; replace the old private AppStyle feedback implementation with a thin entry wrapper; bounded PageHost push/pop wiring; new platform/ReducedMotion.h, macos/ReducedMotion.mm, win32/ReducedMotion.cpp, linux/ReducedMotion.cpp. Tester owns source registrations in GUI/platform CMake to avoid concurrent edits.

Atomic Action app-layer boundary: W3 validates the complete serializable tree before any text/output, and guards direct ServerConfig stream output before its first byte. save(path) must validate before opening/truncating or use the approved safe write path. GUI CoreProcess::persistServerConfig delegates the same safe save(path), retaining its external-file branch and existing failure handling; it must not open/truncate first. Invalid action/direction/lock/modifier input returns failure with the existing output sentinel/model/INI intact, no fallback command or dropped entry. All previously legal encodings remain unchanged. Safe GUI labels avoid calling unchecked Action::text. This touches only GUI application config/controller boundary, not the sharing engine/parser/protocol.

## 中文

R3 权威现已冻结：spec.md 修订 R3 及不可变构建包摘要见英文，UI-013..015 覆盖本次隔离/选项/动效修正。UI-014 已取代前述允许转换建议：旧8..10及未知 Action 必须只读、禁保存、取消无损，不隐式转换。PE 接受具体方案；PM 已接受并开启 G4 实际检查建立，产品修改仍须等待实际检查充分性。接口已发独立 tester 并获 ACK，当前未改产品源码或 CMake。

RuntimeContext 仅含 root 与 instanceKey，通过 configureIsolatedRuntime 一次性在首次路径/工厂/身份访问前设置；首次访问封存，拒绝重复或过晚设置。提供状态、准确 TestSettings、实例身份、旧存储工厂及四个 Settings 路径访问函数。生产无上下文时保持原平台路径公式、查找和原生旧存储方式，不增加生产环境变量或命令行激活方式。根必须是 canonical 临时目录的真实子目录，拒绝空/相对/符号链接/越界；所有目标固定派生在 root，失败无日常回退、不写存储。用户/系统 conf与extra、状态、两个旧INI、准确测试配置和日志位置及 API 精确名称见英文；fixture实例名使用短随机UUID。临时旧INI只证明真实迁移算法，不等同macOS原生旧后端验收。

具体文件：新增 common/RuntimeContext.h/.cpp；改 Settings.h/.cpp、QSettingsProxy.cpp；改扩展 SettingsMigration、SettingsScope、TestSettings、FeatureHandler、gui_hook 的路径调用点；已有 SettingsTests.h 改访问函数拼写。FeatureHandler 单由 W2 修改，scope 用 findData/currentData 和 originalScope 布尔属性；W3配合修改 SettingsDialog 加载/修改检测。隔离上下文默认日志也归临时根，夹具仍关闭写日志。

真实入口继续使用同一 synergy-gui.cpp 与所有真实 hooks；只有独立 EXCLUDE_FROM_ALL/无安装测试目标定义 SYNERGY_STARTUP_PROBE，并链接 tester 所有的 R3StartupProbeBootstrap。测试在 QApplication 前安装上下文，记录真实迁移结果后，创建 MainWindow 前关闭自动core/更新；共享内存和GUI socket统一唯一测试身份，生产默认各自保持。阶段名、bootstrap函数和精确文件边界见英文。测试专用过滤器只可关闭预期迁移信息页，记录出现并验证真实通知标记；意外对话框必须失败，不自动同意。生产目标不含测试宏/辅助源码/选项/资源，不能把测试程序宣称为交付程序字节相同，也不声称测试了实际自动core分支。

动效冻结接口为 MotionMode(System/Reduced/Animated)、setMotionModeForTesting、reducedMotion、animateContentTransition；按钮入口仍是 installButtonFeedback，改为统一观察 QAbstractButton pressed/released/clicked 和 tab/combo，PageHost前进/返回显式触发。按钮约110ms，页面/分页/选项约160ms。叠层/动画对象名和 motionOpacity 属性见英文；属性必须配合真实中间帧检查。只绘制/透明度，不拦输入、不抢焦点、不移动布局、不调用或延迟业务；快速替换只留下最后状态，减少动态效果立即结束所有效果。新增 Motion文件、AppStyle薄入口、PageHost接线及 platform下各系统读取文件；tester单独登记CMake，避免冲突。

平台减少动态效果边界：macOS只读 NSWorkspace原生属性，已核本机SDK并链入Apple官方来源；Windows只读 SPI_GETCLIENTAREAANIMATION，TRUE代表启用，已链入Microsoft官方来源；不设置系统偏好、不请求权限。Linux明确返回Unknown，可参考Qt效果设置但不能声称通用Linux原生辅助偏好已支持。内部确定性测试覆盖不增加生产CLI/环境变量入口；每次过渡重新读取偏好，运行中减少动效须由中央策略结束。

Action原子拒绝的最小应用层边界：W3先验证完整可序列化树再输出任何字节，直接ServerConfig流入口也须先拒绝。save(path)不能先截断；GUI CoreProcess::persistServerConfig委托同一安全save(path)，保留外部配置分支和已有失败处理。非法动作/方向/锁定/修饰值必须返回失败，保留原输出哨兵文件、模型与INI，不伪造指令、不静默丢项；原合法编码不变。GUI摘要避免未验证Action::text。该边界仅属GUI配置/控制器，不改共享引擎或协议。

## English

W2 runtime implementation todo:
- [completed] After PM actual RuntimeContext check adequacy ACCEPT, implemented internal context/accessors/factory/callsites and test-only real-entry hooks; motion remains gated.
- [completed] Inspected/formatted runtime diff, rechecked identities and handed stable source to the sole build owner; actual startup execution remains pending.

## 中文

W2 Runtime 实现 todo：
- [已完成] PM 接受实际 RuntimeContext 检查充分性后，实现内部上下文/访问函数/工厂/调用点和测试专用真实入口接线；动效仍受闸门约束。
- [已完成] 检查/格式化 Runtime 差异、核对身份并交给唯一构建角色；真实启动运行验收仍待执行。


## English

Runtime source handoff: implemented after PM independently accepted the concrete 14-case RuntimeContext contract. RuntimeContext and common registration, Settings four accessors/initial state/log/portable bypass, QSettingsProxy and real migration factory, Scope/TestSettings/FeatureHandler/hooks paths, originalScope bool and the actual entry's compile-only bootstrap/milestone/IPC wiring are stable. Production path formulas and native legacy construction remain the default. Context-only path and legacy providers are selected before any singleton/store use; no HOME override or production environment switch exists. The tester owns bootstrap/driver/test CMake; W2 owns the narrow production common CMake registration. No build or application launch was performed by W2; formatting and git diff --check passed.

Actual startup check adequacy reviewed: three fixtures with ordered ten phases, real hooks/migration backups/clear/notified markers, migrated auto-core receipt before declared safety overrides, explicit temporary legacy/TestSettings and unique IPC identity/witness, normal exit0 and no unexpected prompt. The main/helper phase split was reconciled so context-ready and clean-exit emit once from helper callbacks. The child-QProcess count alone cannot prove no core because MainWindow's CoreProcess is a value member outside that QObject child graph; the pre-MainWindow auto-start denial and bounded source trace remain necessary evidence.

New direct user steering changes delivery authority: the user identifies the R2 package as failing, requires GitHub builds and a version line starting2.1.1, and requests local uninstall followed by testing latest Release download. PM is amending the same master specification. W2 continues only the authorized source/actual-check work and does not uninstall/build delivery/launch local GUI or publish. The earlier R2 user-artifact uncertainty is historical; no user-specific root cause is claimed from source alone. Real startup probe and final native acceptance must use the newly authorized GitHub artifact path; local unit results are supplementary.

Motion actual-check source was inspected: real intermediate frames, exact duration, transparent input, unchanged geometry/focus, exactly-once actions, six activation routes, tab/option/Back, interruption/resize/destruction and immediate reduced state are concretely represented. One contract interpretation is pending PM: QAccessible press can use Qt's built-in animateClick release scheduling, so a pure paint layer must not alter that business schedule to satisfy an immediate-click-count assertion. Direct click remains immediate; any baseline-relative native timing clarification requires approved test lineage, never silent weakening. Motion product remains unmodified until adequacy authorization.

## 中文

Runtime 源码已交接：PM 独立接受实际14种 RuntimeContext 检查后实现。上下文与 common 登记、Settings四访问函数/首次状态/日志/portable隔离、QSettingsProxy和真实迁移工厂、Scope/TestSettings/FeatureHandler/hooks路径、originalScope布尔值、实际入口的测试宏/阶段/IPC接线已稳定。生产路径公式和原生旧存储仍为默认；隔离上下文在首次单例/存储前选择，无HOME覆盖或生产环境变量开关。tester负责bootstrap/driver/测试CMake，W2只登记生产common小项。W2未构建或启动应用，格式化及diff检查通过。

已复核真实启动实际检查充分性：三组夹具、十阶段、真实hooks/迁移备份/清空/通知标记、覆盖前自动core迁移结果回执、临时旧源/TestSettings、唯一IPC与见证、正常exit0和意外提示拒绝均有具体检查。main/helper已协调context-ready和clean-exit各只记录一次。仅检查MainWindow子QProcess数量不能单独证明无core，因为CoreProcess是其值成员且不在该QObject子树，仍须结合创建MainWindow前禁止自动启动及源码追踪。

用户直接新约束已改变交付权威：确认R2包失败、要求GitHub构建、版本从2.1.1开始，并要求卸载本地后测试latest Release下载。PM正在修改同一主规格。W2只继续受权源码/实际检查，不自行卸载、构建交付、启动本地GUI或发布。此前用户实际副本不明仅属历史，源码仍不能独自证明用户具体崩溃根因。真实入口probe和最终原生验收须走新授权GitHub产物，本地unit证据仅补充。

动效实际检查源码已读：真实中间帧、时长、输入透明、几何/焦点不变、一次业务、六种激活、分页/选项/返回、替换/缩放/析构和减少动态效果均有具体断言。尚有一项契约解释待PM：原生可访问性press可能用Qt既有animateClick延迟释放，纯绘制层不能为满足立即clicked断言而改Qt业务时序。直接click仍须立即；任何按原生基线澄清必须保留审核过的测试修订，不能暗中减弱。动效产品尚未修改，仍待充分性授权。


## English

- [completed] Corrected the tester-observed first compile blocker: RuntimeContext.cpp now includes the generated header as common/Constants.h, matching the target's binary include root. Original failed build log remains tester evidence; no runtime result yet.

## 中文

- [已完成] 修正测试角色发现的首个编译阻断：RuntimeContext.cpp 改用 common/Constants.h 引用生成头，符合目标二进制 include 根。原失败日志由tester保留，尚无运行结论。

## English

W2 motion implementation todo:
- [completed] Implement approved central motion policy, transparent feedback/content drawing and PageHost/selector wiring after actual pixel/lifecycle checks and approved stock-Qt AX timing clarification.
- [completed] Inspect/format bounded motion diff and hand stable source to tester; corrected the observed Qt enum compile error to UI_General, preserving failed build evidence.
- [completed] Investigate primary Qt Cocoa tray/Dock Quit role handling, review actual proxy-routing checks and report the smallest evidence-supported correction to PM; no menu change before authorization.
- [completed] Apply native tab/section/action-tier visual correction after PM actual pixel-check adequacy ACCEPT; stable stylesheet handed to pages/tester, pixel execution remains pending.
- [completed] Implement approved tray NoRole proxy after PM authorization and hand actual source to tester; includes dynamic text/visible/enabled presentation synchronization. Final native acceptance remains pending.
- [in progress] Apply approved GitHub workflow source-identity/both-platform release gate correction after reading the actual stdlib contract, then hand single workflow ownership to tester.

## 中文

W2 动效实现 todo：
- [已完成] 在实际像素/生命周期检查及Qt原生AX时序澄清获批后，实现集中策略、透明按钮/内容绘制与PageHost/选项接线。
- [已完成] 检查/格式化有界动效差异并交给tester；将实测Qt枚举编译错误修为UI_General，保留失败构建证据。
- [已完成] 查证Qt Cocoa托盘/Dock退出角色的原始源码，审核实际代理路由检查，将最小且有证据的修正交PM；获授权前不改菜单。
- [已完成] PM实际像素检查充分性ACCEPT后，修正原生分页/分区/动作层级样式；稳定样式交pages/tester，实际像素执行仍待完成。
- [已完成] PM授权后实现托盘NoRole代理并把实际源码交tester，含动态文案/可见/启用状态同步；最终原生验收仍待完成。
- [进行中] 实读stdlib契约后修正获批GitHub工作流源码身份/双平台发布门禁，再将该工作流唯一写入权交tester。

## English

Current successor authority: spec.md UI-2026-11-17-R3-GH SHA-256 46cbb513917138c8f65bf2ba89e9fa779441cc5fb9c63a01869ad5ef01597aa0; PM recorded immutable successor package SHA-256 172fdda4230678c93872232d0a4a80bf1f3ff7e2c1ca549c8fd3ca5858b6a4ac. GitHub-only final delivery supersedes local packaging acceptance.

Tray mechanism source evidence: Qt6.11 [QCocoaMenuItem::sync](https://raw.githubusercontent.com/qt/qtbase/6.11/src/plugins/platforms/cocoa/qcocoamenuitem.mm) maps explicit QuitRole to the application quit item and marks it merged (lines241-297). [QCocoaMenu::insertNative](https://raw.githubusercontent.com/qt/qtbase/6.11/src/plugins/platforms/cocoa/qcocoamenu.mm) skips merged items (lines122-123). A separate NoRole tray presentation is therefore warranted while canonical QuitRole/CmdQ stays intact. Actual R3TrayQuitTests covers repeated population, visibility/state, no duplicate shortcut and exactly-once forwarding; source adequacy accepted, PM authorization requested. This establishes a mechanism, not final native reproduction. [Qt Cocoa application delegate](https://raw.githubusercontent.com/qt/qtbase/6.11/src/plugins/platforms/cocoa/qcocoaapplicationdelegate.mm) retains normal application termination; no custom Dock menu is currently justified. Final GitHub Release native tray/Dock observations remain required.

## 中文

当前后继权威：唯一spec.md修订R3-GH及摘要见英文；PM已记录不可变后继包摘要。最终交付必须来自GitHub，本地诊断不能代替包验收。

Qt6.11原始源码表明：显式QuitRole被合并到应用退出项，托盘插入会跳过已合并项，因此需要单独NoRole托盘呈现并保留原生QuitRole/CmdQ。R3TrayQuitTests具体检查重复建立、可见/启用状态、无重复快捷键及恰好一次转发；源码充分性已审，已请PM授权。该证据说明机制，不代表最终原生复现。Qt应用delegate保留正常系统终止流程，暂无依据添加自定义Dock菜单；最终GitHub Release仍须原生观察托盘及Dock的退出可见性与实际结束。
