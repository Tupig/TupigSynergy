# macOS 签名、首次安装与权限 / macOS Signing, First Install, and Permissions

> **Language / 语言**: [English](#english) | [中文](#中文)

## English

### Boundaries

Apple Development signing gives the GUI and `synergy-core` stable identities
(`com.tupig.synergy` and `com.tupig.synergy.core`). It does **not** notarize a
release, guarantee Gatekeeper acceptance on another Mac, remove quarantine from
a later browser download, or silently preserve/grant Accessibility or Input
Monitoring approval. Developer ID signing and notarization are a separate mode.

The first setup deliberately transfers an encrypted P12 and its export password
to GitHub Secrets. A trusted manual runner must decrypt and import that private
key into a per-run Keychain to sign, then removes the temporary P12 and
Keychain on success or failure. PR, fork, push, scheduled, and automatic routes
do not receive signing secrets.

### One-time credential setup

In Xcode open **Settings → Accounts → Manage Certificates** and create/select
exactly one **Apple Development** certificate. In Keychain Access select that
certificate and private key, then **File → Export Items** to a
password-encrypted `.p12`. Never export an unencrypted key.

From the repository root, substitute your local P12 path and run:

```sh
./scripts/setup-macos-signing.sh /path/to/development.p12
./scripts/setup-macos-signing.sh --gui /path/to/development.p12
```

Terminal mode reads the password without echo. `--gui` uses a hidden macOS
dialog; the password is never put in an argument, terminal output, or file. The
setup rejects empty/multiple/unpaired/expired P12 identities and non-Apple
Development certificates. It is bound to the checkout rather than the current
directory and displays only the selected repository and non-secret fingerprint.

| GitHub name | Storage | Value |
| --- | --- | --- |
| `APPLE_P12_CERTIFICATE` | repository Secret | Base64-encoded encrypted P12 |
| `APPLE_P12_PASSWORD` | repository Secret | P12 export password |
| `APPLE_CODESIGN_ID` | repository Secret | selected SHA-1 certificate fingerprint |
| `MACOS_SIGNING_MODE` | repository Variable | `development` only after every upload succeeds |

The script first writes `MACOS_SIGNING_MODE=disabled`, so a failed partial
upload cannot select a stale identity. Never place the P12, password, private
key, or GitHub token in a command, issue, log, or source file.

### Workflow and first install

Use **Actions → TuPig Synergy → Run workflow** and the trusted manual
`workflow_dispatch` route. CI checks the exact configured identity in its
ephemeral Keychain, signs nested Mach-O code/bundles, then the fixed core and
GUI identities, and reads them back with strict verification. A pinned
`Install.command` is generated only beside a certificate-signed app.

`development` mode reports development signing and stops before notarization.
Choose the separate Developer ID mode only when its Developer ID certificate and
notarization credentials are configured; only it submits, staples, and reports
notarization.

Run `Install.command` only for a first install. It checks source, staged copy,
and destination against GUI/core IDs, strict signatures, and the configured
certificate fingerprint. It rejects an existing
`/Applications/TuPig Synergy.app`, including a symlink; quit an installed app
and use its built-in updater instead.

The sole permitted quarantine action removes `com.apple.quarantine` from a
private verified staged copy: before a first install, or before the in-app
updater exchanges its verified staged contents. The helpers never change
global Gatekeeper/spctl policy, create a trust exception, reset/grant
Accessibility or Input Monitoring, or change another app. The administrator
prompt authorizes only the first-install file write. In **System Settings →
Privacy & Security**, the user must approve Accessibility/Input Monitoring. A
later same-identity build can be checked for signing continuity, but actual TCC
retention requires a real user-approved system test. Never reset a valid grant
to manufacture a result.

## 中文

### 边界

Apple Development 签名会固定 GUI 与 `synergy-core` 身份
（`com.tupig.synergy`、`com.tupig.synergy.core`）。它**不会**公证发布包、保证
另一台 Mac 的 Gatekeeper 放行、移除未来浏览器下载的 quarantine，也不会静默保留/授予
辅助功能或输入监控权限。Developer ID 签名与公证是独立模式。

首次配置会有意把加密 P12 及其导出密码传到 GitHub Secrets。可信手动 runner 必须在每次
运行专用钥匙串解密并导入私钥来签名，完成或失败时都会删除临时 P12 与钥匙串。PR、fork、
push、定时和自动路径拿不到签名机密。

### 一次性凭据配置

在 Xcode 的**设置 → 账户 → 管理证书**创建/选择**唯一一张 Apple Development**证书。
在“钥匙串访问”同时选择证书和私钥，通过**文件 → 导出项目**导出带密码 `.p12`；不要导出
未加密私钥。

在仓库根目录运行（替换为自己的 P12 路径）：

```sh
./scripts/setup-macos-signing.sh /path/to/development.p12
./scripts/setup-macos-signing.sh --gui /path/to/development.p12
```

终端模式无回显读取密码；`--gui` 使用隐藏密码的 macOS 对话框，密码不会进入参数、终端
输出或文件。配置会拒绝空/多组/不配对/过期 P12 身份以及非 Apple Development 证书。脚本
绑定仓库检出而非当前目录，只显示选定仓库与非机密指纹。

| GitHub 名称 | 存储位置 | 值 |
| --- | --- | --- |
| `APPLE_P12_CERTIFICATE` | 仓库 Secret | Base64 编码的加密 P12 |
| `APPLE_P12_PASSWORD` | 仓库 Secret | P12 导出密码 |
| `APPLE_CODESIGN_ID` | 仓库 Secret | 选定证书的 SHA-1 指纹 |
| `MACOS_SIGNING_MODE` | 仓库 Variable | 所有上传成功后才为 `development` |

脚本先写入 `MACOS_SIGNING_MODE=disabled`，部分上传失败不会使下一次运行选中旧身份。切勿
把 P12、密码、私钥或 GitHub token 写入命令、Issue、日志或源码。

### 工作流与首次安装

在 **Actions → TuPig Synergy → Run workflow** 走可信的手动
`workflow_dispatch` 路线。CI 会在临时钥匙串确认精确身份，先签嵌套 Mach-O 代码/bundle，
再签固定 core 和 GUI 身份，并以严格校验读回。仅带证书签名的 app 才会生成固定身份的
`Install.command`。

`development` 模式只报告开发签名并在公证前停止。仅在配置 Developer ID 证书和公证凭据
后选择独立 Developer ID 模式；只有它会提交、钉附并报告公证。

`Install.command` 仅用于首次安装。它对来源、暂存副本和目标检查 GUI/core 身份、严格签名
和配置证书指纹；若存在 `/Applications/TuPig Synergy.app`（含符号链接）就拒绝。已安装应用
请退出后使用内置更新器。

唯一允许的 quarantine 操作，是从私有已验证暂存副本删除
`com.apple.quarantine`：首次安装前，或应用内更新器交换已验证暂存内容前。助手不会修改全局 Gatekeeper/spctl、创建信任例外、重置/授予辅助
功能或输入监控，也不会改动其他应用。管理员提示只授权首次写入文件。辅助功能/输入监控
必须由用户在**系统设置 → 隐私与安全性**批准；后续同身份版本可检查签名连续性，但 TCC
实际承接仍需真实且用户已批准的系统测试。不要重置有效授权来制造结果。
