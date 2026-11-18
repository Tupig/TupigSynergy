# R4 sharing field columns / R4共享字段列检查

## English

PM-authorized R4-FFORM005 actual check under unchanged HTML530-544/F09/UI-011/012. Six actual fields (clipboard limit, corner size, heartbeat, protocol, switch delay and double-tap) use the same120label+10gap+180field reference column: field left142 relative to its real owning card and width180. Source object `sbSwitchCornerSize` locates the corner-size field (not an invented sbCornerSize). Object names locate fields; HTML supplies the geometry.

Extend existing real sharing-page checks at EN/ZH540/600/820; `sharing-fields` selects six rows. Record every field/card-relative rectangle plus actual label text, label rectangle and text-available/bounding rectangles. QLabel may genuinely wrap; checkbox uses its actual style contents rectangle. Text must fit available width/height without clipping; fields stay visible even when their independent enable flags are off. Retain original checkbox order, all four corners, protocol typed enum/save/cancel checks and existing configuration ownership. No widget or backend change in this actual artifact. Preserve measured RED before a one-design-file column correction; do not claim all six fields failed merely from a screenshot estimate.

## 中文

PM授权FFORM005实际检查，绑定不变HTML530-544/F09/UI-011/012。真实六字段（剪贴板限制、死角大小、心跳、协议、切换等待、双击）同列：卡内left142、width180，来源120标签+10间隙+180字段。死角真实定位名为sbSwitchCornerSize，不能造sbCornerSize；期待取参考而非属性自证。

现共享页双语三宽，sharing-fields组六行；记录每字段相对真实卡片矩形、实际标签文字/矩形及文字可用/测量矩形。QLabel允许真实换行；checkbox取实际style文本内容rect，文字不能超宽/高裁切，独立enable关闭时字段仍可见。原复选框顺序/四角/typed协议及保存取消/所有权全留，不改widget或backend。先保留实测RED再只修设计文件列布局；不能按截图估计声称六字段全部失败。
