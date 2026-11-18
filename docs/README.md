# Documentation index / 文档索引

## English

This is the maintained documentation entry point. User guides keep their existing paths. The project layout stays `src/apps`, `src/lib`, `src/unittests`, `cmake`, `config`, `scripts`, `deploy`, `translations` and `triplets`; documentation consolidation does not relocate build inputs.

| Guide | Purpose |
|---|---|
| [Build](build.md) | Host prerequisites, manifest dependencies, platform builds and packaging |
| [Configuration](configuration.md) | GUI settings and core command examples |
| [Protocol](protocol.md) | Wire protocol reference |
| [Troubleshooting](troubleshooting.md) | Startup, services and operational diagnosis |
| [Security](security.md) | Security policy and reporting |
| [macOS signing](macos-signing.md) | Signing and Accessibility identity |
| [UI verification](ui-verification.md) | Current verification procedure and evidence boundaries |
| [Contributing](../.github/CONTRIBUTING.md) | Branches, review and commit conventions |

### Current reports

- [R4 local verification](reports/20261118-ui-visual-r4.md): current local implementation and bounded evidence, with native drag, distribution and hardware limits retained.
- [Initial space receipt](reports/20261118-ui-space-cleanup.md) and [per-item JSON](reports/20261118-ui-space-cleanup.json).
- [Repository consolidation receipt](reports/20261118-project-cleanup.md) and [machine-readable summary](reports/20261118-project-cleanup.json).

The exact final probe cleanup receipts are `build/ui-current-evidence-20261118/final-probe-cleanup-before.json` and `build/ui-current-evidence-20261118/final-probe-cleanup-after.json`. These paths take precedence over the frozen R4 report's older bare path under `native-final`; that report remains byte-identical during this consolidation. Local build evidence is ignored by Git and is not promised as part of the public repository.

### History and authority

The [migration map](archive/ui-20261117/migration-map.json) resolves all 35 old `docs/reports` paths to byte-identical originals in `docs/archive/ui-20261117`. It also identifies the [previous verification document](archive/ui-20261117/ui-verification-before-20261118.md) and the two generated inputs moved to the local build receipts. Archived text retains its historical names, hashes, failures and missing-evidence statements; an old report reference does not establish that a raw image or executable still exists. No per-report redirect copies are maintained.

`docs/aegis/` retains immutable execution records at their original paths. The root `spec.md`, `build-package.md`, `status.md` and `journal.md` remain local Aegis records, excluded through the existing local Git policy. Current lifecycle updates belong to the PM. The version source remains `cmake/Version.cmake`; documentation does not duplicate a current version number.

The application-output scope is `build/bin`: its remaining product bundle is `TuPig Synergy.app`. Qt's `Designer.app`, `pixeltool.app` and `Info.plist.app` templates inside dependency caches are dependency tools/templates, not copied project preview apps, and are retained. This is not a claim that the entire disk contains only one app bundle.

## 中文

这里是持续维护的文档入口。用户指南保持原路径；项目继续使用 `src/apps`、`src/lib`、`src/unittests`、`cmake`、`config`、`scripts`、`deploy`、`translations` 与 `triplets`，本次整理不搬动构建输入。

| 指南 | 用途 |
|---|---|
| [构建](build.md) | 宿主前置工具、清单依赖、分平台构建与打包 |
| [配置](configuration.md) | GUI 设置与 core 命令示例 |
| [协议](protocol.md) | 线协议参考 |
| [故障排查](troubleshooting.md) | 启动、服务与运行诊断 |
| [安全](security.md) | 安全策略与报告方式 |
| [macOS 签名](macos-signing.md) | 签名及辅助功能授权身份 |
| [界面验证](ui-verification.md) | 当前验证流程与证据边界 |
| [贡献约定](../.github/CONTRIBUTING.md) | 分支、审查与提交规范 |

### 当前报告

- [R4 本地验证总报告](reports/20261118-ui-visual-r4.md)：当前本地实现及有界证据，保留原生拖拽、分发包与硬件限制。
- [初次空间回执](reports/20261118-ui-space-cleanup.md)及[逐项 JSON](reports/20261118-ui-space-cleanup.json)。
- [仓库整理回执](reports/20261118-project-cleanup.md)及[机器可读汇总](reports/20261118-project-cleanup.json)。

最后探针清理的准确回执位于 `build/ui-current-evidence-20261118/final-probe-cleanup-before.json` 和 `build/ui-current-evidence-20261118/final-probe-cleanup-after.json`。这两条优先于冻结 R4 总报告中较早的 `native-final` 裸路径表述；本次整理保持该报告原字节不变。本地 build 证据被 Git 忽略，不宣称公开仓库包含这些原件。

### 历史与权威记录

[集中迁移映射](archive/ui-20261117/migration-map.json)将 35 条旧 `docs/reports` 路径对应到 `docs/archive/ui-20261117` 中的同字节原件，并记录[旧验证说明](archive/ui-20261117/ui-verification-before-20261118.md)和转入本地 build 回执目录的两份生成输入。归档正文保留历史文件名、摘要、失败及证据缺失陈述；旧报告引用不证明原始图片或可执行文件仍存在。不维护逐报告跳转副本。

`docs/aegis/` 的不可变执行记录保留原路径。根目录 `spec.md`、`build-package.md`、`status.md`、`journal.md` 仍是本地 Aegis 记录，沿用现有本地 Git 排除策略；当前生命周期由 PM 更新。版本唯一来源仍为 `cmake/Version.cmake`，文档不复制当前版本数字。

应用输出范围限定为 `build/bin`，其中保留的产品 bundle 是 `TuPig Synergy.app`。依赖缓存中的 Qt `Designer.app`、`pixeltool.app` 和 `Info.plist.app` 模板属于依赖工具或模板，不是项目复制的试验 app，继续保留。这不表示整块磁盘只有一个 app bundle。
