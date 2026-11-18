# R4 owned-window drag trace plan / R4局部窗口拖拽跟踪计划

## English

Pending bounded test-only observation under accepted M08 and the unchanged R4 package/master. PM native CUA Add-click/delete/guards/Cancel were observed, while attempted Add/grid drags showed no model change and screenshots retained the pointer at the source. This is an evidence gap, not a product drag-failure claim. No product drag change is authorized by this plan.

Extend existing R4 manual observer only when `SYNERGY_R4_DRAG_TRACE=1` and automated mode is absent. Install a passive QObject application event filter that records only widget events within the owned isolated MainWindow and only Add/Delete tools, grid/view viewport and their legacy forwarding widgets. Record MouseButtonPress/MouseMove/MouseButtonRelease local position/buttons/modifiers/receiver identity plus DragEnter/DragMove/Drop/DragLeave type/action/MIME type, time and pre-dispatch acceptance. Always return false; never synthesize or mutate events, accept/reject drops, change focus/geometry or intercept product behavior. Do not record key text, clipboard data or unrelated windows.

On relevant pointer events queue a zero-delay observation of QDrag children beneath the owned window. Record newly observed QDrag object/source identity, safe MIME type, actionChanged/targetChanged and destruction; explicitly distinguish object existence from proof of completed drag. Connect the real grid model dataChanged/rowsMoved/modelReset/layoutChanged signals and record changed indexes, avoiding data payloads/device names. Write append-only JSONL to the same owned evidence directory with current source/binary/launch identity. Do not hold event pointers across dispatch or fabricate post-dispatch acceptance. Startup and native Quit flush the file by closing after each append.

PM repeats actual CUA Add→empty, existing tile→empty and tile→Delete on that freshly identified isolated variant; review raw events to distinguish missing delivered moves/button-state, delivered threshold-crossing moves without QDrag, and QDrag/drop/model events. A trace that receives no movement is a tool-delivery limitation, not evidence that product dragging failed. Screenshots plus observed model changes and inherited rollback/guards remain required. No global input injection, accessibility permission, event tap, OS automation fallback or ordinary app interaction. Tester remains out of GUI while PM owns native observation.

Entry: actual observer implementation reviewed by PE before building it. Exit: trace identity and actual received event sequence retained, uncertainty and tool limits explicit; no automatic drag-success claim. Final automated tests and native binary acceptance remain distinct.

## 中文

这是绑定M08和不变R4包/规格的待执行测试观察计划。PM已观察点击新增/删除/保护/取消；CUA尝试拖动没有模型变化且截图指针仍在起点，当前是证据缺口，不能直接归产品失败，不授权据此改产品拖拽。

只在手动R4且SYNERGY_R4_DRAG_TRACE=1时装被动QObject应用过滤器，范围仅隔离MainWindow内新增删除、网格viewport及旧转发控件。记录Press/Move/Release位置/按钮/接收者和DragEnter/Move/Drop/Leave类型、动作、MIME类型、时间及投递前acceptance；始终返回false，不注入/改写/吞事件，不改变焦点几何或业务。不记录键盘正文、剪贴板或其他窗口。

相关鼠标事件排零延时观察本窗口QDrag子对象，记录新对象/source/MIME类型、actionChanged/targetChanged/销毁；对象存在不能自称拖拽成功。绑定真实model的dataChanged/rowsMoved/reset/layoutChanged，仅记索引不抄设备数据。日志在同证据目录追加JSONL，每次关闭落盘，绑定源码/二进制/启动身份；不跨分派保留event指针、不伪造分派后acceptance。

PM在新身份隔离实例重复CUA新增→空格、已有→空格、已有→删除，根据原始事件区别未投递Move/按钮态、已越阈值但无QDrag、以及真实QDrag/drop/model变化。无Move只能记工具投递限制，不归产品。原图/模型变化及保护/回滚仍须验证。禁止全局输入注入、权限、event tap、系统自动化绕行和日常应用；PM独占时测试代理不跑GUI。

实际observer须PE审后才构建；最终保留真实事件和限制，不自动宣称拖拽通过，自动检查/原生交付程序验收分列。
