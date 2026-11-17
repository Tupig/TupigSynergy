# Editor log diagnostics successor / 编辑器日志诊断后继包

## English

Immutable BP-EDITOR-LOG-20261118, created2026-11-18. Sole master spec.md revision UI-2026-11-18-GH-TWO-TARGETS SHA256 `1182eaf98f950de91564f40d6305998b5a190df2d3c89b2fe72bceed788732c8`; predecessor docs/aegis/20261118-github-two-targets-build-package.md SHA256 `b0ad79d3ae38ca78f1f50c285a5a28e02d75ed39c800b497f65d406a206257ff`. Covered existing UI-007/008/012/017: full10000-record history, latest6 visible, complete wrapped long lines, native tests and exact-source GitHub delivery. No new product law/master; high public-release risk/ADR/roles continue. Baseline972ded750d7657c17d07ae0188a26256cbeddc39/tree523db36d97468311c768e9b1c142c1a591f1c044; independent CE check/source and Windows rename corrections retain separate identities.

Real run44170059420 MacQt6.11.1/shared/offscreen Buildpassed, PageHostpassed, EditorTransactionTests timed out60.06s after first3business slots. Nextslot logsFilterCountAndBoundHistory has10003 synchronous appends then20000-char line/show/processEvents. No modal/core path. Auto-header layout cost and repeated zero-delay height updates are hypotheses only. Issue383 tracks actual failure, not assumed Qt regression or product deadlock.

Contract C-EDITOR-LOG-R1 preserves every existing assertion, loop count10003, retained limit10000, long-line length20000, original invocation order and60s timeout. EL1 tester adds only fixed stderr-flushed QElapsedTimer phase markers to the existing slot: entrance/append0/each1000/loopend, before/after longappend, resize, show, processEvents; about20lines total, no event pump/wait/signal connection beyond original calls, no -vs output flood. Independent PE planned/actual adequacy precedes execution. After CE builds finish, serially build the existing EditorTransactionTests target only; local installed staticQt uses cocoa, CI prebuiltQt uses offscreen. No new Qt installation/app/target/ALL/product build. Run selected slot -v1 with hard60s, then whole original suite. Sample only the exact owned unitPID for about3s when useful, no broad process lookup/elevation; record phase/stack/exe/source identity, real exit and cleanup.

EL2 begins only after actual timing/stack localizes a source cost. Propose/review the smallest LogWidget.cpp/h correction against unchanged functionality and lifecycle/width contracts; no blind timeout increase, skip, fewer records or weakened assertion. Preserve all source evidence; run same full suite, affected checks and exact-head Macarm64/Windowsx64 CI before release acceptance. Only two check/product work items are rebuilt; unrelated CI21/CE remain identity-scoped. Tests never launch core, touch fixed IPC, inject input, mutate live clipboard, grant permissions or read ordinary private settings. Generated unit executable is captured then removed; caches/downloads retained. Mutable progress/decisions only rootstatus/journal; do not rewrite this package.

## 中文

不可变BP-EDITOR-LOG-20261118，主规格/前序/基线完整身份见英文；落实原UI-007/008/012/017（10000全历史、末6可见、完整长行及原生回归/交付），不增产品法。沿用高发布风险/ADR/角色，CE及Windows命名纠正独立身份保留。

真实MacQt6.11.1 shared/offscreen编译通过，PageHost通过，Editor前3业务slot后60.06秒超时；下一slot为10003追加及20000字长行/show/processEvents，无modal/core调用。表头布局/零延迟高度更新仅候选，383记录真实失败，不能先称Qt回归或产品死锁。

固定契约保所有断言、10003循环/10000上限/20000字、原调用顺序/60秒。先仅既有slot加约20条QElapsedTimer+stderr显式flush分段标记，不加事件/等待/信号、不海量-vs。计划/实际独立接受后执行；等CE构建完，串行构建既有EditorTransactionTests target，本机用已装静态cocoa，CI预编译offscreen，不装新Qt/造app/target，不ALL/产品构建。单slot -v1硬60秒后原完整suite；必要仅确切自有unitPID采样约3秒，无泛搜/提权，记录原文/身份/退出/清理。

实测时序/栈定位后才另审最小LogWidget cpp/h修正，保功能/生命周期/宽度；不延时/跳过/减数据/弱断言。完整原套件、受影响检查及准确head双平台CI后再交付。仅检查/产品两项，其他证据按各自身份。无core/固定IPC/真实输入剪贴板/权限/日常设置；单元生成exe留证后清，保依赖下载缓存。状态审批只根记录，不改本包。
