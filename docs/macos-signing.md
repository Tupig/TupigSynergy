# macOS 签名与辅助功能授权 / macOS Signing and Accessibility

> **Language / 语言**: [English](#english) | [中文](#中文)

## 中文

### 升级后显示已授权，启动仍被拦

macOS 按代码签名身份识别获得辅助功能授权的应用。临时签名（ad-hoc）绑定到具体构建。更新后，列表可能还显示旧构建的授权，而系统已经拒绝新构建使用它。关闭再打开旧条目的开关可能仍然保留旧签名。 [Apple 关于代码签名身份的说明](https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements)

在「系统设置 → 隐私与安全性 → 辅助功能」中移除旧的 TuPig Synergy 条目，再添加「应用程序」中的 TuPig Synergy.app 并打开开关。退出并重新打开应用后，点击「启动」。

也可用以下命令清除这一个应用的旧记录，然后在系统设置中重新授权。

```sh
tccutil reset Accessibility com.tupig.synergy
```

### 正式发布签名

GitHub 的 macOS 发布流程支持 Developer ID Application 签名及 Apple 公证。该流程启用签名后就会提交公证。Apple 公证要求有效的 Developer ID 证书。 [Apple 公证要求](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)

发布维护者通过 GitHub Actions 的仓库机密配置以下值。

- `APPLE_CODESIGN_ID` 是 Developer ID Application 签名身份。
- `APPLE_P12_CERTIFICATE` 和 `APPLE_P12_PASSWORD` 是签名证书导出文件的 Base64 内容与导出密码。
- `APPLE_NOTARY_USER`、`APPLE_NOTARY_PASSWORD` 和 `APPLE_TEAM_ID` 是现有公证流程使用的 Apple 账号、App 专用密码和团队 ID。

从临时签名切换到 Developer ID 后，先为新的签名身份授权一次。随后用两个由同一团队签署的正式构建实测应用内升级，确认升级后能直接启动。

## English

### Accessibility appears enabled, but Start is blocked after an update

macOS ties Accessibility approval to the app’s code-signing identity. An ad-hoc signature is specific to one build. After an update, System Settings can still show the old build as allowed while denying the new build. Toggling that existing entry can leave the old signature on record. [Apple’s code-signing identity guide](https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements)

In System Settings → Privacy & Security → Accessibility, remove the old TuPig Synergy entry. Add TuPig Synergy.app from Applications and enable it. Quit and reopen the app, then select Start.

The following command clears the old approval for this app. Grant it again in System Settings.

```sh
tccutil reset Accessibility com.tupig.synergy
```

### Release signing

GitHub’s macOS release workflow supports Developer ID Application signing and Apple notarization. Enabling signing also enables submission to the notary service. Apple requires a valid Developer ID certificate for that service. [Apple’s notarization requirements](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)

Release maintainers configure these values as GitHub Actions repository secrets.

- `APPLE_CODESIGN_ID` is the Developer ID Application signing identity.
- `APPLE_P12_CERTIFICATE` and `APPLE_P12_PASSWORD` are the Base64-encoded certificate export and its password.
- `APPLE_NOTARY_USER`, `APPLE_NOTARY_PASSWORD` and `APPLE_TEAM_ID` are the Apple account, app-specific password and team ID used by the existing notarization workflow.

When moving from ad-hoc signing to Developer ID, approve the new signing identity once. Then test an in-app update between two releases signed by the same team and verify that Start works after the upgrade.
