# R3 correction build package / R3修正构建包

## English

Immutable BP-UI-20261117-R3, created2026-11-17. Sole master spec spec.md revision UI-2026-11-17-R3 SHA-256 a6472b99194b8a991dd9c63ea618d9b202502a4d597ef428fd5b883de47a1784. Predecessor docs/aegis/20261117-ui-r2-build-package.md remains historical. Covered UI-001..015; changes UI-013..015, contract C-UI-R3 R3-V01..05. Standard risk; bounded GUI/runtime testability and local package only. Full delegation and explicit startup/options/motion feedback authorize specification amendment and G4 local execution; no installation, permissions, host sharing, commit/push or publication.

Pre-code PE: reports 20261117-ui-r3-startup-review.md, 20261117-ui-r3-pages-review.md and 20261117-ui-r3-tester.md under docs/reports. Confirmed action ID/index corruption; reported startup identity unknown. No speculative startup cure. Same real main source/all hooks with test-only bootstrap is not a byte-identical shipped executable test.

W1 independent tester (r2_tester): finalize planned contracts then create actual checks under src/unittests and test CMake. R3-V01 temporary RuntimeContext before QApplication; real hooks and migration; fresh/current/legacy temporary INI fixtures, phase/path/IPC receipts, first event loop, clean quit, safety autoCore/update override after migration. R3-V02 all stable enum data+legacy/unknown preservation+invalid serialization with existing sentinel file preserved. R3-V03 real popup activation/save/cancel/reopen, explicit recipients/system language and modifier mapping. R3-V04 actual nonfinal render frames, completion/rapid replacement/resize/destruction and reduced motion; exactly-once standard activation. Preserve tests/evidence; product edit starts only after independent adequacy approval.

W2 host/runtime coder (r2_host), depends W1 adequacy: internal common/RuntimeContext paths/factory/identities with one-time fail-closed test DI; Settings accessors and QSettingsProxy, migration/scope/TestSettings/FeatureHandler/hooks path callsites; production default formulas/algorithms unchanged. Test-only entry activation/milestones in synergy-gui.cpp; tester owns target. Motion in AppStyle.*, PageHost.* and bounded helper/native platform files+CMake, pure paint/opacity, central native reduced-motion policy; no layout/focus/business delay. Exit R3-V01/V04, current tests and independent source review.

W3 pages/config coder (r2_pages), depends W1 adequacy: ActionDialog/Design, SettingsDialog/Design, ScreenSettingsDialog/Design and other audited selectors use stable itemData. Action.* validation/safe summary; ServerConfig.* prevalidation/atomic save; only GUI CoreProcess persistServerConfig delegates safe save(path), unchanged external config branch. No engine/parser/protocol modification. Unknown data preserved/read-only, no fallback commands or silent coercion. Translation additions zh_CN/ja as needed; all other retained translations preserved. Exit R3-V02/V03 and independent review.

W4 tester, depends W2/W3+PE: project scripts/build.sh release with selected Xcode, manifest dependencies and interactive OFF. Run new contracts and full safe CTest; record Disabled separately. CUA sole owner, observe actual popup and native activation in isolated real-entry test build; no shell UI automation. Inspect affected bilingual widths540/600/820 and motion frames; preserve old R2 tests, update positional oracles only through R3 semantic lineage. Assemble build/ui-delivery-r3 successor app/DMG, exact staged/extracted manifests, Mach-O architecture/dependencies, license/catalog/signature and isolated core loader audit. Original R1/R2 packages unchanged; no SLA acceptance or HOME override.

W5 PM: independently inspect actual checks before source; review source/test/render/native/payload raw evidence and PII-scoped SecOps findings, update open issue380, bilingual delivery/status/journal and successor input/source/authority inventories. All todos reflect actual outcomes; unknown user startup and untested platform/hardware paths remain explicit. Role mapping continues authorized available gpt-6.1-sol high; no new roles/models. PM non-implementing.

## 中文

不可变BP-UI-20261117-R3，绑定唯一spec.md修订R3及摘要a6472b99194b8a991dd9c63ea618d9b202502a4d597ef428fd5b883de47a1784；前序R2不可变包保留。覆盖UI-001..015，修正UI-013..015，契约R3-V01..05，standard风险。用户全权委托及具体启动/菜单/动效反馈授权本机执行与规格修订，不含安装、授予权限、真实共享、提交推送或公开发布。

W1独立tester先写实际检查，PM独立审核后W2/W3产品修改。W2负责一次性内部测试路径上下文、真实main全部hooks/迁移算法与独立IPC，以及纯绘制的页面/分页/选项/按钮过渡和减少动态效果策略；W3负责稳定选项语义、未知旧值只读无损、摘要安全和导出前验证/原子保存。GUI CoreProcess只委托同一安全写出，不改core协议。W4唯一构建/CUA角色做新契约、原安全CTest、中英文540/600/820渲染、真实弹层激活及后继app/DMG审计；W5 PM独立核证据、同步380及最终清单。英文逐项定义文件/接口边界、前置和退出条件。

启动测试是同一生产main源码加测试专用引导，不冒称交付二进制字节相同；迁移夹具临时INI不冒称原生后端验收。实际用户入口仍未知，不能臆测已修好其崩溃。所有旧产物/失败证据保留，真实硬件和未测平台不计通过。
