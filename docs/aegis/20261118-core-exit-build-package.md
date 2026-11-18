# Core exit classification successor / Core退出分类后继包

## English

Immutable BP-CORE-EXIT-20261118, created2026-11-18. Sole master spec.md revision UI-2026-11-18-GH-TWO-TARGETS SHA256 `1182eaf98f950de91564f40d6305998b5a190df2d3c89b2fe72bceed788732c8`; predecessor docs/aegis/20261118-github-two-targets-build-package.md SHA256 `b0ad79d3ae38ca78f1f50c285a5a28e02d75ed39c800b497f65d406a206257ff`. Covered existing UI-001/008/012 real GUI process lifecycle and regression, UI-017 reviewed GitHub delivery; no new product law or master amendment. Baseline commit972ded750d7657c17d07ae0188a26256cbeddc39/tree523db36d97468311c768e9b1c142c1a591f1c044. High release risk, continuing explicit user autonomous issue-fix/test/push authority; no published tag or asset exists for2.1.1. Existing release ADR/roles and no real sharing/permissions boundary continue.

Observed source defect: CoreProcess::onProcessFinished discards QProcess::ExitStatus and branches on exitCode. Qt defines that value only for NormalExit. A CrashExit raw0 can reach healthy retry, or raw5 can reach existing-core probing. This is a confirmed classification defect, not proof of the historical code9/IPC incident. Preserve #378 as unresolved until actual causal/native acceptance; track this defect separately. No protocol, core binary, daemon/service, migration, IPC naming or retry policy redesign.

Contract C-CORE-EXIT-R1: intentional Stopping finishes as Stopped with no crash warning/retry; other CrashExit finishes Stopped/Disconnected, clears any pending retry and reports a crash independent of raw0/9; it must not enter normal success/duplicate branches. NormalExit0 keeps current stopped/started retry semantics, NormalExit9 keeps ordinary failure. Existing normal duplicate5 branch remains byte-equivalent and is reviewed without executing fixed live IPC. Test the actual handler through existing R3SerializationTests friend access and isolated RuntimeContext/settings, seed only in-memory state/timer, capture actual messages/state, stop all timers before returning. No process launch, IPC connect, live clipboard/input, permission or ordinary settings. This callback fixture is not a real QProcess/core ten-cycle or physical acceptance claim.

Dependency order: CE1 tester plans concrete safe cases only, independent PE/SecOps planned acceptance; continuing scoped G4 then tester writes actual tests solely in existing src/unittests/gui/R3SerializationTests.cpp, independent PE-CHECK, targeted RED. CE2 bounded coder fixes only src/lib/gui/core/CoreProcess.cpp, preserve intentional-stop and NormalExit branches, no new production test seam/API; independent review and full same targeted suite GREEN/format/diff. Two affected downstream work items are rebuilt; CI21 and unrelated R4 evidence stay scoped to their identities. Existing GitHub14critical wiring already includes R3SerializationTests; no workflow/new test app target. Only the existing targeted unit executable may build locally using installed pinned dependencies, capture logs/identity and remove that generated executable after verification; no product app/all-build. Then manifest/public-delta review, Chinese fix commit and samePR379 push; exact-head Macarm64/Windowsx64 CI follows before integration/release. Mutable approvals/progress remain root status/journal, never rewrite this package.

## 中文

不可变BP-CORE-EXIT-20261118，主规格/前序/当前提交和树的完整绑定见英文。仅落实既有UI-001/008/012生命周期及回归和UI-017交付，不增加产品法、不另造规格。沿用用户自主处理issue、测试、推送权限，高发布风险/ADR及无真实共享权限边界保留；2.1.1尚未发布。

已证源码丢弃ExitStatus后按仅NormalExit有效的退出码分类；CrashExit原始0可能误入正常重试、5可能误入探测既有core。此缺陷独立追踪，不代表历史code9/IPC根因已证，378仍保持开放。不改协议、core/daemon、IPC名字、迁移或重试策略。

固定契约：Stopping保持主动停止；其他CrashExit(raw0/9)必须停止/断连、清重试并报崩溃，不按正常码分类；NormalExit0保留已有正常/重试，NormalExit9保留失败。正常duplicate5分支原字节保留并源码复核，本地不调用固定实时IPC。复用R3SerializationTests既有friend和隔离RuntimeContext/设置，仅设置内存状态/定时器，检查真实handler状态/日志，退出前停所有定时器，无进程/IPC/真实输入/权限/日常设置。夹具不冒充真实core十次或物理验收。

先测试计划、PE/SecOps，再有界G4/实际检查/独立充分性/定向RED，后仅CoreProcess.cpp最小配对修复和独立完整同套件GREEN。仅重建检查/产品两项；现有CI21/R4保留原身份。既有14关键项已含本测试，不加workflow或app target；本地只构建既有单元测试可执行文件，留证后清该生成文件，不构建产品app/ALL。然后精确新清单/公开增量、中文fix提交、同PR379推送与准确双平台CI。状态审批只写根status/journal，不改本包。
