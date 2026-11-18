# Independent UI completion review / 界面完整性独立审核

## English

### VISUAL-R4 review tasks

- [x] Trace production and test style initialization.
- [x] Compare reference tab/card design with actual checks.
- [x] Report causes and evidence limits without changing product.
- [x] Independently review R4 group2 actual checks before paired source work.
- [ ] In progress: review group2 RED and paired correction; other-page checks remain separate.

### Main language native follow-up tasks

- [x] Review actual main-window language regression.
- [x] Review the narrow retranslation correction.
- [x] Review final successor raw evidence.

### UI-008/014 native follow-up tasks

- [x] Review the actual same-dialog scope-language regression.
- [x] Review the narrow FeatureHandler correction and additive Action layout check.
- [x] Inspect successor raw verification and update this report.

### Resumed review tasks

- [x] Review frozen host/update patch and locale successor.
- [x] Review the new actual flow checks and report findings.
- [x] Review corrective source and verify available evidence identities.

### Live review tasks

- [x] Read governing skill, project rules, specification and execution package.
- [x] Assess existing actual checks against the contracts and report pre-code adequacy.
- [x] Review changed product sources and scoped privacy risks.
- [x] Review available supplements and first product patch, verify identities and finish interim report.
- [x] Cancelled: final follow-on source/evidence review is deferred to a separate PM follow-up because those changes are not ready.

### Resumed review, 2026-11-17

Host/update frozen source hashes match the 11 identities in `docs/reports/20261117-ui-completion-host.md`. The subsequently reviewed mechanical unused-capture repair to UpdateDialog.cpp has SHA-256 `7c4079981dbb6f10a8e89bf44d6bd7ffc7b563101542fd8ea0b15c0f882441d7`. Static review of name draft handling, cancellation and retry ownership, rate/stall timer and button sizing found no confirmed actionable issue. No runtime or visual success is inferred. Locale successor `R3OptionContractTests.cpp` SHA-256 `9eea164789db15981810d64a958fe33c9949be65fef19452a1c19bf7e177bed8` accepted: missing Traditional catalogue asserted and existing system-language persistence checks retained; prior RED remains historical. Timeout evidence capture `R3StartupContractTests.cpp` SHA-256 `992dfa34ae2dc3552e1a76f0a6f91686efa2518b49e92cc2344dc223fc3fee8d` accepted as diagnostic only; failed timeout remains failed. Static screenshot settle helper SHA-256 `379e26360d3462eadbaaceb42793e8d2ba2ba29b90575bd098a11753274f6ea5` and four associated capture revisions accepted for endpoint screenshots; intermediate-frame contract remains separate.

### Accepted flow-check bundle

The following SHA-256 identities were independently verified before paired corrections: `MainWindowFlowTests.cpp` `1563163b59cec3cb64fc71b5f1c66929b988cff138d43a76e5cc53e9e3707a27`; GUI CMake `b92e0054dd8e4e644f853361e90c0aa799cc045ae4dff7509dc765493d1df74e`; `ScreenSetupModelTests.cpp` `5144003cccb1f105b1c741031b4227a130888114d53692bc587d2d4f60dec61a` and header `6e261f725838e6a4d56274be6586c5e617604a9fcd36154984869e5d15ab2f2d`; `R3OptionContractTests.cpp` `3fd0d51112d41485757e4407e69880ed8c906cfa6fa11bf34b2d35e26b38da25`; `R2UiPreview.cpp` `3f3edb3b6ce455d07c3f7cc754f6f587ec309df544231600614cbe710b5eb2e0`; manual successor `docs/reports/20261117-ui-completion-manual-checks-r2.md` `607726587736c49ad777aa40348e6cc83349c2e2786dc57ee24425871babf946`. Accepted for bounded regression, not proof of execution or full native acceptance. The later per-test isolated settings/state revision of MainWindowFlowTests has SHA-256 `a74dcb811c1b287fb5044818dc3f08b6e412ab9c6af70ac8bda2d53d0454e791` and was independently accepted; its semantic assertions are unchanged.

### Startup matcher successor

Accepted test-only bootstrap SHA-256 `a79d66a161fbf9cdaa3a4acf01ddbee60c1e97f467bc0c2f6aff3835dfc341ad`: for the synthetic legacy fixture it matches the exact translated migration body, temporary system backup path and contact, information icon and sole OK, with macOS-only empty-title allowance. Other dialogs are not accepted; prior timeout remains historical. This is not evidence of native backend migration.

### Compile correction review

Reviewed removal of `const` from private `SettingsDialog::initConnections` declaration and definition, needed for child construction; no cast or logic change. Verified SHA-256: cpp `f72e635fc1e5488498efc5bd165fbceb4b1002f0a08fa7298c554683bfab2d88`; header `f665dd6493c15b5b85d561dcaa1223845b033096c65a889e24b3c80d40d57785`. Compile outcome belongs to tester evidence.

### R4 probe packaging boundary, static

The test CMake defines R4VisualProbe and R2UiPreview without install rules. `src/apps/synergy-gui/CMakeLists.txt:75` installs the explicitly named production target; macOS deploy and CPack signing scripts operate on the specifically named staged app, and recursive globs inspected are inside that app, not `build/bin/*.app`. The scoped script/workflow search found no test-bundle collection glob. This is static evidence only; no install, package or extraction was executed.

### VISUAL-R4 presentation worksheet

Accepted `docs/reports/20261117-ui-visual-r4-presentation-checks.md` SHA-256 `548f75920df23b97c4129c0c66b8dc57f806825f0859993fd77cf82d4bb3f442`: P01–P03 specify actual readable keyboard/mouse/action semantics, language wording and immutable core/serialization/draft contracts. After evidence is pending. Accepted exact actual successors: R4VisualContractTests `8c54fb23a13d2c3af11fab3cf1cc9581ee62dff090cb08cefbf14e925df9da7f`, MainWindowFlowTests `894f2c895e0e6f1742246b2dce2fe504857a4673e6f1f0688a0e2e9343e7624f`, R3OptionContractTests `23b96089735cad5da3f16adba68c9da875cb13c7794e8f87c0c06b42a269ee93`. Separate Record is now distinguished from KeySequenceWidget; Flow/Scope only change UI wording expectations while preserving model/transaction/protocol tests. P02 all-eight readable summaries remain manual follow-on evidence, not established by two Flow examples.

### VISUAL-R4 remaining source review

P presentation fixture successor `7137151df39269279503a7fb9d6b533d1599873eb9638a028de82aa7072671c4` was accepted after correcting the loaded-object baseline and const overload; the intervening `e7f61738...` build failed and is retained. Clean `presentation-red-r2` built successfully and reports 30 failing data rows, two lifecycle passes, zero skips, 9.760 seconds. The 24 key/mouse rows reached their aggregate failures after Esc, model, saved map and Cancel assertions; failures concern reference field height, protocol captions and the absent independent Record control. Six General rows fail the specified labels. The 1091-file maps match during the test, content manifest `509233993d91857c219f84525d5dfbfa1d913bd56a7126b1ff030d3cb763fbce`. Flow/Scope and full P02 manual cases remain separate.

The later `remaining-green/raw.txt` reports 90 passing data rows (92 including lifecycle), zero failures/skips, 46.692 seconds and exit 0. The 1091-file maps before and after this test agree, with content manifest `79b2ba8616cd401b6a8f60f3161466514cbb93046b8b38c63ac5884b00956863`. Eight reviewed product-source identities below match; the Chinese TS at test time was `3e2656ac56e01ba95f743d0bdda44fec1da4c6ab713ba16aba625fbe0c0be4f8`, differing from the previously reviewed version, so build-period translation changes require separate review. This does not establish manual or native acceptance.

Reviewed the frozen nine-file composition patch. Update notes stay read-only plain text and downloader cancellation/unknown-size behavior remains; fingerprint source is plain text and reject default preserved; subtle text is HTML-escaped before wrapping; list styles do not change models; Server actions reuse the same button box. No confirmed new source defect; fresh execution, layout and transaction evidence remain required. Identities:

```text
9cf0bd431e611556b23e2a1571f8ef3969a8faf06b00a494edb879c4f9728046  src/lib/gui/AppStyle.cpp
a6fd626e1e9793b8507aa8ffa3bb54ca47bd025865dbf4a2fe788bfd728e45ac  src/lib/gui/dialogs/CompactLayout.h
437cf3d185613f2a843bf131475551093507a31694f60df66b2fd13f3e347fed  src/lib/gui/dialogs/SettingsDialogDesign.cpp
4156e297d617e90bf795fb2ec053590cdfd9733c9d6c8f21cbbd9a236e74c289  src/lib/gui/dialogs/ServerConfigDialogDesign.cpp
0f89f912df9faed4b3626a7f54faf14641130e4dd4616f3489aeee92094d1944  src/lib/gui/dialogs/AboutDialog.cpp
fae2d713c083e5b5dbf6de7e4ff4aaaa6a1655b59a79f94af6e0985c1547eb4c  src/lib/gui/dialogs/FingerprintDialog.cpp
0eb7896ecf414ed6e36751ddfffdfc761dc794e296eacea77cc5f80c01bf529b  src/lib/gui/widgets/FingerprintPreview.cpp
95b626012e3a802cb77c396a973021413a6c56a9bb10cc6f52ecefba8572033a  src/lib/gui/update/UpdateDialog.cpp
66cca9a58d42953693dc80c32f9ecb69cd0c763981314e64ddd56d69208389d8  translations/synergy_zh_CN.ts
```

### VISUAL-R4 manual and remaining checks

Accepted bounded list/subtle manual worksheet `docs/reports/20261117-ui-visual-r4-manual-checks.md` SHA-256 `994d1657fc840ca1756d52e164c749c827417f6c599c82325915c893ab8f7224`; reference, bound before evidence, partial older-image identity, after-state and native limitations are explicit. Accepted remaining executable successor `R4VisualContractTests.cpp` SHA-256 `89162eb91921fb46f3d090b688b955f5a464b126ceffc2e0cdb3d39886dcae90` after it added exact 32-byte comparisons for both fixture fingerprints and actual stalled waiting/no finite ETA plus cancellation; About verifies real semantic button routes intercepted in-process. Message states are synthetic components, not proof of OS permission or destructive operation paths. Execution pending.

### VISUAL-R4 common successor result

Independently read `build/ui-visual-r4-evidence/common-green/raw.txt`, build/test exits and summary: all 90 data rows passed, QtTest 92 including lifecycle, zero failures/skips, 42.364 seconds; status/pill/gap samples and 26/30/32 outer heights match the bounded reference checks. Summary reports zero source changes during test under manifest `398b2793e8be4e620321133dc01b5eadedf45236ede314abf340478c8345a949`. Remaining pages, full regression and native acceptance are separate.

### VISUAL-R4 common source review

Reviewed AppStyle `ad69f4fcb75b552a3d5856ce05c17579a2159a077244331b0b4362954bb08a8d`, StatusBar `e744cf2d5f31dc7a7e640a7df7dbabe92c8d10243749bc7d3d26d9a7d6567a83`, LogPanel `f3584a46c8f3791486493a8f7aa20c161ac6b62d323402d87ee2b93daf5a1e45`, PageHost `19dda60f7c98da9159b95b28cf4a35bf9a00b81ed846591d3bfa4adcd772bf3e`. Page inset uses assignment, not accumulated margins; layout measurement uses actual content. No confirmed business/protocol defect in static pass. Cross-platform Qt box behavior, natural height and regression remain runtime evidence requirements.

### VISUAL-R4 common actual check review

An initial common acceptance was withdrawn after detecting that ordinary control font thresholds were incorrectly applied to log chips/search and that status color was asserted without dark-theme qualification. Accepted corrected `R4VisualContractTests.cpp` SHA-256 `a632af13d0a2e8ebbbaf5bac4ff323e92124e92f06ee4c3546651524784954a8` for common group only: distinct reference variant font thresholds and dark-only golden colors; light colors are observed without invented golden. Remaining-group About action oracle remains unaccepted pending semantic correction. Corrected common actual RED independently inspected at `build/ui-visual-r4-evidence/common-red-r2`: 90 data rows failed, 2 lifecycle passes, 0 skips, 42.404 seconds; summary reports selected pill #24372f rather than #31443a, zero tab/card gap, status #1c1c1e rather than #232326 and outer-height differences. Badge false positives and mode activation errors were zero. This is bounded common evidence, not full-page acceptance.

### VISUAL-R4 group2 successor result

Independently read `build/ui-visual-r4-evidence/group2-green/raw.txt`, exit files and summary: all 132 data rows passed, QtTest 134 including setup/cleanup, zero failures/skips, 69.951s; build and CTest exits 0. Client six-row negative remains passing. The pre-build/post-test source manifest is not zero drift: six translation TS files changed, classified in the summary as lupdate during build before test launch. Final frozen source identity must be re-established; no claim is made for tabs or other pages.

### VISUAL-R4 group2 source review

Reviewed page action reparenting, combined-card ownership, recipient section and stable item data; no confirmed new semantic defect. Actual current-tab sizing, visibility and transactions remain subject to fresh tests. Identities:

```text
c1514e8fe1d8a975b1096fcd3ad3ad36c38b00445fa91a00212c725a35a57875  src/lib/gui/dialogs/CompactLayout.h
79d403d075bb2b4a5c1c89ba506493f95ed75ab97f14b033f6109a2775e3a2e3  src/lib/gui/dialogs/SettingsDialogDesign.cpp
8d15329b23b53f2b56db1c3fc3f1d2e179180c1c902be0b459744cc64cca1e03  src/lib/gui/dialogs/ScreenSettingsDialogDesign.cpp
666ee7174035fb004eadeb3611261bca4e1a92123847c2f61ababe161471dd68  src/lib/gui/dialogs/HotkeyDialogDesign.cpp
a94ce0c18b4fd5cae4a163ca356d383d14967ce50611cce1ed42c27341cc2ef4  src/lib/gui/dialogs/ActionDialogDesign.cpp
9166161ee2d93fee0a703e9b44cef3edd75f75772148af623fb139634de4b86b  src/lib/gui/dialogs/ActionDialogDesign.h
```

### VISUAL-R4 group2 actual check review

Initially rejected the actual artifact because missing action boxes silently passed and the Client outside-card negative was absent. Accepted successor `R4VisualContractTests.cpp` SHA-256 `699c4eb8961709a2c4b7d6c72dd046b92bac0e502afe25fa853c173cf509f066` for group2 only after those checks were added. It covers eight Action types, real popup changes, relevant fields, key recipients and Cancel preservation. Pixel/tab and other-page checks skipped by group2 remain unreviewed and require independent acceptance. Independent raw inspection of `build/ui-visual-r4-evidence/group2-red/raw.txt` and summary: 132 data rows; Client 6 rows passed and 126 rows failed (QtTest 8 passed including setup/cleanup, 126 failed, 0 skipped, 70.147s). Failures concern reference card/heading/action placement and Action form/recipients placement. This is bounded group2 RED, not a full-page conclusion.

### VISUAL-R4 pre-code plan review

Accepted BP-UI-20261117-VISUAL-R4 SHA-256 `19c647caafac45a711f47584dd297bc282095b0031ae63a5c8803d6b54f8e03b`; master spec SHA `46cbb513917138c8f65bf2ba89e9fa779441cc5fb9c63a01869ad5ef01597aa0` verified. The two-layer production-equivalent appearance and reference-derived actual-page checks address the prior generic-widget blind spot. Actual check review remains mandatory before paired source edits; thresholds, page/state coverage, no artificial palette, preserved transactions and source/binary/launch identity must be inspectable. This acceptance does not assert execution.

### VISUAL-R4 source/reference distinctions

PM determined PID 19168 started before the disk executable was rebuilt; the historical rejection screenshot cannot establish current-source pixels. The current-source comparison still identifies concrete reference differences requiring visual confirmation: HTML Preferences cards start directly with fields/checks, while `SettingsDialogDesign.cpp` supplies visible card headings; HTML Action uses one card with its actions, whereas `ActionDialogDesign.cpp` uses separate action/recipient cards and places the button box in its outer layout; HTML Computer pages put save actions in each card whereas `ScreenSettingsDialogDesign.cpp:344` uses outer layout. Additional source mapping: reference Server Sharing/Config actions are inside cards but ServerConfigDialogDesign puts a common buttonBox outside the tab widget; reference Hotkey is one card while current source makes two cards and an outer button box; reference available update puts notes and actions in one card while UpdateDialog separates them; reference About puts project actions in its first card while source creates a separate project card. Client reference intentionally places actions outside its cards, matching the broad placement rather than requiring blanket relocation. Main/message states need fresh state-by-state observation. These are source/reference observations, not claims about the old running pixels. Assess against UI-011 functionality and other binding requirements and verify fresh production-equivalent native output before edits.

### VISUAL-R4 read-only review

The production main explicitly applies `appStyleSheet()` at `src/apps/synergy-gui/synergy-gui.cpp:97`; the preview and visual/layout tests also apply it. The later Fusion style selection excludes macOS. No later application-wide stylesheet overwrite was found in the scoped GUI search. Therefore missing production stylesheet is not established; the observed native difference is real user/PM evidence but its precise runtime cause (native tab painting, stylesheet application, or binary identity) remains unproven. PID 19168 is the requested bundle executable; disk SHA-256 `a1c613fbb100b6a65b96cb67f3aa8313da3c8e4dc2cb37f99eec33d3fd0f7635` identifies the file at inspection time, not the loaded image or source.

Reference HTML `.tabs` is transparent independent buttons with selected rounded pill and 8px gap to pane; Preferences actions belong inside each card. Existing `tabsHaveTransparentBlankAreaAndNoNativeBaseline` constructs a generic QTabWidget under a special local workspace stylesheet and samples only the area after the final tab. It does not examine underlying pixels under/between each actual Preferences tab, selected outline, whole strip, or real on-screen native painting; `everyEditorKeepsCardsAndActionTiers` checks object hierarchy/property values and cannot establish visual similarity. Prior images are offscreen Qt render captures with independently installed app QSS and some synthetic palettes; they are useful geometry evidence, not a faithful production-on-screen acceptance oracle.

Recommend an independent reference-derived page/tab/state inventory and actual checks before paired edits: cover both languages and 540/600/820; measure selected-pill shape/color, unselected and between-tab/under-tab backgrounds and baseline across the full actual tab bar; verify card/action containment, order, padding, and page-specific structural relationships against HTML, not the presence of implementation properties. Capture full-size output with production entry/QSS/style/palette and compare in a controlled isolated-settings harness plus safe actual native observation; record device pixel ratio, theme and executable/source identities. Review each page/tab/state alongside reference and preserve prior RED; never substitute fixture captures for native observation or functional/security regressions. Do not claim every page differs without comparing it.

### Main-window language follow-up

Accepted pre-code `MainWindowFlowTests.cpp` SHA-256 `3085187d6b3e47bb7dcc1897680660636c3ec2ff17ba3b682fd4c3f833901bcc`. The same actual window exercises Preferences language popup Cancel/Save/reopen, literal visible/accessibility values, and unchanged mode/address/port and effective settings across six combinations. Clean RED retained at `build/ui-completion-evidence/final-main-language/red-r2.log`: six stale-text rows failed, no unexpected settings mismatch. Test fixture successor SHA-256 `3cf446e8ec6b66cc6f16d933ab42886d180a1ea0c49f5baa3047fbb0fef1e4d4` accepted; all-key comparison and real activation remain. Reviewed `MainWindowDesign.cpp` SHA-256 `b9e4aa21648c85bc09e09e7d5033996c61a2dbb6f50445f39590e20e9b2c352e`: eight existing translation setters added to retranslation, no state mutation. An initial post-fix observation failed when only deferred deletion was flushed; the subsequent prematurely dispatched full run was stopped (exit 130), not counted as a pass. Accepted `MainWindowFlowTests.cpp` SHA-256 `fe1352b8ac44c1220e1895fd4a8de25abc381cfe5efc6145b596a8c400ede5a7`: existing normal-event settling precedes unchanged strict text/config assertions. Independently reviewed `final-main-language/green-r2.log` and exit 0: six data rows passed, QtTest 8 including setup/cleanup, 0 failed/skipped. Entrypoint build exited 0. Independent inspection of `full-safe-r2.log`, `.exit` and JUnit confirms 76 executed/passed, two Disabled/Not Run, 135.42 seconds; 78 registered and zero failures. The stopped exit-130 run remains separate. `summary-r2.json` reports `r3_pngs=0`; image accounting is not independently reconciled here. Native same-dialog retest remains PM-owned.

### Native scope-language follow-up

The PM observed that injected scope label, choices and tooltip remain English during an in-place switch to Chinese. Accepted the actual regression `R3OptionContractTests.cpp` SHA-256 `717a176f05100bd5b1ea64e2c9b280c8cad668e191c417418527e28381b5afa1`: real language-popup transitions, literal translations, typed scope draft, dirty state, Cancel/reopen and no promotion in six combinations. FeatureHandler.cpp SHA-256 `23c47f64318a18bb5491409022535c70cf4cc4128fa28b53348b3ab1e1332819` reviewed: dialog-owned connection updates labels/items/tooltip, preserves typed selection with signal blocker, and retains accepted-only scope switch. Action geometry actual check `R3VisualContractTests.cpp` SHA-256 `acd56e00fac2b7c01a7a4c7de416511717a2d67c6bc723474008ac6ff933f333` also accepted before paired edit; it measures real geometry across six combinations. ActionDialogDesign.cpp SHA-256 `d586d90ea86871369fec6ca173c95055c69c8fd497a80e8bf9e9710532ef9b98` reviewed: form alignment changed while option/transaction handling remains intact. Independently read `final-scope-successor/entrypoint-build.exit` and `full-safe.exit`, both 0; verbose log and JUnit show 76 executed/passed, 2 Disabled/Not Run, 134.45 seconds. Six scope-language and six Action geometry rows each passed. Final native same-dialog observation and source manifest comparison remain PM-owned. Earlier 76-pass evidence applies to its earlier source identity.

### Final independent local evidence review

Accepted the bounded local source/SecOps/test evidence after independent raw inspection. `final-frozen-entrypoint-build.exit` is 0 and its log ends with build complete; `final-safe.exit` is 0 and verbose CTest records 76 executed/passed, two Disabled/Not Run (`OSXClipboardTests`, `OSXKeyStateTests`), 163.55 seconds. JUnit reports 78 registered, zero failures and two disabled. Representative full-size final images independently inspected: Chinese 540 fingerprint and English 600 log. This is a sample, not an independent inspection of the full matrix. Final native menus, installed/GitHub package, physical sharing/permissions, Windows/Linux execution and release acceptance remain separate.

### Final bounded source review

Reviewed final fingerprint/update source and verified SHA-256: FingerprintDialog.cpp `f896cf5b6ddb7051b66d306c8765b0128de837b2dfd6be30bb93dea6d8fed555`; FingerprintPreview.cpp `cb20d3160d89d7227d46021577142835df867ed6eaa5c862025a421246c9e9a5`; UpdateDialog.cpp `e7d1b826c6f10725784c05e5c768ec7e84ca48ede5b8e657cefd0803bd653410`; zh_CN `9cd1f432ea3c69c68c19471ffa8eb26919835a260f738e6b91628fa0419949da`. Local SHA bytes are complete and grouped by 16, empty footer hidden, local OK primary, update actions right-aligned; global formatter and remote trust decisions unchanged. No additional confirmed source/security defect. Repeated pattern/context PII scan across changed/new source and reports found only synthetic documentation/loopback fixtures; no suspected PII leak in this bounded scope.

Independently read `build/ui-completion-evidence/follow-on-fixed-MainWindowFlowTests.txt`: 38 passed, zero failures/skips, including six language/width rows each for nested action and distinct-certificate cases. These apply to that captured source state; final full suite and native observations remain pending.

### Target-level corrective evidence

Independently read `build/ui-completion-evidence/follow-on-fixed.log` and its `.exit`: R3VisualContractTests, ScreenSetupModelTests and MainWindowFlowTests passed (3/3, exit 0). This file contains CTest target summaries only; individual Flow row output requested. R3Option popup diagnostic records two failures despite one activation and changed index, because the view remained visible immediately. Test successor SHA-256 `a901eb137751b3b0894a336abab4de897bfa6d110f53e63dcd6cb4cfb630c664` is accepted: exactly-one activation and selected row are required immediately, then the real popup and its container must close within one second; no forced hide. The successor raw log `build/ui-completion-evidence/popup-closure-successor.log` shows both mouse and keyboard rows passed; QtTest totals 4 passed including setup/cleanup, zero failures.

### Local fingerprint visual check

Accepted additive `R3VisualContractTests.cpp` SHA-256 `fd4b4fad9b6b7c9e27456127433428998f3dbf3abd58c02b69da4f28c39fe09f` before paired product work: complete 32-byte value, two fitted lines, alignment and local controls across six language/width combinations. Existing compare/trust checks remain independent. Execution pending.

### Corrective source review

Reviewed parent action refresh, explicit saved-certificate label/path and missing-target save guard. PE-UI-001/002 are source-corrected pending fresh regression and visual evidence; trust dialog semantics remain unchanged.

```text
f8243a856c9cfa5f6b055d89ef0034d1a7e7e36fd757f6326160567d3e7927df  src/lib/gui/dialogs/ServerConfigDialog.cpp
8363da3599a32c8e9ca1df3d231c58dbf05affe9e7c36b3d899b4f0eaae2c83f  src/lib/gui/dialogs/SettingsDialog.cpp
a654993ac78e92a31732f540856d669792f5bed191382eeb49c229048ea02a72  src/lib/gui/dialogs/SettingsDialogDesign.cpp
955d77b6cb10e75bbce64b8159b608e3a766fc032738eb7c749248c39dc8126d  src/lib/gui/dialogs/FingerprintDialog.cpp
3c71ccb9795fdb787d6d50abc7fc924c2341bf80edbb5a4eda086d4a85f8036a  src/lib/gui/dialogs/FingerprintDialog.h
b60e4281fb7e4c94eecfa69d6b5d5c7b6bc84f53226d95b1d4969ba339a06882  src/lib/gui/dialogs/ActionDialog.cpp
```

### Open finding PE-UI-001 (UI-003/011)

`src/lib/gui/dialogs/ServerConfigDialog.cpp` `editHotkey()` replaces the accepted hotkey but refreshes only the hotkey list, leaving the adjacent action list stale. If the nested editor removes the first of two actions, the old first row still appears while the current row-zero data is the second action; Edit/Remove then targets the wrong action. Refresh the action list and its selection/button state after nested acceptance, and verify with multiple distinct actions, nested removal/addition and parent Edit/Remove. Status: source correction reviewed and corresponding six synthetic regression rows passed; broader native visual acceptance remains separate.

### Open finding PE-UI-002 (UI-005/011)

`SettingsDialog.cpp:262` reads the saved certificate for View fingerprint while the adjacent editable path may contain a different draft; the generic label and local fingerprint page do not identify the saved source. This can make the displayed fingerprint appear to belong to a different certificate. Explicitly identify saved-certificate semantics or use the draft with clear semantics; verify with two distinct synthetic certificates and Cancel/no-write. No daily certificate was read. Status: source correction reviewed and corresponding six synthetic regression rows passed; broader native visual acceptance remains separate.

### Pre-code adequacy

The existing checks cover bounded option, serialization, isolation, motion, layout and action-model contracts. They do not alone establish overall UI-001–016/019 acceptance. LayoutAcceptanceTests separately renders ten page kinds, all tabs and eight action selections across both languages and three widths, and checks geometry. R3VisualContractTests performs the specific pixel checks on the log page; neither automated geometry nor saved screenshots alone establish a human comparison of every page/state against the reference. The tray check exercises a constructed menu model, not native macOS display or application termination. Per-page reference review and actual native observations remain necessary. Check design is not test execution evidence.

### Source and privacy review

Reviewed changed startup, settings/migration, menu, motion, serialization, configuration, update, layout and platform implementation paths. No confirmed actionable security/privacy defect identified in this static pass; final changed source and runtime evidence remain pending. PII scope: changed/new source and review reports; searched for email, IPv4 and local user paths, then inspected matches in context. Matches were explicitly synthetic documentation-address or loopback test fixtures; author attribution was not treated as a leak. This bounded pattern-based review cannot establish absence of all personal data or inspect unrelated settings, build outputs or secrets. No test or build was executed by this reviewer.

### Reviewed check identities (SHA-256) / 已审核检查身份

```text
bfe92507e96871c4a81355e5c889bc85bcd19fe32f1efd7ec7297c619cf50926  src/unittests/gui/R3VisualContractTests.cpp
9bc9225cf428d109eec246f50905eb32dc425a8ad0d5ecfcf884092edcfc9af2  src/unittests/gui/R3TrayQuitTests.cpp
7fbb87658dada771b968d24a831741330892472bb86df83db826d864c8f1396d  src/unittests/gui/R3OptionContractTests.cpp
9799a414ac92426129ffd080f27e97aa8aee11d1d64bb14c6259941f9ebdb623  src/unittests/gui/R3SerializationTests.cpp
5e43c474ed19796b493f930abf9b52002fbfe32e8fd0337f58ae3a7c11a3b229  src/unittests/gui/R3RuntimeContextTests.cpp
7e5520be047f8e394579af9ba38357023ccdc6d503d6ad0b9d25803e37fa0e28  src/unittests/gui/R3StartupContractTests.cpp
a60e9cf783105ad70d596f7bd9c214238c9787864a4d2bc7c1a4353c0e98d588  src/unittests/gui/R3MotionContractTests.cpp
a6abb065180659942a378af9a74acc96e58ad2cfa00dc566c578bcac646b07aa  src/unittests/gui/LayoutAcceptanceTests.cpp
71421c0572d34e69f8c7f6f370fea499f466d4fce99df24f1d336ca4301c2e36  src/unittests/gui/NativeDesignTests.cpp
7924d0a03c099f05bd4021497af290d7dc4ecf5197e3333397d74cb9cd966187  src/unittests/gui/EditorTransactionTests.cpp
```

### COMPLETE-R1 supplemental review

Accepted for bounded isolated diagnostic use, subject to compilation and receipt verification. Four test mains configure the temporary runtime before QApplication; the probe receives a test-only core copy, while its existing bootstrap controls autostart. The preview blocks direct core/update commands and configures isolated fixture stores; observation is limited to intended pages and menus. A process monitor cannot prevent an earlier macOS permission request. These diagnostic checks do not prove production startup or native release acceptance. The following identities supersede the corresponding earlier identities:

```text
b17d1b748e151d61866048470d473382e415d77cbbbd40b22f687fbd2ddd388f  src/unittests/gui/R2UiPreview.cpp
937257067029cf60b97ea4260158079fb2129cd5977c7731ed4e55d56a9f51fc  src/unittests/gui/NativeDesignTests.cpp
27053a7dabff3ebde7821238dd8486ae84de8dff158386e9f677da2a687338ef  src/unittests/gui/LayoutAcceptanceTests.cpp
5f658e258ec5d64a6c6238fa746b6c9f59a0dddb4815ef3f225344d6c8c16118  src/unittests/gui/DialogRenderTests.cpp
b8e36aeeb73cccdc7e7a56565c0b4846396a00a86982e46b520c94aef5e41a6f  src/unittests/gui/EditorTransactionTests.cpp
70551fed5c87db100a417c097096dff9e854bf5305196ad13a853f2621550a58  src/unittests/gui/CMakeLists.txt
```

### First product patch

Source review found no additional confirmed actionable defect in the reviewed option mapping, readonly state, settings proxy, card ownership and TLS/draft paths. Delivery independently identified the untranslated Follow system label after preview as a pending correction. No execution result is inferred. The later mechanical unused-capture repair was separately reviewed; SettingsDialog.cpp SHA-256 `ff3199cebf39a44756658c6e70f44aa7ca9c450c6f887b14342b60c009c568bf` supersedes the earlier identity below. Identities:

```text
2906f72b6a511f416be76238d97c13c803f391e40cc7ecbcf1172c868d8633fa  src/lib/gui/dialogs/CompactLayout.h
a804898c3ab6d8a76a2e49db4d47df7b652a9ae4dd65953b89208b3de3212a41  src/lib/gui/dialogs/ActionDialog.cpp
b6a321c0793acc18debe6133e4fbbbf851f2388380e772608972e16954bd5262  src/lib/gui/dialogs/ActionDialog.h
d73a5bc9b76266f2614d5fe47b90daf8f68e56601a63b47fc9d7f636fa1d5579  src/lib/gui/dialogs/ActionDialogDesign.cpp
4ca3c3d6d50300d2da6f7ae9aa96803bb924aafc0c5aa2ca75a7898c98c987ed  src/lib/gui/dialogs/ClientConfigDialogDesign.cpp
16d94bf1818be5407c087e24c23e7641bc758b5c5a1ee2fa2cd0fb08c8a1b7e5  src/lib/gui/dialogs/HotkeyDialogDesign.cpp
833523c8167be38172cd45bd5af15fe6a17023b3fead4b53d2a894bc043a5188  src/lib/gui/dialogs/ScreenSettingsDialog.cpp
a8986dbf52d9176983a0927f9c1c4b12546735c97f69c61544b3ea5bc91193c5  src/lib/gui/dialogs/ScreenSettingsDialog.h
b7b3e79113113f3a7a2c645df71cd52faa7bedc1da853ed70d59e0a59a0b0849  src/lib/gui/dialogs/ScreenSettingsDialogDesign.cpp
985fd4adaa5077fdf8ada260bb8613e82b026a48ee851116610be05b220c4379  src/lib/gui/dialogs/ServerConfigDialog.cpp
72ce9e26b81e7cd614b5cb4084027120849adbe488037a7f74e993201fb419e5  src/lib/gui/dialogs/ServerConfigDialogDesign.cpp
b2bb5d165d4ca72a96ae122784d58724ead2039241aeb864d9b733797e6cf49b  src/lib/gui/dialogs/ServerConfigDialogDesign.h
b7780b49070f96393bade0fcb007db667e95f907df903a816098ab949a62c13a  src/lib/gui/dialogs/SettingsDialog.cpp
4f4a3675fa8273e57216157c6f419043b0884bfc9c6010ec9243f14c95e7b281  src/lib/gui/dialogs/SettingsDialogDesign.cpp
e06fd1595e747c19cd90f0946fc54d346c0cf679b6296550caee092f4cf708de  src/lib/gui/dialogs/SettingsDialogDesign.h
f99b7c7c98eeab3a1c8bf1f2d6e58ae79d6f594187f163ad43ad71809b841828  src/lib/gui/AppStyle.cpp
80d477feb9a8ec89d5aa4d642eb976b37b83b6a9e71ee7f90601b3d201203b46  translations/synergy_zh_CN.ts
```

### Manual follow-on check

Accepted M-C01..07 at `docs/reports/20261117-ui-completion-manual-checks.md`, SHA-256 `c4f99e4434d30ff24a74d4590015d4dfd5230c75fd85fd107f64f9a316d0f425`, with all observations pending; M-C05/06 require diagnostic fixtures and raw receipts.

## 中文

### 本轮续审

主窗口与更新补丁 11 个源码摘要均与 `docs/reports/20261117-ui-completion-host.md` 一致；静态审查名称草稿、取消重试生命周期、速率与停滞计时和按钮尺寸未发现已确认可操作问题，不代表运行或视觉通过。语言后继检查摘要、启动超时回执修订及静态截图稳定辅助的具体摘要见英文区；均已按各自限定范围接受。原始失败仍保留，超时仍为失败，动效中间帧检查保持独立。

### 已接受流程检查工件

配对修正前已独立核对七个检查和预览工件的 SHA-256，具体路径与摘要见英文区。接受其用于限定范围的回归检查；不代表已经执行通过，也不替代完整原生验收。

### 启动提示后继检查

接受测试专用引导后继，摘要见英文区：仅合成旧版夹具，严格匹配翻译后的迁移正文、临时系统备份路径、联系地址、信息图标和唯一 OK；仅 macOS 允许标题为空。其他弹窗不予接受；原超时历史保留，不代表原生设置后端迁移通过。

### 编译修正审核

已复核私有 `SettingsDialog::initConnections` 声明和定义移除 `const`，供创建子组件使用；无强制转换及逻辑更改。两个源码摘要见英文区，编译结论以测试方证据为准。

### R4 探针打包边界静态审核

测试 CMake 定义 R4VisualProbe 和 R2UiPreview，但没有其安装规则；GUI 安装明确指定生产目标，macOS 部署及 CPack 签名脚本使用暂存区具名应用，已检查的递归 glob 仅限该应用内部，并未发现从 `build/bin/*.app` 收集测试 bundle 的规则。此为限定源码证据；未执行安装、打包或解包。

### VISUAL-R4 可读呈现检查表

已接受 P01–P03 人工检查表，摘要见英文区：覆盖键鼠与动作的可读语义、双语文案及不可变核心/序列化/草稿契约；修改后证据仍待执行。已独立接受三份实际检查后继（摘要见英文区）：录制按钮与输入字段明确区分；Flow/Scope 只改可见文案期待并保留模型、事务与协议断言。P02 八种可读摘要仍须人工后续证据，不以 Flow 两个示例代替。

### VISUAL-R4 其他页面源码审核

已复核冻结九文件：远程更新说明保持只读纯文本，下载取消与未知总量逻辑保留；指纹来源为纯文本且默认拒绝不变；说明文字先转义再包裹，列表样式未修改模型，服务端复用原按钮盒。未确认新增源码缺陷；仍需新执行、布局和事务证据。具体摘要见英文区。

### VISUAL-R4 人工与其他页面检查

已接受列表/说明文字的限定人工检查表（摘要见英文区）：参考、已绑定的修改前证据、旧图身份限制、修改后待办和原生观察边界清楚。已接受 remaining 可执行后继：双指纹逐字节比较两份夹具，停滞下载检查真实等待、无有限 ETA 和取消；关于页核对真实按钮及进程内截获路由。消息为合成组件状态，不证明系统权限或危险操作真实路线。摘要见英文区，执行待核。

### VISUAL-R4 公共检查后继结果

独立核对 `build/ui-visual-r4-evidence/common-green/raw.txt`、构建/测试退出码及摘要：90 个数据行全部通过，QtTest 92 项含初始化/清理，0 失败/跳过，42.364 秒；状态栏、胶囊、间距和 26/30/32 外高采样符合该限定检查。摘要报告测试时源码清单零变化，身份见英文区。其他页面、完整回归和原生验收仍分开。

### VISUAL-R4 公共源码审核

已审核 AppStyle、StatusBar、LogPanel 和 PageHost 冻结源码（摘要见英文区）：页签间距赋值而非累加，测量读取实际布局；静态未确认业务或协议缺陷。跨平台 Qt 盒模型、自然高度及回归仍须运行证据。

### VISUAL-R4 公共实际检查审核

首版公共检查因日志筛选/搜索误套普通字号阈值、状态栏颜色未限定暗色而撤回接受。修正后仅接受 common 批：分别检查参考控件字号，参考颜色只用于暗色，浅色记录不臆造颜色；摘要见英文区。remaining 的关于页动作 oracle 尚待按语义修正后审核，修正后的 common 原始 RED 已独立核对：90 个数据行失败、初始化/清理 2 项通过、0 跳过，42.404 秒；选中胶囊、页签卡片间距、状态栏颜色及控件外框高度存在差异，徽章误报与模式激活错误均为 0。此为限定公共检查证据，不代表全页验收。

### VISUAL-R4 group2 后继结果

独立读取 `build/ui-visual-r4-evidence/group2-green/raw.txt`、退出码及摘要：132 个数据行均通过，QtTest 共 134 项（含初始化/清理）、0 失败/跳过，69.951 秒；构建和 CTest 退出 0。客户端六行卡外负例仍通过。构建前与测试后源码清单并非零漂移：六个翻译 TS 文件变化，摘要将其归因于测试启动前构建时 lupdate。最终冻结源码身份仍须重新核定；此结果不覆盖整条页签或其他页面。

### VISUAL-R4 group2 源码审核

已静态审核分页操作盒迁移、合并卡片所有权、接收对象 section 和稳定数据；未确认新增语义缺陷。实际活动页高度、可见性和事务仍需新测试，六个源码摘要见英文区。

### VISUAL-R4 group2 实际检查审核

首版因缺少按钮盒时静默通过、客户端卡外负例缺失而未接受。后继 `R4VisualContractTests.cpp` 摘要见英文区，补齐两项后仅接受 group2：覆盖八种动作、真实弹层、相关字段、按键接收范围和取消保留。group2 跳过的像素页签及其他页面尚未接受，须另行独立审核；已独立核对 group2 原始日志：132 个数据行中客户端 6 行通过、126 行失败（QtTest 合计 8 通过含初始化/清理、126 失败、0 跳过，70.147 秒）；失败集中于参考卡片、标题、操作归属和动作字段布局。此结果仅限 group2，不代表全页面结论。

### VISUAL-R4 编码前计划审核

已接受执行包及核对主规格摘要（见英文区）。生产等价外观和参考衍生实际页面检查两层方案可针对既有通用控件盲点；产品修改前仍须独立审核实际检查，并能核实阈值、逐页逐状态覆盖、无人工调色板、事务保留及源码/二进制/启动身份。此接受不代表执行通过。

### VISUAL-R4 当前源码与参考稿的区别

PM 核实 PID 19168 启动早于磁盘程序构建，历史拒绝截图不能证明当前源码的实际像素。当前源码对照仍发现需实测的结构差异：参考稿首选项卡片直接从字段或复选框开始，`SettingsDialogDesign.cpp` 则添加可见卡片标题；参考稿动作页是一张含操作的卡片，`ActionDialogDesign.cpp` 分为动作及接收对象卡片并把按钮盒放在外层；参考稿计算机页操作位于各卡片内，`ScreenSettingsDialogDesign.cpp:344` 放在外层。进一步对照：参考稿服务端共享/配置操作在卡片内，当前按钮盒在页签外；参考稿快捷键为一张卡片，当前拆两张并将按钮放外层；参考稿可用更新将说明与操作放在一张卡片，当前拆分；参考稿关于页项目操作在首卡，源码另建项目卡。客户端参考稿本就把操作放卡片外，不应为统一而一概迁移。主页及消息状态需新版本逐状态观察。上述属于源码与参考对照，不是对旧进程像素的断言；须结合 UI-011 功能约束，并在修改前验证新版本生产等价原生输出。

### VISUAL-R4 只读审查

生产入口 `src/apps/synergy-gui/synergy-gui.cpp:97` 明确安装 `appStyleSheet()`，预览及视觉/布局测试也安装同一函数；macOS 不进入后续 Fusion 设置。限定范围搜索未发现后续应用级样式覆盖。因此不能认定生产没有加载 QSS；用户/PM 的真实原生差异需认真处理，但具体是原生页签绘制、样式应用还是运行二进制身份造成，尚未证明。PID 19168 路径与指定 bundle 一致；盘上可执行文件摘要见英文区，只证明检查时文件身份，不能单独证明已加载映像或源码身份。

参考稿 `.tabs` 是透明背景上的独立按钮，选中项为圆角胶囊，与页面间隔 8px；首选项操作位于各卡片内。现有页签检查构造通用 QTabWidget，并使用特殊局部背景，仅采样最后页签之后的空白，未覆盖实际首选项各页签下方、页签之间、完整横条或原生在屏绘制；编辑器检查只核对层级和属性，不能证明与设计稿视觉一致。既有图片是离屏 Qt 渲染并由测试主动安装样式、部分采用合成调色板，只能作为布局证据，不能代替生产在屏验收。

建议依据参考稿建立逐页/页签/状态清单并先建实际检查：中英文及 540/600/820 宽，检查真实页签整条区域的选中胶囊颜色形状、非选中及间隔/底线像素，逐页核对卡片、操作位置顺序、内边距和结构，而不是只检查实现属性。在隔离设置的生产同入口/样式/调色板环境下获取全尺寸图，并辅以安全原生在屏观察；记录缩放、主题及源码/可执行文件身份。逐页对照参考稿，保留既有失败；不能以夹具图片代替原生观察或功能安全回归，也不能在逐页比较前断言每页均有差异。

### 主页语言后续检查

已在源码修正前接受主页语言回归工件，摘要见英文区。同一真实窗口通过首选项语言弹层执行取消/保存/重开，检查实际显示和辅助信息，以及六组场景的模式、地址、端口和有效设置保持。六行原始失败已保留；规范化夹具和实际弹层检查后继已审核。主页修正只在重新翻译时调用已有八个翻译设置，不改业务状态；源码摘要及失败日志见英文区。修正后的初次观察仅刷新延迟删除而失败；随后误启动的完整测试被停止（退出 130），不计通过。已接受正常事件循环稳定后执行原严格断言的检查后继，摘要见英文区；独立核查后继专项日志：六行通过，QtTest 共 8 项（含初始化/清理），0 失败/跳过，退出码 0；入口构建退出 0。完整安全测试详细日志及 JUnit 为 76 项执行通过、2 项 Disabled/Not Run、135.42 秒，78 项注册、0 失败。此前退出 130 单列。`summary-r2.json` 的 `r3_pngs=0` 未在本审核中独立核对截图统计；原生同对话框复测由 PM 负责。

### 原生作用域语言后续检查

PM 观察到同一首选项对话框切换为中文后，注入的作用域标签、选项和提示仍为英文。已接受实际回归检查，源码摘要见英文区：六组数据使用真实语言弹层，核对翻译、作用域类型与草稿、保存启用状态、取消重开和不提升作用域。FeatureHandler 修正的信号、对象生命周期和作用域语义已审核；动作表单的六组真实几何检查也已在配对修改前接受。源码摘要见英文区。独立复核 `final-scope-successor`：构建和完整安全测试退出码均为 0，76 项执行通过、2 项 Disabled/Not Run，耗时 134.45 秒；作用域语言与动作几何各六行通过。最终原生同对话框观察及源码清单对比由 PM 负责。此前 76 项通过仅对应此前源码身份。

### 最终本机证据独立审核

已独立核查并接受限定的本机源码、SecOps 和测试证据：`final-frozen-entrypoint-build.exit` 为 0，日志显示构建完成；`final-safe.exit` 为 0，CTest 详细日志为 76 项执行通过，`OSXClipboardTests`、`OSXKeyStateTests` 两项单列 Disabled/Not Run，总耗时 163.55 秒。JUnit 为 78 项注册、0 失败、2 禁用。独立查看全尺寸中文 540 指纹与英文 600 日志截图，是抽样而非本人逐张审核完整矩阵。原生菜单、安装/GitHub 包、物理共享/权限、Windows/Linux 实测和发布验收仍须分别处理。

### 最终有界源码审核

已核对最终指纹、更新和中文翻译四个摘要（见英文区）。本机 SHA 字节完整且按 16 字节换行、空页脚隐藏、本机 OK 为主操作、更新按钮右对齐；全局格式化及远端信任决策未变。未发现其他已确认源码或安全缺陷。对变更/新增源码及报告重做有界隐私模式和上下文检查，仅命中合成文档地址与回环夹具，未发现疑似隐私泄漏。

独立读取 `build/ui-completion-evidence/follow-on-fixed-MainWindowFlowTests.txt`：38 通过、0 失败/跳过，其中嵌套动作和双证书各覆盖六组语言/宽度。结果仅对应该记录的源码状态，最终全套和原生观察尚待完成。

### 修正后的目标级证据

独立读取 `build/ui-completion-evidence/follow-on-fixed.log` 与 `.exit`：视觉、屏幕模型、主窗口流程三个 CTest 目标通过（3/3，退出 0）；该日志仅含目标汇总，已请求流程逐行原始输出。原弹层诊断有两个失败：动作已各触发一次且索引已变，但视图未立即隐藏。后继检查保留即时单次激活和索引断言，仅等待真实弹层与容器在一秒内关闭，不强制隐藏；已接受，后继原始日志显示鼠标与键盘两行均通过，QtTest 合计 4 项（含初始化与清理），0 失败。摘要见英文区。

### 本机指纹视觉检查

配对修改前已接受新增本机指纹检查，覆盖完整 32 字节、两行适配、对齐和本机控件的六组语言/宽度；原有比较及信任检查独立保留。执行结果待核，摘要见英文区。

### 修正源码审核

已静态复核父动作列表刷新、已保存证书指纹标识与路径、缺失目标保存保护；两项发现源码已修正，仍待新回归及视觉证据。信任弹窗语义保持不变，六个源码摘要见英文区。

### 未解决问题 PE-UI-001（UI-003/011）

`src/lib/gui/dialogs/ServerConfigDialog.cpp` 的 `editHotkey()` 保存子编辑结果后只刷新快捷键列表，旁边的动作列表仍显示旧项。嵌套编辑删除两个动作中的第一项后，旧第一行仍显示，但实际第零项已变成第二个动作，继续编辑或删除会作用于错误动作。应在子编辑接受后同步刷新动作列表、选中项和按钮状态，并用多个不同动作验证嵌套增删后父页面的编辑/删除。状态：修正源码已审核，对应六组合成回归通过；更广泛的原生视觉验收另行处理。

### 未解决问题 PE-UI-002（UI-005/011）

`SettingsDialog.cpp:262` 查看的是已保存证书，但相邻路径可含不同的未保存草稿；按钮和本机指纹页均未说明指纹来源，可能使用户误以为指纹属于草稿所示证书。应明确显示已保存证书的语义，或明确采用草稿证书；用两个不同的合成证书检查指纹来源及取消不写入。未读取日常证书。状态：修正源码已审核，对应六组合成回归通过；更广泛的原生视觉验收另行处理。

### 编码前检查充分性

现有检查覆盖选项、序列化、隔离启动、动画、布局及动作模型的有界契约，但不足以独立证明 UI-001–016/019 的整体验收。LayoutAcceptanceTests 还覆盖十类页面、全部页签与八种动作在双语和三种宽度下的渲染和几何；R3VisualContractTests 的专项像素检查集中在日志页。自动几何检查与保存截图本身不能证明逐页逐状态的人为设计稿对照。托盘检查只验证构造的菜单模型，不能证明 macOS 原生显示或进程退出。仍需逐页参考图检查及实际原生观察。检查设计本身不代表已执行通过。

### 源码及隐私审查

静态审查了变更涉及的启动、设置与迁移、菜单、动画、序列化、配置、更新、布局和平台实现路径，本轮尚未确认可操作的安全或隐私缺陷；最终源码和运行证据仍待复核。隐私检查范围为变更及新增源码和审核报告，搜索邮箱、IPv4 与本机用户路径并结合上下文核查；命中属于明确的文档地址或回环测试夹具，作者署名不视为泄漏。此有界模式检查不能证明不存在其他个人信息，也未读取无关设置、构建产物或密钥。审核人未执行构建或测试。

### COMPLETE-R1 补充审核

同意用于限定范围的隔离诊断，仍须由测试方编译并核实回执。四个测试入口在 QApplication 前配置临时运行上下文；启动探针仅在测试目标复制 core，自动启动由原有引导控制。预览阻断直接 core/更新命令并使用隔离夹具，仅用于约定的页面和菜单观察。进程监视无法阻止发生在创建进程之前的 macOS 权限请求。诊断结果不证明生产入口或原生发布验收。具体摘要见英文对应区，后继摘要取代之前同名文件的摘要。

### 首批产品补丁和后续检查

源码静态审核未在选项映射、只读状态、设置代理、卡片所有权及 TLS/草稿路径发现其他已确认的可操作缺陷。交付方已发现语言预览后“跟随系统”标签未翻译，待修复；不据此推断任何测试通过。17 个源码身份见英文区；后续移除未使用捕获的机械修正亦已复核，以英文区后继摘要为准。后续人工检查 M-C01..07 已按上文摘要接受，全部观察尚待执行，其中 M-C05/06 须有诊断夹具和原始回执。

### VISUAL-R4 审核任务

- [x] 追踪生产及测试样式初始化。
- [x] 对照参考稿和现有页签、卡片检查。
- [x] 报告原因与证据边界，不修改产品。
- [x] 在配对源码修改前独立审核 R4 group2 实际检查。
- [x] 审核 group2 真实失败、配对修正及限定范围绿色证据。
- [x] 审核人工检查表及逐页检查表的待执行契约。
- [x] 审核当前可用的后续真实失败和执行证据。
- [x] 已取消：尚未产生的后续配对修正及最终验收证据由后续独立复核，不将其视为通过。

### VISUAL-R4 后继独立复核

- [x] 审核并接受协议控件变更的实际检查工件；产品换型在实际 RED 前保持不动。
- [x] 审核协议旧控件 RED、冻结展示层及后继源码；拖放原生结果由主代理独立观察。
- [x] 核对 P02 276 条 typed 语义/两层父行记录，逐图审核 F15–19 双语三宽及受影响动作后继；合成提示与真实路线缺口单列于账本。
- [x] 核对后继执行证据及限定范围隐私结果；真实系统提示和跨平台/硬件验收仍未证明。

2026-11-18 新独立 lineage `build/ui-current-evidence-20261118/` 的五组 raw 与 1095 文件测试前后映射已核，内容摘要 `a5dd26f755b7bb872e1e61187dc67e43e39ecb2a14e465b24b98d6d9e3817f67`。Dialog 6 数据行通过；R3 指示框 6 行失败（widget/card x10，indicator 局部 x0，布局内边距12）；打开后语言切换 8 行失败，稳定构造对照无错；计算机页 18 行通过、6 个修饰键页右缘失败；共享字段 6 行失败，五个字段起点偏离142。各组零跳过；均为对应限定范围的证据，待配对修正及重验。旧证据目录目前不可用，先前图片实际查看与当时哈希见独立图片账本，不能重新核实原图字节。

The fresh lineage above has matching before/after source maps. Dialog has six passing data rows; the indicator, live-language, computer-action alignment and sharing-field checks have meaningful scoped RED results as detailed in the Chinese paragraph. Corrections and new execution remain pending. Earlier image bytes are currently unavailable; the separate ledger retains only observations and hashes recorded when actually viewed.

七项配对产品 patch SHA `fef6560d34203ac3f5052617da056579739eabaf264b183049d70ed1123f347e` 及 27 文件清单 SHA `f76e15d8605ae4fd281e73aa16dc42c0545a31998963af3c50075379ae0ee621` 已静态审核，列出文件和三个核心身份匹配。FP 在基类事件后依据真实 checked 状态恢复按钮和拒绝默认，但子 ButtonBox 后续语言事件顺序须实测；共享字段多行完整可读性须实测；两个主题 caret 资源存在。此接受仅限源码，后继 GREEN、真实 popup 和原生验收待执行。

The seven-item paired patch and 27-file freeze above were reviewed with matching file identities. Fingerprint child-event ordering, wrapped sharing labels, actual themed caret and popup remain execution questions; source acceptance is not a GREEN or native acceptance.

配对后 focused 原始执行：构建 exit 0，1095 文件测试前后相同，摘要 `06bb057c0e79bbb0bcf49899fdd4a8a944400ab96a9d95254ea825a0769b6c4a`。Dialog、R3、Computer 分别 6、6、24 数据行通过；Language 4 行通过、4 个指纹行仍在切语言后按钮文字失败；Sharing 6 行均因 clipboard 左缘 144 而目标142失败。各组 0 跳过。原 RED/首轮失败保留，剩余两项不能称 GREEN。

The paired focused execution passed Dialog, R3 and all 24 computer rows, but four fingerprint language rows and all six sharing rows still failed. Source maps match during test; the remaining failures require correction and re-execution.

后继两组 focused 原始结果：语言8数据行、共享6数据行通过，0失败/跳过；测试期间1095文件映射一致，摘要 `88e392f9a555991ac9aa46bc20d20c95589d090d65a22369ca63a587bde990ac`。P02 276结构化回执全部保留，但人工图片发现未知动作只读文字在180px下拉截断、指定接收者勾选位置偏离；后继实际检查中未知24行完成事务后因缺独立说明失败，recipient 6行完成真实交互/保存等事务后因位置失败。两者产品修正及重验待完成。Advanced 中文两词 patch `caff1e225a834712db5248b2ecc9231af7974c4293df2bce9db97298e9bd4487` 已核与屏幕进入/离开命令绑定一致，catalogue构建和图待验。

The two focused successors passed eight language and six sharing rows. P02 observations identified clipped read-only text and displaced recipient indicators despite recorded transaction passes; the accepted checks produced 24 and six respective RED rows after transactions. The two-string Advanced translation patch above matches the underlying screen-enter/exit settings; catalogue build and successor captures remain pending.

两项动作后继在 `action-visual-green` 的真实检查中，接收者6行、未知动作24行均通过、0失败/跳过；1095文件前后映射相同、摘要 `057de28f5fd58ce25ad573250779f91a731f65fb6db9692345d6aad70c0b993e`。审核人另逐看24张未知动作及18张接收者后继整图，完整说明、居中勾选及可辨空框可见；图片哈希见独立账本。Advanced后继6张显示正确中文进入/离开屏幕词，QM三处相同；未执行命令。最终低空间安全清单79项目中77 Passed、2 Disabled/Not Run（OSXClipboard/OSXKeyState）；R4明确复用同源码/二进制的已执行完整运行，原始 QtTest 304通过（含生命周期）、0失败/跳过，179.212秒；其余目标新构建运行。旧整轮2项失败仍是历史事实；此结果不代表交互禁用目标、跨平台、原生包或硬件验收。

The two action successors passed six recipient and 24 unknown data rows, and 42 corresponding whole images were independently reviewed. Six Advanced images show the corrected screen-enter/exit wording. The final low-space inventory records 77 Passed and two Disabled / Not Run; R4 explicitly reuses the same-source same-binary complete execution (304 Qt passes including lifecycle), while the other targets were fresh. Earlier full-suite failures remain historical; interactive-disabled, other-platform, native package and hardware acceptance are not established.

协议检查合同 SHA `5a245454909f67f60ce53c838c616e7edcdf2f713cd031c376120d0c504a3482`、实际测试 SHA `d3e3fec5c61cac8d7a241ba525181dcc8f1e8842cbd38396c944e77bf22aa30c` 已核并接受：24 组合覆盖固定 enum、持久字面量、实际鼠标/键盘弹层及完整模型/设置/core 保存取消重开事务。旧 radio 回退不掩盖缺失下拉的累计失败；真实 RED 和后继绿色尚未执行。

Protocol contract and actual-test identities above are accepted for the bounded widget replacement. The 24 combinations cover fixed enum and persisted literals, real mouse/keyboard popup activation, and model/settings/core Save/Cancel/reopen transactions. The old-radio fallback retains the missing-dropdown failure; actual RED and successor green remain pending.

P02 抓图/meter 观察后继源码 SHA `63ebf4d0c952f2cbdb49d05d6d8ec81574cab9855db36f8d3dfa59f2aa7fd257`、合同 SHA `dfd9bda984afa63ab5e16fdfc658ba2d6684d0aaf78296f9873d83938f9109e8` 已核并接受。抓图只记录父摘要与 typed 模型、保存映射/core/取消证据，不自判语义通过；meter 检查真实可见速率和 ETA，仍保留字节及取消断言。最终执行计划 SHA `baa8785f77e53bb3c22048da5e3eea386fc9fa253914a1aa9f4c442f9f381fa8` 仅作为待执行计划接受，全部预计数量和人工结论以实际结果为准。

P02 capture/meter successor and final execution-plan identities above are accepted as pending work. Captures record real summaries and typed model, saved map, core and Cancel evidence without declaring semantic readability; the meter checks visible rate and ETA while preserving byte and cancellation assertions. Planned counts and manual conclusions require actual evidence.

展示层源码冻结清单 SHA `6e0d3d38c5ca4ad3a3620c19a051bcebde4f3dd0910c096b5fc114199e4f19de` 的 22 文件及三核心文件哈希均匹配。**更正此前未确认新缺陷的结论：R4-FKEY001 已确认。** `ActionPresentation.h` 的 `keySequenceCaption` 对每个非修饰键覆盖同一个 `key`，而 `KeySequence.cpp` 支持并序列化多个条目，因此 Ctrl+K+L 等会丢失前面的 K。必须先接受实际回归并保留 RED，再审展示层修正；核心序列化文件未变。方向/锁态索引由序列化有效性检查保护，等待/未知速率不产生有限 ETA。M08 的 Qt 隐藏目标事件转发、真实拖动创建/排列/删除及主屏/空白/外部配置保护必须实测；此为源码审查，非构建或验收通过。

All 22 frozen product identities and three unchanged core identities in the manifest above match. Correction to the earlier review: R4-FKEY001 is confirmed. The caption loop overwrites earlier non-modifier keys although KeySequence supports multiple entries, so Ctrl+K+L loses K in the displayed caption. Accepted executable regression and RED must precede its presentation-layer correction. Actual Qt event forwarding to the hidden legacy target, drag/create/rearrangement/delete and primary/empty/external guards still require execution; this is not a build or acceptance result.

**R4-FCAP002 已确认：**已知动作类型的无效方向或锁态被 `actionCaption` 统一标为“只读”，但 `ActionDialog` 只在类型未知时进入只读；已知类型可选择有效值修复，在修复前保存禁用。摘要与实际可编辑行为不一致。须先接受真实已知无效值及未知类型负例检查并取得 RED，再修正摘要；未知类型真实只读保持。

**R4-FCAP002 confirmed:** the caption labels a known action with an invalid direction or lock mode as read-only, while the editor permits correcting its value and disables saving until valid. Executable known-invalid and unknown-type negative controls and RED are required before correcting the caption; unknown types remain truly read-only.

**R4-FTILE003 设计对照差异已确认：**参考 HTML 的 `.screen.filled` 为绿、后置 `.screen.self` 为蓝，参考本机同时带两个类而客户端仅 filled；冻结的 delegate 却对 local 或 selected 使用绿、未选客户端使用蓝，且选中覆盖角色色。旧 remaining-red-r2 截图早于该 delegate，不能证明新二进制实际像素。须用当前身份的真实两角色及选中/未选抓图或独立颜色测量建立 RED，再配对修正。

**R4-FTILE003 reference discrepancy confirmed:** the HTML applies blue to self and green to a filled client, while the frozen delegate assigns green to local or selected and blue to an unselected client. The older screenshot predates this delegate and is not evidence of current pixels. Obtain identity-bound real-role/selection captures or independent color measurements and RED before correction.

主代理以探针 `103187...`、源码清单 `31f2c3e2...` 原生观察到本机绿色、未选客户端蓝色、选中客户端绿色及方形选中/悬停底衬，FTILE003 的真实 RED 成立。M08 点击新增/保存名称/删除、主屏/空白/外部配置保护及父页取消重开已观察；两种拖动均未见变化且指针未离起点，**拖动结果未知**，不能判通过或产品故障，需隔离控件事件跟踪。

The PM's identity-bound native observation confirms the reversed tile colors and square selection/hover underlay. M08 click flows, deletion guards and parent Cancel were observed; drag attempts did not move the pointer from their origin, leaving drag behavior unknown rather than passing or confirmed broken.

FKEY/FCAP 实际检查后继 SHA `57a2eb5a47a1c4d68573ef92efc6d701595fd428362225f8be9a546bed3168fc` 已接受；K/L 使用独立边界以避免匹配 Ctrl 中的 l。首次对 22 文件冻结构建退出码 1：`SettingsDialogDesign.cpp` 的成员捕获及 `ScreenSetupView.cpp` 的 protected 访问编译失败；没有执行产品 RED 或 GREEN。构建期六份 TS 自动变化单列，修正后需新冻结和重新构建。

The FKEY/FCAP actual successor above was accepted with separate K/L boundaries. The first frozen-source build exited 1 on member capture and protected access errors; no product RED or GREEN was executed. Six automatic TS changes during build are separately recorded, and a new freeze/build is required.

机械修正 patch `09b9d1d3a9ab772677d2a039778bc934ab683e357230d76e68e443dd62c744dc` 与后继冻结清单 `7b0b5ae32b11296b76e075d697c317cd792ce9e04c081dbaa86bf0edb5026b78` 已审核，重建 exit 0。FKEY 实际 30 行通过、6 行失败（含生命周期 QtTest 32 通过），失败行摘要为 `⌘L`，核心为 `Control+k+l`，取消/模型/保存图与两次状态信号已执行。FCAP 实际 8 行通过、4 行失败（QtTest 10 通过），已知无效值两种双语行完整事务后因错误只读摘要失败，未知/legacy 负例通过。Flow/Scope 各 6 行通过，另各 2 生命周期。协议 24 行中仅 23 行完成事务，首个中文夹具在快照后写入语言键；不得称干净 RED，待夹具后继及重跑。

The mechanical successor above was reviewed and rebuilt successfully. FKEY has six meaningful RED rows after the transaction assertions, and FCAP has four known-invalid RED rows with eight unknown/legacy controls passing. Flow and Scope each passed six data rows. Protocol produced only 23 complete transactions: the first Chinese fixture changed its language setting after the snapshot, so a corrected fixture and complete rerun are required.

协议语言夹具后继 `919dc7c0979836c6faf9ee35a8de68368c4ad15845b9c77bc1e77627d403b0a6` 已审核，干净重跑 24 数据行均完成严格事务后仅因无可见两选下拉失败；QtTest 2 生命周期通过、0 跳过、32.306 秒，72 次真实旧 radio 路由。测试前后 1094 文件映射相同，内容摘要 `76bfbf172bc41ace14daf7e254d48b2e14099ef5d8c91849bdfdb88fc52677be`。后继产品和 GREEN 尚待核。

The corrected protocol fixture above produced 24 meaningful RED rows after complete transactions, with 72 legacy radio activations and no skips; test source maps agreed. The paired product and GREEN remain pending.

最终配对 patch `387fa78a19d01d9623768b4e34b3a8c87891005b6a87cae6411f5271d7675fba` 及 24 文件清单 `5ed848a5b0fd9f37a2d5d308b1d5fffcec39c9abab1707da564759b59921a95a` 已逐项核对身份并静态审查：多键按存储项逐个呈现，已知无效值有界提示、未知保留只读；协议双选经既有 radio 业务槽同步；本机/客户端颜色按角色选择。未在上述配对差异中确认新缺陷；构建和真实事务/视觉验证尚未执行。

The final paired patch and 24-file identities above were checked. Static review found no additional confirmed defect in the bounded changes; build, actual protocol and caption transactions, drag behavior and visual acceptance remain pending.

tile 暗色 token 后继 `ScreenSetupView.cpp` SHA `59398bece5dabeac800dceea18c8828ed6886932b35a1bfbc8e0d7c0781d30ef`，完整 patch `6e1f527648d711117038df973345d585d8a20fccc0ab3d1e2942c4728e6fb6d0` 与冻结清单 `272f3d3db5a666664b8205a87a23bde55ab5090d35f229eb9f0895d2949efdf5` 已复核；列出文件与核心身份全部匹配。本机/客户端文字色、填充透明度、边框、空格底色及圆角按参考设定，实际渲染和交互待检查。

The final tile token successor and full freeze identities above match. Its dark role text, fill alpha, border, empty background and radius follow the reference in source; rendered pixels and interaction remain pending.

最终有界隐私复核：按冻结清单 24 个产品文件，加 R4 driver/被动 trace header，以及 `docs/reports/20261117-ui-*.md`、`docs/aegis/20261117-ui-*.md`，共 60 文件，搜索邮箱、IPv4 与 `/Users/<name>` 模式。driver 两处 IPv4 为隔离 loopback `127.0.0.1`；`docs/reports/20261117-ui-completion-tests.md:78` 的历史预览路径使用用户本轮明确提供的工作目录用户名，与公开项目别名一致，并未发现额外真实姓名或秘密。建议改为仓库相对路径属于文档可移植性整理，不是确认隐私泄露，也不代表已批准公开发布。未读取日常设置或无关私密文件；有限模式搜索不能证明不存在其他个人信息。

Bounded privacy review searched 60 files: the 24 frozen product files, R4 driver and passive trace header, and matching UI reports/governance documents. Email, IPv4 and macOS home-path patterns found two loopback fixtures and a historical preview path using the workspace username supplied in this conversation, consistent with the public project alias; no additional real name or secret was identified. A repository-relative path is a portability cleanup, not a confirmed privacy leak or publication approval. Daily settings and unrelated private files were not read; pattern matching is not exhaustive.

人工检查表后继 SHA-256 `aaa1a89c3c1fbcb08c628d79c7483c27282acbbe5ac6ba8ef70c479f91718b26` 与逐页检查表 SHA-256 `b7ace8bb85ed7925df2159a155c6c37e3256916dd97611c6a07dbdeb571cc9df` 已核对并作为待执行检查工件接受；这不代表 after 或原生验收已完成。M08 必须实际检查隐藏旧大型工具后拖拽创建入口仍可用，以及排列和删除保护；F09 若更换协议控件类型，必须先接受真实 itemData、弹层、保存/取消、重开与序列化的可执行检查。

remaining-green 原始回执为 90 数据行通过，含生命周期共 92，通过以外为 0 失败、0 跳过，用时 46.692 秒、退出码 0；测试前后 1091 文件映射一致，摘要为 `79b2ba8616cd401b6a8f60f3161466514cbb93046b8b38c63ac5884b00956863`。九个先审查文件中八个源码身份匹配，中文 TS 测试时为 `3e2656ac56e01ba95f743d0bdda44fec1da4c6ab713ba16aba625fbe0c0be4f8`，不同于此前审核版本，构建期间翻译差异须另行复审；不能据此称人工或原生验收通过。

P 检查在修正加载后对象基线及 const 重载后接受 SHA `7137151df39269279503a7fb9d6b533d1599873eb9638a028de82aa7072671c4`；中间 `e7f61738...` 编译失败已保留。干净 RED 构建成功，30 数据行失败、2 个生命周期通过、0 跳过，9.760 秒。24 键鼠行的 Esc、模型、保存映射及取消断言走完后，因字段高度、协议式文案及缺独立 Record 汇总失败；6 个 General 行因指定文案失败。测试前后 1091 文件映射相同，内容摘要 `509233993d91857c219f84525d5dfbfa1d913bd56a7126b1ff030d3cb763fbce`。Flow、Scope 与 P02 八类型人工逐例另计。

The successor manual worksheet and page worksheet identities above are accepted only as pending inspection artifacts, not executed after-state or native acceptance. M08 requires an actual usable drag-to-create entry point and verification of layout and deletion guards after the legacy tools are hidden. Any F09 protocol widget replacement requires accepted executable coverage of real itemData, popup activation, Save/Cancel, reopening and serialization first.

### 主页语言原生观察后续任务

- [x] 审核主页语言切换实际回归检查。
- [x] 审核窄范围重新翻译修正。
- [x] 核查最终后继原始证据。

### UI-008/014 原生观察后续任务

- [x] 审核同一对话框切换语言的作用域回归检查。
- [x] 审核 FeatureHandler 的窄范围修正及动作布局新增检查。
- [x] 核查后继原始验证并更新报告。

### 本次续审任务

- [x] 审查冻结的主窗口/更新补丁和语言后继检查。
- [x] 审查新增实际流程检查并报告发现。
- [x] 复核修正源码并核对现有证据身份。

### 实时审核任务

- [x] 阅读治理技能、项目规则、规格与执行包。
- [x] 核对现有实际检查与契约，报告编码前充分性。
- [x] 审查变更产品源码及限定范围内的隐私风险。
- [x] 复核现有补充检查与首批产品补丁，核对身份并完成阶段报告。
- [x] 已取消：后续最终源码与证据尚未就绪，依 PM 指示交由后续独立复核。
