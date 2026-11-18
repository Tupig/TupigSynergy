# VISUAL-R4 delivery diagnosis / 交付视觉诊断

## English

### Scope and evidence

This is a read-only diagnosis record, not a product change or acceptance report.
Reference: `/private/tmp/tupig-ui-full.html`, especially its CSS and page functions
`pageMain` through `pageMenus`. Compared with current `src/lib/gui` source and existing
renders in `build/ui-completion-evidence/final-main-language/renders-r2/`.
No new build, test, application launch, installation, computer control or remote write
was performed for this diagnosis. Daily settings were not read or changed.

The running PID 19168 started at 09:35:55 on 2026-11-17; its executable on disk was
modified at 16:41:20 +0800. The running image therefore predates the latest disk build.
Its old native TLS group title and tab strip cannot establish a defect in current source.
The process was not controlled or stopped.

Current production startup already applies the global font, `appStyleSheet()` and
button feedback (`src/apps/synergy-gui/synergy-gui.cpp:91`). Hooks do not remove this
stylesheet. `PageHost` currently configures document-mode tabs without a base line.
Previous functional passes remain historical functional evidence; they do not establish
visual equivalence or current production runtime identity.

### Difference map

| Surface | Reference and current difference | Bounded correction / evidence limit |
| --- | --- | --- |
| Shared cards | `CompactLayout::normalizeCards` converts every legacy nonempty title into an in-flow heading, including titles absent in the reference. | Opt in to meaningful headings; remove redundant titles per page, not globally. |
| Shared lists and hints | Native square black list surfaces lack reference rounding, padding and icons. Many hints are ordinary labels; muted styling uses 11px versus reference 11.5px. | Style real lists and hints; retain real row data and readable empty states. |
| Shared tokens | Selected tab uses transparent accent-soft instead of solid `#31443a`. Button/input border-box height and status-bar paint/background may differ. | Require actual geometry/pixel RED evidence before changing sizes or paint. Reference button/input outer heights are 32/30px; status background is `#232326`. |
| Main | Overall identity, mode card and log order are close. Fixture shows Edit/Save/Cancel together; expanded-fit log clips the first row. Extra server save icon has no reference equivalent. | Verify actual editing/nonediting and log scroll states first. Keep real save and core-state behavior; move contextual controls only with evidence. |
| Preferences: General, Window, Log, Network, Advanced | Extra legacy headings. Long English Window labels shift columns. Log warning is not subtle. Certificate actions use a vertical layout. Command inputs are narrow and lack 24px indent. Non-Windows service explanation is absent. | Remove only redundant headings; use 120px label / 180px field geometry where appropriate; allow long paths to expand. Use a wrapping certificate-action row and full-width command option blocks. Retain Reset/Defaults and existing command/service bindings. |
| Server: Layout | Accessible Add/Delete coexist with visible legacy trash/new-screen tools. | Keep drag and keyboard access while reducing duplicate visible controls. |
| Server: Hotkeys | Native hotkey/action list presentation differs. | Use rounded, padded, readable rows; retain edit/remove capabilities. |
| Server: Sharing | Four legacy section headings and two-column checkboxes make the page taller. Protocol uses radios. | Flatten and order according to reference; compact form, with stable protocol mapping if a combo is used. Preserve four corner flags and independent heartbeat/delay/double-tap enable flags. |
| Server: Config | Hint and Browse presentation differ; actions outside the card. | Move actions into this card, make hint subtle, retain editable path and browse behavior. Do not impose this rule on every Server tab. |
| Client | Two titled cards and external actions already match the reference structure. Retry range exists only in a tooltip. | Preserve external actions and Reset/Defaults; make the real retry range visible. Keep scroll scale values and direction semantics. |
| Computer: all four tabs | Redundant Info/Modifiers/Aliases/Dead Corners headings; actions outside tabs. Name uniqueness hint is absent. Alias list is native. | Actions belong in each reference card. Keep only the compatibility subsection heading. Retain real validation, aliases and modifiers; verify compact corner-size unit display. |
| Hotkey | Two cards and external actions instead of one card with two meaningful sections and internal actions. Raw action expressions are displayed. | One outer card; retain record control and real recording semantics, associated-action CRUD and draft behavior. Present real actions readably. |
| Action: all eight types | Redundant Action heading, separate recipient card, external actions and varying label columns. | One card with related fields and internal actions; actual 120/180 geometry. Keep stable IDs, unknown-value read-only protection and only supported All/Specified recipients. |
| Fingerprint | Local heading and saved source are outside the fingerprint card. Comparison warning is external to the hash cards. | Move local heading/source into its card; retain complete hash copy, 16-byte lines, source identity, image toggle and safe Reject/Trust behavior. Do not invent unavailable peer identity. Comparison needs fresh evidence. |
| Update: checking/latest/failure | Existing MessagePage has an internal action row, but icon/title/hint details differ. | Keep real cancellation and retry semantics; adjust only justified presentation. |
| Update: available/downloading | Summary, notes and progress are separate cards with external actions. Notes use a native text-edit surface. Download metrics occupy one line. | One state card with real metadata/notes/actions. Download progress, amount/percent, rate and ETA use distinct rows, preserving unknown-total and zero-rate behavior; never copy mock values. |
| About | Version and links/actions are split across cards; license card content differs. | First card contains identity/version/copy/home/contributors; second presents real license information and access to existing legal content. Keep external Close. |
| Five messages and menus | Existing evidence has a generic confirmation sample, not all five real message paths; no current menu rendering. | Supplement actual controlled entry checks. Do not copy illustrative deletion/permission/peer semantics. macOS menus remain native and follow menu roles. |

Observed renders cover all Preferences and Server tabs, Client, all Computer tabs,
Hotkey, all eight Action types, local Fingerprint, About, available Update, a generic
message, Main server/client and update checking/failure/download rate states.
Fingerprint comparison, all five real messages, latest-update state and native menu
appearance lack complete current render evidence. These are not visually accepted.

### Minimum delivery groups

1. Public tokens/list/hint styling, with measured geometry/pixels before size or
   status-bar corrections.
2. Preferences, Computer, Hotkey and Action structure; optional `CompactLayout`
   action attachment per page. Client keeps external actions. Preserve Reset/Defaults.
3. Server layout tools, Sharing form and Config card.
4. Update state composition, About card composition and Fingerprint card content.

For each group: tester-owned actual failing checks, independent adequacy review,
authorization, bounded product changes, then actual English/Chinese renders at
540/600/820 and appropriate entry/relationship checks. This report authorizes none
of those later steps and claims no new check results. Source paths and observations
must be remapped if later edits change the implementation.

## 中文

### 范围与证据

本文是只读诊断记录，不是产品改动或验收报告。参考为
`/private/tmp/tupig-ui-full.html` 的 CSS 及 `pageMain` 至 `pageMenus`。
对比对象为当前 `src/lib/gui` 源码及既有渲染目录
`build/ui-completion-evidence/final-main-language/renders-r2/`。
本次诊断未新跑构建、测试、启动、安装、电脑控制或远端写入，未读取或修改日常设置。

PID 19168 于 2026-11-17 09:35:55 启动，磁盘可执行文件于 16:41:20 +0800 更新。
因此运行映像早于最新磁盘构建。旧窗口的原生 TLS 分组标题与标签条不能直接证明当前源码仍有
同一缺陷。未控制或结束该进程。

当前正式启动链已应用全局字体、`appStyleSheet()` 与按钮反馈
（`src/apps/synergy-gui/synergy-gui.cpp:91`），hooks 不会移除样式。
`PageHost` 当前已设置无基线的 document-mode 标签。此前功能通过只是历史功能证据，
不能证明视觉等价，也不能证明用户当前运行的是最新程序。

### 差异映射

| 页面 | 参考与当前差异 | 最小修正及证据边界 |
| --- | --- | --- |
| 公共卡片 | `CompactLayout::normalizeCards` 无差别把非空旧标题转成卡片内 heading，包括参考没有的标题。 | 按页面保留有意义标题，去冗余标题，不能全局删除或新增。 |
| 公共列表与说明 | 列表仍是原生黑色直角表面，缺少圆角、行内边距及图标。多处说明使用普通标签；muted 为 11px，参考为 11.5px。 | 样式化真实列表和说明，保留真实数据及可读空态。 |
| 公共 token | 选中标签为透明 accent-soft，而参考为实色 `#31443a`。按钮/输入外高及状态栏绘制、背景可能不一致。 | 尺寸和绘制须先有实际几何/像素 RED 证据。参考按钮/输入外高 32/30px，状态栏背景 `#232326`。 |
| 主界面 | 身份、模式卡片、日志顺序接近。夹具同时显示 Edit/Save/Cancel；expanded-fit 日志首行裁切。服务端额外保存图标不见于参考。 | 先核验真实编辑/非编辑及日志滚动态，保留真实保存及 core 状态行为；有证据再调整上下文入口。 |
| 设置：通用、窗口、日志、网络、高级 | 多出旧标题；英文窗口标签过长推宽列；日志警告非 subtle；证书操作固定纵排；命令输入窄且缺少 24px 缩进；非 Windows 缺服务说明。 | 去冗余标题；适用处用实际 120px 标签 / 180px 字段，长路径仍可扩展。证书用可换行横排，命令用全宽 option 块。保留 Reset/Defaults 与真实绑定。 |
| 服务端：布局 | 新增/删除与旧垃圾桶、新屏幕工具重复可见。 | 保留拖拽、键盘入口，收敛重复视觉控件。 |
| 服务端：快捷键 | 快捷键和动作列表仍是原生表面。 | 圆角、行内边距及可读行内容，保留编辑/删除。 |
| 服务端：共享 | 四个旧分组标题、双列复选框比参考高；协议仍为两行 radio。 | 按参考排序平铺并使用紧凑表单；若换 combo，映射既有协议 ID。保留四角和独立心跳/延迟/双击启用语义。 |
| 服务端：配置 | 说明、浏览入口及外部操作区与参考不同。 | 此页操作进卡片，说明 subtle，保留路径与浏览。不把该规则套到所有服务端标签。 |
| 客户端 | 两张有标题卡片及外部操作区本就符合参考。重试范围仅在 tooltip。 | 保留外部操作及 Reset/Defaults；显示真实重试范围，保留滚动缩放与方向。 |
| 计算机四标签 | 信息/修饰键/别名/死角标题冗余，操作在 tabs 外；缺名称唯一性说明；别名是原生列表。 | 操作进入对应参考卡片，只保留兼容性子标题。保留真实验证、别名、修饰键，核验死角尺寸单位与宽度。 |
| 快捷键 | 两卡片加外部操作区，参考为含两个语义分节和内部操作区的单卡片；动作显示原始表达式。 | 合为一个外卡，保留真实录制控件、录制语义、动作增改删与草稿行为；可读呈现真实动作。 |
| 动作八类型 | 多出 Action 标题，接收对象另成卡片，操作在外，标签列随类型变化。 | 单卡片、相关字段、内部操作区及实际 120/180 几何。保留稳定 ID、未知值只读保护、仅 All/Specified 接收对象。 |
| 指纹 | 本机标题、已保存证书来源在卡片外；比较提示在哈希卡片外。 | 本机标题/来源进卡片，保留完整复制、每行 16 字节、来源身份、图像切换及安全拒绝/信任行为。不得编造未知对端身份；比较页待新证据。 |
| 更新：检查/最新/失败 | MessagePage 已有卡内操作，但图标、标题、说明细节不同。 | 保留真实取消、重试语义，仅修有依据的呈现。 |
| 更新：可用/下载 | 摘要、说明、进度拆卡，操作在外；说明为原生文本编辑表面，下载指标合成一行。 | 单状态卡含真实元数据/说明/操作；进度、数量/百分比、网速、剩余时间分行，保留未知总量及零速逻辑，不能复制模拟数据。 |
| 关于 | 版本与链接/操作拆卡，许可卡片内容不同。 | 首卡含名称/说明/版本/复制/主页/贡献者，次卡显示真实许可信息并保留原法律内容入口，关闭仍在外部。 |
| 五类消息及菜单 | 既有证据只有通用确认样板，非五条真实消息路径；无当前菜单渲染。 | 补受控真实入口检查，不能照抄示意删除/权限/对端语义。macOS 菜单保持原生及正确角色。 |

已观察渲染涵盖设置及服务端全部标签、客户端、计算机四标签、快捷键、动作八类型、
本机指纹、关于、可用更新、通用消息、主界面服务端/客户端，以及更新检查/失败/下载网速状态。
指纹比较、五类真实消息、已是最新状态、原生菜单外观仍缺完整当前渲染证据，不能算视觉通过。

### 最小交付分组

1. 公共 token、列表、说明；尺寸及状态栏先取得实际像素/几何证据。
2. 设置、计算机、快捷键、动作结构；`CompactLayout` 按页选择是否附加操作区。
   客户端保留外部操作，保留 Reset/Defaults。
3. 服务端布局工具、共享表单、配置卡片。
4. 更新状态组合、关于卡片归位、指纹卡片内容。

每组依次需要测试代理的实际失败检查、独立充分性审核、授权、有限产品改动，
再以英/中及 540/600/820 宽度渲染和相应真实入口/关系检查验证。
本文不授权这些后续步骤，不声称新增检查结果。后续实现变化时，须重新映射源码路径与观察。
