# Repository consolidation package / 仓库收敛执行包

## English

Immutable BP-REPO-20261118, created2026-11-18. Master spec `spec.md`, revision UI-2026-11-17-R3-GH, SHA256 `46cbb513917138c8f65bf2ba89e9fa779441cc5fb9c63a01869ad5ef01597aa0`; predecessor `docs/aegis/20261117-ui-visual-r4-build-package.md` SHA256 `19c647caafac45a711f47584dd297bc282095b0031ae63a5c8803d6b54f8e03b`. Covered UI-008 evidence/documentation and repository R7/R9/R11 directory and retention policy; no product-law or executable change. User explicitly requests project/documentation consolidation and temporary cleanup before the already-authorized GitHub release. Risk low for reversible documentation migration and reproducible owned temporary files; release remains high and separate. Root PM maintains lifecycle and verifies; existing delivery agent performs bounded document moves; tester owns generated-file inventory/cleanup; PE independently reviews plan/results. Existing approved role/model mapping remains.

Work order: D1 PM freezes current reports, accepts D01–D08 artifact contract and records current input hashes; D2 delivery creates one docs/README navigation, keeps source/build/user-guide paths, moves35 stage reports byte-identically to docs/archive/ui-20261117 and snapshots old docs/ui-verification before rewriting its current scope; D3 tester removes only classified untracked generated tmp/test, caches and OS metadata after identity/rebuild receipt, preserving raw evidence and vcpkg/build dependencies; D4 PM independently checks mapping, protected source/package hashes, current Markdown targets/anchors, bilingual text and diff hygiene. Publication resumes only after consolidation results and CI fixes are reviewed.

Artifact contract: D01 before inventory records path/type/size/SHA; D02 each of35 moves is one-to-one, no collision/missing/redirect duplicate, archive bytes equal source; old ui-verification snapshot equals original; D03 spec and all existing immutable docs/aegis paths/bytes unchanged, root4 stay local excluded, PM alone owns status/journal; D04 all1095 frozen build inputs and other user guides unchanged, no build; D05 current README/docs README/ui-verification/R4/report local Markdown file and anchor targets resolve; historical raw refs are classified instead of rewritten to fabricated evidence; D06 historical paths distinguish mapped report/current file/removed with receipt/unavailable before cleanup/external history; D07 new docs have English and Chinese sections in one file; D08 white-listed changes only, fresh git diff --check and final storage/app/process check. Actual validation uses deterministic per-file SHA comparison, migration-map reconstruction and a current Markdown-link walk, reviewed by PM before D2. Failed checks are corrected without changing the contract.

Rollback: migration-map restores original paths with identical bytes; existing tracked README/ui-verification are recoverable from Git and the new historical snapshot. No source, dependency cache, raw test evidence, private settings or unrelated apps are removed. Temporary receipts record recreation commands and hashes. Historical missing evidence stays missing, never reconstructed. This package is execution evidence only; mutable state belongs to status/journal. No new approval question is required under the direct autonomous instruction.

## 中文

不可变执行包BP-REPO-20261118，创建2026-11-18，绑定上文唯一主规格/前序包完整身份。覆盖UI-008文档证据及仓库R7/R9/R11目录保留规则；无产品规则或可执行变更。用户明确要求先整理整合项目/文档、清临时再续GitHub交付。本次可恢复文档迁移/自有可再生成临时文件为低风险；公开发布仍单独高风险。

沿用既有角色映射，PM独占生命周期并独立核验；文档代理按有界表移动；测试代理清点/处理生成文件；PE审计划/结果。D1冻结报告并接受D01–D08，D2统一文档入口、35阶段报告原字节归档/旧验证说明快照后重写现行说明，D3仅处理已分类未跟踪临时项，D4检查映射/受保护摘要/当前Markdown文件与锚/双语/差异/单app空间。无构建，无源码迁移，不改用户指南既有路径和不可变包。

验证契约逐项与英文对应：迁移前身份，一一迁移同摘要，无缺失/覆盖/重复壳；主规格/既有包与1095构建输入不变；四根本地记录继续排除Git；当前本地链接可达；旧证据路径准确分类；新文档单文件中英；白名单差异与最后磁盘/app/进程核查。用逐文件SHA、集中mapping重建和当前Markdown链接遍历实际验证，失败修交付不改契约。

回滚依集中映射可原字节移回，README/旧验证说明有Git及快照。保留源码、依赖缓存、原始证据、隐私设置及无关app；临时项留摘要和重建方式。此前缺失原件不伪造重建。该包仅是执行证据，状态在status/journal，遵循用户自主授权不重复提问。
