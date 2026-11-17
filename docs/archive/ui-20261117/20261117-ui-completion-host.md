# Host and update delivery / 主窗口与更新交付

## English

Scope: assigned host name, update-page and update-download presentation changes only. Existing dirty work is preserved. No build, installation, settings mutation, commit or publication is performed by this delivery agent.

Todo:
- Completed: inspect the approved package, actual checks and assigned source.
- Completed: implement explicit name save/cancel, neutral IP presentation and update-page lifecycle.
- Completed: implement update-card headings and truthful download rate/ETA.
- Completed: apply PM action-height correction and refresh source identities for independent testing/review.

## 中文

范围：仅负责分配的名称编辑、更新页面与更新下载展示修正，保留既有未提交改动。本交付代理不执行构建、安装、日常设置修改、提交或发布。

实时待办：
- 已完成：核对已批准执行包、实际检查清单及负责源码。
- 已完成：实现名称显式保存/取消、中性 IP 展示及更新页面生命周期。
- 已完成：实现更新卡片内部标题和真实下载速率/剩余时间。
- 已完成：落实PM操作按钮高度修正并刷新源码身份供独立测试与审查。

## English evidence

Pre-edit identities rechecked: spec `46cbb513917138c8f65bf2ba89e9fa779441cc5fb9c63a01869ad5ef01597aa0`; package `2b3a4e79973be2094114c5f894dabea896494a01047a0d96273b0ba98f58b531`; actual manual checks `c4f99e4434d30ff24a74d4590015d4dfd5230c75fd85fd107f64f9a316d0f425`, accepted independently by PM/PE before product edits. Covered checks: M-C01/05/06/07.

Implemented explicit name Save/Cancel, Enter as Save and Escape as Cancel (including button focus); focus loss retains the draft. Save/editor entry require writable settings and stopped core. Core start/restart controls cannot become enabled through unrelated updates while editing. The PM additionally authorized only the coherent name-editor state handling in MainWindowConnection.cpp; running transitions discard the name draft without writing it. Existing name validation and server/settings behavior remain.

Checking progress now belongs to MessagePage card content. Manual cancellation disconnects result callbacks before deleting the owned checker; one terminal flag prevents repeat results. Failure presents Close plus primary Retry, disables Retry on activation, closes the failure page, then invokes checkForUpdates(true). The protected factory defaults to a real, MainWindow-owned UpdateChecker and enables independent loopback tests without production endpoint configuration changes.

Update cards have inner heading labels; the release/later/install action row retains reference-standard 32px buttons after PM review, and only the disclosure is small. Download presentation uses successive byte samples, one owned 500ms timer and a 1.5s no-progress boundary. A stalled/zero-rate interval displays Waiting for network without positive speed or finite ETA; completed receipt displays Verifying download until the unchanged downloader digest check completes. Positive measured speed uses significant digits so tiny positive rates do not round to a displayed zero with finite ETA. Reject stops the timer and deletes the existing owned transfer. URL policies, SHA-256 verification, installer launch/confirmation behavior are untouched. IP text uses the existing theme muted tier with original truthful invalid-IP tooltips retained.

Fresh source validation: bounded git diff --check exited 0 after clang-format on the owned files. No build or runtime test was executed by this agent. Independent tester owns compile, regression and loopback evidence; PM owns actual CUA observations, and independent PE must still review the final sources. No M-C row is claimed passed here. Added translation strings were handed to ui_delivery, which exclusively owns zh_CN. Product files are frozen pending independent findings.

## 中文证据

修改前重新核对主规格、执行包及实际人工检查表摘要，见上文；检查表已由PM/PE独立接受后才开始产品修改，覆盖M-C01/05/06/07。

名称改为显式保存/取消，Enter保存、Escape取消（按钮焦点也适用），失焦保留草稿。进入与保存须设置可写且core停止。编辑中其他状态更新不能重新启用启动/重启。PM追加授权MainWindowConnection.cpp的名称编辑状态一致性处理；core离开停止态时丢弃草稿但不写设置。原校验与服务端设置语义保留。

更新检查进度移入提示卡片内部，取消先断开结果回调再销毁检查器，并用单一终态标记防重复结果。失败页提供普通关闭和主操作重试，重试先禁用自身并关闭失败页，再调用真实checkForUpdates(true)。受保护工厂默认返回归主窗口所有的真实检查器，仅便于独立loopback测试，不改变生产端点配置。

更新卡片改为内部标题；根据PM审核，发布页/稍后/安装操作保留设计稿标准32px高度，仅披露按钮用小号。下载按相邻字节样本计算速率，只有一个500ms计时器，超过1.5秒无进展即显示等待网络，不展示伪正速率或有限剩余时间；收齐字节但校验未完成时显示正在验证。正速率用有效数字避免微小值被显示为零却仍有有限ETA。取消停止计时器并销毁所属传输；URL白名单、SHA-256、安装启动/确认行为均未修改。IP使用既有主题弱化文字色，保留真实无效IP提示。

本代理新执行的检查仅为限定文件git diff --check（退出0）及源码检查，并格式化负责文件。未运行构建或运行时测试；独立测试代理负责编译、回归和loopback，PM负责CUA观察，最终独立源码审核仍需执行。本报告不把任何M-C条目标为通过。新增翻译字符串已交付独占zh_CN的ui_delivery代理。产品文件现冻结等待独立发现。

## Source identities / 源码身份

| Path | SHA-256 |
|---|---|
| `src/lib/gui/MainWindowDesign.cpp` | `175ec5dc3ef5b1d0050d0c0c04eabd579e65bc4c68b55b82676b35fa1a18dda5` |
| `src/lib/gui/MainWindowDesign.h` | `344c6308224a6da91b27710bc34f08c5d20e4dad406656975c058e07c31e534e` |
| `src/lib/gui/MainWindow.cpp` | `1756a290b3216890bb54b5ce3295e3a8189e9d3bc34faf894c4cb888ff5382cd` |
| `src/lib/gui/MainWindow.h` | `dde998f3f7f2cb790107887ec8aeac676dfa9142b3401d552b9339277a440b21` |
| `src/lib/gui/mainwindow/MainWindowConfiguration.cpp` | `8f7cc9d772f8a359686d04d826c52e95c3b49da23187c6f45dcd5858e32f656f` |
| `src/lib/gui/mainwindow/MainWindowConnection.cpp` | `751c448e71ac49c5fe6b97fe4bfd5a1a3a1fb0c10bc930e6caf3187be17a70f0` |
| `src/lib/gui/mainwindow/MainWindowWindow.cpp` | `72e8655ec4687920f874e79808fa6ba42971d6b97e929615df303d9ce537f042` |
| `src/lib/gui/update/UpdateDialog.cpp` | `d8ff3ca67f69ccde23abce4552406ab7cd2f872a034ff4187ec92ecfb054dfe8` |
| `src/lib/gui/update/UpdateDialog.h` | `ed5e57a629fc18683b7b4b42c3ae79e25d61c24d03b4299545d734304e9d5449` |
| `src/lib/gui/widgets/MessagePage.cpp` | `091996589f52fd634bfff2ba2e9529473b146227e7d866ab0650b1815f5a2ecf` |
| `src/lib/gui/widgets/MessagePage.h` | `0814457e46cac18b6916abbed275f7f45ebaa59060d6fca59fbfcfa578c5a728` |
