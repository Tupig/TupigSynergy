# ADR: GitHub-only delivery / 仅GitHub交付

## English

Decision2026-11-17: obey the user's explicit instruction to start public versions at2.1.1 and replace local-build use with then-latest GitHub Release. Local2.1.7 and R2 results are historical, not distribution authority. Current GitHub latest is1.21.11; no existing2.1.1 release/tag is overwritten. ReusePR379, require reviewed source and actual CI, tag the integratedmain SHA and publish only both-native successful artifacts. Remove automatic source-bumping CI commits that would separate testedsource from releaseidentity; keep existing versioncomposition and releaseworkflow otherwise bounded. App migration/security/protocol policies unchanged.

Risk/high: public packages and ordinary installed startup can affect user data. Mitigation: no unreviewed source/secret/localdiagnostics publication; exactsource/tag/run/assetsha/provenance; selfcontainedpayload audit; privately back up appownedsettings; recoverable oldapp removal; no permissiongrant or actualsharing. Existing tags/assets remain. Rollback: retire the newinstalledbundle recoverably and restore priorapp/settings only if needed with exactbackup identity; never rewrite publictag code. A failedpublishedbinary is fixed in a newincrementedGitHubversion. Task remainsactive until release/download/install/latesttests complete or a genuineexternalblock is evidenced.

Authorization: direct user full delegation plus explicit GitHub package/version/uninstall/Release/latest-test instructions; this replaces only earlier localdelivery exclusions. No manual/hardwareacceptance is fabricated.

## 中文

2026-11-17决策：遵照用户明确要求，公开版从2.1.1开始，改为GitHub Release届时latest。local2.1.7/R2只作历史，当前GitHub latest1.21.11，不覆已有2.1.1。沿PR379先审源码/真实CI，绑定整合main SHA的tag，双原生平台成功才发。移除会导致已测源码与交付身份分离的CI自动版本提交，保留其余版本组合/发布流程，迁移/安全/协议不改。

风险high：公开包和正常安装启动可能影响数据。按源码/tag/run/hash/来源证明和依赖完整性核验，设置私下备份、旧app可恢复停用，不公开秘密/本地诊断，不授权限或真实共享。旧tag资产保留；必要时可恢复旧app/设置，但不改公共tag代码，发布后缺陷用新GitHub版本修。用户全权及明确GitHub/2.1.1/卸载/Release/latest测试授权取代旧本机限制，不伪造人工/硬件验收。
