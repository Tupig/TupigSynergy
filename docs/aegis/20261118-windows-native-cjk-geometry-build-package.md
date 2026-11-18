# Windows native CJK geometry diagnosis / Windows 原生中文几何诊断包

## English

Immutable BP-GH-20261118-WINDOWS-CJK-GEOMETRY, created2026-11-18. Sole master spec.md revision UI-2026-11-18-GH-TWO-TARGETS SHA256 `1182eaf98f950de91564f40d6305998b5a190df2d3c89b2fe72bceed788732c8`. Predecessors docs/aegis/20261118-windows-offscreen-font-build-package.md SHA256 `c60f634063f8101c4e173a009c383efeab8c5940ac9cbd7041c65b7d02e19cb4` and docs/aegis/20261118-native-critical-backend-build-package.md SHA256 `1d003721e5bf858aaa581c5de23763753a540471552dd9ff666cd1bb2f566abe`. Covers UI-002/008/010/012/017, GH-V02 and issue385. High public-delivery risk and existing direct autonomy continue. This is diagnosis first; it does not authorize weakening geometry assertions or choosing a font fallback.

Observed exact run44221381517/head3800162 Windows Qt6.10.3 native `windows`: build and PageHost/Editor pass; NativeDesign is 71 passed/2 failed, only `zh_CN-{light,dark}-client-expanded` at the unchanged footer containment assertion (line377). English, responsive widths, server and collapsed matrices pass. Raw SHA256 `ee8f57a1c4c3c66f6017b105353762f91d44c3d5f36f746552e081378534cbfc`. Native evidence reports Segoe UI 13 exact, Microsoft YaHei present, Noto Sans CJK SC absent, familyCount138, and the Windows LogWidget explicitly uses Consolas 12. The previous offscreen run passed 73/0 with a different font inventory, so it is not equivalent evidence.

Contract C-WIN-CJK1: after G4, add only bounded diagnostic evidence to the existing NativeDesign test or its accepted test evidence path, preserving the full matrix, 600x470 window, `ensureWidgetVisible`, and the original full-footer containment assertion. Record fixed numeric/boolean geometry and font facts only: window/workspace/viewport/workspaceContents/logPanel/expanded/records/table/footer mapped rectangles; footer size/minimum; vertical scrollbar value/max/pageStep; message-column width; six row heights; LogWidget/logPanel fixed/minimum heights; resolved font family/metrics; and the fixed synthetic translation text length or hash, never hostnames, settings, paths, or raw screenshots in public evidence. The diagnostic must distinguish layout height from scroll extent and font fallback from widget visibility. Do not claim a root cause from the current two failures alone.

Only after the diagnostic identifies a source-level cause may a successor product correction be planned. Any correction must keep the full Chinese/English, light/dark, client/server, expanded/collapsed matrix, 600x470 geometry, footer containment, 540/600/820 responsive checks, existing font policy and native Windows backend. No skipped Chinese rows, larger test window, relaxed containment, offscreen fallback, missing-glyph substitution, system font installation, or CI-only workaround. Accurate Windows native and macOS critical reruns remain required; this issue is separate from Motion386 and the Windows link fixture.

No source change, build, or runtime execution is released by this package. Raw/JUnit/summary/PNG remain ignored; only bounded reviewed diagnostics may be committed. Existing package/CodeQL/release gates remain unchanged.

## 中文

不可变 BP-GH-20261118-WINDOWS-CJK-GEOMETRY，绑定同一主规格1182及前序字体包c60/原生包1d0037。映射UI002/008/010/012/017、GH-V02及385，风险和用户授权不变。先诊断，不准放宽几何判断或随便换字体。

准确44221381517/3800162的Windows Qt6.10.3原生windows：编译、PageHost、Editor通过；NativeDesign 71过2失败，只有 `zh_CN-{light,dark}-client-expanded` 在原footer完整可见断言失败。英文、宽度、server、collapsed均过。原文SHA `ee8f57a1c4c3c66f6017b105353762f91d44c3d5f36f746552e081378534cbfc`。系统字体报告Segoe UI13、Microsoft YaHei存在、Noto Sans CJK SC不存在，LogWidget在Windows用Consolas12；旧offscreen运行不同字体环境下73/0，不能当同一证据。

C-WIN-CJK1：G4后只在现有NativeDesign测试/证据加有限诊断，保完整矩阵、600x470、ensureWidgetVisible及footer完整可见原断言。只记固定数值/布尔几何和字体信息：各层矩形、footer size/minimum、滚动条value/max/pageStep、消息列宽、6行高度、LogWidget/logPanel固定/最小高度、解析字体指标及固定合成文本长度或hash；不公开主机名、设置、路径或原图。区分布局高度、滚动范围、字体fallback和可见性，不能凭两次失败直接定根因。

诊断明确源码原因后才能另包修复。修复仍须保中英/明暗/client/server/expanded/collapsed全矩阵、600x470、footer断言、540/600/820宽度、原字体策略和Windows原生后端。禁止跳中文、放大测试窗口、放宽断言、退offscreen、缺字替代、装系统字体或CI特判。准确Windows原生和Mac关键测试仍需重跑；与386及链接夹具分开。

本包不释放源码、构建或运行修改；raw/JUnit/summary/PNG继续忽略，只能提交审核后的有限诊断。包、CodeQL、发布门禁不变。
