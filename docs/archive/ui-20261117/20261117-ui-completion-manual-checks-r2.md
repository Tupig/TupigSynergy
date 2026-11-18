# Follow-on check refinement / 后续检查细化

## English

Predecessor: `docs/reports/20261117-ui-completion-manual-checks.md`, SHA256 `c4f99e4434d30ff24a74d4590015d4dfd5230c75fd85fd107f64f9a316d0f425`; preserve that artifact and pending results. This successor strengthens M-C03 and clarifies M-C04 before their paired source corrections. All safety, language/width and evidence requirements remain.

- C-UI-R3-M03-R2: Seed two distinguishable valid actions. In the nested Hotkey editor remove the first and add another distinct valid Action, accept the Hotkey, and require parent visible action rows/count/order to match the new draft. Open Edit on the actual surviving first row and require its selector ID to match that survivor. Cancel Action and Server parent; persistent original two actions and order remain. This detects stale visible rows routing to different underlying actions.
- C-UI-R3-M04-R2: Generate distinct temporary certificates A and B, record their distinct complete SHA-256 fingerprints. Save A, edit unsaved path B. The visible action explicitly says `View saved certificate fingerprint`; its real child shows complete hash A and source path A. Back retains draft B. Cancel and reopen Preferences retains saved A. This adopts PM-approved saved/active-certificate semantics, without a trust-policy change. A single-certificate or unrelated-port draft cannot establish this check.

Executable artifact: additive `src/unittests/gui/MainWindowFlowTests.cpp`; all six language/width global-data combinations run the actual widget routes. Preserve prior RED and inspect fresh output. Until execution, both successor checks remain Pending.

## 中文

前件：`docs/reports/20261117-ui-completion-manual-checks.md`，SHA256为上文值；保留原工件和待执行状态。本后继在配对源码修正前强化M-C03、明确M-C04。原安全、中英文/宽度及证据要求继续有效。

- C-UI-R3-M03-R2：准备两个可区分的合法动作。嵌套Hotkey页删除第一个、再增加另一个可区分合法Action，接受Hotkey后，父页动作的可见行数/文字/顺序须和新草稿一致。编辑实际存活的首行，选择器ID须对应存活动作。取消Action及Server父页，持久化原来两个动作及顺序不变。此检查专门发现可见旧行与真实动作不一致而错编辑的问题。
- C-UI-R3-M04-R2：临时生成不同证书A、B，记录完整且不同的SHA-256。保存A、把路径草稿改成B。可见按钮须明确“查看已保存证书指纹”，真实子页显示A完整指纹及A来源路径。返回保留B草稿；取消并重开首选项仍保存A。这采用PM已选的当前已保存证书语义，不修改信任策略。单证书或只改端口不足以完成此检查。

对应可执行工件为新增 `src/unittests/gui/MainWindowFlowTests.cpp`，六个语言/宽度全局数据组合都经过真实控件。保留既有失败并检查新输出；未执行前两项均为待办。
