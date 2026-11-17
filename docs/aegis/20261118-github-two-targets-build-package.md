# Two-target GitHub successor / 双目标GitHub后继包

## English

Immutable BP-GH-20261118-TWO-TARGETS, created2026-11-18. Sole master spec.md revision UI-2026-11-18-GH-TWO-TARGETS SHA256 `1182eaf98f950de91564f40d6305998b5a190df2d3c89b2fe72bceed788732c8`. Predecessor docs/aegis/20261118-github-delivery-build-package.md SHA256 `9585fd655ee487ef4606cc8e83231642b161443b16fbb5dd51aab8f8948bc91c`, pre-amendment master46cbb513 remains in its historical binding and local snapshot. Direct authority: user explicitly removes Linux GitHub builds and requires only macOS arm64/Windows x64. Covered UI-017/GH-V02 amendment; original UI008/012/018/019 and other contracts continue. Riskhigh remains, existing release ADR and roles/model mapping remain. This is execution evidence, not a second spec.

Meaningful scope reduction: cancel the single newly added Linux-wiring check and any uncommitted Linux job/gate/docs promise, preserve their original source/raw/hash as history. Restore the prior18method semantic contract exactly and add a separate positive/negative two-target planner/configure contract, actual check first then independent PE-CHECK. Exercise actual Plan builds shell for PR/release/snapshot selections; assert one Windows x64 and one Mac arm64 target and actual compiler/configuration wiring, unsupported selection/old published trigger reject, no Linux/Flatpak product jobs or gates. Do not confuse Ubuntu helper jobs with Linux product builds. No version/API/suite/TS assertion is relaxed.

Dependency order: TT1 PM binds this amendment, tester writes actual two-target check after direct G4, PE/SecOps accept same planned identity and PE-CHECK approves actual artifact; TT2 CI owner freezes inherited six-file GH2 WIP, removes only current Linux increment and corrects actual two-target mismatches after baseline; TT3 independent tester executes full current contract plus original18 and same real Qt fixture, PE/SecOps review exact source/public staging; TT4 original GH5/GH6 sequence continues: Chinese scoped commits/PR379, exact-head native CI, reviewed integratedmain/tagv2.1.1, successful GitHub-built Windowsx64/macOSarm64 assets/provenance/payload then permitted native/issue evidence. No Linux job is required for this user's delivery. No routine reapproval, helper/controller, new credential, local app variant or local full-build.

Earlier RED19methods/65failureentries/0errors remains evidence for its original source and then-current contract; it is not current all-green proof. All prior source/test receipts stay immutable. Existing6file WIP may not be called accepted before the revised complete check/source review. Preserve old releases; no directmain push or published tag/asset rewrite; code9/physical/native gaps are never closed from mocks. Resulting source/spec identities belong to final receipts; mutable progress only status/journal.

## 中文

不可变BP-GH-20261118-TWO-TARGETS，创建2026-11-18，绑定上文同一主规格新修订及完整摘要，前序9585fd65/原主规格46cbb绑定留作历史并有本地原件。用户两条具体指令直接取消GitHub Linux构建，仅Mac arm64/Windows X64。映射UI-017/GH-V02，其他既有需求/安全边界不变，风险仍high，沿用ADR/角色映射，不造第二规格。

取消唯一新增Linux接线检查及未提交Linuxjob/gate/文档承诺，原源码/RED/摘要原样保留。原18方法语义精确恢复，另补真实planner/configure双目标正反例，先实际检查再独立接受；不放宽版本/API/suite/TS。PR、release、snapshot均只能选双目标，fullrelease两端必成，实际编译器/架构接线也需核；Ubuntu辅助job不误算Linux产品。旧19方法65失败零环境结果保留其历史，不当现行通过。

TT1绑定修订/独立检查，TT2既有CI作者冻结六文件WIP后按基线窄修，TT3独立完整新合同/原18/真实Qt及源码SecOps/公开暂存，TT4续既有中文提交/PR379/准确head双平台CI/main-tag2.1.1/来源载荷/获准原生及issue链。用户明确授权自主执行，不复问常规审批，不增工具/凭据或本机app，不全量本机构建。旧Release/tag/证据不覆写，不直推main，不凭mock关闭code9/物理/原生缺口；最终源码/规格摘要单列，状态只在status/journal。
