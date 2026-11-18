# Native acceptance checklist / 原生验收检查表

## English

Contract lineage: C-UI-R1/R2/R3 and UI-001,003,005,011,016. This is an actual manual check artifact for the follow-on correction, written before paired product changes. The PM owns CUA; the tester does not run a second CUA session. Status: awaiting independent adequacy review; all observations pending.

Safety and setup: use only the independently accepted `build/bin/R2UiPreview.app` harness, its generated canonical temporary root and synthetic Test-Server/Studio/Workstation fixture. Record preview source/executable hashes, marker receipt, language and width. Observe both English and Chinese, at540/600/820px. Never invoke real core, permissions, live sharing, clipboard-copy, external links or actual update endpoints. Disabled preview commands are an explicit diagnostic difference. Stubbed/check-state fixtures may render cards, but never count as real release-download acceptance.

| ID | Procedure and required result | Evidence to retain | Status |
|---|---|---|---|
| M-C01 | Open the main computer-name editor. Type a synthetic replacement. Focus loss and explicit Cancel each leave displayed and persisted original unchanged. Escape cancels. Reopen, type another replacement, use Save, and verify displayed value and temporary settings agree after reopen; Enter commits only as the explicit Save equivalent. | Before/edit/after captures and redacted synthetic settings-value receipt; language/width. | Pending |
| M-C02 | In Server layout, select the primary tile, empty space and external-config mode in turn: Delete is visibly disabled. Select a nonprimary named tile: explicit Delete is available and removes only that draft tile. Cancel then reopen restores original list; repeat Delete+Save then reopen preserves the removal. | Captures of all disabled/enabled states; before/after synthetic saved screen-name lists. | Pending |
| M-C03 | In Hotkey, use the associated-action context/configure route to open the real nested Action editor. Modify a field, Cancel child, and verify parent action unchanged. Save child then Cancel parent, reopen from Server, and verify persistent action unchanged. Save both child and parent and verify reopened value. Back rejects only the current child. | Context/child/parent captures and synthetic Action serialization comparison; record each route. | Pending |
| M-C04 | In Preferences Network/Security, change an unsaved harmless field, then use View Fingerprint. The real certificate fingerprint page shows every SHA-256 byte and Back returns to the same preferences draft. Cancel keeps prior persistent values; reopen confirms no write. Use only a generated fixture certificate under the temporary root. | Source fixture certificate digest, full hash capture, before/after draft and saved-value receipts. | Pending |
| M-C05 | Use a deterministic diagnostic check-state fixture: checking progress is inside the message card and actions stay adjacent. On controlled failure, a Retry action is visible and routes exactly once to checking state; Back/Cancel do not leave an orphan progress card. Do not contact a live update endpoint. | Checking/failure/retry captures and route count from fixture receipt; disclose fixture limitation. | Pending |
| M-C06 | Render real UpdateDialog download states using isolated loopback/diagnostic data. Each titled card uses an inner heading. At zero received-byte rate, show a truthful waiting state without fabricated positive speed or finite ETA. At a known positive rate, display a consistent rate/ETA; Cancel ends the owned transfer. | Full-size waiting/progress/cancel captures and loopback receipt; retain prior downloader security tests. | Pending |
| M-C07 | On the actual preview main window, inspect identity/IP text in both languages at every width. IP uses the neutral muted text tier and remains readable; no magenta/purple accent. Do not treat symbolic stylesheet names as pixel evidence. | Full-size main screenshots plus sampled pixel/contrast description, keeping OS focus selection separate. | Pending |

Checks M-C01..04 require real production widgets/bindings in the isolated preview; a picture-only replica is insufficient. M-C05/06 may use test fixtures because remote update calls are excluded. Root records actual observations and tester reconciles them against raw screenshots; no Pending row constitutes acceptance.

## 中文

契约沿革：C-UI-R1/R2/R3，对应 UI-001、003、005、011、016。本文件是后续修正的实际人工检查工件，在配对产品修改前写成。PM 独占 CUA，测试代理不启动第二个 CUA 会话。状态：等待独立充分性审核；所有实测仍待执行。

安全准备：只使用独立审核接受的 `build/bin/R2UiPreview.app`，以及它生成的规范临时目录与 Test-Server/Studio/Workstation 合成数据。记录源码/执行文件哈希、marker回执、语言和宽度；中英文各覆盖540/600/820px。不得启动真实core、授权权限、真实共享、复制系统剪贴板、打开外链或调用真实更新服务。预览禁用命令须作为诊断差异披露；合成状态不代替真实发布下载验收。

| 编号 | 操作及必须观察到的结果 | 保留证据 | 状态 |
|---|---|---|---|
| M-C01 | 主页编辑计算机名。输入合成新名后分别失焦、取消和Escape，显示名及持久化值均不变。重开编辑后保存另一个名称，显示与临时设置一致；Enter只能作为明确保存的等价操作提交。 | 修改前/编辑中/操作后截图，合成设置值回执，语言及宽度。 | 待办 |
| M-C02 | 服务端布局分别选中主机、空白和外部配置模式，删除按钮均禁用。选中非主机有名磁贴后允许删除且只改草稿；取消并重开恢复原列表，再删除+保存后重开保留删除。 | 各禁用/启用状态截图，保存前后合成屏幕名列表。 | 待办 |
| M-C03 | 从Hotkey的关联动作上下文/配置入口进入真实嵌套Action页。改字段后取消子页，父动作不变；保存子页后取消父页，从Server重开持久化动作不变；子父均保存后重开保留新值。返回只拒绝当前子页。 | 上下文/子页/父页截图，合成Action序列化对比及每个入口。 | 待办 |
| M-C04 | 首选项网络/安全页修改一个未保存的无害字段后查看指纹。真实指纹页显示完整SHA-256；返回时保留首选项草稿，取消后重开持久化值不变。证书只用临时目录内生成夹具。 | 夹具证书摘要、完整指纹截图、草稿及持久化前后回执。 | 待办 |
| M-C05 | 用确定性诊断更新状态夹具观察检查进度位于消息卡片内部，操作按钮相邻。受控失败可见重试，点击只路由一次回检查中；返回/取消不遗留进度卡。不得调用真实更新端点。 | 检查/失败/重试全尺寸截图及夹具路由计数，披露夹具限制。 | 待办 |
| M-C06 | 用隔离loopback/诊断数据呈现真实UpdateDialog下载状态；标题均在卡片内部。零字节速率时显示真实等待状态，不伪造正速度或有限ETA；已知正速度时速率/ETA一致；取消结束本轮传输。 | 等待/进度/取消全尺寸截图与loopback回执；保留既有下载安全测试。 | 待办 |
| M-C07 | 实际预览主页中英文三宽度检查身份/IP；IP为中性弱化文字且可读，不呈紫红强调。样式名不能代替像素证据。 | 全尺寸主页截图及像素/对比描述，另记系统焦点选择效果。 | 待办 |

M-C01..04须经隔离预览中的真实生产控件/接线；静态复制图不足。M-C05/06因排除真实更新服务，可用明确标注的测试夹具。PM记录实测，测试代理核对原始截图；待办行不代表验收通过。
