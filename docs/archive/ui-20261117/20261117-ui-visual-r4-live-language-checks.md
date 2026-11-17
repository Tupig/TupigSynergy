# R4 active editor language checks / R4活动编辑页语言检查

## English

PM-authorized actual regression under unchanged UI-003/012/014 and R4 package/master. The prior540 captures are preserved; pending LanguageChange before construction may explain some picture state, but must not hide actual failures when an already open editor changes language. No product changes are included.

Extend the existing real-main driver with8rows: Fingerprint Local/Compare, Update available and About, each initially EN/ZH at fixed540px. Apply initial language and explicitly deliver queued LanguageChange before constructing the real editor; capture this stable-construction baseline. Then change EN↔ZH twice while the same real editor stays open and drain LanguageChange before each observed state. No cross-width comparison or new formatter is an oracle. Visible pageTitle must equal the actual active editor title after every transition. Fingerprint's real Help mode caption must remain localized Show image; comparison must retain explicit localized Reject/Trust after verification, Reject default and nondefault Trust. Preserve complete5a/6b hashes, accepted/rejected signal counts zero during language changes, original ServerConfig and owned isolation/no-core/no-permission constraints. Capture every phase and actual captions. Aggregate UI errors only after both directions and strict checks execute; never invoke trust/download/install/links.

The default matrix adds these8actual rows; a separately selectable `live-language` batch records RED first. Testing fixture construction drain is distinct from active-editor retranslation. No fresh acceptance is claimed by this contract; PE reviews actual code before its execution and paired product correction follows meaningful RED.

## 中文

PM授权在不变UI-003/012/014及R4规格/包内建立真实回归。旧540图保留；构造前待投递语言事件可能解释图片异常，但不能掩盖已经打开编辑页再改语言的真实问题。本工件不改产品。

复用真实main driver，共8行：本机/比较指纹、可用更新、关于，各以EN/ZH初始语言固定540宽。先设初始语言并明确投递队列LanguageChange，再构造真实编辑页、保存稳定基线；同页保持打开，两次EN↔ZH切换/drain后观测。不能用不同宽度互证或新formatter作期待。每轮可见pageTitle须等于真实活动editor标题；指纹Help须保留本地化Show image，比对须保留明确Reject/Trust after verification、拒绝默认/信任非默认。5a/6b完整hash、切语言中accepted/rejected信号零、原ServerConfig及隔离安全严格保留。逐阶段截图和实caption回执，两次切换及严格检查全走完后汇总UI失败，不点信任/下载/安装/外链。

默认矩阵新增8实际行，live-language组先单独记录RED。构造前drain与活动页即时翻译不同；PE先审实际代码再执行，真实RED后才配对改产品，不将合同当通过。
