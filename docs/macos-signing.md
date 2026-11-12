# macOS 签名与辅助功能授权 / macOS Signing and Accessibility

> **Language / 语言**: [English](#english) | [中文](#中文)

---

## 中文

### 为什么每次升级都要重新授权

macOS 的「辅助功能」授权（TCC）绑定到应用的**代码签名身份**：

- 未签名/ad-hoc 构建：身份是 **cdhash**，每次编译都会变化，系统把新版本当成另一个应用，于是重新弹窗。
- Developer ID 或**自签名证书**构建：身份是证书的 designated requirement，只要证书不变，授权就跨版本保留。

本项目默认没有 Apple Developer 证书，CI 使用 ad-hoc 签名，所以每次发布都会重新询问。**不需要付费账号**也能解决：
用一张自签名代码签名证书签名所有发布包即可。

### 一次性配置（免费）

1. 打开「钥匙串访问」→ 菜单「证书助理」→「创建证书…」。
2. 名称：`TuPig Synergy Local`；身份类型：**自签名根证书**；证书类型：**代码签名**；有效期按需（如 3650 天）。
3. 创建后导出为 `.p12`（设置一个导出密码）。
4. 把证书加入 GitHub 仓库机密：
   - `APPLE_P12_CERTIFICATE`：`.p12` 的 base64（`base64 -i cert.p12 | pbcopy`）
   - `APPLE_P12_PASSWORD`：导出密码
   - `APPLE_CODESIGN_ID`：证书的**常用名称**，即 `TuPig Synergy Local`
5. 之后 CI 会用同一身份签名每个 macOS 包，辅助功能授权只授权一次。

本地构建也可以复用：

```bash
cmake -B build -DAPPLE_CODESIGN_ID="TuPig Synergy Local" ...
```

或使用 `APPLE_CODESIGN_DEV`（开发签名，见 `cmake/MacCodesign.cmake`）。

### 限制

- 自签名**不能公证**（公证需要付费的 Apple Developer Program）。首次打开可能提示「无法验证开发者」，
  右键「打开」即可；之后的更新由应用内自动更新完成。
- 自动更新（下载→替换→重启）不依赖签名；签名只影响 TCC 是否记住授权。
- 一旦改用 Developer ID 证书，`APPLE_*` 机密可直接替换，无需改代码。

## English

### Why every update re-prompts

macOS Accessibility (TCC) grants are bound to the app's code-signing identity. Ad-hoc/unsigned builds are
identified by their **cdhash**, which changes on every build, so the system treats each release as a new app.
A Developer ID **or a self-signed code-signing certificate** gives a stable designated requirement, so the
grant survives updates. No paid account is required for that.

### One-time setup (free)

1. Keychain Access → Certificate Assistant → Create a Certificate.
2. Name `TuPig Synergy Local`; Identity: **Self Signed Root**; Certificate Type: **Code Signing**.
3. Export as `.p12` with a password.
4. Add repository secrets: `APPLE_P12_CERTIFICATE` (base64 of the p12), `APPLE_P12_PASSWORD`, and
   `APPLE_CODESIGN_ID` (the certificate common name).
5. CI then signs every macOS package with the same identity.

### Limitations

Self-signed builds cannot be notarized (notarization requires the paid Apple Developer Program). The first
launch may need right-click → Open; subsequent updates use the in-app updater. Switching to a Developer ID
certificate later only changes the secrets, not the code.
