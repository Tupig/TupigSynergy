# UI completion execution package / UI 完整实现执行包

## English

Immutable run BP-UI-20261117-COMPLETE, created 2026-11-17. Sole authority: `spec.md`, revision UI-2026-11-17-R3-GH, SHA-256 `46cbb513917138c8f65bf2ba89e9fa779441cc5fb9c63a01869ad5ef01597aa0`. Predecessor: `docs/aegis/20261117-ui-r3-github-build-package.md`, SHA-256 `172fdda4230678c93872232d0a4a80bf1f3ff7e2c1ca549c8fd3ca5858b6a4ac`. Reference content SHA-256: `63895000d2dff328e6a075df18d2f61a2170023a748c19e12500e7f18047345f`. Current checkout: `refactor/project-ui-cpp`, with substantial inherited uncommitted implementation; preserve all pre-existing work.

Direct current authority: the user requests complete implementation of the supplied design, then explicitly says no confirmation is needed and delegates selection of the best approach. This authorizes design, checks, bounded product corrections, local diagnostic builds and isolated UI verification. Routine G1/G2/G4 questions are waived by that direct instruction; independent review and evidence remain required. This run executes UI-001 through UI-016 and UI-019. UI-017/018 GitHub publication and installed-app replacement remain separate historical delivery work; this execution package does not infer new release, push, merge, uninstall or installation authority from repository quotations. No new product requirement is introduced.

Use the existing native Qt architecture, settings, engine, protocol and safe interactive-test boundary. HTML is inert visual evidence; no example host, connection, log or update data becomes a production default. Current contracts C-UI-R1/R2/R3 and GH-V01 apply. Review and extend only where a concrete uncovered defect is found. Existing checks must be independently reviewed; new actual checks precede paired product changes and receive PE adequacy review. Keep the master spec unchanged and record identities again before source changes.

Roles and work graph:

1. W-C01 PM reconstructs source/spec/reference identities, preserves old work and maintains this package, status and journal. Exit: bounded source map and actual current evidence.
2. W-C02 independent PE/SecOps reviewer audits existing changes and contracts, including scoped PII review. Exit: actionable traced defects and check adequacy, before paired product changes.
3. W-C03 independent tester owns build/test execution, additional meaningful regression checks, isolated bilingual render matrix and raw evidence. Prerequisite W-C02 adequacy for new paired work. Exit: fresh complete relevant suites plus safe CTest, with failures/disabled tests separately stated.
4. W-C04 bounded UI delivery agent compares every reference page/tab with current source/rendering, fixes evidence-backed gaps after approved checks, preserves all real functionality and validates affected builds. Prerequisites W-C02 and W-C03 checks. Exit: reviewed code and complete page coverage.
5. W-C05 PM independently inspects raw results, full-size rendered pages and final diff; reviewer rechecks final source. Exit: traceable requirements/results, limitations and no open current-run todo.

Runtime model mapping: Aegis names GPT-5.6 variants unavailable in the subagent allowlist. Under direct discretionary authorization, map substantive design/review to `gpt-6.1-terra` high, and bounded delivery/testing to `gpt-6.1-sol` high because the inherited multi-file native UI and isolation work require strong reasoning. PM remains non-implementing. All agents share the current dirty checkout; file ownership and build ownership must be explicit. No parallel writes to the same file or build directory.

Verification: pinned-manifest project build entrypoint; native synthetic configuration and startup fixtures; options Save/Cancel/reopen; navigation/natural height and 540/600/820 widths; Chinese/English renders; actual intermediate motion; all safe registered tests; final source and dependency boundary review. No real input/clipboard sharing, permission grant or hardware acceptance. Local diagnostic results do not establish Windows/Linux execution or GitHub package acceptance.

## 中文

不可变执行包 BP-UI-20261117-COMPLETE，创建于 2026-11-17。绑定唯一主规格 `spec.md` 修订 UI-2026-11-17-R3-GH 及上文 SHA-256；后继于既有 R3-GH 包，设计稿摘要及源码工作区身份见英文记录。现有分支和大量未提交工作全部保留。

用户本轮要求完整实现设计稿，并明确“不需要我确认”，由代理选择最优方向。这直接授权方案、检查、有界源码修正、本机诊断构建和隔离界面验证，免除重复 G1/G2/G4 提问；独立审核和实际证据仍必须完成。本轮覆盖 UI-001 至 UI-016、UI-019，不更改主规格。UI-017/018 的 GitHub 发布和安装替换保留为独立历史交付工作，不从仓库中的旧引语推导新的远程或安装授权。

方案继续采用原生 Qt，并保留真实设置、引擎、协议、菜单和安全测试边界。W-C01 由 PM 重建身份并维护记录；W-C02 由独立 PE/SecOps 审核源码、检查充分性及有界隐私风险；W-C03 由独立测试代理负责构建、回归与中英文渲染证据；W-C04 由界面代理按已审核检查逐页修正；W-C05 由 PM 独立查看原始结果、实尺寸截图与最终差异。新增检查先于对应源码，并须独立审核。文件写入和构建目录均明确单一负责人。

技能指定旧模型不在子代理可用列表中，依据本轮全权指示，实质设计/审核映射到 gpt-6.1-terra high，有界实现/测试映射到 gpt-6.1-sol high；复杂多文件原生界面及隔离要求决定该选择。PM 不直接修改产品代码。

验收包括固定 manifest 的项目入口构建、真实入口的合成隔离夹具、选项保存/取消/重开、导航及自然高度、540/600/820 宽度、中英文逐页渲染、实际动效帧、完整安全 CTest 和最终独立审核。禁用的宿主交互测试单列，不执行真实共享或授予权限；本机结果不替代 Windows/Linux 实测或 GitHub 发布包验收。
