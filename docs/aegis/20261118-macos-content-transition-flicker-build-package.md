# macOS content transition flicker / macOS 页面切换闪动包

## English

Immutable BP-UI-20261118-CONTENT-FLICKER, created2026-11-18. Sole master spec.md revision UI-2026-11-18-GH-TWO-TARGETS SHA256 `1182eaf98f950de91564f40d6305998b5a190df2d3c89b2fe72bceed788732c8`. Covers UI-009/010/015/017, R3-V04/GH-V02. Existing Motion package `3b15996e28b96b54ccf24c22d3cf77b938f3893b87da8c447e8ac6b04d8cf09a` and native backend package `1d003721e5bf858aaa581c5de23763753a540471552dd9ff666cd1bb2f566abe` remain separate. This package addresses a user-observed macOS visual defect in menu/page transitions and does not change menu actions or page routing.

Source evidence: PageHost calls `animateContentTransition(this)` after page push and after page rejection. Motion creates a full-target `contentTransitionOverlay`, starts it at opacity `0.18`, paints a white rectangle and fades it to zero over 160 ms. The same content overlay is used for tab and option changes. On dark UI this produces a full-surface white flash at the first visible frame. Existing R3Motion tests only require a non-endpoint frame and do not assert a brightness bound or duplicate transition count. This is a source diagnosis, not a hosted runtime capture.

Contract C-CONTENT-FLICKER1: after G4, add a bounded real-render diagnostic to the existing Motion contract. Exercise page push, Back, tab and option changes through existing routes. Record the overlay count, direct animation duration, first rendered overlay opacity, peak opacity and completion/lifetime. Require one active content overlay, 160 ms duration, one page action, unchanged geometry/focus, and a visible intermediate frame. The check must fail on duplicate overlays or a first-frame opacity above the approved lower visual bound. Keep the existing six button activations, AX latency, 110 ms button duration, 160 ms content duration, original sampling windows and lifecycle/reduced-motion checks. Do not remove the transition or replace image checks with state-only checks.

MP2 candidate changes only the content overlay peak opacity from `0.18` to a lower value chosen by the reviewed rendered evidence, while preserving the 160 ms OutCubic curve, target geometry, routing, input transparency, reduced-motion behavior and completion cleanup. Button feedback is handled by the separate MP package. The candidate must first produce a deterministic RED on the current source, then a GREEN after the one-line change, followed by accurate macOS and Windows critical suites. No menu action, shortcut, native menu role or page transaction may change.

No raw screen capture, private settings, or new app is required. Keep diagnostic output local and ignored. Existing release, CodeQL, package and physical core acceptance gates remain unchanged.

## 中文

不可变 BP-UI-20261118-CONTENT-FLICKER，绑定主规格1182、动效包3b1599和原生环境包1d0037。对应UI009/010/015/017及R3-V04/GH-V02，处理用户反馈的Mac菜单/页面切换闪动，不改菜单动作或页面路由。

源码显示，PageHost打开页面和返回时都会调用页面动效。Motion会在整个页面上创建 `contentTransitionOverlay`，起始透明度为0.18，绘制白色矩形，再用160ms淡出。标签和选项切换也使用同一层。深色页面第一帧会立刻变亮，容易看到闪动。现有测试只检查有中间帧，没有检查亮度上限和重复创建次数。这是源码诊断，尚未代替真实Mac观察。

C-CONTENT-FLICKER1：G4后在现有Motion检查里通过页面打开、返回、标签和选项切换记录真实覆盖层数量、动画时长、第一帧透明度、峰值和结束清理。要求只存在一层、160ms、动作只执行一次、几何和焦点不变，并保留真实中间帧。新增检查应能拦住重复覆盖或第一帧过亮，同时保留六种按钮激活、AX时延、110ms按钮、160ms页面、原采样窗口、生命周期和减动效检查。不能删除动效或只看状态值。

MP2候选只把页面覆盖层峰值从0.18降到基于真实画面审核的较低值，160ms OutCubic、目标几何、路由、输入透传、减动效和清理保持。按钮动效由另一个包处理。当前源码先真实红，单行修复后真实绿，再跑准确Mac/Windows关键测试。菜单动作、快捷键、原生角色和页面事务不能改变。

不需要原始截图、私设或新App，诊断输出只留本地忽略目录。发布、CodeQL、安装包和真实core验收条件不变。
