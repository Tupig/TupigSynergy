# Generated artifact cleanup / 生成产物清理

## English

The user explicitly reports excessive app copies and storage use. This cleanup removes only reproducible generated outputs after the tester relinquished their use. It does not remove source, the supplied design, reports, compiler objects, installed vcpkg dependencies, downloads or dependency caches.

Before cleanup the build tree occupied 5,678,296 KiB. Removed 76 no-longer-needed generated Mach-O test executables and one obsolete extracted app plus its downloaded DMG (78 selected paths). Their recorded allocated blocks total **3.533 GiB**. Every selected path is now absent; the build tree is **2,093,216 KiB (1.996 GiB)**. APFS free-space reporting can lag or include snapshots; the reported reclamation is the sum of allocated blocks on the removed paths, not a guarantee about global free-space change.

Two current bundles remain temporarily in build/bin: the production app and the one R4 isolated UI probe. Two diagnostic executables and one test core remain for the active check. Reuse the existing probe; create no new app variants or duplicate bundles. After native verification remove the probe and keep one production app. Future tests should build/run/remove individual test executables so the roughly 4 GiB aggregate does not accumulate again.

The former build/ui-visual-r4-evidence directory and older R2/R3 probe bundles were already absent when inventoried around23:56, before the00:04 cleanup script. Their disappearance cause is unconfirmed; earlier in this run the PM/PE actually inspected their images. They were not removed by this cleanup, and their historical reports do not substitute for missing raw evidence. New checks use a separately named dated evidence directory and fresh source/binary identities. No historical result is recreated or represented as new original evidence.

Per-path SHA256, allocated bytes, retention and regeneration method are in the adjacent JSON receipt. No installed TuPig/Synergy bundle was found directly in /Applications or the user's Applications directory; those directories were not modified. No app process was terminated by this cleanup.

## 中文

用户明确反馈应用副本过多、占用空间。本次在测试方交出使用窗口后，仅清理可重新生成的构建产物；源码、设计稿、报告、编译对象、vcpkg依赖、下载与依赖缓存均保留。

清理前build占5,678,296 KiB。移除76个已无需保留的Mach-O测试程序，以及一份旧临时解包app和对应DMG，共78个路径；它们记录的已分配磁盘块合计 **3.533 GiB**。所有选中路径已核实不存在，build降为 **2,093,216 KiB（1.996 GiB）**。该数字为移除路径的已分配磁盘块合计，APFS快照或延迟可能影响系统全盘可用空间显示，不把它冒充全盘自由空间增量。

build/bin暂留当前正式app及唯一R4隔离界面探针，另保留两个当前诊断程序和一个测试core。后续复用同一探针，不新增app变体或重复bundle；原生验证后删除探针，只留正式app。测试按单目标构建、运行后清理，避免约4GiB测试程序重新堆积。

旧build/ui-visual-r4-evidence目录及R2/R3探针在约23:56清点时已经缺失，早于00:04清理脚本，消失原因未确认；本轮稍早PM/PE确实读取过原图。本次清理未删除该目录；历史报告不能替代已缺失的原始证据。新检查使用另行注明日期的目录及新源码/二进制身份，不伪造恢复旧原件。

相邻JSON回执记录每个路径的摘要、磁盘块、保留及重建办法。/Applications和用户Applications直属目录未发现TuPig/Synergy安装包，本次未改这两个目录，也未结束任何应用进程。
