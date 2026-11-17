# Native UI reconstruction / 原生界面重制交付

## English

Delivered 2026-11-17: the existing Qt desktop application now follows the supplied compact card design, with real settings and sharing-engine bindings preserved. Source is an uncommitted local change on `refactor/project-ui-cpp`, based on `a2793eff1f9b8d51900f2b47270ab346f2fbab4d`. Issue: https://github.com/Tupig/TupigSynergy/issues/380 (open for user inspection and later code integration).

### Result

The main shell, offline line icons, same-window navigation and editor insets are unified. Server layout uses rounded named tiles while preserving drag/drop and model behavior; computer settings use four tabs. Preferences have180px short fields and dropdowns bound to existing settings transactions. Fingerprint cards show full SHA-256, explicit trust/reject and copy actions; rejection remains the default. Real update/check/download/cancel states and compact About/license pages are connected to existing behavior. Live logs follow the tail, pause for manual history reading and resume at the bottom.

### Verification

| Check | Fresh result | Scope/limit |
| --- | --- | --- |
| `scripts/build.sh release` | Passed | macOS arm64 manifest, Qt6.11.2, OpenSSL3.6.5; cached dependency artifacts reused |
| Full final CTest |67 passed,0 failed;72.16s |69 registered;2 interactive targets Disabled |
| NativeDesignTests |61 passed,0 failed,0 skipped |600px shell/editor geometry, English/Chinese, palette variations, transactions, hashes, update cancellation, log follow/pause/resume |
| Final capture-only rerun |1 target passed;61 cases passed |Test fixture waits for nested geometry after clear/reappend; no product/package change |
| Chinese catalogue check |410 sources covered |Existing project checker |
| Encoding check |881 files,0 violations |UTF-8 policy |
| DMG payload |53 entries match staged payload |Read-only independent extraction; paths, bytes, modes and symlinks compared |
| Architecture/dependencies |2 arm64 Mach-O images,0 malformed |GUI/core and actual dependency closure audited from extracted payload |
| Signature/container |Strict deep ad-hoc verification and DMG checksum passed |Local signature; not Developer ID notarized |
| Isolated GUI/core help startup |Both exit0; only package/OS loader paths |Cocoa/loader startup; no actual core sharing connection or hardware acceptance |

`OSXClipboardTests` and `OSXKeyStateTests` remain Disabled/Not Run and are excluded from passes. Rendering uses isolated synthetic data, temporary settings and native Qt widgets; it never injects host input or writes the live clipboard. Actual OS appearance switching was not performed; light captures exercise the application palette and may retain native Cocoa dark control styling.

Historical evidence is retained: the first About check rejected the existing build-date suffix and was corrected under approved V07-R2; the log-tail regression failed before the product fix (scroll0 vs148) and passed after it. The first DMG was superseded and is not the delivered file. Loader validation was corrected to accept localized help rather than require the English word Usage. These earlier failures are not counted as passing runs.

### Deliverables and identity

- DMG: `build/ui-delivery-final/synergy_2.1.7_mac_arm64.dmg` (50,488,571 bytes).
- Complete app: `build/ui-delivery-final/TuPig Synergy.app`; the DMG-extracted audited copy is under `build/ui-delivery-final/extracted/`.
- DMG SHA-256: `5d1e66c407149e788f883bb93ff4a9b331af575c4b4442130719caed1b29053f`; checksum file `build/ui-delivery-final/SHA256SUMS`.
- Source manifest: `build/ui-delivery-final/verification/source-manifest.json`; SHA-256 `e4ffc8003a556033cf53773a6b47ce9a6d43fcd8fa1261c17b34c7ed00f71e52` (1104 source input entries).
- Raw build/test/package/loader and initial failure records: `build/ui-delivery-final/verification/`.
-108 native capture previews: `build/ui-delivery-final/previews/`; main hero `main-zh_CN-dark-server-reference.png`. Captures show synthetic rendering fixtures.
- Detailed role reports: `20261117-ui-shell.md`, `20261117-ui-pages.md`, `20261117-ui-tester.md` in this directory.

Bounded PE/SecOps source review found no suspected secret/PII leak in the affected GUI/test/design scope; it is not a whole-repository or live-settings privacy certification. TLS trust, download origin/digest enforcement, protocol, core/daemon separation and dependency policy are unchanged. No commit, push, PR, installed app replacement or public release was performed. Windows/Linux native builds, actual multi-computer sharing and hardware/permission acceptance remain unverified.

## 中文

2026-11-17 本机交付：现有 Qt 桌面应用已按设计稿统一为紧凑卡片界面，保留真实设置和键鼠共享引擎的接线。源码位于 `refactor/project-ui-cpp` 分支，基于 `a2793eff1f9b8d51900f2b47270ab346f2fbab4d`，保留为未提交的本地差异。问题追踪：[界面重制 #380](https://github.com/Tupig/TupigSynergy/issues/380)，等待用户查看与后续代码整合。

### 实现结果

主窗口、离线线性图标、同窗返回及页面留白已统一。服务端使用名称可读的圆角屏幕卡，保留拖放与模型行为；电脑设置改为四分页。首选项短字段180px，启动/关闭下拉框接回原设置事务。指纹卡默认显示完整 SHA-256，提供复制与显式信任/拒绝，拒绝仍是默认。更新检查、下载、取消使用真实状态；关于页采用紧凑标题并保留版本与完整许可。日志新增自动跟随尾部、向上阅读暂停、回到底部恢复。

### 已执行验证

- 本机 arm64 Release 入口构建通过，使用固定 vcpkg manifest（Qt6.11.2 / OpenSSL3.6.5），复用了已有依赖缓存。
- 最终完整 CTest：67个启用目标通过、0失败，耗时72.16秒；共注册69项，2项交互测试禁用。禁用项为 `OSXClipboardTests` 和 `OSXKeyStateTests`，按 Disabled/Not Run 单列，不计通过。
- 新增 NativeDesignTests：61个用例通过、0失败、0跳过，覆盖600px主壳与全部子页、中文/英文、深浅配色几何、保存/取消、完整指纹、下载取消及日志跟随。最后仅改渲染夹具的布局等待，再单独复跑该目标，61项继续通过；产品与已验证安装包未变。
- 中文目录检查覆盖410个源码字符串；编码检查881文件、0违规；差异空白检查通过，仓库自有文件没有新增或发现 `.ps1`。
- DMG只读挂载、独立解包，53项负载的路径、字节、模式及符号链接与已审查暂存包一致。两个 arm64 Mach-O 和运行依赖闭包检查通过，随包提供31项第三方许可及应用/Qt翻译。
- 临时签名深度严格校验和DMG校验通过；GUI/core隔离help加载启动均返回0，仅使用包内与操作系统加载路径。此为Cocoa/加载器启动证据，不代表core真实连接或硬件验收。

原始失败保留：首轮关于页检查错误排除了旧有构建日期后缀，按V07-R2修正检查；日志跟随回归修复前实际为0、预期148，修复后转绿；首个DMG已被最终包替代。help检查曾错误要求英文Usage，后按稳定参数与退出码验证本地化输出。未将这些历史失败当作通过证据。

### 文件与证据

最终DMG：`build/ui-delivery-final/synergy_2.1.7_mac_arm64.dmg`，50,488,571字节，SHA-256 `5d1e66c407149e788f883bb93ff4a9b331af575c4b4442130719caed1b29053f`。完整app、DMG解包审查副本、校验文件和108张原生截图均在同一交付目录；上文给出具体子路径。主预览为 `previews/main-zh_CN-dark-server-reference.png`，使用安全的合成设备/IP/日志。

确定性源码输入清单共有1104项，位于 `verification/source-manifest.json`，摘要 `e4ffc8003a556033cf53773a6b47ce9a6d43fcd8fa1261c17b34c7ed00f71e52`。构建、最终全量测试、夹具复测、打包、原始失败、解包对比和加载器证据集中在 `verification/`。三个角色报告位于本报告同目录。

安全评审范围限受影响GUI/测试/设计稿源码，未发现疑似秘密或PII泄漏，不代表全仓或实时配置隐私认证。保留TLS信任、更新来源/摘要验证、协议、core/daemon分离及依赖策略。没有提交、推送、PR、替换已安装应用或正式发布。没有切换系统外观，浅色截图只证明应用配色与几何；Windows/Linux原生编译、真实双机共享及硬件权限仍待对应环境验证。本机包使用临时签名，未公证。
