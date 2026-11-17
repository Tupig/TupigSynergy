# R3 page semantics and motion review / R3页面语义与动效审查

## English

Current scope: read-only source/reference investigation after reports of startup crash, mismatched subpage option behavior and absent click/navigation animation. No R3 product edit, build, native launch or computer-use action is authorized by this review. Implementation waits for the PM-bound R3 specification and independently adequate actual checks.

Live tasks:
- [completed] Locate current source/reference identities and enumerate real option bindings and semantics.
- [completed] Compare display order, item data, enums and Save/Cancel/reopen behavior against the supplied HTML while retaining supported backend modes.
- [completed] Review existing press/hover and PageHost/tab/selection transitions; propose bounded measurable motion with interruption/focus/size/reduced-motion safeguards.
- [completed] Verify the review against source and report evidence, limitations and the smallest implementation boundary to the PM.
- [completed] Review the newly supplied real Preferences Log screenshot against the HTML and define bounded structural/pixel checks.
- [completed] Align final serialization/selector APIs and independent test oracles; actual checks and source window approved.
- [in progress] Implement approved semantic selectors, fail-safe serialization and UI016 visual correction.
- [completed] Apply approved version2.1.1 consistency changes.
- [pending] Verify final source and independent runtime/serialization/results against GitHub-built delivery evidence; record limits and final status.

## 中文

当前范围：用户报告启动崩溃、子页选项表现与实际不一致、缺少点击/切换动画后的只读源码与参考稿调查。本审查不授权R3产品修改、构建、原生启动或电脑使用；实现须等待PM绑定R3主规格及独立充分的实际检查。

实时任务：
- [已完成] 定位当前源码/参考稿身份并清点真实选项绑定语义。
- [已完成] 对照HTML核查显示顺序、itemData、枚举及保存/取消/重开，保留后端支持模式。
- [已完成] 审查已有按压/hover与PageHost/分页/选择反馈，提出可测且可打断、不影响焦点尺寸、尊重减少动态效果的窄方案。
- [已完成] 按源码核对审查结果，向PM报告证据、限制与最小实现边界。
- [已完成] 对照HTML审查新增真实首选项日志截图，提出结构/像素检查。
- [已完成] 对齐最终序列化/选项API及独立oracle，实际检查与源窗口已批准。
- [进行中] 实施已批准的语义选项、安全序列化及UI016视觉纠正。
- [已完成] 实施已批准的2.1.1一致性改动。
- [待办] 按GitHub构建交付证据核对最终源码及独立运行/序列化结果，记录限制和最终状态。

## English

### Source identities and evidence boundary

Read-only cutoff: current `spec.md` is still UI-2026-11-17-R2, SHA256 `6c6aac1f73965cc4b496cf4e7ccccf0a774d0fe45e8ad9dcbb98d2afaf622a29`; R3 product law/checks are not yet approved here. The authorized reference `/private/tmp/tupig-ui-full.html` SHA256 is `63895000d2dff328e6a075df18d2f61a2170023a748c19e12500e7f18047345f`, verified from inert file content only. No HTML execution/server/browser bypass was used. Source snapshots: AppStyle.cpp `dfead373a7bb304ba8c8951b3f4eea348dd77e8b9d3828384ba926a100bd7d9a`; SettingsDialog.cpp `9d0c1563ac1f83f4082d6275dad79cf959199b62ba3141f5186755425d75e5fd`; PageHost.cpp `2a795aa0d0a80cc30078bdfb910477c63015af4685350b553c7603e4d81ffc47`. These identities describe the investigation cutoff, not a later R3 implementation identity.

### Display order and actual semantics

| Control | Reference and current difference | Current save/load authority | Minimal proposal / check boundary |
|---|---|---|---|
| Window startup | Both show main, background. | Display indexes0/1 bridge hidden Show/AutoHide radios; Save persists Autohide bool; load restores radios. No itemData. | Attach explicit false/true itemData; preserve order and platform behavior; test both choices Save/Cancel/reopen. |
| Main-window close | HTML572 Hide to tray, Quit. Current SettingsDialogDesign554-555 is Exit, Send to background. | SettingsDialog116-131 bridges index0 to Exit radio and1 to CloseToTray; Save line409-ish persists the radio bool; MainWindowWindow269-300 hides on spontaneous close only when CloseToTray true. | Confirmed display-order mismatch. Prefer Hide/true then Quit/false with explicit data, findData on load, stable bool save. Reordering labels alone would invert behavior. Test both booleans and actual close route separately from Quit. |
| Tray style / scope | Color/Mono and Current user/All users match. | Tray indexes0/1 bridge false/true radios. Scope already has false/true data, while accepted hook uses index and deferred apply. | Preserve capabilities and accepted-only scope transaction; add semantic data assertions rather than change scope behavior. |
| Language | HTML564 includes Follow system before named languages. Current SettingsDialog59-64 lists detected catalogs only. | Current native label maps to ISO code through I18N; unchanged Save/Cancel preserves original empty system setting. I18N.cpp startup recognizes empty as system policy, but setLanguage(empty) rejects unknown code. | Confirmed missing explicit system-choice UI, not a supported `setLanguage("")` preview path. If included in R3, add empty data and use an existing-policy system-resolution helper or clearly apply it on restart; never map empty to arbitrary English. Needs PM scope decision and transaction checks. |
| Log level | HTML578 Info/Debug/Warning/Error; actual severity order Fatal/Error/Warning/Info/Debug/Verbose. | Canonical data FATAL/ERROR/WARNING/INFO/DEBUG/VERBOSE; load/save use data, but warning and default checks still use indexes. | Retain all six backend levels. If matching reference's common-level order, data must stay canonical and index-derived warning/default logic must become semantic. Do not drop Fatal/Verbose. |
| Network / RSA length | HTML includes illustrative addresses; real UI uses Automatic plus discovered/stored addresses. RSA2048/4096 matches. | Addresses have real string data; Automatic currently lacks data and becomes empty. RSA uses numeric itemText; ordinary edits draft-only, explicit successful saved-path regeneration commits matching bits. | Do not copy illustrative addresses. Give Automatic explicit empty string and RSA numeric data if normalizing bindings; preserve TLS draft/regeneration contract exactly. |
| Server protocol / corners | Reference uses select menus; actual protocol radios Synergy/Barrier and four independent corner flags support the same plus multi-corner combinations. | Actual protocol setter and Cancel rollback, flags serialized independently. | Keep all actual supported combinations; a single-corner reference menu cannot replace four booleans without capability loss. Representation changes require explicit semantic bridge, not backend edits. |
| Six modifier rows | HTML607 repeats row-name/None/Ctrl/Alt/Meta/Super and sometimes duplicates the row name; omits complete Shift/AltGr choices. Current all rows use Shift/Ctrl/Alt/Meta/Super/AltGr/None. | ScreenConfig::Modifier is Shift0, Ctrl1, Alt2, Meta3, Super4, AltGr5, None6; ScreenSettingsDialog loads/saves currentIndex; Screen::screensSection writes exact mapped names when not identity. | Preserve all seven real targets and six source rows. Candidate row order self, None, then remaining canonical targets without duplicates, each enum stored in itemData. Load findData and save data; test all42 mappings, cancel and serialized names. Do not copy incomplete reference arrays. |
| Action trigger | HTML632 press/release equals actual0/1. | activeOnRelease bool directly uses currentIndex. | Store explicit false/true data; no behavior change. |
| Action type | HTML633 Switch/Direction/Tap/Press/Release/Next/Lock/Restart; actual enum order Press/Release/Tap/Switch/Direction/Next/Lock/Restart. | ActionDialog46 loads by index,82 saves index, ActionTypes constants and visibility use index. Backend enum0..10 includes three legacy mouse types beyond the eight editor choices. | Supported display data order `[3,4,2,0,1,5,6,7]` matches reference while preserving IDs. All visibility/validation/load/save must use semantic data. Legacy8..10 and unknown values need explicit read-only presentation and Save disabled, preserving original draft; no fake conversion/protocol feature. |
| Direction / cursor lock | HTML635 right/left/up/down versus current left/right/up/down. HTML does not define separate lock choices. | Direction enum0 left,1 right,2 up,3 down; lock0 toggle,1 on,2 off. Both currently direct index load/save. | Direction display data `[1,0,2,3]`; preserve lock toggle/on/off with explicit data and clear labels. Test exact serialized direction/lock words, not indexes. |
| Event recipients | HTML636 Current/All/Specified; actual All/Specified. | All serializes wildcard; Specified preserves checked names including all currently known names; mouse events disable recipients. | Current-computer mode has no backend representation. Keep truthful All/Specified only and existing mouse restriction; test future computer exclusion and Cancel/reopen. |

### R3-PAGE-01: source-confirmed action poisoning path

This is a functional defect, beyond visual order. A stored Action type8..10 reaches the eight-item combo's setCurrentIndex(type), producing -1. canSave returns true for non-key index-1; accept writes type-1; the parent's accepted callback renders action.text(), whose first line indexes m_actionTypeNames.at(type()). See ActionDialog.cpp46,82,154-169; Action.cpp16; ServerConfigDialog.cpp348 and305-306. Thus supported legacy data can be silently rewritten into invalid data, followed by an out-of-range access. Invalid direction/lock indexes also reach unchecked at() in Action::text45/50. This source chain is corroborated independently by tester/host review; it does not prove this is the user's startup crash without runtime reproduction.

Recommended minimum GUI treatment is an explicit stored-ID/name read-only placeholder, all editing/Save disabled and Cancel preserving the original Action. Tester prefers no conversion affordance in this correction; host allows only deliberate explicit supported conversion. PM must settle that choice before checks. This review recommends the smaller read-only scope. GUI summaries must reject unknown IDs before calling Action::text; that does not replace validation before core/config serialization. Invalid persisted data and non-truncating failure of export are a separate PM/PE safety boundary, not permission to invent fallback commands or alter the protocol.

## 中文

### 源身份及证据边界

只读截止时`spec.md`仍为UI-2026-11-17-R2，摘要`6c6aac1f73965cc4b496cf4e7ccccf0a774d0fe45e8ad9dcbb98d2afaf622a29`，本审查尚无R3产品/检查授权。获授权参考稿`/private/tmp/tupig-ui-full.html`摘要已核为`63895000d2dff328e6a075df18d2f61a2170023a748c19e12500e7f18047345f`，只作为惰性文本读取，不执行、不起server或绕行浏览器。AppStyle/SettingsDialog/PageHost源码摘要见英文，均为调查截止身份而非未来R3实现身份。

### 显示顺序与实际语义

| 控件 | 参考稿与当前差异 | 真实绑定/存储 | 最小建议与检查范围 |
|---|---|---|---|
| 启动行为 | 两者均主窗口/后台。 | 索引0/1桥接隐藏radio，保存Autohide布尔，无itemData。 | 明确false/true数据，保留顺序，测两个选项保存/取消/重开。 |
| 关闭主窗口 | HTML572先隐藏托盘、后退出；当前先Exit、后Send background。 | 索引0=Exit、1=CloseToTray；保存radio布尔；真实closeEvent仅自发关闭且true才隐藏。 | 已确认顺序不符。建议隐藏true在前、退出false在后，按data加载保存；只调文字会反转功能。退出菜单与关窗分别验证。 |
| 托盘/作用域 | 彩色/单色、当前用户/所有用户顺序符合。 | 托盘索引桥接布尔；作用域已有布尔data但accepted hook按index延后提交。 | 保留全部平台选项及仅保存后作用域提交，不为重排改后端。 |
| 语言 | HTML含跟随系统，当前只列检测到的语言包。 | native名称转ISO；不改选择可保留原空配置。I18N启动认识空=系统策略，但setLanguage空会拒绝。 | 缺少明确系统入口。如纳入R3须空data并用现有系统策略解析或明确重启生效，不伪造空=English；由PM定界并测事务。 |
| 日志 | HTML Info/Debug/Warning/Error，实际Fatal/Error/Warning/Info/Debug/Verbose。 | 存储规范字符串；加载保存已用data，警告/default仍用index。 | 保留六级。如按参考常用顺序，data保持规范、警告/default改语义判断，不能丢Fatal/Verbose。 |
| 网络/RSA | 参考地址为示意；实际自动及真实发现/已存地址。RSA2048/4096相同。 | 地址data真实字符串；自动目前无data等价空；RSA依数字itemText。 | 不复制示例地址；可给自动空string、RSA整数data统一绑定；保留TLS草稿/显式生成事务。 |
| 协议/死角 | 参考为下拉，实际Synergy/Barrier radio、四角可多选。 | 协议即时setter与Cancel回退，角标独立序列化。 | 保留真实多角功能，不用单选菜单损失能力。 |
| 六行修饰键 | HTML数组有重复且缺完整Shift/AltGr；实际每行七种目标。 | enum=Shift0/Ctrl1/Alt2/Meta3/Super4/AltGr5/None6，现按index存取，按实际名称写配置。 | 保留六源×七目标。候选每行自身、None、其余规范项去重，data存enum；按data加载保存，测试42映射/取消/序列化，不能硬抄不完整数组。 |
| 动作触发 | 按下/松开与实际0/1一致。 | activeOnRelease布尔直接用index。 | 用显式false/true data，不改功能。 |
| 动作类型 | 参考Switch/Direction/Tap/Press/Release/Next/Lock/Restart，实际Press/Release/Tap/Switch/Direction/Next/Lock/Restart。 | 显隐、校验、加载保存均依index；后端另有8..10旧鼠标类型。 | 支持项显示顺序data `[3,4,2,0,1,5,6,7]`；全部逻辑改语义data。旧8..10及未知值明确只读、禁存、原草稿不变，不虚构协议能力。 |
| 方向/光标锁 | 参考先右后左，实际先左后右；参考未定义光标锁单独三选项。 | direction=左0右1上2下3；lock=toggle0/on1/off2，现直接index。 | 方向显示data `[1,0,2,3]`，锁保留真实三项并显式data，测实际序列化词。 |
| 接收者 | 参考当前/全部/指定；实际全部/指定。 | 全部通配，指定保留名称（含全选当前集合），鼠标不支持接收者选择。 | 当前电脑无后端表示；继续诚实两项及原鼠标限制，测未来电脑排除/取消/重开。 |

### R3-PAGE-01：源码确认的动作数据污染路径

这超出审美顺序：旧Action type8..10被八项combo的setCurrentIndex(type)变成-1；非键动作canSave返回true，accept写回-1；父级accepted回调显示action.text，首行m_actionTypeNames.at(type())越界。路径为ActionDialog.cpp46/82/154-169、Action.cpp16、ServerConfigDialog.cpp348及305-306。direction/lock非法索引也会进入Action::text45/50的无边界at。测试/host已独立源码佐证，但没有运行重现前不能说就是用户启动崩溃原因。

最小GUI建议为存储ID/名称明确只读占位、整页不可编辑/保存、取消原样保留。测试角色建议本次不加转换入口，host允许显式支持项转换；由PM在检查前定界，本审查推荐更小的整页只读。未知ID摘要须在action.text前守卫，但不能替代core/config导出前校验；非法持久化数据及导出失败不截断文件属于PM/PE另行安全边界，不能伪造fallback指令或扩协议。

## English

### Existing feedback: why the user can perceive no animation

AppStyle.cpp41-117 implements only a QPushButton child overlay. It animates white opacity to0.06 on hover or0.15 on mouse press over110ms, then retargets on release/leave; events pass through. A quick press/release can retarget before reaching a visible peak. The filter does not observe standard clicked(), keyboard/AX activation, QToolButton, radio/checkbox, tabs or combo selection. Existing QSS hover/pressed rules switch instantly; pressed adds1px padding. The app installs the global filter during startup (synergy-gui.cpp81). The filter creates child widgets/raises them from mouse/resize/enabled events; constructor/reentrant/destruction safety must be included in startup review rather than assumed. This review does not claim that this is the startup crash cause.

PageHost.cpp push switches current stack immediately; finished removes the page and switches Back immediately. watchContent connects tab changes only to preferred-height invalidation; there is no opacity/property animation or transition controller in this source. The reference HTML has no explicit transition/animation CSS. Motion therefore comes from the user's new request and must be fixed in the R3 master spec, not invented as a previously met HTML requirement.

### Proposed motion contract (PM decision before implementation)

1. **Buttons:** preserve native clicked/pressed/released semantics; show a short visible press pulse, e.g. immediate bounded tint peak followed by110ms fade, plus130ms hover retargeting. Include mouse, keyboard and accessibility activation. Disabled controls never pulse or activate. Use a paint-only transparent overlay; do not scale/move the actual widget or its hit area. The instantaneous QSS focus/pressed state remains available even with motion off.
2. **Page push/Back and tabs:** apply a160ms fade-in of the already committed incoming content (e.g. opacity0.7 to1). Keep geometry/hit areas/focus and preferred-height computation unchanged; do not animate window height, layout margins or old hidden page dimensions. Incoming controls must be readable immediately. A transition must not delay actual Save/Cancel/finished, navigation or destructive object cleanup.
3. **Option selection:** keep native popup and keyboard behavior. A user-activated combo choice can pulse its field/frame accent for160ms after the real selected label/data changes; radio/checkbox selection can use the same bounded emphasis. Initialization, cancelled popup, no-op selection and programmatic loading should not flash as though the user changed a setting. This is presentation feedback, not delayed setting application.
4. **Rapid input/lifetime:** stop the old animation, settle the old target to its final appearance and start only the latest target; use guarded QObject targets and owned animation contexts. Switching/deleting pages or closing the window mid-animation must cancel pending callbacks and never touch freed widgets. No nested event loops, wait timers, queued duplicate writes or accumulated navigation transitions.
5. **Reduced motion:** one explicit policy must control all effects; when reduced motion is active, set final visual state immediately and create no running transition. The local pinned Qt6.11 public QAccessibilityHints header exposes contrastPreference only; it does not supply a motion preference API here. The PM must choose a verified OS preference adapter and/or a clear app-level reduced-motion override, with known fallback and tests. Do not claim that Qt automatically honors every OS preference. New platform reads, if selected, belong under src/lib/platform/<os>/; no third-party runtime dependency is needed. A deterministic test override may exercise enabled/disabled policy but is not proof of live OS preference acceptance.
6. **Safe attachment:** prefer controllers attached after a complete widget/page is constructed, or a guarded deferred attachment with duplicate suppression. Do not depend on creating an overlay while an arbitrary QPushButton is being constructed/destructed. This must be reviewed together with the actual startup-crash reproduction before retaining or replacing the existing global filter.

### Concrete checks to create before paired product edits

- For startup/close, tray, scope, every supported Action type, direction, lock and modifier target: assert label + stable itemData + actual stored value, Save/Cancel/reopen and translation stability; display order is checked separately from value identity. For close Hide/Exit, inspect the real handler decision with isolated settings, never actually quit the user's app.
- Legacy Action8/9/10 and invalid-1/99: constructing/selecting the page does not crash, the stored ID is visibly explained, Save is disabled, Cancel preserves every field, and GUI summaries never invoke unchecked formatting on unknown IDs. Backend serialization rejection must be independently tested before output file truncation under its own approved scope.
- All42 real modifier mappings: identity returns the same key, None remains an actual supported target, each cross mapping serializes its canonical backend name, and order/translation changes cannot change stored enum IDs.
- Animation checks must inspect initial/intermediate/final paint state under controlled time, not just assert that a QPropertyAnimation object exists. Verify the intended110/130/160ms bounds, a visibly different midpoint, latest-only completion under rapid changes, disabled/no-op initialization behavior, destroyed-target cleanup and reduced-motion immediate final state.
- Re-run natural-height/action-gap/540-600-820/en-zh matrix during and after transitions: no horizontal clipping, new minimum-size retention, transient control overlap, stale active page height or altered focus/Save behavior. Check actual native/keyboard activation separately from synthetic time-state evidence. No live input/clipboard injection or OS permission change.

### Smallest implementation boundary

Semantic presentation: SettingsDialogDesign/SettingsDialog bindings, ActionDialogDesign/ActionDialog bindings, ScreenSettingsDialogDesign/ScreenSettingsDialog bindings and a narrow ServerConfigDialog action-summary guard. The GUI can use private enums/typed itemData and findData/currentData without changing Action/Screen protocol enums. Preserve existing object names, validators, draft owners, native host, recipient restriction and TLS regeneration semantics. Translation additions belong in existing zh_CN/ja catalogs. Follow-system UI and log-order choice require PM decisions before expanding this boundary.

Motion: AppStyle.{cpp,h} plus a small shared GUI motion policy/controller if needed, PageHost.{cpp,h} navigation/tab hooks, and their GUI CMake registration; button attachment placement may require the existing application bootstrap call site. Platform preference adapters are conditional only if the approved reduced-motion contract requires them; they must be named in the package before edits. No service/core/backend/protocol or release change is proposed. R3 startup-crash fixing belongs to its separately evidenced work item; the unbounded Action formatter/export hazard cannot be declared solved by dropdown edits.

## 中文

### 已有反馈及用户为何看不到动画

AppStyle.cpp41-117只有QPushButton子overlay：hover白色不透明度0.06、鼠标按下0.15，110ms过渡，释放/移出立即重新指定目标，事件透传。快速按下/释放可能尚未达到明显峰值就转向，视觉很弱。它不观察标准clicked、键盘/AX、QToolButton、radio/checkbox、分页、combo选择。现有QSS hover/pressed为即时切换，pressed仅1px padding。全局filter在synergy-gui.cpp81启动时安装，事件中创建/raise子控件；构造/重入/析构安全须随启动崩溃证据审查，不能预设安全，也不能凭此声称是崩溃根因。

PageHost push立即换stack，finished立即移除并Back；tab变化仅使自然高度失效，当前源码无opacity/property转场或统一控制器。原HTML无明确transition/animation CSS，因此动效来自用户本次新要求，须由R3唯一主规格固定，不能编造成此前已满足的HTML需求。

### 动效候选契约（实施前由PM决定）

1. **按钮：**保留原clicked/pressed/released语义；按压立即给有限tint峰值再110ms淡出，hover约130ms重定向。覆盖鼠标/键盘/辅助功能，禁用不闪不触发。仅透明paint overlay，不移动/缩放真实控件及命中区，关闭动效仍保留静态焦点/按压状态。
2. **页面/Back/分页：**真实切换已提交后，活动新内容约160ms从opacity0.7到1。自然高度/几何/命中/焦点不变，不动画窗口高度、margin或隐藏旧页尺寸，新控件立即可辨。不能延迟Save/Cancel/finished、导航与对象清理。
3. **选项：**保留原生popup及键盘选择；用户真实activated后已更新label/data的字段边框可160ms强调，radio/checkbox同理。初始化、取消popup、未改选择和程序加载不冒充用户修改闪动；设置应用不延迟。
4. **快速操作/生命周期：**停旧动画并收敛旧target到最终外观，只保留最新目标；QObject目标保护并有明确owner。切页/删页/关窗中止回调，不碰释放对象，不加嵌套eventloop、等待、重复写或导航队列。
5. **减少动态效果：**统一策略控制全部效果，开启时即时最终状态且无运行transition。本机固定Qt6.11公共QAccessibilityHints只有contrastPreference，没有motion入口；PM须选已验证OS读取adapter及/或清晰的应用开关和fallback，不能假称Qt自动遵守各OS。若读OS放src/lib/platform/<os>/，无需新运行依赖；测试override可验证分支但不能代替实际OS偏好验收。
6. **安全挂载：**优先在控件/页面完整构造后挂控制器，或保护的延后挂载并去重，不能依赖任意QPushButton构造/析构时创建overlay。是否保留/替换全局filter须结合真实启动崩溃再审。

### 配对产品修改前须建立的实际检查

- 启动/关闭、托盘、作用域、所有支持动作/方向/锁/修饰键均断言label、稳定data、真实保存值、取消/重开与翻译稳定；显示顺序与值身份分别检查。关闭隐藏/退出以隔离设置核真实handler决策，不退出用户日常app。
- 旧Action8/9/10及非法-1/99：构造/选择不崩，存储ID解释清楚，Save禁用，取消所有字段原样，GUI摘要不对未知ID调用无边界formatter。后端非法导出须在另行批准范围测截断前拒绝。
- 六源×七目标共42修饰映射：自身恒等、None真实可用、交叉映射写规范名称，顺序/翻译不改enum。
- 动画检查真实初始/中间/最终paint，不能仅断言存在QPropertyAnimation。测110/130/160ms时长、可辨中间态、快速切换最终唯一、禁用/无修改/初始化、不存活target取消、reduce-motion即时最终。
- 转场中及结束重跑自然高度/动作距离/540-600-820/中英文矩阵，无水平裁切、旧最小高度、临时重叠、焦点/Save变化。原生/键盘激活与合成时序证据分开；不注入宿主键鼠/剪贴板、不改权限。

### 最小实现边界

语义呈现限SettingsDialogDesign/SettingsDialog、ActionDialogDesign/ActionDialog、ScreenSettingsDialogDesign/ScreenSettingsDialog及ServerConfigDialog窄摘要guard；GUI用typed itemData及findData/currentData，不改Action/Screen协议enum。保留objectName、校验、草稿owner、原生host、接收者限制和TLS显式生成语义。翻译只加既有zh_CN/ja。语言系统入口/日志排序须先由PM决定是否纳入。

动效限AppStyle.{cpp,h}、必要时小共享GUI policy/controller、PageHost.{cpp,h}导航/分页挂钩及GUI CMake；安全挂载可能需现有bootstrap调用点。OS偏好adapter仅在已批准减少动态效果契约要求时加入，并在构建包先明确。无服务/core/backend/protocol/发布变更建议。启动崩溃修复属于另有运行证据的工作项；不能说下拉改动就解决无边界Action格式化/导出风险。

## English

### Frozen R3 semantic candidate and exact serialization entry boundary

PM's intended correction is reviewed as follows: all subpage menus use stable semantic itemData; supported Action IDs display in `[3,4,2,0,1,5,6,7]` order; directions in `[1,0,2,3]`; Close is Hide/true then Quit/false; language has explicit empty/system data; all seven real modifier targets are retained; recipients remain All/Specified. Legacy Action8..10 and unknown values get an entirely read-only editor with Save disabled and unchanged Cancel. This is the smaller tester-recommended disposition, rather than adding a conversion operation. No actual check or product edit for this scope was performed by this reviewer.

Concrete product candidates, subject to spec/check authorization:

- `dialogs/SettingsDialogDesign.{cpp,h}` / `SettingsDialog.{cpp,h}`: construct semantic bool/code/number data, use findData/currentData; remove index-dependent close and optional log-order coupling while preserving hidden-radio hooks, language preview rollback, defaults and explicit TLS-generation semantics. Empty language must retain the existing startup system-default policy; adding a preview resolver, if required, is a narrow `common/I18N.{cpp,h}` boundary and must not alter the Chinese/English system-default rule.
- `dialogs/ActionDialogDesign.{cpp,h}` / `ActionDialog.{cpp,h}`: data-bearing display choices; semantic visibility/validation/load/save; explicit unsupported/legacy read-only explanation. `dialogs/ServerConfigDialog.cpp` must guard invalid IDs before action-summary formatting.
- `dialogs/ScreenSettingsDialogDesign.{cpp,h}` / `ScreenSettingsDialog.cpp`: seven unique data-bearing modifier targets per source row; identity/None/cross-mapping preserved. `gui/config/ScreenConfig.*` canonical names and protocol enum order stay unchanged.
- `gui/Action.{cpp,h}`: add a side-effect-free serializability predicate for known type0..10 plus only relevant direction0..3/lock0..2; protect formatting from out-of-range access. Invalid formatting must report failure, not manufacture a fallback command, reassign type or silently export a shortened configuration. Initialize the existing restart flag deterministically if covered by the identity/serialization contract.
- `gui/config/ServerConfig.{cpp,h}`: validate all nested actions before the first output character; stage the complete text in memory and check stream status; only then open/commit a `QSaveFile` at the requested destination. GUI export currently enters `save(QString)` at ServerConfig.cpp43-52, which opens WriteOnly before validation. Atomic commit preserves the existing destination on invalid data, short write or failed commit. Valid serialization text must remain byte-equivalent under the existing QTextStream encoding behavior. The whole-model validator should additionally reject invalid persisted modifier targets before Screen::screensSection calls modifierName(raw); default-identity `-1` remains valid. This adjacent boundary requires PM/test coverage, not an unapproved modifier migration.
- `gui/core/CoreProcess.cpp` only at `persistServerConfig()`636-651: replace the separate open(WriteOnly|Truncate)+save(QFile&) sequence with delegation to the same validated atomic `ServerConfig::save(path)`. Return existing false/path outcome on failure and do not start the core from invalid generated configuration. The external-config readable-path branch is unchanged. No process launching, network protocol/parser, daemon, service or runtime connection behavior is otherwise changed.

The public `save(QFile&)` overload cannot reverse a caller's earlier truncate, so safety cannot be implemented only inside that function. Its only production call found is CoreProcess above. Retain it with explicit prevalidation/status if needed for existing callers/tests, or replace it with a non-owning checked write only after mapping every caller; all production path-based saves must use the central atomic entry. MainWindowConfiguration::saveServerConfig already delegates to save(path) and can retain its existing failure message.

This boundary changes validation and GUI-model persistence before launch; it does not add protocol commands, change supported canonical IDs/names, translate stored values, reinterpret legacy commands, modify trust/crypto/update policy or permit dropping unsupported actions. Tests must preserve an existing sentinel file under invalid type/direction/lock/modifier and write failure, prove the core persistence helper returns failure without launching any process, and compare valid canonical serialized output. A source-only discovery of these paths remains distinct from a reproduced cause of the reported startup crash.

## 中文

### R3固定候选及确切序列化入口边界

对PM拟定范围的审查结论：所有子页菜单用稳定语义itemData；八支持Action显示ID顺序`[3,4,2,0,1,5,6,7]`，方向`[1,0,2,3]`；关闭为隐藏true再退出false；语言明确空/系统data；七种真实修饰目标全保留；接收者仍全部/指定。旧Action8..10与未知值整页只读、禁存、取消原样。这采用测试角色推荐的较小边界，不新增转换动作。本审查未为此范围执行实际检查或产品修改。

具体产品候选，仍须规格/检查授权：

- `dialogs/SettingsDialogDesign.{cpp,h}`及`SettingsDialog.{cpp,h}`：语义布尔/语言代码/数字data，用findData/currentData，解除关闭及可选日志排序的index耦合；保留隐藏radio hook、语言预览回退、默认值与显式TLS生成。空语言保留原系统默认规则；若需即时预览解析，仅可窄改`common/I18N.{cpp,h}`，不得改中文/英文系统默认策略。
- `dialogs/ActionDialogDesign.{cpp,h}`及`ActionDialog.{cpp,h}`：data选项、语义显隐/校验/加载/保存、不支持/旧类型明确只读；`ServerConfigDialog.cpp`摘要须先guard未知ID。
- `dialogs/ScreenSettingsDialogDesign.{cpp,h}`及`ScreenSettingsDialog.cpp`：每行七种去重data目标，保留恒等/None/交叉；`ScreenConfig.*`规范名与协议enum不变。
- `gui/Action.{cpp,h}`：无副作用可序列化校验，已知type0..10及仅相关direction0..3/lock0..2，防格式化越界。非法必须报告失败，不能造fallback、改type或悄悄少导出动作；既有restart布尔如契约覆盖则明确初始化。
- `gui/config/ServerConfig.{cpp,h}`：写第一个字符前验证全部嵌套动作，先内存串流完整文本并核status，再QSaveFile提交。现GUI导出save(QString)43-52先WriteOnly后写，非法/短写/commit失败应保留旧目标；合法输出以原QTextStream编码语义证明字节相同。同模型validator还应拒绝非法持久化modifier target，避免Screen::screensSection modifierName(raw)越界，默认恒等`-1`有效；这一邻接边界须PM和实际检查覆盖，不作修饰映射迁移。
- `gui/core/CoreProcess.cpp`仅`persistServerConfig()`636-651：把另行open(WriteOnly|Truncate)+save(QFile&)改为委托相同validated atomic save(path)，失败沿既有false/path返回，不能据非法生成配置启动core。external可读配置分支不动；不改其他进程启动、网络协议/parser、daemon/service/连接行为。

公开save(QFile&)无法撤销caller此前Truncate，所以只改该函数不够。查到唯一production caller为上述CoreProcess；若现有测试/caller需保留该重载，应明确前置校验/status，或穷尽caller后改为非owner的checked write。全部production路径保存必须走中央atomic入口。MainWindowConfiguration::saveServerConfig已委托save(path)，可保留原失败提示。

该边界改启动前GUI模型验证及持久化，不增加协议命令、不改规范ID/名称、不把译文存值、不重解释旧指令、不改信任/加密/更新策略，也不允许丢弃不支持动作。检查须证明非法type/direction/lock/modifier及写失败时旧sentinel文件不变，core持久化helper失败且不启动进程，合法规范输出相同。此处源码发现仍与用户启动崩溃运行重现分开。

## English

### Approved authority update and W3 hold

PM subsequently amended the same master spec to UI-2026-11-17-R3, verified SHA256 `a6472b99194b8a991dd9c63ea618d9b202502a4d597ef428fd5b883de47a1784`. Immutable package `docs/aegis/20261117-ui-r3-build-package.md` assigns W3 selector/model validation/atomic-save work; runtime/motion belongs to W2. The earlier read-only R2 cutoff above remains historical. PM accepted validation of only relevant direction/lock and modifier bounds, retention of valid legacy encodings, deterministic initialization of the existing restart flag tofalse, central atomic path-save and the CoreProcess persistence-only delegation.

Tester API agreement: `Action::isValidForSerialization() const`; Action::text and ServerConfig::save(path) retain existing return types. Independent oracles hardcode canonical command bytes/data IDs and sentinel file bytes, never call the product formatter to generate expected output. Only Action type4 validates direction0..3 and type6 validates lock0..2; irrelevant stored parameters remain untouched on edits and do not change valid output. Modifier -1/default and0..6 are valid; other values are preserved read-only in the editor and rejected before export. No current check artifact or adequacy approval authorizes W3 product edits yet. Common/I18N's exact necessary system-preview API boundary is being confirmed with PM; no language/default policy edit has occurred.

## 中文

### 批准权威更新与W3等待

PM随后在同一主规格批准UI-2026-11-17-R3，已核摘要`a6472b99194b8a991dd9c63ea618d9b202502a4d597ef428fd5b883de47a1784`。不可变`docs/aegis/20261117-ui-r3-build-package.md`分配W3选项/模型验证/原子保存，运行隔离/动效属W2。上文R2只读截止身份保留为历史。PM接受仅相关direction/lock及modifier边界校验、合法旧编码保留、restart既有布尔确定初始化false、中央atomic路径保存及CoreProcess仅持久化委托。

测试角色API已对齐：`Action::isValidForSerialization() const`，Action::text及ServerConfig::save(path)返回类型不变。独立oracle硬编码规范命令字节/data ID及sentinel文件字节，不调用产品formatter生成expected。仅type4验证direction0..3、type6验证lock0..2；无关已存参数编辑不变，不改变合法输出。modifier -1/default及0..6合法，其余编辑器只读保留、导出前拒绝。目前尚无实际检查充分性授权W3改产品；Common/I18N跟随系统预览的必要API边界正与PM确认，没有语言/默认规则修改。

## English

### New user screenshot: concrete Log presentation rejection

The newly supplied real Preferences Log screenshot was visually inspected and compared to HTML577-583 and component CSS. It shows actual native tab baseline/pane lines across a rectangular strip; card titles anchored at the card's top edge; two Log cards instead of the reference's one; a file-log group checkbox drawn as a group-box border header; a short crowded path input and oversized browse action; and an undifferentiated action bar. No user path text is reproduced in this review. This is an observed visual rejection, not merely an itemData issue.

Root causes supported by source: SettingsDialogDesign creates a separate checkable groupLogToFile card, puts its title/checkbox in native QGroupBox subcontrols, and gives logCard a native group title. AppStyle::title uses margin origin at the top edge; the tab rules leave native QTabBar base/pane behavior insufficiently suppressed. The Settings constructor's generic grid loop141-155 removes field-column stretch and adds a right spacer, including long path forms, so a long-input row can be treated like a short180px selector. Browse uses a generic flat QPushButton without a32px icon bound. No Save primary property is assigned in SettingsDialog, so enabled Save has the same visual hierarchy as ordinary buttons.

Minimum proposed correction, after new visual actual checks and PM authority:

- Compose the Log controls into one content card in the reference order: level, usable path + browse, normal in-content file-log checkbox, GUI debug checkbox, actual warning/help, then shared actions. Use an ordinary QCheckBox for the visible file flag. The existing groupLogToFile object can remain a hidden checkable business proxy with a two-way bridge, preserving all current bindings/settings and the original path-enabled rule; do not paint its checkbox/title at a card border.
- Avoid native group headers for this Log card; title/field text must be normal card content if retained. Do not merely increase padding around an unchanged native header. Preserve single workspace inset and content-fitting height. Long-path controls should use remaining width rather than the generic short-field spacer; browse is a32px icon action with its original accessible label and real file-dialog behavior.
- Host-owned tab styling must remove the full native strip/base and pane outline, using transparent unselected space and the existing rounded selected pill; retain actual QTabWidget selection/focus/keyboard behavior. W3 does not edit host-owned AppStyle during parallel work.
- Mark enabled Save as primary accent, Cancel neutral, Reset/Restore Defaults as lower-emphasis retained utilities. Disabled Save remains disabled and visually muted; do not manufacture enabled state to match a screenshot. Shared button objects/callbacks and default focus semantics remain unchanged.

Actual geometry/pixel oracle proposal: at540/600/820 in en/zh, the Log controls occupy one visible card; the file checkbox indicator/text are fully within the card with at least12px left and10px top inset; path outer width is at least180px and grows with the available row, browse outer size32px and row gap8px, all outer rectangles stay inside card bounds. Long-path text's legitimate horizontal cursor scrolling is not treated as clipping. Sample the unused tab strip/top/bottom away from text/selected pill and require workspace colors rather than a continuous native border; render an enabled Save after a real draft change and verify accent fill versus neutral Cancel and muted utilities. Keep the original <=24px action-gap, natural height, disabled/read-only and Save/Cancel/Reset checks. Screen scaling/font rasterization needs tolerant color regions and geometric bounds, not byte-exact screenshot hashes.

The user also newly identified this Mac's R2 copy as the crashing build and authorized a GitHub-built2.1.1 release/download/reinstallation workflow. That changes the PM's delivery authority; it does not authorize W3 to alter version/release/install files before the same master specification and checks are amended. W3 will handle the assigned version change only after that scope is fixed. No new product visual/version edit or build/CUA occurred in this review.

## 中文

### 用户新增截图：日志页具体视觉否定

已实际查看新增首选项日志截图，并与HTML577-583及组件CSS对照。图中存在横贯矩形strip的native tab基线/边线，标题贴卡片顶边，日志拆成两卡，记录到文件checkbox成为groupbox边沿标题，路径输入短而拥挤、浏览按钮过宽，操作按钮缺层级。本报告不复写用户路径内容。这是已观察到的视觉否定，不是仅itemData问题。

源码支撑：SettingsDialogDesign单独创建checkable groupLogToFile，用native QGroupBox标题/indicator；logCard也用native group title。AppStyle标题以margin origin贴顶，tab样式未充分去除native base/pane。Settings构造141-155通用grid遍历清field stretch并加右spacer，也作用于长路径表单，误把长输入当180px短选项。Browse为普通flat按钮，无32px icon上限。SettingsDialog没有给Save primary属性，启用后仍与普通按钮同层。

最小纠正候选，先有新视觉实际检查与PM授权：

- 日志合为一张内容卡，顺序为级别、可用路径+浏览、卡内普通文件checkbox、GUI debug、真实提示/警告及共享动作。可见文件开关用普通QCheckBox；原groupLogToFile可作隐藏checkable业务proxy双向桥接，保留原设置/绑定/路径启停，不再把checkbox/title画在卡边。
- 本Log card不使用native组标题；如保留标题则为正常卡内文字，不只围旧header加padding。单工作区inset及自然高度保留。长路径使用行内剩余宽，不套短字段右spacer；浏览为32px图标动作，保留可访问名称和真实文件对话框。
- 宿主负责tab样式移除native strip/base/pane外框，未选区域透明，选中保留圆角pill；原QTabWidget选择/焦点/键盘行为不变。W3不与host并行改AppStyle。
- 启用Save用主色，Cancel中性，Reset/恢复默认保留但弱化为工具；未修改时Save仍真实禁用/灰态，不能为截图强制启用。共享按钮对象/回调/default焦点不变。

实际几何/像素oracle建议：540/600/820及中英文时Log仅一张可见卡；文件checkbox indicator/text全部在卡内，左距至少12、顶距至少10；路径外框至少180且随行可用空间增长，browse32px，行内gap8，所有外框在卡内。不把长路径合法水平光标滚动算裁切。避开文字和选中pill采样tab空余区域/上下边，要求workspace底色而非贯穿native边线；真实修改草稿后渲染启用Save，检查主色与Cancel/工具层级。保留<=24动作距离、自然高度、禁用/只读及保存/取消/重置契约。缩放/字体抗锯齿用区域色和几何容差，不用整图哈希死比。

用户还新确认本Mac的R2副本崩溃，并授权GitHub构建2.1.1版本/下载/重装流程，这是PM交付授权变化，不等于W3可在同一规格/检查修订前改版本/发布/安装文件。W3待该范围固定后承担被分配的版本修改；本审查没有新的产品视觉/版本修改或构建/CUA。

## English

### GH/screenshot authority update

The same master specification is now UI-2026-11-17-R3-GH, verified SHA256 `2b10916030c3c8a93a5ef3dec22fd732a6a540e09fb4ef061a6c3aa337150ab3`; UI-016 fixes the screenshot-derived child composition/chrome, UI-017 requires GitHub-builtv2.1.1 with both-platform release gates, UI-018 requires recoverable local retirement then testing the downloaded latest Release app. These supersede the earlier local-package-only delivery description. The rejected screenshot SHA256 is `483a3850e11483ffe6b5cab687573fb6398eef32bfd74d56b7a8d35b2935e265`.

PM approved the necessary narrow common/I18N language binding: reuse the existing Chinese/English system resolver, add explicit nonpersistent preview while existing callers retain their default behavior, Save empty as empty and Cancel restore the original effective language and original stored value without transient setting writes. W2 alone owns FeatureHandler scope/path changes; its scope property is now `originalScope` QVariant bool, so W3 load and dirty checks must use semantic data rather than originalScopeIndex. AppStyle/PageHost/motion remain W2-owned.

Independent check review found and the tester repaired two mechanical issues before meaningful red execution: protected accept must be invoked via the real metaobject/button path, and Alt/Ctrl fixtures must send the actual modifier key before the letter (a letter event's modifier flag alone does not add that key to this existing KeySequence model). The independent hardcoded canonical command oracle was retained. PM explicitly approved only a global forward declaration and narrow friend of `R3SerializationTests` in CoreProcess.h to exercise the private production persistence helper without starting a process; W3 applied that access-only seam, with no product-body changes. All other W3 product work remains held for the real red/check-adequacy window, including the new visual/version work.

## 中文

### GitHub/截图权威更新

同一主规格现为UI-2026-11-17-R3-GH，已核摘要`2b10916030c3c8a93a5ef3dec22fd732a6a540e09fb4ef061a6c3aa337150ab3`；UI-016纠正截图来源的子页结构/边框，UI-017要求GitHub构建v2.1.1及双平台发布门禁，UI-018要求可恢复移除本地副本后测试下载latest Release。它们取代早期仅本机包交付描述。被退回截图摘要为`483a3850e11483ffe6b5cab687573fb6398eef32bfd74d56b7a8d35b2935e265`。

PM已批准必要的窄common/I18N绑定：复用原中文/英文系统resolver，预览明确不持久化，既有调用默认不变，空选择保存空，取消恢复原effective语言和原存储值且不临时写配置。W2单独负责FeatureHandler作用域/路径，scope属性改为QVariant bool的`originalScope`，W3加载/dirty须用data而非originalScopeIndex。AppStyle/PageHost/motion仍W2单owner。

独立审查发现并由测试角色在有效红灯前修正两项机械问题：protected accept须走真实metaobject/按钮；Alt/Ctrl夹具须先发送真实修饰键再字母，单字母事件的modifier flag不会在既有KeySequence模型自动加入修饰键。硬编码独立规范命令oracle保留。PM另明确批准CoreProcess.h仅global forward及R3SerializationTests窄friend，直接测试private持久化helper且不启进程；W3已加这一访问seam，没有产品body修改。其余W3产品仍待真实red/充分性窗口，包括新增视觉与版本工作。
