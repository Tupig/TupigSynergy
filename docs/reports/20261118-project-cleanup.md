# Repository consolidation receipt / 仓库整理回执

## English

Documentation moves and the bounded generated-file cleanup have been executed. Author byte/type checks and a fresh independent D01–D08 checker execution passed. The tester will rerun the checker after this final receipt update. This receipt records directory maintenance, not a new product build, GUI run, release or installation.

### Executed changes

| Item | Result |
|---|---|
| Stage reports | 35 moved from `docs/reports` to `docs/archive/ui-20261117`, with identical bytes, hashes and file types; no redirect copies |
| Previous UI verification | Original saved as `docs/archive/ui-20261117/ui-verification-before-20261118.md`; current page rewritten with current procedure and evidence limits |
| Navigation | `docs/README.md` is the maintained index; root README points to it and describes the retained directory layout |
| Stale README examples | Removed duplicated current-version literals and outdated prebuilt-workflow description; `cmake/Version.cmake` remains the version authority, with build details linked to the existing guide |
| Generated cleanup inputs | Two input files moved byte-identically from `docs/reports` into `build/ui-current-evidence-20261118/project-cleanup` |
| Tester cleanup | 24 eligible generated files removed, 0 retained/skipped; 150316 logical bytes removed; removed files accounted for 217088 allocated bytes; five empty directories removed |

The five removed empty directories were `tmp/test`, `tmp`, `build/tmp/test`, `build/tmp` and `scripts/ci/__pycache__`. The root `.DS_Store` remains. The allocated-byte total is not a measured APFS free-space increase. No system service was stopped. Source, raw test evidence and dependency caches were retained.

The [migration map](../archive/ui-20261117/migration-map.json) contains the 35 source/destination/hash/size identities, the verification snapshot and the two local generated inputs. The [JSON receipt](20261118-project-cleanup.json) provides sanitized per-item cleanup identities and local receipt paths/hashes. Public receipts use repository-relative paths; absolute machine paths and full local checker output are not copied here.

### Preservation and evidence interpretation

The three retained current reports remain byte-identical: [R4 total](20261118-ui-visual-r4.md), [space receipt](20261118-ui-space-cleanup.md) and [space JSON](20261118-ui-space-cleanup.json). All seven `docs/aegis` execution packages, the master specification and root build package remain unchanged. The four root Aegis records stay local and excluded through existing Git policy; the PM owns status/journal updates. Author rehashing matched all 1095 frozen source/build inputs. Other user-guide paths/content and source/CI files were not edited.

Archived bodies retain their historical references. The mapping distinguishes `mapped_report`, `current_file`, `removed_with_receipt`, `unavailable_before_cleanup` and `external_history`. In particular, the old `build/ui-visual-r4-evidence` directory was unavailable before the recorded cleanup; reports and recorded hashes do not recreate it or establish current raw-file availability.

Final probe cleanup receipts are exactly `build/ui-current-evidence-20261118/final-probe-cleanup-before.json` and `build/ui-current-evidence-20261118/final-probe-cleanup-after.json`. These paths take precedence over the older bare `native-final` path in the frozen R4 report. That report was deliberately not rewritten during this migration.

The single remaining product/preview output bundle is scoped to `build/bin/TuPig Synergy.app`. Qt `Designer.app`, `pixeltool.app` and `Info.plist.app` templates in `build`/`vendor` dependency caches remain as dependency tools/templates; they are not copied project preview apps. This receipt does not claim that the whole disk contains only one bundle.

### Verification and rollback

The immutable baseline is `build/ui-current-evidence-20261118/project-cleanup/before-v2.json`, SHA256 `26b39047d80cbeb0c1be3ce68c62899b712433c7a4d939b8751cba1f4c910008`. The accepted checker is `check_directory.py` in the same directory, SHA256 `5f3b360d6f834caa895cb62e94cfeb78e97d629e5e6e94bc5348f234ea4fce07`. The tester freshly executed `after` with exit 0: all D01–D08 passed, 1095 source inputs matched, 35 report moves/three retained reports/two local input moves remained byte-identical, all 68 current local Markdown targets passed, and all 24 removed generated files were independently absent. The whitelist, protected hashes, bilingual structure and diff checks passed; `build/bin` contains only the product app and the named test/preview processes were absent. A final `after` rerun follows this receipt update.

Rollback follows the map: restore each archived report to its original path with identical bytes, restore the verification snapshot, and recover the working README from the local baseline snapshot or Git as appropriate. Historical raw absence remains documented. The completed first-pass local output is `build/ui-current-evidence-20261118/project-cleanup/after-v2.json`, observed SHA256 `715b0a181ac88de152975afa99fbe58d7db98e8adb1acae2ad17112292e42653`. The independent receipt is `build/ui-current-evidence-20261118/project-cleanup/independent-final-recheck.json`, SHA256 `b1c85d4e536fbce03df4b380a7028041356156c009fedcc09f9b4576267a3eff`. The final rerun regenerates `after-v2.json`; the observed hash identifies the completed pass rather than predicting that future file's identity. The separate independent receipt retains that result.

The authorized GitHub delivery chain follows consolidation review; no package, native drag or hardware acceptance is added by this receipt.

## 中文

文档迁移和有界生成文件清理已执行，作者逐字节及类型核对，以及独立 D01–D08 检查器新执行均通过；测试代理会在本次最终回执落盘后复跑检查器。本回执记录目录维护，不代表新增产品构建、GUI 运行、发布或安装。

### 已执行改动

| 项目 | 结果 |
|---|---|
| 阶段报告 | 35 份从 `docs/reports` 迁入 `docs/archive/ui-20261117`，字节、摘要及文件类型不变，无跳转副本 |
| 原界面验证说明 | 原件保存到 `docs/archive/ui-20261117/ui-verification-before-20261118.md`；当前页面改写为现行流程及证据边界 |
| 导航 | `docs/README.md` 为统一维护入口；根 README 链接该索引并说明保留的目录结构 |
| README 过期示例 | 移除重复当前版本数字及过期预编译流程描述；版本仍以 `cmake/Version.cmake` 为准，构建细节链接既有指南 |
| 生成清理输入 | 两份输入从 `docs/reports` 同字节迁入 `build/ui-current-evidence-20261118/project-cleanup` |
| 测试代理清理 | 删除 24 个已批准生成文件，保留/跳过 0；删除文件逻辑字节合计 150316、分配字节合计 217088，并移除五个空目录 |

移除的五个空目录为 `tmp/test`、`tmp`、`build/tmp/test`、`build/tmp` 和 `scripts/ci/__pycache__`。根 `.DS_Store` 保留；分配字节合计不等同实测 APFS 空闲空间增量。没有停止系统服务；源码、原始测试证据和依赖缓存保留。

[集中迁移映射](../archive/ui-20261117/migration-map.json)包含 35 份报告的旧/新路径、摘要与大小，以及验证快照和两份本地生成输入。[JSON 回执](20261118-project-cleanup.json)列出清理逐项身份的公开安全版本及本地回执路径/摘要。公开回执只使用仓库相对路径，不复制本机绝对路径或完整本地 checker 输出。

### 保留与证据解释

三份现行报告保持原字节：[R4 总报告](20261118-ui-visual-r4.md)、[空间回执](20261118-ui-space-cleanup.md)及[空间 JSON](20261118-ui-space-cleanup.json)。七份 `docs/aegis` 执行包、主规格与根 build-package 保持不变；四份根 Aegis 记录继续按现有 Git 策略保留本地并排除，status/journal 由 PM 更新。作者重新计算的 1095 份冻结源码/构建输入摘要全部相符。其他用户指南路径/内容及源码、CI 均未编辑。

归档正文保留历史引用。集中映射区分 `mapped_report`、`current_file`、`removed_with_receipt`、`unavailable_before_cleanup`、`external_history`。旧 `build/ui-visual-r4-evidence` 目录在所记清理前已经不可用，报告及历史摘要不能重建它，也不证明原件当前存在。

最后探针清理的准确回执是 `build/ui-current-evidence-20261118/final-probe-cleanup-before.json` 与 `build/ui-current-evidence-20261118/final-probe-cleanup-after.json`。以上优先于冻结 R4 总报告中较早的 `native-final` 裸路径，本次迁移按要求不重写该报告。

“仅剩一个产品/preview 输出 bundle”的范围限定为 `build/bin/TuPig Synergy.app`。`build`/`vendor` 依赖缓存中的 Qt `Designer.app`、`pixeltool.app` 及 `Info.plist.app` 模板继续保留，属于依赖工具或模板，不是项目复制的试验 app；不宣称全磁盘仅有一个 bundle。

### 验证与回滚

不可变基线为 `build/ui-current-evidence-20261118/project-cleanup/before-v2.json`，完整 SHA256 见英文段；已接受的 `check_directory.py` 与其摘要亦见上文。测试代理已新执行 `after`，exit 0，D01–D08 全通过：1095 输入一致，35 份迁移/三份保留报告/两份本地输入同字节，68 个当前本地 Markdown 目标检查通过，24 个删除项独立核对均不存在；差异白名单、受保护摘要、双语及 diff 检查通过，`build/bin` 仅产品 app，无所列测试/preview 进程。本次回执落盘后再最终复跑。

回滚按映射将报告原字节移回旧位置，恢复验证说明快照，并按需要从本地基线快照或 Git 恢复 README；历史原件缺失继续如实保留。首次实际通过输出为 `build/ui-current-evidence-20261118/project-cleanup/after-v2.json`，独立结果为同目录 `independent-final-recheck.json`，两份完整观测摘要见英文段。最终复跑会重新生成 `after-v2.json`；上方摘要只标识已完成通过，不预告未来文件身份，独立回执保留该次结果。

已授权的 GitHub 交付在本次整理审查后继续，本回执不新增包、原生拖拽或硬件验收结论。
