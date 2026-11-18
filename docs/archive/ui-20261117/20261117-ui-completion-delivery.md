# UI completion delivery audit / UI 完整实现交付审查

## English

Scope: W-C04, UI-001..016 and UI-019, reference `/private/tmp/tupig-ui-full.html`.
Inherited dirty work is preserved. No product edits, builds, test execution,
installation, publication or live settings access are authorized to this agent
before PM assignment and check adequacy approval.

Todo:
- Completed: read repository rules, master specification and current execution package; inspect inherited status. (Initial reads preceded this file; repaired before further work.)
- Completed: audit every reference page/tab against current source and inspect existing check/render evidence.
- Completed: send concrete gaps, bounded ownership proposal and contract needs to PM.
- Completed: implement first approved dialogs/AppStyle corrections; independent first focused rerun reduced68 option failures to3 and13 visual failures to6.
- Completed: finish approved M-C02..04 follow-on, source-backed popup/checkbox corrections, single-card sharing/compatibility composition and documented language fallback; freeze source for independent checks.
- Completed: apply PM's temporary compile-only ownership transfer for UpdateDialog.cpp116 unused lambda capture; no behavior change.
- Completed: check that mechanical repair with source inspection, SHA-256 and `git diff --check`; notify sole build owner.
- Completed: inspect independent follow-on RED: model/visual pass; name/delete/update flow passes, while parent-action refresh and fingerprint labels fail6rows each and absent target fails.
- Completed: implement the three approved semantic corrections; independent Flow38rows, Visual and Model passed. Popup observer confirms actual activation/index immediate and stock closure asynchronous; bounded observer successor passed.
- Completed: implement approved fingerprint finishing after actual6row RED (left-aligned16-byte lines/full hash, local empty footer hidden, local primary OK) and update action leading stretch at verified540width; final source freeze.
- Completed: inspect final source diff, frozen37-file identity, full safe CTest output and fresh full-size Log/Fingerprint renders; hand bounded delivery back to PM for whole-run matrix/native closeout.
- Completed: inspect FeatureHandler scope injection and the independently accepted actual6row language-popup RED; same-dialog labels remain stale in both language directions.
- Completed: apply approved lifetime-bound scope text-only refresh, preserving scope data/draft/Save behavior; source frozen at23c47f64318a18bb5491409022535c70cf4cc4128fa28b53348b3ab1e1332819.
- Completed: inspect actual6row Action form RED (left offsets123–285px) and apply only explicit Qt left/top form alignment; combined source frozen.
- Completed: independently inspect successor entrypoint/full-safe/targeted outputs, fresh Action render and38-file frozen source identity; return the source-frozen successor to PM.
- Completed: compare main setup/retranslate setters only and inspect independently accepted clean6row Pref popup Cancel/Save RED; the8 stale texts reproduce with settings/state invariants intact.
- Completed: add exactly8 approved existing-context/string setters only in Ui::MainWindow::retranslateUi; no other source changes. Frozen SHA-256b9e4aa21648c85bc09e09e7d5033996c61a2dbb6f50445f39590e20e9b2c352e.
- Completed: independently inspect final main-language focused8row GREEN, full76-target safe run, both saved-language images and39-file frozen identity; return source-frozen delivery to PM.

Audit evidence is current source plus inert HTML; historical preview PNGs below
illustrate previously captured defects only, with no claim that they identify
the current binary. No tests were executed by this agent. Existing
`build/r3-test-evidence/options-prefixed-red.log` records eight stable-ID
failures; it is historical RED evidence, not this run's fresh baseline.

| Surface / reference lines | Current source evidence and remaining gap | Bounded proposal / checks |
|---|---|---|
| Main server/client, name editing, log; HTML487-508 | MainWindowDesign.cpp133-145 and MainWindowConfiguration.cpp293-344 expose only a field/Enter commit while reference has explicit Save/Cancel. Main shell, real IP, modes, server/client controls and bounded log already exist. | PM-owned additional host assignment/check needed for explicit name Save/Cancel; do not invent mock state. Retain existing NativeDesignTests main/log coverage. |
| Server layout; HTML510-526 | ServerConfigDialogDesign.cpp118-166 keeps native group title,180px three-row view, drag-only trash/new-screen icons; ServerConfigDialog.cpp53-65 adds accessible Add but no explicit selected-screen Delete. | Inner-flow heading and primary OK covered by R3Visual. Additional selected-delete oracle/assignment needed to match reference keyboard alternative; retain model protection/local primary behavior. |
| Server hotkeys/actions; HTML527-536 | Both compact row-sized lists and adjacent Add/Edit/Remove exist (Design.cpp196-262); native group titles remain. | Convert card headings; preserve list observers, transactional Action/Hotkey children and all Edit/Remove controls. |
| Server sharing/switching; HTML537-549 | Design.cpp301-448 retains four titled cards, protocol radios and real independent enable flags. Reference has one card/form with protocol select. | Merge visual subgroups into one outer card with inner headings and retain the four corner flags, heartbeat/delay/double-tap flags and both protocols. Do not simplify real data into reference's illustrative single dead-corner choice. Existing R3Visual checks native-title absence; render review needed for actual composition. |
| Server external config; HTML548-549 | Design.cpp475 checkable group places control at border, unlike normal in-card checkbox. | Hidden legacy checkable proxy plus visible bound checkbox; retain disabling layout/hotkeys/sharing and validation. Existing visual oracle plus external-config transaction check. |
| Client; HTML551-557 | Design.cpp177/187 native group titles; existing connection/language flags and compact scale/invert rows are real. Retry explanation is tooltip only. | Inner headings, visible concise retry explanation and primary Save; retain all flags/Reset/Defaults. Visual and existing client gap/layout checks. |
| Preferences General; HTML560-569 | SettingsDialog.cpp60-66 builds only detected native labels, no explicit system empty ID. FeatureHandler.cpp230-268 already places scope selector in generalCard. | Stable language IDs plus explicit empty option and preview rollback; preserve scope hook and change semantics. R3Option has popup, preview, Save/Cancel/reopen paths. |
| Preferences Window; HTML570-578 | SettingsDialog.cpp127-143 binds index0/1 to hidden radios; Design.cpp552-556 orders Exit then background, contrary to Hide then Quit requirement. | Bind behavior via bool itemData and reorder Close labels; preserve stored bool and two-way radio proxies. R3Option/NativeDesign transactions. |
| Preferences Logs; HTML579-588 | Design.cpp273-332 separates logCard and visible checkable groupLogToFile; warning precedes path; browse lacks32px property; trailing buttonBox is outside active card. | Single logCard level/path/file-checkbox/debug-checkbox/warning/action flow; hidden business proxy; stretch usable path. R3Visual exact geometry/pixels/transactions is already written. |
| Preferences Network/Security; HTML589-600 | Design.cpp375-376 shows checkable TLS group; certificate controls/real key metadata and peer requirement exist. View fingerprint entry from reference absent here (available via main/TLS). | Single card with normal TLS checkbox and retained certificate/key/peer controls; primary save/actions in card. Fingerprint navigation needs added oracle before new product connection. |
| Preferences Advanced; HTML601-603 | Design.cpp463-476 checkable service group and separate commands card; non-Windows handling in SettingsDialog remains real. | Visible normal service checkbox plus hidden proxy; merge inner sections and preserve platform visibility and enter/exit enable bindings. R3Visual plus settings transactions. |
| Computer Info; HTML604-606 | Design.cpp373 titled group; true name validation and empty-error hiding already implemented. | Inner heading/primary Save; keep inline errors and transaction. |
| Computer Modifiers; HTML607 | ScreenSettingsDialog.cpp102-107 and166-171 load/save indices. Unknown values become -1 and can be rewritten. Seven real targets are present. | Populate stable IDs0..6, preserve unsupported selections with read-only/Save disabled; no implicit conversion. Existing R3Option modifier/unknown oracle. |
| Computer Corners/Compatibility; HTML609-615 | Design.cpp399/406 separate native titles. Four normal labeled corners and compatibility flags exist. | Inner section headings in one outer card; retain controls and180px short size. |
| Computer Aliases; HTML608 | Real row-sized list and inline Add/Remove/validation exist; native title remains (Design.cpp412). | Inner heading and primary Save; preserve multi-selection and row-height invalidation. |
| Hotkey; HTML618-627 | HotkeyDialog.cpp24-35 truthful recording state and Save-disable exist; native title in Design.cpp80. Reference also shows associated actions, while production owns them in parent server hotkeys page. | Inner heading, primary OK. Additional associated-action context/navigation would need an oracle and PM allocation; avoid silently adding draft ownership to child. |
| Action all8 types; HTML629-640 | ActionDialogDesign.cpp114-127 orders IDs0..7; ActionDialog.cpp43-47,84-115,150-183 treats indices as IDs; unsupported8..10/unknown become index-1 yet canSave returns true and accept destroys fields. | Stable Action IDs3,4,2,0,1,5,6,7; direction1,0,2,3; trigger/lock/recipient IDs; unsupported original clearly read-only, disable Save, preserve all fields. Validate relevant fields before accept; retain All/Specified only. Existing R3Option coverage is substantive. |
| Fingerprint local/compare; HTML642-651 | FingerprintDialog.cpp82-96 explicit Trust/Reject with Reject default, preview SHA256/copy already exists. FingerprintPreview.cpp78-85 centers hash while reference left-aligns code. | No security behavior changes needed; align card code only if new visual check approved. Current tests cover complete hash/default reject; request compare-mode fresh renders. |
| Update checking/latest/available/downloading/failure; HTML673-696 | MainWindowWindow.cpp129-186 implements checking/latest/failure; UpdateDialog.cpp48-115 native card titles; rate is transfer-average and zero shows dash (not waiting text); check failure has OK only instead of Retry. | Existing UpdateDialog within unassigned update/ path: report to PM. Separate retry/waiting/transient-state checks needed before edits. Never emulate reference fake data or bypass hash/install boundaries. |
| About; HTML698-703 | AboutDialog.cpp86-180 concise brand/actual version/project/contributors/notices preserved; project URLs use TupigSynergy while AGENTS tracker uses Tupig_synergy. | Verify canonical repository before changing URLs; retain original notices and Lucide license disclosure. Existing NativeDesign checks brand/version but not URL correctness. |
| Messages all5; HTML705-712 | MessagePage.cpp24-105 provides card/actions, but live call sites still use QMessageBox converted by MainWindow::showPage; close-reminder uses actual settings. | Fresh render all5 and inspect live action mapping; no speculative changes. Copy in design is illustrative; avoid changing deletion semantics from mock text. |
| Native main/tray/Dock; HTML715-740 | AppMenus.cpp49-136 correctly shares main actions and NoRole tray proxy. MainWindow.cpp325 routes Quit to non-spontaneous close; MainWindowWindow.cpp270 permits app quit. No Dock-menu setup exists in inspected GUI source. | Root-owned native observation/assignment; R3Tray tests logical proxy only. Add real isolated native menu observation, keep UI-019 final GitHub-native limitation open. |
| Tabs/chrome/motion | AppStyle.cpp218-233 sets CSS but no explicit qproperty-documentMode tab-bar workaround; captured old PNGs show native enclosing strip. R3Visual exact background pixel test covers this. Motion/PageHost are implemented and have actual intermediate-frame tests. | AppStyle only after tester RED. Do not rewrite passing Motion/PageHost without a concrete failing oracle. |

Implementation order proposed: (1) stable option data/unsupported preservation,
(2) a small inner-card heading helper and shared button-role helper in
dialogs/CompactLayout.h, (3) single-card settings/proxy rebuild and adjacent
actions, (4) apply helper to server/client/computer/hotkey/action and targeted
composition, (5) tabs CSS/native style fix, (6) independent rerun/render and
review. Dialog source ownership and AppStyle only; host/update/native changes
remain for PM assignment. No new check artifacts belong to this agent.

Historical visual inspection: full-size `preferences-en-600-tab2.png`,
`computer-en-600-tab2.png`, and `server-en-600-tab0.png` under
`build/r2-test-evidence/previews-layout-r5-prefinal/` visibly show baseline,
border titles, split log card and neutral Save; they are supporting regression
examples, never current verification.

Patch checkpoint 1 (2026-11-17): PM approved after fresh independent RED in
`build/ui-completion-evidence/baseline-r3-console.log`. Actual paired source
changes now implement typed option IDs, unsupported read-only preservation,
relevant-only Action writes, explicit system language with preview store
restoration, hidden settings/external-file business proxies with normal
checkboxes, single Log/Network cards, active-card preference actions, inner
card headings, primary Save/OK and32px browse controls, expanding log path and
document-mode tab styling. Source frozen for independent compile/test cycle.
`git diff --check` passed; this agent has not compiled or executed checks.
PM additionally allowed the three existing unfinished Chinese translations
(Follow system and the two unsupported/read-only labels); contexts preserved.
Tester owns QM generation. The subsequent approved correction also refreshes
the Follow-system label during language preview.

Follow-on proposals awaiting actual approved supplements: main explicit name
Save/Cancel should reuse validated setHostName and restore stored text on cancel;
selected Delete should be a normal button using the existing model removal
and primary protection; Hotkey associated actions need a local Hotkey draft
and nested Action draft so Cancel never mutates the parent's draft; Preferences
fingerprint view should derive the current saved certificate through existing
TlsUtility/FingerprintDialog and remain a read action; update progress should
live inside its card, check failures expose a real Retry, zero recent bytes
show waiting text/unknown ETA without bypassing digest or install approval.
Root separately owns native menus, host magenta IP link, and source identity.
Canonical repository remote was verified by PM as `Tupig/TupigSynergy`; the
existing About links are correct, so no URL correction is planned.

Patch checkpoint 2: M-C02..04 and composition are implemented and frozen.
Delete checks primary/null/external state both when updating availability and
inside its handler. PM-approved `ScreenSetupModel.cpp` change only enables
selection/navigation of empty cells, keeping them nondraggable. Hotkey's
optional config pointer keeps old callers compatible; real Server callers
pass their config, and local draft/actions only reach the caller after parent
acceptance. Fingerprint uses the saved certificate through the real utility,
with no certificate creation or draft write. Sharing and compatibility use
one outer card, transparent inner sections and existing controls. Log warning
is always informative, and22px checkbox rows retain readable spacing.
Three popup helper families now use real QListView. The Chinese translations
include exactly the requested new host/update/dialog labels in their original
contexts; XML parses and contains no duplicate source per context.

PM approved a source-backed locale-check successor because no Traditional
Chinese catalogue is shipped: existing I18N lookup prefers Taiwan, then China,
then English. `docs/configuration.md` now explains this in both sections;
production common/I18N source is untouched. Dialog preview follows the same
available-catalogue fallback. Independent first-patch compile initially found
one unused lambda capture under-Werror; it was removed and the subsequent
focused test executable built. No failures are concealed as passes.

Post-patch independent review opened two precise corrections: parent Server
action-list rows can become stale after nested Hotkey action insert/remove;
and generic Preferences fingerprint wording hides that it reads the saved
certificate rather than an unsaved draft path. PM chose coherent parent-list
rebuild/cleared selection, and explicitly labeled saved-certificate/source
semantics. Tester created actual successor checks, independently accepted
before paired source changes; all6 rows for each correction then passed.
An additional source candidate was sent to PM: an absent existing SwitchTo
target currently falls back to the first combo entry; it must not be silently
rewritten without a deliberate user selection. Its actual regression now
passes after the unavailable original is retained with Save disabled until
deliberate replacement. No speculative product edit was applied.

Historical bounded-agent checkpoint (2026-11-17, before the reopened
scope-language and Action-form corrections):

- Sole tester executed the real `scripts/build.sh release` entrypoint; final
  frozen build receipt `build/ui-completion-evidence/final-frozen-entrypoint-build.exit`
  is0. Chinese QM generation reports490 finished,0 unfinished.
- Independently read complete final CTest tail/exit: `final-safe.exit` is0,
 76 registered targets passed,0 failed,163.55seconds. Two interactive targets
  were explicitly Disabled/Not Run: OSXClipboardTests and OSXKeyStateTests.
  They are not counted as passes. See `final-safe-console.log` in the same
  evidence directory. No tests/builds were run by the delivery agent.
- Independently inspected the fresh full-size
  `final-r3/visual/fingerprint-en-540.png` and `log-zh_CN-600.png`: complete
 32-byte two-line hash is left aligned and contained, local OK is primary,
  empty footer gap is removed; Log has a single card, usable path, normal
  checkboxes with breathing room, persistent explanation and adjacent tiers.
- Popup diagnostic proves actual selection signal exactly once and correct
  index immediately, then stock popup closure93–108ms later. The observer-only
  bounded closure successor retains the immediate assertions and reports4 Qt
  rows passed,0 failed in2.541seconds; no further popup product change was made.
- A direct deterministic final review found all37 assigned-source-scope files
  equal to the tester's frozen manifest. Source manifest aggregate identity:
  `dc5f2b4c2176f70657ef021d7911931b4968e0ba27067b9ba8193b9d8c3feb0d`;
  manifest JSON file SHA-256:
  `7aaa5ff3e6b74a79976f6bdfade0b5fc49196d1b55aa950947d4f9de32806f53`.
  HEAD remains inherited `a2793eff1f9b8d51900f2b47270ab346f2fbab4d` plus
  preserved dirty source; no commit, push, installation or publication.
- `git diff --check` passes. Chinese TS context/source/value comparison was
  exact for the31 metadata-only completions; XML and per-context uniqueness
  pass. Source frozen after the final three presentation edits.

| Final artifact | SHA-256 |
|---|---|
| `src/lib/gui/dialogs/ActionDialog.cpp` | `b60e4281fb7e4c94eecfa69d6b5d5c7b6bc84f53226d95b1d4969ba339a06882` |
| `src/lib/gui/dialogs/ServerConfigDialog.cpp` | `f8243a856c9cfa5f6b055d89ef0034d1a7e7e36fd757f6326160567d3e7927df` |
| `src/lib/gui/dialogs/SettingsDialog.cpp` | `8363da3599a32c8e9ca1df3d231c58dbf05affe9e7c36b3d899b4f0eaae2c83f` |
| `src/lib/gui/dialogs/HotkeyDialog.cpp` | `0a1be8bcf6d1c18ab40a5cfff18b6c99399d9c893ba02e15c7add7fb5c3da516` |
| `src/lib/gui/dialogs/CompactLayout.h` | `946da6e463a59db6a36bb047870093a28253a1c36a8de65e03636737c5d734f0` |
| `src/lib/gui/ScreenSetupModel.cpp` | `3dc5ff6ba1fe84ee0c926c1e8c6428f6f7f5dd97e1d2813f2d2cf4e0b3514a12` |
| `src/lib/gui/dialogs/FingerprintDialog.cpp` | `f896cf5b6ddb7051b66d306c8765b0128de837b2dfd6be30bb93dea6d8fed555` |
| `src/lib/gui/widgets/FingerprintPreview.cpp` | `cb20d3160d89d7227d46021577142835df867ed6eaa5c862025a421246c9e9a5` |
| `src/lib/gui/update/UpdateDialog.cpp` | `e7d1b826c6f10725784c05e5c768ec7e84ca48ede5b8e657cefd0803bd653410` |
| `translations/synergy_zh_CN.ts` | `9cd1f432ea3c69c68c19471ffa8eb26919835a260f738e6b91628fa0419949da` |
| `docs/configuration.md` | `8a046c5b09cf2cc2fe477e3d483454c163476ceb0a500a7e4733a7960debce36` |

This historical checkpoint closed the prior bounded todo; the current
successor todo above governs the reopened work. Complete settled page matrices, final native observation and
release/hardware boundaries remain with their existing owners. Local isolated
checks do not establish Windows/Linux execution, real sharing/clipboard,
permission acceptance or GitHub-distributed binary acceptance.

Scope/Action successor checkpoint (before reopening the native main-window
live-language setter finding):

- FeatureHandler now refreshes only the injected scope label, bool-data-matched
  option text and tooltip on `I18N::languageChanged`; the dialog is the QObject
  context and QSignalBlocker guards text refresh. Existing current index,
  bool itemData, original scope, dirty draft and accepted scope-switch callback
  are preserved. No backend promotion/restart change or new language string.
- ActionDialogDesign adds exactly one explicit left/top form-alignment setter.
  It does not alter values, relevant fields, widths or global/native style.
  The accepted actual six-row RED had123–285px inset; successor raw geometry
  shows the label at12px in every language/width and fields remain180px.
- Independently read `final-scope-successor/entrypoint-build.exit`,
  `full-safe.exit`, and `green.exit`: all0. The project entrypoint built;
  Chinese QM remains490 finished/0 unfinished. Complete safe CTest again reports
 76 passed,0 failed,134.45seconds, with the same2 interactive targets Disabled
  / Not Run. Full R3Option is77 Qt rows passed; full R3Visual is27 rows passed.
  The specific real-popup same-dialog scope check is8 Qt rows passed,0 failed,
 2.370seconds, covering all6 language/width cases and draft/data/Cancel/reopen.
- Fresh full-size `final-scope-successor/r3/visual/action-alignment-en-820.png`
  confirms the form begins at the card's inner left edge. Tests, builds and
  render execution remain exclusively with the tester; this agent read their
  raw results and inspected images/identity only.
- All38 assigned-source-scope files match the successor frozen source manifest.
  Source aggregate SHA-256:
  `89e89fe8f9c75790e270e03e7af0f26d61502e278a99fff9ab09d5984ae87e36`.
  Manifest JSON file SHA-256:
  `ec6194ac9c7c1991a5192637045c159a6cf0bc50d6859025336b02fc57f46e0d`.
  FeatureHandler.cpp:
  `23c47f64318a18bb5491409022535c70cf4cc4128fa28b53348b3ab1e1332819`.
  ActionDialogDesign.cpp:
  `d586d90ea86871369fec6ca173c95055c69c8fd497a80e8bf9e9710532ef9b98`.
  `git diff --check` remained clean. This closed that scope/Action successor;
  the later main-window setter todo above governs the current reopened work.

Main live-language final successor (current bounded source checkpoint):

- Exactly8 setters were added inside `Ui::MainWindow::retranslateUi` in
  `MainWindowDesign.cpp`, reusing setup's existing ModeCard/QObject contexts and
  source strings: Server/Client titles, export tooltip/accessibility name,
  hostname tooltip/accessibility name, port accessibility name and restart
  tooltip. No helper, header, painting implementation, new string, protocol,
  defaults or state mutation was introduced. State-aware Start/Stop stays with
  its existing owner.
- Actual real Pref popup Cancel/Save tests first reproduced all6 cases in
  `final-main-language/red-r2.log` (2 setup/cleanup passed,6 failed,5.259s).
  A first observational rerun checked immediately after DeferredDelete-only
  flushing and missed normal queued LanguageChange delivery; its focused
  failure and interrupted partial full-safe exit130 are preserved, not counted
  as passes. PE/PM accepted event-loop settling before the same strict text and
  settings assertions, without a forced production retranslate call. Product
  source remained unchanged. Canonical current-store fixture values avoid3
  pre-existing default-storage representation normalizations while retaining
  the strict full effective-settings snapshot; Save/default policy was not changed.
- Independently read `green-r2.log`/exit0:8 Qt rows passed,0 failed,7.498s,
  covering6 language/width cases with real popup activation, Cancel restore,
  Save/reopen, all8 visible/accessibility values, no mode toggles and unchanged
  mode/address/port/settings except the deliberately saved language.
- `entrypoint-build-r2.exit` is0: the real project entrypoint configured and
  deployed translation resources; existing compiled product objects were reused.
  `full-safe-r2.exit` is0 and the complete raw run reports76 CTest targets passed,
 0 failed,135.42s. The same2 interactive targets remain Disabled/Not Run.
  MainWindowFlow now has44 Qt rows passed,0 failed (25.50s executable time).
- Inspected both full-size `green-r2-renders/flow-main-language-saved-zh_CN-600.png`
  and `flow-main-language-saved-en-600.png`: mode titles and subtitles agree
  after Save. Exact tooltips/accessibility values are separately asserted by
  the executable check, not inferred from pictures.
- MainWindowDesign.cpp SHA-256:
  `b9e4aa21648c85bc09e09e7d5033996c61a2dbb6f50445f39590e20e9b2c352e`.
  All39 assigned-source-scope files match the final frozen manifest. Aggregate
  source identity:
  `e8ddb505c19f360664da29a8ef81bd9ced18de1dabcd6f94f3c6f812aa883b37`.
  `final-main-language/source-manifest-before-r2.json` file SHA-256:
  `853294c1604de9ec19faa837b383e390061da2d1bd79a3a225b89cee769d0b22`.
  `git diff --check` passes. All this agent's todo items are complete again;
  source remains frozen for PM native/whole-run acceptance, with prior platform,
  hardware, permission and release limitations unchanged.

## 中文

范围为 W-C04、UI-001..016 与 UI-019，对照设计稿 `/private/tmp/tupig-ui-full.html`。
保留现有未提交工作；产品修改须等待主代理分配文件并确认实际检查充分性。
构建、测试修改与执行由测试代理独占；本代理不安装、发布或读取日常设置。

实时待办：
- 已完成：读取仓库规则、主规格及本轮执行包，查看现有变更。（首次读取先于本清单，已在继续工作前补记。）
- 已完成：逐页逐标签审查设计稿与源码，检查现有检查及渲染证据。
- 已完成：向主代理提交可定位差距、文件归属建议及检查需求。
- 已完成：实施首轮 dialogs/AppStyle 修正；独立首次复跑将68个选项失败降至3个、13个视觉失败降至6个。
- 已完成：完成已批准 M-C02..04 后续、弹层/复选框修正、共享/兼容性单卡及语言回退文档；源码冻结交独立检查。
- 已完成：主代理临时移交 UpdateDialog.cpp116 未使用捕获的编译修复，仅移除无用捕获，不改行为。
- 已完成：源码、摘要和 `git diff --check` 核对修复，通知唯一构建负责人。
- 已完成：核对独立后续RED：模型/视觉通过；名称/删除/更新流程通过，父动作刷新与指纹文字各失败6行，缺失目标失败。
- 已完成：实施三项语义修正，独立 Flow38行、视觉与模型通过；弹层实际激活及索引立即正确，关闭属于原生异步，观察后继检查通过。
- 已完成：实际6行RED后实施指纹视觉收尾（完整指纹每行16字节左对齐、本地隐藏空尾行、主色OK）及已核540宽度的更新动作前置伸缩；最终源码冻结。
- 已完成：复核最终差异、37文件冻结身份、安全CTest原始结果与新日志/指纹实尺寸图；交回主代理完成全局矩阵/原生收口。
- 已完成：检查 FeatureHandler 注入及独立认可的实际6行弹层语言预览RED，确认同一窗口双向切换时文字不刷新。
- 已完成：作用域文字刷新已以父对话框为信号上下文，保留值、草稿及保存行为；源码冻结摘要见英文条目。
- 已完成：核对实际6行动作表单RED（左偏移123–285px），仅增加Qt表单左/顶部对齐；合并源码已冻结。
- 已完成：独立核对后继项目入口/完整安全测试/专项结果、新动作页图及38文件冻结身份，源码冻结交回主代理。
- 已完成：仅对照主窗口setup/retranslate并核对独立认可的干净6行真实首选项弹层取消/保存RED；仅8处文字失败，设置/状态不变量保持。
- 已完成：仅在Ui::MainWindow::retranslateUi补8个现有上下文/源串的设置调用，无其他源码修改；冻结摘要见英文条目。
- 已完成：独立核对主窗口语言专项8行GREEN、完整76目标安全测试、双语保存后图及39文件冻结身份，源码冻结交回主代理。

审查结论：当前源码仍存在主色按钮、原生卡片标题/边框勾选、日志拆卡、
语言及行为索引语义、动作8..10/未知值写坏风险、修饰键未知值重写等明确差距。
上述英文逐页矩阵给出全部页面、参考行、源码定位及检查映射，均为本轮源码审查；
历史日志的8个选项失败和三张旧截图仅作回归样本，不能冒充当前构建通过。

建议先完成已有检查约束的稳定选项与无损拒绝，再做卡内标题/按钮助手、
首选项单卡/隐藏业务代理和各子页重排、最后纠正原生分页条并交独立测试。
本代理仅修改明确分配的 dialogs/* 与 AppStyle；主页名称保存/取消、更新失败重试、
服务端显式删除、快捷键关联动作入口及原生Dock菜单尚需主代理分配与对应检查。
所有真实引擎、存储、协议、权限、更新安全和嵌套草稿边界继续保留。

后续补丁现已按主代理批准实现并冻结：删除入口保护主机/空白/外部配置，空白格仅增加选择及键盘导航能力；
快捷键使用本地草稿与真实动作子页，只有父页确认后才回写；首选项指纹读取已保存证书且不改草稿。
共享与兼容性各合并为单外卡，保留原控件并以内部小节呈现；日志说明常驻，勾选行增为22px。
补齐主代理分配的主机/更新/子页中文字符串，保持翻译上下文，XML解析和无重复源串检查通过。
文档准确说明未提供繁体包时台湾→简体中国→英文的现有回退，common/I18N未修改。
首次编译暴露一个-Werror未使用捕获，已移除；其后已由独立测试构建选项/视觉程序，剩余问题继续跟踪。

以下为重开作用域语言/动作表单修正前的历史有界交付证据：真实 `scripts/build.sh release` 冻结构建退出0，中文QM为490完成、0未完成；
完整安全CTest退出0，76个目标通过、0失败，163.55秒。OSXClipboardTests、OSXKeyStateTests两项明确
Disabled/Not Run，未计为通过。交付代理未执行构建或测试，所有执行由唯一测试负责人完成。

重新查看最终英文540指纹与中文600日志实尺寸图，已确认32字节完整指纹左对齐两行且不裁切、本地OK主色、
空尾行移除；日志单卡、长路径、普通复选框、持续说明和紧邻动作均可见。真实弹层激活恰好一次且索引立即正确，
原生关闭晚93–108ms；只修观察等待后4行通过，未追加猜测性的产品改动。

37个负责范围内源码文件与测试冻结清单逐字节摘要一致；清单总身份、清单文件摘要和关键文件精确摘要见上表。
`git diff --check`、中文XML与上下文去重检查通过。该历史节点关闭了当时的有界待办；现按上方重开待办继续。
完整稳定矩阵、原生观察和整体Aegis验收仍由原负责人继续。
本机隔离结果不代表Windows/Linux实测、真实共享/剪贴板、权限或GitHub发布二进制验收；未提交、推送、安装或发布。

本次原生发现的后继收口：FeatureHandler仅连接语言变更并刷新注入的作用域标签、布尔数据对应项文字和提示，
以父对话框为QObject上下文，使用QSignalBlocker；选择索引、布尔数据、原始作用域、未保存草稿及确认保存的切换
回调均保留。无作用域提升、重启或后端策略改动，也无新增语言源串。动作页仅增加一行明确左/顶部表单对齐，
不改选项值、相关字段、控件宽度或全局/原生样式。

已独立读取 `final-scope-successor/` 原始证据：项目入口构建退出0；完整安全CTest再度76目标通过、0失败、
134.45秒，原2项宿主交互目标继续Disabled/Not Run；中文QM保持490完成/0未完成。完整选项检查77行通过，
完整视觉检查27行通过。真实同一窗口语言弹层的作用域专项8行通过、0失败、2.370秒，覆盖中英文三宽度及
数据、草稿、取消、重开。动作表单实际标签左边缘在六行中均由123–285px纠正为12px，字段仍180px；
已查看英文820实尺寸后继图确认左侧流式位置。构建、测试、渲染执行仍由唯一测试代理负责。

38个负责范围内源码文件与后继冻结清单一致，清单总摘要、清单文件摘要和两个修改文件完整摘要见英文后继段。
`git diff --check`通过，该作用域/动作后继节点已完成；随后主窗口文字刷新发现按上方新待办继续，验收边界不变。

当前主窗口语言后继收口：仅在 `Ui::MainWindow::retranslateUi` 内补8个设置调用，复用初始化时已有的
ModeCard/QObject上下文与源串，覆盖双模式标题、导出配置提示/可访问名、主机名提示/可访问名、端口可访问名、
重启提示。未新增辅助函数、头文件、绘制逻辑、源串、协议、默认策略或状态修改；启动/停止仍由原有状态逻辑管理。

真实首选项弹层取消/保存先得到干净6行RED；首次观察复跑仅清理DeferredDelete而未等待正常LanguageChange
队列，留下专项失败及中断的完整测试退出130，均保留且不算通过。独立审核认可在同样严格文字/设置断言前
正常处理事件循环，未强制调用产品retranslate，也未再改8设置调用。测试夹具使用已有存储规范值，避免3处旧默认值
表示规范化的干扰，仍保留全部有效设置严格快照；未修改保存或默认策略。

已独立读取最终原始结果：专项8行通过、0失败、7.498秒，覆盖中英文三宽度的真实弹层、取消恢复、保存重开、
全部8文字/可访问值，以及模式切换信号为0、模式/地址/端口/非语言设置不变。真实项目入口再运行退出0，完成配置
与翻译资源部署，已有编译产物复用。完整安全CTest退出0，76目标通过、0失败、135.42秒；原2项交互目标继续
Disabled/Not Run。MainWindowFlow共44行通过、0失败，程序耗时25.50秒。

已查看中文600和英文600保存后的主窗口实尺寸图，双模式标题与副标题语言一致；提示/可访问文字由执行检查另行
逐项断言，未用图片推断。39个负责范围源码与最终清单一致，源码/清单完整摘要见英文当前后继段，
`git diff --check`通过。本代理待办再次全部完成，源码冻结交回PM，整体原生、平台、硬件、权限与发布边界保持原记录。
