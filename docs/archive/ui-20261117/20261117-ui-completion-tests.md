# UI completion independent testing / 界面完成独立测试

## English

Main-language bounded successor reopened: native evidence found ModeCard titles and setup-only accessible/tooltip strings do not follow Preferences language Save. Scope successor remains historical. Evidence directory: `build/ui-completion-evidence/final-main-language/`.

Current final main-language successor verified; prior scope/predecessor runs remain historical. Evidence below is under `build/ui-completion-evidence/final-main-language/`.

| Current successor | Actual result |
|---|---|
| Project entrypoint | `scripts/build.sh release`, exit0, `entrypoint-build-r2.log/.exit`; cached installed dependencies reused |
| Complete safe CTest |78 registered;76 executed targets passed,0 failed;2 Disabled/Not Run;135.42s, exit0; `full-safe-r2.log`, console, XML and exit |
| Complete Qt rows |75 current raw text logs matched to full verbose console,1389 passes including setup/cleanup,0fail/skip; MainFlow44, Option77, Visual27, Native73, Layout87, Startup5; `raw-r2/`, `qt-counts-r2.json` |
| Fresh renders |511 PNGs =347 endpoints +164 R3 motion/visual;60/60 base matrix rows, all8 Action states; `render-index-r2.json`, `layout-render-matrix-r2.json` |
| Frozen identity |1089 files, manifest `e8ddb505c19f360664da29a8ef81bd9ced18de1dabcd6f94f3c6f812aa883b37`;0changed/added/missing during tests and post-preview recheck; before/after-r2 manifests and `summary-r2.json` |
| Fresh preview |Build exit0, Mach-O arm64; executable SHA256 `8aa3d2a1fa693008c7974aa20fd911a447f0c19f6b82b8e774efd481dde485fa`; `preview-build-r2.log/.exit`, `preview-identity-r2.json`; no launch by tester |

JUnit truncates long system-out, so row counts use complete raw text matched to the full console. R3 images live in motion/visual subdirectories and are recursively indexed; the initial aggregation-only undercount was corrected without rerunning or changing tests. Full-size saved Chinese540 and restored English540 main renders were visually inspected; both ModeCard titles follow language. Tooltips/accessibility and strict all-effective-setting preservation are asserted by the six actual flow cases. The earlier prematurely dispatched `full-safe.exit=130` remains excluded. PM owns the remaining bounded native recheck; no final installed/distributed application acceptance is implied.

Main-language live todo:
- [completed] Inspect omitted setters; independently accept same-window six-row Preferences Cancel/Save actual check (refined SHA3cf446e8ec6b66cc6f16d933ab42886d180a1ea0c49f5baa3047fbb0fef1e4d4).
- [completed] Capture clean RED before source:2 setup/cleanup passes,6 failures,5.259s; only eight stale text fields fail, effective settings stay unchanged except language.
- [completed] Eight-setter source corrected; accepted observer SHAfe1352b8 passes focused8 rows, exit0; project entrypoint r2 exit0 independently read before full dispatch.
- [completed] Complete safe full-safe-r2:76 pass/2 Disabled,135.42s,1389 Qt passes;511 PNGs,60/60 matrix,1089 files unchanged.
- [completed] Fresh excluded R2UiPreview built exit0, arm64; executable/source/core identity recorded without launch.
- [completed] Rechecked1089-file identity after preview build,0drift; no GUI/test process found and exclusive GUI control handed to PM.


Current bounded successor verified: the same-dialog injected Scope label, both items and tooltip now follow real language preview without losing the unsaved scope selection; Cancel restores original scope/language. Action short-form content starts12px inside its card at all six language/width combinations. Prior final evidence remains historical and unchanged. Current evidence directory: `build/ui-completion-evidence/final-scope-successor/`.

| Current successor | Actual result |
|---|---|
| Entrypoint | `scripts/build.sh release`, exit0 (`entrypoint-build.log/.exit`) |
| Complete safe CTest |78 registered;76 passed,0 failed;2 Disabled/Not Run;134.45s, exit0 (`full-safe.log`, `full-safe.xml`, `full-safe.exit`) |
| Per-row evidence |75 Qt text targets,1383 reported passes including setup/cleanup,0 failures/skips; one additional non-Qt target (`raw/`, verbose log) |
| Current matrices |499 PNGs =335 endpoint +164 R3;60/60 base layout rows present (`render-index.json`, `layout-render-matrix.json`) |
| Current source |1089 files, manifest `89e89fe8f9c75790e270e03e7af0f26d61502e278a99fff9ab09d5984ae87e36`;0 changes during execution. Exactly4 approved changes from predecessor: FeatureHandler, ActionDialogDesign, R3OptionContractTests, R3VisualContractTests |
| Current preview |R2UiPreview executable SHA256 `3be38e7c3beed84bc676f94c00b0bb5d66f4edaa0458c3c35b22450850ab3e24`; preview source unchanged; PM-exclusive native handoff (`preview-identity.json`) |

Successor live todo:
- [completed] Inspect real injection/language-preview paths; PE accepted actual regression SHA717a176f05100bd5b1ea64e2c9b280c8cad668e191c417418527e28381b5afa1.
- [completed] Capture RED before paired source correction:2 passes/6 failures,1.306s, exit6; no scope Save/promotion.
- [completed] Add/PE-review Action geometry six-case regression, preserve six-row RED; scope focused successor passes all six real-preview/Cancel rows.
- [completed] Verify combined frozen scope/Action fix with project entrypoint and complete safe CTest successor.
- [completed] Reconcile raw outputs/source identity and hand off fresh preview to PM; no tester GUI execution remains.


Predecessor result retained as historical: the earlier frozen source passed the pinned-manifest project build and the complete safe CTest run. Test execution is finished; the tester has handed exclusive native observation to the PM and will not start another GUI process during that review.

| Item | Actual result | Evidence under `build/ui-completion-evidence/` |
|---|---|---|
| Project entrypoint | `scripts/build.sh release`, exit0; Release, arm64, static Qt6.11.2, pinned baseline `61d7eaeb094a6acbc23d9c2e54de9208175fda47` | `final-frozen-entrypoint-build.log`, `.exit` |
| Complete safe CTest |78 registered;76 executed targets passed,0 failed;163.55s, exit0 | `final-safe-ctest.log`, `final-safe-console.log`, `final-safe-ctest.xml`, `final-safe.exit` |
| Disabled interactive tests | OSXClipboardTests and OSXKeyStateTests: Disabled/Not Run, never counted as passes | `final-ctest-inventory.json`, JUnit and full log |
| Per-row results |75 Qt targets report1371 passes including setup/cleanup,0 failed/0 skipped; ProtocolUtilAllocationTests is the additional non-Qt target | `final-safe-raw/` (75 text files), full verbose log |
| Settled screenshots |335 endpoint PNGs plus158 R3 motion/visual PNGs =493; every10-page-kind x2-language x3-width base row exists (60/60), with all actual tabs and8 Action states | `final-render-index.json`, `final-layout-render-matrix.json`, `final-renders/`, `final-r3/` |
| Source identity |1089 runtime/build/test/source files; before/after manifest `dc5f2b4c2176f70657ef021d7911931b4968e0ba27067b9ba8193b9d8c3feb0d`;0 changed or missing files | `final-source-manifest-before.json`, `final-source-manifest-after.json`, `final-safe-summary.json` |
| Native preview | Fresh isolated R2UiPreview built, exit0, arm64; exclusive PM handoff, no tester GUI process remains | `final-preview-build.log`, `.exit`, `final-preview-identity.json` |

The fresh entrypoint reconfigured the pinned manifest and rebuilt/relinked affected targets; installed vcpkg ports and build caches were reused as logged. This is not a claim of a cold dependency rebuild. Every enabled CTest target above executed freshly.

Live todo (tester-owned work):
- [completed] Read AGENTS/spec/build package and inert HTML source; audit inherited tests and isolation.
- [completed] Run the diagnostic build and existing R3 RED baseline; preserve raw failures.
- [completed] Repair test-only isolation and probe payload completeness after PM authorization and independent PE acceptance.
- [completed] Add independently reviewed real-widget, two-action, two-certificate, absent-target, empty-grid and endpoint-render checks before paired fixes.
- [completed] Preserve compile failures, correct test mechanics/source-owned build faults, and revalidate without relabeling failures.
- [completed] Verify paired fixes and approved observer successors; retain old evidence and lineage.
- [completed] Run the final entrypoint and complete safe CTest; preserve raw text/JUnit and full render matrix.
- [completed] Reconcile raw receipts, source hashes and representative full-size endpoint images; hand off the final preview to the PM.

The new MainWindowFlow suite reports38 Qt rows (36 actual cases plus setup/cleanup): English/Chinese at540/600/820 cover explicit name Save/Cancel/focus loss/Enter/Escape and disk readback; protected selected-screen deletion and draft Save/Cancel; nested Hotkey/Action removal/addition, parent row refresh, survivor editing and parent Cancel; two generated certificates with saved A/draft B, explicit saved-certificate label, full hash/source A, Back preserving draft B and Cancel/reopen preserving A; real MainWindow update checking/failure/Retry/cancel through a loopback checker; actual partial HTTP bytes, positive rate, stalled waiting with no rate/ETA and transfer deletion on Cancel. Existing log/TLS/downloader security/menu/tray/serialization tests remain. The fingerprint visual supplement verifies complete two-line16-byte SHA-256 layout, left alignment, no clipping, hidden empty footer and primary local OK in all six combinations. Existing remote trust/reject defaults are checked separately.

Historical evidence is retained. `baseline-r3-ctest.log` had Option68 failed rows, Visual13, and all3 startup fixtures timed out. After initial fixes, `first-patch-option-visual.log` and `first-patch-other.log` retained remaining failures and293 early images. `follow-on-red.log` confirmed stale parent Action rows, ambiguous saved-certificate presentation and lost absent targets before fixes. `fingerprint-visual-red.log` retained six centered-hash failures before the final local formatter correction. Product compile failures and the mechanical include-path failure remain in their numbered build logs.

Approved check lineage is explicit: the missing Traditional catalogue oracle now requires catalogue absence and expects the existing Taiwan-to-China fallback while retaining empty stored system-language assertions; no Traditional catalogue acceptance is claimed. The isolated legacy notice observer accepts only the exact generated migration body, owned backup path, Information icon and sole OK action, permitting macOS's empty title; full startup assertions remain, and production migration is untouched. Popup activation still must occur immediately exactly once with the requested index; only actual popup/container closure gets a bounded1s observation wait. All16 observed choices closed after93..108ms. Static images wait for named motion overlays to settle; R3Motion still verifies genuine intermediate frames. Prior failed evidence was not overwritten.

Predecessor preview identity (historical): `/Users/tupig/Workspace/TupigSynergy/build/bin/R2UiPreview.app`, executable SHA256 `ddca272e2921d0f02c2bf514badbc10ccc64c1f031308ce5bf5bb2d21038459c`, source SHA256 `3f3edb3b6ce455d07c3f7cc754f6f587ec309df544231600614cbe710b5eb2e0`. The generated marker uses QDir::temp; this host expects `/private/var/folders/w9/6pqrm3k56616x83f5z81kdhc0000gn/T/tupig-r2-preview-latest.json`, and only a marker refreshed after PM launch is current. The bundle's diagnostic version field is empty; the displayed application version comes from the compiled version. It seeds an isolated generated certificate with TLS/automatic core/update disabled and blocks Start/Restart/Updates/Clear Settings commands. It omits production entry hooks; the separately tested R3StartupProbe exercises real main/hooks with temporary INI migration fixtures. Neither is byte-identical delivered-app or macOS NativeFormat migration acceptance.

Scope limits: tester changed only test artifacts and this report, ran local diagnostics, and preserved the inherited dirty implementation. No real input/clipboard, OS permission grant, live sharing/core connection, installation/replacement, final DMG/package, GitHub publication, Windows/Linux execution or hardware acceptance is claimed. Final native CUA observations belong to the PM's separate evidence. No GUI execution will resume without coordination.

## 中文

主窗口语言有界后继重开：原生观察发现ModeCard标题及仅setup写入的无障碍/提示文案未随首选项语言保存刷新。范围后继证据保留为历史，新证据目录 `build/ui-completion-evidence/final-main-language/`。

主窗口语言最终后继已验证；范围后继及更早结果保留为历史。当前证据均在 `build/ui-completion-evidence/final-main-language/`。

| 当前后继 | 实际结果 |
|---|---|
| 项目入口 |`scripts/build.sh release`退出0，见`entrypoint-build-r2.log/.exit`；复用已安装依赖与构建缓存 |
| 完整安全CTest |78注册，76实际执行目标通过、0失败；2项Disabled/Not Run；135.42秒、退出0；见`full-safe-r2`日志、console、XML及exit |
| 完整Qt行 |75份本轮原始文本与完整verbose日志逐目标核对，1389通过（含准备/清理）、0失败/跳过；MainFlow44、Option77、Visual27、Native73、Layout87、Startup5；见`raw-r2/`及`qt-counts-r2.json` |
| 新渲染矩阵 |511张PNG＝347端点＋164R3；60/60基础布局行齐全，覆盖8种Action；见`render-index-r2.json`、`layout-render-matrix-r2.json` |
| 冻结身份 |1089文件，摘要见英文当前表；测试前后及预览构建后0改动/新增/缺失；见前后r2清单及`summary-r2.json` |
| 新预览 |构建退出0、Mach-O arm64；执行文件摘要见英文当前表与`preview-identity-r2.json`；测试代理没有启动 |

JUnit会截断较长system-out，逐行统计因此采用完整原始文本，并与完整console逐目标比对。R3图在motion/visual子目录，已递归索引；初次汇总脚本少计已修正，不重跑、不改测试。已查看540宽中文保存及英文恢复的全尺寸主窗口图，两张ModeCard标题跟随语言；提示/无障碍与全有效设置严格保持由六个实际流程案例断言。误提前派发并中断的`full-safe.exit=130`保留且不计验证。剩余有界原生复核由PM负责，不暗示已安装/交付应用验收。

主窗口语言实时任务：
- [已完成] 检查遗漏setter并独立接受同窗六组首选项取消/保存实际检查，精化摘要见英文段。
- [已完成] 源码修正前保留干净失败：2行准备/清理通过、六组失败、5.259秒；仅八项旧文案失败，除语言外有效设置不变。
- [已完成] 八setter已修正；独立接受观察SHAfe1352b8聚焦8行通过、退出0，正式入口r2退出0已在完整执行前明确读回。
- [已完成] 完整安全full-safe-r2：76通过/2禁用，135.42秒，1389个Qt通过行；511张图、60/60矩阵、1089文件无变化。
- [已完成] 独立R2UiPreview重建退出0、arm64；记录执行文件/源码/core身份，没有启动。
- [已完成] 预览构建后重核1089文件无变化，没有GUI/测试进程，已交回PM独占原生观察。


当前有界后继已验证：同一窗口的注入“范围”标签、两项文字及提示随真实语言预览刷新，保留未保存范围选择；取消恢复原范围/语言。Action短表单六个语言/宽度组合都从卡片内12px开始。原最终证据保留为历史，当前证据目录为 `build/ui-completion-evidence/final-scope-successor/`。

| 当前后继 | 实际结果 |
|---|---|
| 项目入口 | `scripts/build.sh release`退出0，见`entrypoint-build.log/.exit` |
| 完整安全CTest |78注册，76通过、0失败，2项Disabled/Not Run；134.45秒、退出0，见`full-safe.log/xml/exit` |
| 逐行证据 |75个Qt文本目标报告1383通过（含准备/清理），0失败/跳过；另一个非Qt目标，见`raw/`与verbose日志 |
| 当前矩阵 |499张PNG＝335端点＋164R3；60/60基础布局行均存在，见`render-index.json`及`layout-render-matrix.json` |
| 当前源码 |1089文件，摘要见英文当前表，执行前后0变化；相对前件只有FeatureHandler、ActionDialogDesign、R3Option、R3Visual四份已批准改动 |
| 当前预览 |新执行文件SHA256 `3be38e7c3beed84bc676f94c00b0bb5d66f4edaa0458c3c35b22450850ab3e24`；源码不变，PM独占原生观察，见`preview-identity.json` |

后继实时任务：
- [已完成] 检查真实注入/语言预览路径；PE接受实际回归，摘要见英文段。
- [已完成] 配对修正前记录2通过/6失败、1.306秒、退出6；不执行范围保存/提升。
- [已完成] 新增/PE审核Action几何六组合，保留六行失败；范围后继六个真实预览/取消组合均通过。
- [已完成] 用项目入口及完整安全CTest后继验证冻结的范围/Action修复。
- [已完成] 核对原始输出/源码身份并交接新预览，测试代理无运行中的GUI。


以下为保留的历史前件结果：此前冻结源码通过固定manifest项目入口构建及完整安全CTest。测试执行已经结束；测试代理已把原生观察独占权交给PM，在该审核期间不再启动GUI进程。

| 项目 | 实际结果 | `build/ui-completion-evidence/`内证据 |
|---|---|---|
| 项目入口 | `scripts/build.sh release`退出0；Release、arm64、静态Qt6.11.2；固定baseline见英文段 | `final-frozen-entrypoint-build.log`、`.exit` |
| 完整安全CTest | 注册78项，实际执行76项全部通过，0失败；163.55秒、退出0 | `final-safe-ctest.log`、`final-safe-console.log`、`final-safe-ctest.xml`、`final-safe.exit` |
| 禁用交互测试 | OSXClipboardTests、OSXKeyStateTests为Disabled/Not Run，不计通过 | `final-ctest-inventory.json`、JUnit及完整日志 |
| 逐行结果 |75个Qt目标报告1371通过（含准备/清理），0失败/0跳过；另一个非Qt目标为ProtocolUtilAllocationTests | `final-safe-raw/`75份文本及完整verbose日志 |
| 稳定截图 |335张端点截图，加158张R3动效/视觉截图，共493张；10类子页×2语言×3宽度共60基础行均存在，包含实际全部标签及8种Action状态 | `final-render-index.json`、`final-layout-render-matrix.json`、`final-renders/`、`final-r3/` |
| 源码身份 |1089份运行/构建/测试源码，前后清单摘要见英文段；0变化、0缺失 | `final-source-manifest-before.json`、`final-source-manifest-after.json`、`final-safe-summary.json` |
| 原生预览 |重新构建隔离R2UiPreview，退出0、arm64；已独占交接PM，测试代理没有运行中的GUI | `final-preview-build.log`、`.exit`、`final-preview-identity.json` |

本轮项目入口重新配置固定manifest并重编译/重链接受影响目标，复用了日志中已安装的vcpkg端口和构建缓存，不声称冷启动重建全部依赖。上述全部启用CTest目标均为本轮新执行。

实时任务（测试代理负责范围）：
- [已完成] 读取AGENTS/主规格/执行包及设计稿惰性源码，检查继承测试与隔离。
- [已完成] 执行诊断构建及原有R3失败基线，保留原始失败。
- [已完成] 经PM授权及PE独立接受后，修复测试专用隔离和启动probe载荷完整性。
- [已完成] 在配对修正前，新增并独立审核真实控件、双动作、双证书、缺失目标、空网格和端点渲染检查。
- [已完成] 保留编译失败，修正测试机械问题及源码负责人处理的构建缺陷，不重命名或掩盖失败。
- [已完成] 验证配对修复及获准的观察后继，保留原证据和沿革。
- [已完成] 执行最终项目入口及完整安全CTest，保存逐行文本/JUnit和完整渲染矩阵。
- [已完成] 核对原始回执、源码摘要和代表性全尺寸端点图，交接最终预览给PM。

新增MainWindowFlow共38个Qt行（36个真实案例及准备/清理），中英文在540/600/820各覆盖：名称显式保存/取消/失焦/Enter/Escape及磁盘回读；选中计算机删除保护和草稿保存/取消；嵌套Hotkey/Action删除/增加、父页行刷新、编辑存活动作及父页取消；不同生成证书的已保存A/草稿B、明确按钮、完整A指纹与A来源、返回保留B及取消重开保留A；真实MainWindow经loopback检查/失败/重试/取消；真实部分HTTP字节的正速率、停滞等待（无速率/ETA）及取消销毁传输。既有日志/TLS/下载安全/菜单/托盘/序列化检查保留。新增指纹视觉检查在六组合验证每行16字节的完整两行SHA-256、左对齐、不裁切、空footer隐藏和本地主要OK；远端信任/默认拒绝另有既有检查。

历史失败全部保留：`baseline-r3-ctest.log`中Option有68失败行、Visual13行、三个启动夹具均超时；`first-patch-option-visual.log`及`first-patch-other.log`保留首轮残余失败和293张早期图；`follow-on-red.log`在修正前证实父Action旧行、已保存证书表述歧义及缺失目标丢失；`fingerprint-visual-red.log`保留最后局部格式修正前六行居中失败。产品编译失败与测试include机械失败仍在各编号构建日志中。

检查沿革明确记录：缺少繁体目录的后继先断言目录不存在，再按现有台湾→大陆回退期待简体，同时保留跟随系统的空存储值检查，不声称繁体目录验收。隔离legacy提示只接受完整生成迁移正文、已拥有备份路径、Information图标及唯一OK，允许macOS空标题；完整启动断言不变，产品迁移不改。弹出选择仍要求立即且仅一次激活并选中目标，只对真实弹层/容器关闭进行最多1秒观察；16次实测关闭为93..108ms。静态图等待具名动效层结束，R3Motion继续验证真实中间帧。原失败未覆盖。

预览路径、执行文件/源码摘要及当前清单摘要均见英文段和`final-preview-identity.json`。marker通过QDir::temp生成，本机规范路径为上文`/private/var/folders/.../T/tupig-r2-preview-latest.json`，只有PM启动后重新写入的marker才是新证据。诊断bundle版本字段为空，界面版本来自编译常量。预览生成隔离证书，关闭TLS/自动core/自动更新并阻断Start/Restart/Updates/Clear Settings；不调用生产入口钩子。另测的R3StartupProbe使用真实main/钩子和临时INI迁移夹具；两者均不等于交付应用逐字节身份或macOS NativeFormat迁移验收。

边界：测试代理仅修改测试工件和本报告、执行本机诊断，保留继承的大量脏实现。不声称真实键鼠/剪贴板、权限授权、真实共享/core连接、安装/替换、最终DMG/软件包、GitHub发布、Windows/Linux实测或硬件验收。最终原生CUA属于PM的独立证据；未经协调不恢复GUI执行。


Successor lineage / 后继沿革：Scope actual check SHA717a176f05100bd5b1ea64e2c9b280c8cad668e191c417418527e28381b5afa1 was independently accepted before RED/source; `red.log` preserved2 setup/cleanup passes and6 stale-label failures, then `green.log` passed8 rows after FeatureHandler text-only refresh. The selection/data/original-scope identity and existing dirty/Cancel semantics are checked; no scope Save/restart was performed. Action geometry check SHA28412fa3b80c27029922c592903473486c7e3d960c8c796b385066788cec0bdf was independently accepted after a direct-header mechanical repair; `action-red.log` preserved six123..285px offset failures. The single form-alignment source line now yields12px in every case, retaining180px fields and prior full natural-height checks. Scope检查先获独立接受再执行失败/修复验证，未保存或提升范围；Action保留真实123..285px失败，最终六组合均为12px，原180px字段和完整自然高度检查不变。原生后继复核由PM继续，测试代理保持GUI执行停止。


Main-language fixture boundary / 主窗口语言夹具边界：Initial `final-main-language/red.log` also exposed existing storage canonicalization (unset command QVariant to empty QString, enum Info to INFO) and one first-page popup readiness observation. PM/PE approved explicit canonical empty command strings/INFO BEFORE the strict all-effective-key baseline plus bounded real page/geometry readiness before actual popup; no ignore list or product default/Save change was introduced. `red-r2.log` then produced six clean failures on all eight intended text fields only. 初始日志里的默认存储表示规范化及首帧弹层准备观察已单独保留；精化仅在严格全有效值基线之前明确夹具表示并等真实页面就绪，不加入忽略键、不修改产品默认值/保存策略。

Native evidence limits / 原生证据限制：Direction AX Pick is unverified because CUA returned AXError.notImplemented; Qt actual Action-popup16 selections passed and do not establish that native Direction path. Dock API timed out and physical tray observer hotkey was not observed. No Dock/physical-tray acceptance is claimed; previously observed application-menu Quit is a separate path. 方向原生AX选择未证实，Dock接口超时、物理托盘观察快捷键未观测，不把Qt动作弹层或应用菜单退出误写成这些原生路径的通过。


Main-language event/pipeline correction / 主窗口语言事件及执行纠正：`green.log` retained six premature Cancel-return observations: I18N/stored language was restored but normal queued LanguageChange had not run after a DeferredDelete-only flush. PM/PE accepted SHAfe1352b8ac44c1220e1895fd4a8de25abc381cfe5efc6145b596a8c400ede5a7, which uses the existing bounded real-window settle after Cancel/Save/reopenCancel, with unchanged strict text/settings assertions and before/after logging. `green-r2.log` then passed8 rows in7.498s, exit0. A full run mistakenly dispatched before the earlier focused failure was inspected was stopped by the tester and retained at `full-safe.exit=130`; it is not validation. The final `full-safe-r2` dispatch is explicitly gated on focused and entrypoint exit0. 初次Cancel观察只刷新DeferredDelete，未处理正常LanguageChange，失败原样保留；精化不强设文案/状态、不忽略设置、不扩展超时。聚焦r2通过后才准许完整r2；误提前启动的部分运行已中止且不计验证。
