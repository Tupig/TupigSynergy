# Native MSI fixture intake / 原生 MSI 夹具取证

## English

This is an actual test-intake artifact for V64-MSI, not a product package verifier.
Run `scripts\ci\test-msi64-intake.bat x64 run01` or `arm64 run01` only on the
approved disposable Windows CI host after independent check-adequacy review.
Use a fresh run name; existing evidence is never deleted. CMake, pinned WiX
5.0.2, certutil and dumpbin are preinstalled CI tools. No custom environment
variable or Python runtime is required.

Append `paired` as the third argument, for example
`scripts\ci\test-msi64-intake.bat x64 run02 paired`, to run the same six
fixtures through the real read-only MSI audit entry. Matching-target payloads
must pass; i386, wrong-target and malformed payloads must fail with precise
stream/action diagnostics. Three copied metadata negatives cover unknown
objects, missing exports and duplicate Binary rows. Missing verifier/helper APIs
are test failures. Production extraction uses pinned WiX source/data exports,
never an administrative install. Full application layout/dependency and native
startup acceptance remain separate from these synthetic format checks.

The ASCII hex files contain inert controlled PE32+/PE32 headers, a truncated
negative, text and an icon. They are test inputs, never application artifacts or
executed programs. The batch/CMake entry decodes them with the documented
`certutil -decodehex`, builds six synthetic MSI databases with the direct WiX
CLI, and decompiles each twice into source and raw WiX-data form. It never uses
MSBuild/SDK targets, `msiexec`, `msi validate`, extensions, service installation,
custom-action sequencing, or payload execution. WiX 5.0.2 has no legacy `-sval`
build switch: ICE validation is a separate subcommand and SDK target.

The first native run must capture version/help, immutable MSI hashes, exact
File/Binary/Icon/CustomAction/SummaryInformation table output and every exported
payload hash. All controlled native and data exports must round-trip; unknown or
missing exports stop the intake. Its round-trip result explicitly says `ROUNDTRIP_ONLY` and
`PRODUCT_MSI_VERIFIER=NOT_RUN`. With `paired`, separate `PAIRED_PRODUCT_CHECK` and `PAIRED_MAPPING_CHECK`
records establish the production checks. The i386/wrong-target/malformed cases
must be rejected by the production verifier; successfully exporting them is **not**
passing 64-bit policy. Both target jobs and real application MSI packages remain
separate acceptance work. Do not change the assumed native schema until its
actual output has independent review.

## 中文

这是 V64-MSI 的实际夹具取证入口，不是产品包验证器。须先经独立充分性审查，
再在批准的一次性 Windows CI 中按上方命令运行；使用新的运行名，不删除旧证据。
依赖现有 CMake、固定 WiX 5.0.2、certutil 和 dumpbin，不新增 Python 或必需自定义环境变量。

第三个参数显式传 `paired`（例如 `scripts\ci\test-msi64-intake.bat x64 run02 paired`），
便把同组六夹具送入真实只读 MSI 审计入口：匹配目标的载荷须通过，32 位、错误目标和损坏
载荷须被拒绝，并报告精确 stream/action。三个复制的元数据反例覆盖未知对象、缺失导出和
重复 Binary；缺少生产入口或 helper API 必须报错。生产提取采用固定 WiX source/data
双导出，不进行管理安装。合成格式检查与真实应用布局、依赖闭包和原生启动验收分别记录。

ASCII 十六进制文件仅为受控、不可执行的正反例输入，不是交付应用。
入口只构建合成 MSI 数据库并只读反编译；不调用 MSBuild/SDK 验证、msiexec、ICE、
custom action、服务安装或载荷执行。精确 5.0.2 没有旧版 `-sval` 开关，
直接 CLI 构建与独立验证子命令必须区分。

首次原生运行须保留版本、帮助、MSI 输入前后摘要、完整相关表和全部导出对象的摘要。
未知或缺失导出一律停止，不能根据源 authoring 猜测已成功取证。
往返结果明确是 `ROUNDTRIP_ONLY`、当时产品 MSI 验证器未运行；启用 `paired` 后另有
`PAIRED_PRODUCT_CHECK` / `PAIRED_MAPPING_CHECK` 记录。32 位、错误 ABI 或损坏
反例能被反编译，不代表通过 64 位政策。启用配对检查时必须由实际产品验证器拒绝这些反例，
并分别验收两个目标和真实应用 MSI。
