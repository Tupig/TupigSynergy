# Windows CJK log font correction / Windows 中文日志字体纠正包

## English

Immutable BP-GH-20261118-WINDOWS-CJK-FONT, created2026-11-18. Sole master spec.md revision UI-2026-11-18-GH-TWO-TARGETS SHA256 `1182eaf98f950de91564f40d6305998b5a190df2d3c89b2fe72bceed788732c8`. Predecessor diagnostic package docs/aegis/20261118-windows-native-cjk-geometry-build-package.md SHA256 `498631ad7636f9040d9591c4610796bd9bfa760b45e59fedbc7eb8068d718916`. Covers UI-002/008/010/012/017, GH-V02 and issue385.

Accurate Windows native run44235606226/head81a9ea6 reports the unchanged footer assertion failing only for `zh_CN-light-client-expanded` and `zh_CN-dark-client-expanded`. The bounded witness records a 600x470 window, 402px viewport, 515px workspace contents, 299px log panel and a footer mapped at y465 with scroll value18/max113. The Windows LogWidget font is exact-match Consolas at 14px, while the CJK advance is 28px and the Latin advance is64px. The expanded log therefore consumes the available height through native CJK fallback metrics. The same run passes English, server and collapsed matrices. Raw witness is local and ignored.

Contract C-WIN-CJK2: after G4, add one product correction in `src/lib/gui/widgets/LogWidget.cpp` that chooses the Windows system general UI font for the log table, leaving non-Windows font selection unchanged. Keep font size, row wrapping, 10000-record history, filters, copy behavior and all geometry assertions unchanged. The correction must not install fonts, change CI environment, replace missing glyphs or resize the test window. Existing C-WIN-CJK1 witness remains in the test source. Accurate Windows native rerun must show reduced CJK metrics and the original full-footer assertion passing in all matrices, with Mac critical suites also rerun. If the general system font still leaves the footer outside the viewport, stop and use the witness to choose the next source-level correction rather than weakening the assertion.

## 中文

不可变 BP-GH-20261118-WINDOWS-CJK-FONT，绑定主规格1182及前序诊断包498631ad。对应UI002/008/010/012/017、GH-V02及385。

准确Windows原生44235606226/81a9ea6的见证显示，原footer断言只有中文client expanded失败。窗口600x470、viewport402、workspace内容515、logPanel299，footer映射到y465，滚动值18、最大113。Windows LogWidget使用精确匹配的Consolas 14px，中文单字宽28、拉丁字宽64，六条日志换行后占满高度。英文、server、collapsed均通过。

C-WIN-CJK2：G4后只改 `src/lib/gui/widgets/LogWidget.cpp` 的Windows字体选择，使用Windows普通系统界面字体，非Windows保持原选择。字号、换行、10000条历史、过滤、复制和全部几何断言不变。不安装字体、不改CI环境、不替换缺字、不放大窗口。保留CJK1见证，准确Windows原生重跑须让中文指标下降并通过原footer完整可见断言，Mac关键测试同步重跑。若普通系统字体仍不能让footer可见，就用见证继续定位源码，不放宽断言。
