# Native UI R2 local delivery / 原生界面R2本机交付

## English

R2 corrects the previous delivery after the user reported excessive whitespace, incomplete adaptation, unchanged menus and incomplete subpages. The native Qt shell and every existing configuration page now use compact cards and natural current-content height; the existing sharing engine remains authoritative. This report supersedes the R1 completion claim for the corrected UI scope. R1 artifacts and failed intermediate evidence are retained.

Local reconstruction and verification are complete. The corrected app and DMG are preserved under build/ui-delivery-r2; user visual acceptance and later integration remain separate.

Implemented behavior:
- Main window starts at600px and supports540/600/820px. Current pages/tabs/action types/list rows determine height; short pages and nested Back shrink. Body overflow alone scrolls, capped at660px and available screen height. Adjacent actions stay within24px of visible content.
- Native menu roles and shared actions cover Application/File/View/Support on macOS and File/Edit/View/Help models on other platforms. Start, Restart and Stop have distinct shortcuts and state; tray model contains Open, Start, Restart, Stop and Quit with the required separators. Cocoa places its native roles and system items; no duplicate carrier menu is shown.
- Preferences, server/client, computer, Hotkey, all8 Action types, About/notices, fingerprint, message and update pages were recomposed. Empty lists occupy a row and grow to at most5 visible rows. Hotkey recording supports mouse, standard accessible click and Space/Return/Enter activation, announces capture, disables Save and restores the latest sequence on Esc/focus loss. Activation keys do not become the captured shortcut.
- Ordinary TLS key-length edits remain drafts until Save; Reset/Cancel preserve storage. Explicit certificate regeneration writes the saved certificate target and commits only matching key-size metadata after success, with the original minimum preserved; failure does not commit. Provider, validation, trust defaults, updater origin/digest checks, core/daemon and platform backends have no changes.
- Validation found and fixed a list-observer teardown crash, empty error rows, first-polish list-height mismatch, default Log Level case mismatch, recipient All/Specified serialization and native checkbox optical-layout text overlap. Failed evidence is preserved instead of counted as passing.

Fresh executed evidence:

| Check | Result | Evidence |
|---|---|---|
| Release entrypoint, manifest arm64-osx | Build complete | build/r2-test-evidence/release-build-r8-accessibility-final.log |
| Complete R2 acceptance |87 passed,0 failed,0 skipped | full-safe-ctest-details-r3-final.log |
| Final safe CTest |70 registered;68 passed,0 failed;2 Disabled;78.03s | full-safe-ctest-r3-accessibility-final.log |
| Existing NativeDesign / EditorTransaction |61 /12 cases passed in safe suite | detailed Qt results |
| Rendered matrix |162 PNGs;27 states ×2 languages ×3 widths | screenshots/dimensions.json |
| Independent visual review |All162 reviewed; changed Settings/Client images reviewed again | PM indexed review sheets and journal.md |
| Client text gap |9px in both rows/all6 combinations | final R2 raw log and Client PNGs |
| Certificate internal row gap |<=16px in all6 combinations | final R2 matrix |
| Chinese catalogue |434 source strings covered | project translation checker |

Interactive OSXClipboardTests and OSXKeyStateTests are Disabled/Not Run and excluded from passes. Tests use isolated temporary settings and certificates, hidden widgets and synthetic input events; no live clipboard or actual sharing is exercised. The optional test-only native preview uses the production MainWindow/gui/controllers/resources with temporary settings, a unique IPC endpoint and an independent witness. It omits production entry migration hooks and substitutes only the migration-notice callback to avoid daily settings; these differences are explicit and do not establish production-entry migration acceptance.

Native screenshot capture is unavailable in the enabled CUA surface; no permission was requested or bypass used. Actual native AX observations and Qt render PNGs are separate evidence. Physical status-item activation, Windows/Linux native builds, a real core connection and hardware/permission acceptance remain outside the local acceptance. The local package is ad-hoc signed and not notarized. The exact delivered GUI entry is not executed because its unchanged startup migration targets daily stores; native GUI/controller startup was observed through the separately identified safe preview. The delivered core help-loader was executed and exited0 with975loaded images confined to payload/OS paths. No installed application replacement, commit, push, PR or public release is part of this delivery. Issue#380 remains the integration/user-inspection tracker. Physical tray activation and its test popup were not observed; their actual shared QAction model/order/state is covered by the automated checks. Native AX did not expose the recorder OK button, so its Save gating is asserted by the real hidden-widget tests; actual AX recording and real Escape restoration were observed.


Local artifacts and package evidence:
- DMG: `build/ui-delivery-r2/synergy_2.1.7_mac_arm64.dmg`,50,501,995bytes,SHA-256 `875359da4a1b71e05935595089a4a1426f09e1a636e01f00952b697e11312129`.
- App: `build/ui-delivery-r2/TuPig Synergy.app`,version2.1.7,arm64; GUI and core have system-only dynamic dependencies. Strict deep ad-hoc signature verification passed for staged and extracted copies;31third-party notices and app/Qt catalogs are included. The test preview is absent from delivery.
- Original DMG checksum passed. Read-only mounting exposed an SLA prompt, which was cancelled without agreement. A disposable byte-preserving UDTO audit container was converted, mounted read-only/no-browse/no-autoopen, extracted and detached. All91paths (53files/symlinks) match staged bytes, modes and link targets exactly; the original DMG hash did not change. Audit derivation is explicit in `verification/extraction-receipt.json`.
- Build-input manifest:1091entries,rehashed with0mismatches; SHA-256 `38c34a27fe8e3e4740d45550a1baf99150712d02870269a5a14858cb40467129`. Mutable reports/status are covered by the final successor source/authority inventory. Foreign es/it/ko/ru generated-only TS changes were restored to HEAD, QMs regenerated directly and packaged hashes verified; zh_CN/ja additions retained.
- Final static checks:434Chinese source strings,891encoding-checked files/0violations, clean diff whitespace, no repository-owned .ps1 files. Local DMG assembly skips only Finder cosmetic positioning to avoid AppleScript UI automation; production packaging sources are unchanged.

## 中文

R2针对用户退回的“留白过多、适配不完整、菜单未改、子界面未改”完成整改。原生Qt主窗口与全部现有配置页均按当前内容自适应为紧凑卡片布局，继续使用原共享引擎。本报告在修正范围内替代R1的完成主张；旧产物及中间失败证据完整保留。

本机重制与验证已完成，修正版app和DMG保存在build/ui-delivery-r2；用户查看验收与后续代码整合另行区分。

主要行为：
- 默认600px，验证540/600/820宽度。窗口随当前页、分页、动作类型和列表行数取高，短页及嵌套返回会收缩；仅超限内容滚动，正文不超过660px和屏幕可用高度，操作按钮紧随内容，间距不超过24px。
- macOS真正使用应用/文件/视图/支持菜单角色，其他平台模型为文件/编辑/视图/帮助。启动/重启/停止快捷键各不相同并随状态启用；托盘模型为打开、启动、重启、停止、退出及规定分隔。Cocoa保留系统角色排序和系统项，没有重复载体菜单。
- 首选项、服务端/客户端、电脑、快捷键、8种动作、关于/许可、指纹、消息、更新页均真实重排。空列表只占一行，非空最多显示5行；录制支持鼠标、标准辅助功能点击、空格/回车/小键盘回车激活，显示状态并禁用保存，Esc或失焦恢复最近组合；激活键本身不会成为组合。
- TLS密钥长度普通修改作为草稿，保存才提交，重置/取消不改存储。显式重新生成使用已保存证书目标，仅成功后提交匹配位数元数据，保留原最小值；失败不提交。未修改provider、证书验证、默认拒绝、更新来源/摘要、core/daemon或平台后端。
- 实测修复列表观察者析构崩溃、空错误提示占行、首次字体取高差异、日志级别大小写导致重置后仍显示修改、指定电脑被改为所有电脑，以及原生checkbox光学布局造成文字挤连。失败日志保留，不计为通过。

新执行证据与英文表格逐项一致：发布入口构建完成；R2完整检查87通过、0失败、0跳过；最终安全CTest注册70、实际68通过、0失败，另2项Disabled，78.03秒；原NativeDesign61例、EditorTransaction12例通过；中英文×3种宽度×27种页/分页/动作状态共162张PNG，已逐张看图并复看修改后的设置与客户端页。客户端两行文字距离六组均9px，证书内部行距六组均<=16px，中文目录覆盖434个源码字符串。

OSXClipboardTests和OSXKeyStateTests保持Disabled/Not Run，不计通过。自动测试使用临时设置/证书和隐藏控件的合成事件，不操作实时剪贴板或实际共享。原生测试预览使用同一生产MainWindow/gui/控制器/资源，通过临时设置、独立实例IPC及见证端点隔离；仅省略生产入口迁移钩子和替换迁移提示回调，避免碰日常设置。该差异明确记录，不能据此宣称生产迁移入口已运行验收。

当前CUA原生屏幕截图不可用，未申请权限或绕过；原生AX观察与Qt实际渲染PNG分别记录。物理状态栏入口、Windows/Linux本机构建、真实core连接及硬件权限验收仍在本机证据边界外。交付包为本机临时签名、未经公证。为避免原生产启动迁移访问日常存储，未运行交付GUI原入口；原生GUI/控制器启动使用另有身份的安全预览。已执行交付core的help加载，exit0，975个加载映像全部位于包内或OS系统路径；本次不替换已安装应用，不提交、推送、创建PR或正式发布。#380继续跟踪后续整合与用户查看。物理托盘与测试弹出均未被当前CUA观察到，真实共享动作模型/顺序/状态由自动检查证明；原生AX未暴露录制页OK按钮，保存门禁由真实隐藏控件检查证明，原生起录及真实Esc恢复已观察到。

最终产物与包证据：
- DMG：`build/ui-delivery-r2/synergy_2.1.7_mac_arm64.dmg`，50,501,995字节，SHA-256为`875359da4a1b71e05935595089a4a1426f09e1a636e01f00952b697e11312129`。
- App：`build/ui-delivery-r2/TuPig Synergy.app`，版本2.1.7、arm64。GUI/core动态依赖仅系统库；已审载荷及解包副本的严格深层临时签名校验通过；随包包含31份第三方声明及应用/Qt翻译，无测试预览程序。
- 原DMG校验通过。直接只读挂载出现SLA提示时取消，未代用户接受协议；转为一次性保留载荷字节的UDTO审计容器，只读/不浏览/不自动打开挂载、提取后卸载。91条路径（其中53文件/链接）的字节、权限、链接目标与暂存包逐项相同，原DMG摘要不变。派生容器边界有明确回执。
- 构建输入1091条重新核验0不一致，清单摘要`38c34a27fe8e3e4740d45550a1baf99150712d02870269a5a14858cb40467129`。可变报告/状态纳入最终后继源码/权威清单；四种无关语言仅生成噪声已恢复HEAD、直接重编QM并核包内摘要，中日新增译文保留。
- 最终静态检查：中文434源串，编码891文件/0违规，差异空白检查通过，仓库自有文件无.ps1。本机打包只跳过Finder图标位置美化，避免AppleScript界面自动化；生产打包源码未改。
