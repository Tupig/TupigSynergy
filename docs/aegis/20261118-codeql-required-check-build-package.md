# Required CodeQL check successor / 必需CodeQL检查后继包

## English

Immutable BP-GH-20261118-CODEQL, created2026-11-18. Sole master spec.md revision UI-2026-11-18-GH-TWO-TARGETS SHA256 `1182eaf98f950de91564f40d6305998b5a190df2d3c89b2fe72bceed788732c8`. Predecessor docs/aegis/20261118-github-two-targets-build-package.md SHA256 `b0ad79d3ae38ca78f1f50c285a5a28e02d75ed39c800b497f65d406a206257ff`. Covered UI-017/GH-V02 accurate-head validation and protected PR integration; no new product law or target. Risk high/public release; existing ADR, user autonomous execution/release authorization and accepted role/model mapping continue.

Observed blocker: active Protect main ruleset24874493 requires ci-passed and Analyze C++, one approval and resolved review threads. Current head e3e0616b00e630f5fe12cc7a0d4bd1b418e2fb5c/tree0817f6a57c8daf5e3f9120a13ffeade95530bc24 has only ci.yml and no Analyze C++ producer/result. CodeQL default setup API is not-configured; last dynamic Advanced Security run43867914542 belongs to old2fe0b1c, not current proof. Preserve rules; do not bypass, self-approve, synthesize successful context or configure unrelated services. An external valid review remains an integration prerequisite.

Remedy: one actual CodeQL helper job in ci.yml named Analyze C++, official init/analyze pinned to verified full commit, language c-cpp and build-mode none on Ubuntu. No autobuild, product compile/package/dependency install, secrets, continue-on-error or conditional successful omission. Job-scoped contents read and security-events write only as required by official action. It executes for PR, manual snapshot and release; both ci-passed and publication gate require its actual success. Existing macOSarm64/Windowsx64 and version/tag/source/no-overwrite/diagnostic/translation contracts remain. Official source: https://docs.github.com/en/code-security/how-tos/find-and-fix-code-vulnerabilities/manage-your-configuration/codeql-for-compiled-languages . No-build analysis excludes generated code and may infer dependencies imperfectly; it does not replace native builds or independent SecOps.

Dependency order and contract C-GH-CQ1: CQ1 tester plans then, after direct G4, creates a minimal actual validation artifact checking real job/init/analyze pinned wiring, precise name, permissions, no build/secret/bypass, all event routes and failure/skipped/cancelled release+ci-passed behavior; PE-CHECK approves adequacy before product edits. CQ2 delivery role changes only ci.yml and necessary bilingual build documentation after actual baseline, preserves original21 semantics, freezes exact patch/source. CQ3 tester executes complete unchanged21 plus CQ1 contract, PE and SecOps independently review same exact delta including PII; no app/ALL/local CodeQL build. CQ4 PM records logical Chinese commit and samePR379 nonforce push after acceptance, then verifies actual accurate-head CodeQL/alerts/native/package results before integration/tag/release. Allow rebuild of only affected CI validation/product work; preserve predecessor checks/raw and make no speculative native acceptance. Delivery owner/tester/PE/SecOps reuse existing agents; PM owns records only.

Entry: observed source/remote mismatch, bound master/predecessor, planned PE/SecOps agreement. Exit: actual independent contracts and exact source accepted, accurate-head native/package/CodeQL jobs pass, ruleset context observed, valid external review, reviewed integrated source; then existing immutable release chain. Recovery: retain PR and prior source; revert scoped workflow commit through PR if needed, never alter published tags/assets. Mutable state belongs only status.md/journal.md and append-only execution receipts.

## 中文

不可变BP-GH-20261118-CODEQL，绑定上文同一主规格1182及双目标前序b0ad完整身份；覆盖UI-017/GH-V02准确提交验证/受保护PR整合，不改产品要求或目标。沿用用户自主执行发布授权、high风险、既有ADR及角色/模型映射。

实际规则要求ci-passed、Analyze C++、一次有效批准及审查线程解决；当前e3e0616/tree0817只有ci.yml且无安全检查生产者/结果，默认CodeQL未配置，dynamic最近成功只属旧2fe0b1c。不改保护、不绕过、不自己批准、不伪造context；外部有效批准仍是整合前提。

最小修复为ci.yml一个真实CodeQL辅助job，精确名Analyze C++，固定官方init/analyze完整commit，c-cpp/build-mode none在Ubuntu运行，仅分析源码不编译或打包Linux，不装产品依赖、不用secret、不continue-on-error；job最小contents读/security-events写。PR/快照/正式发布均实际运行，ci-passed及公开发布AND均需实际成功。保双原生目标、版本/tag/source/no-overwrite/诊断/翻译原合同。无构建分析不覆盖生成代码且依赖推断有限，不替代原生构建/SecOps。

CQ1独立测试计划后沿直接G4建实际最小合同，PE充分性后才产品；CQ2作者只改workflow和必要双语构建说明，先真实基线后冻结；CQ3独立完整原21+新合同及PE/SecOps精确源/PII；CQ4同PR中文提交非强推，准确head真实CodeQL/告警/双平台/包齐并有外部批准后才整合，续原发布链。只重建受影响CI两项工作，保旧证据，不造本机app/ALL。PM只维护记录；复用现有角色。失败走有界PR回退，不改已发布tag/资产；当前状态及执行回执另记。
