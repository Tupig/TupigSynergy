# Troubleshooting Guide / 故障排查指南

> **Language / 语言**: [English](#english) | [中文](#中文)

---

## English

### Quick Diagnosis

| Symptom | Likely Cause | First Check |
|---------|--------------|-------------|
| Can't connect | Firewall / wrong IP / port | `telnet <server-ip> 22211` |
| Connects but no control | Screen layout / links missing | Verify `links` section |
| Mouse jumps / stuck | Corner size / delay | Adjust `switchCornerSize`, `switchDelay` |
| Clipboard not sync | `clipboardSharing=false` | Enable in `[server]` or options |
| High CPU / lag | Logging debug / no TLS | Set `level=info`, enable TLS |
| Wayland: no cursor | Missing libei / Portal | Install `libei`, `libportal` |
| macOS: no input | Accessibility permission | Grant in System Settings → Privacy |
| Windows: UAC prompt fails | Daemon not elevated | Run as Admin, check `daemon` section |

---

### Connection Issues

#### Cannot Connect to Server

Start the server and check its log for the listening address and port. On the client, use
that port and a reachable address or hostname of the server computer. If the log shows an
all-interfaces address, use the server computer's LAN address. If TCP connects but sharing does not start, check that the server
layout contains the client's computer name or an alias for it, then verify the certificate
fingerprints on both computers. On Windows in service mode, check the daemon log when the
startup request does not produce a listening message.

If the client reports a refused connection, check the server's actual listening address
and port first. A timeout calls for checking network reachability and firewall rules.

```bash
# 1. Verify server is listening
netstat -an | grep 22211
# or
ss -tlnp | grep 22211

# 2. Test TCP connectivity
telnet <server-ip> 22211
# or
nc -zv <server-ip> 22211

# 3. Check firewall
# Windows:
netsh advfirewall firewall add rule name="TuPig Synergy" dir=in action=allow protocol=TCP localport=22211
# Linux (ufw):
sudo ufw allow 22211/tcp
# Linux (firewalld):
sudo firewall-cmd --permanent --add-port=22211/tcp
sudo firewall-cmd --reload
# macOS:
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --add /path/to/synergy-core
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --unblock /path/to/synergy-core
```

#### Connection Drops Frequently

| Cause | Solution |
|-------|----------|
| Network instability | Increase `heartbeat` (default 5000ms) |
| Server sleep | Disable sleep on server (`preventSleep=true`) |
| Client timeout | Check keep-alive: `CALV` every 3s |
| TLS handshake fail | Verify the certificate, private key, and saved peer fingerprint on both computers |

**Debug**: turn on file logging. When `file` is omitted the default path is `~/synergy.log`.
```ini
[log]
level=DEBUG
toFile=true
```

#### "Incompatible Version" Error

```text
EICV: Protocol version mismatch
```

**Fix**: Both sides must support common version. Server logs show negotiated version. Update both ends to same major version.

---

### Input Issues

#### Mouse Not Moving / Stuck

1. **Check corner settings**:
   ```ini
   [options]
   switchCornerSize = 6
   switchDelay = 250
   ```

2. **Verify links are bidirectional**:
   ```ini
   section: links
       desktop:
           right = laptop
       laptop:
           left = desktop
   end
   ```

3. **Screen resolution mismatch**: Ensure `DINF` reports correct dimensions.

#### Keyboard Not Working

1. **Modifier keys stuck**: Press `Ctrl`/`Alt`/`Shift` on both machines to reset.
2. **Layout mismatch**: Enable `languageSync=true` in `[client]`.
3. **Half-duplex keys**: If CapsLock/NumLock behave oddly:
   ```ini
   section: screens
       laptop:
           halfDuplexCapsLock = true
           halfDuplexNumLock = true
   end
   ```

#### Clipboard Not Syncing

```ini
# Server config
[server]
clipboardSharing = true
clipboardSharingSize = 2048  # KB

# Or in GUI [server] section
clipboardSharing=true
clipboardSharingSize=2048
```

**Requirements**: Both sides v1.6+, TLS enabled for large transfers.

---

### Platform-Specific Issues

#### 🐧 Linux

| Issue | Solution |
|-------|----------|
| **Wayland: no cursor/input** | Install `libei`, `libportal`; ensure `xdg-desktop-portal` running |
| **X11: cursor not confined** | Set `xtestIsXineramaUnaware = true` on client |
| **Clipboard empty** | Install `wl-clipboard` (Wayland) or `xclip`/`xsel` (X11) |
| **High DPI blurry** | Set `QT_AUTO_SCREEN_SCALE_FACTOR=1` or per-monitor DPI |
| **SELinux blocks** | `setsebool -P allow_execstack=1` or custom policy |
| **Want a systemd service** | This repository does not ship a unit, and `synergy-core` has no `--no-daemon` flag. Run `synergy-core server` in the foreground |

**Debug Wayland**:
```bash
# Check Portal
busctl --user call org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.ScreenCast GetVersion

# Check libei
pkg-config --modversion libei
```

#### 🍎 macOS

| Issue | Solution |
|-------|----------|
| **No input at all** | Grant **Accessibility** + **Input Monitoring** in System Settings → Privacy & Security |
| **Permission seems granted but the app still prompts** | Open the installed copy from Applications and check its Accessibility entry in System Settings → Privacy & Security. An authorization recorded for an older ad-hoc build may be tied to that build's code hash. If a signed update still prompts with the entry enabled, collect the macOS permission log for that Start attempt and record the installed app's path. |
| **Notarization fails** | Codesign with `--options runtime`, notarize via `xcrun notarytool` |
| **Apple Silicon: Rosetta** | Build native `arm64`; avoid Rosetta |
| **Menu bar icon missing** | Grant **Screen Recording** permission (for tray) |
| **Keychain prompts** | Store cert in keychain: `security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain cert.pem` |

For a reset limited to TuPig Synergy, see [macOS signing and Accessibility](macos-signing.md#english).

#### 🪟 Windows

| Issue | Solution |
|-------|----------|
| **UAC prompts / login screen not captured** | The daemon must be registered as a Windows service — see [Register the daemon service](#register-the-daemon-service-windows) |
| **Service mode fails** | Same; the binaries cannot self-install the service, so it must come from the MSI or a manual `sc create` |
| **Antivirus blocks** | Add exclusions for `synergy_<version>.exe`, `synergy-core.exe`, and `synergy-daemon.exe` |
| **High DPI scaling** | Set DPI awareness in manifest; per-monitor v2 |
| **Port already in use** | `netstat -ano | findstr :22211`, kill PID |
| **FIPS mode breaks TLS** | Disable FIPS or use OpenSSL FIPS provider |

**Debug Service**:
```powershell
# Check service status
Get-Service -Name "TuPig Synergy"

# View logs
Get-WinEvent -LogName Application -ProviderName "TuPig Synergy" -MaxEvents 50
```

<a id="register-the-daemon-service-windows"></a>
**Register the daemon service (Windows)**

UAC-prompt and login-screen support requires `synergy-daemon.exe` to run as a Windows
service: from session 0 it duplicates the token of `winlogon.exe` / `logonui.exe` to
start the core on the secure desktop. A user-session process cannot do this, so the
capability is unavailable until the service is registered.

The executables **cannot register themselves** — there is no `--install-service`
option and no `CreateService` call anywhere in the sources. Registration therefore
comes from one of:

1. **The MSI (recommended).** It declares `ServiceInstall` for `synergy-daemon.exe`
   with `Start="auto"`, plus `ServiceControl` to start the service on install and
   stop and delete it on uninstall. Installing the MSI is all that is required.
2. **Manual registration**, as Administrator:

```bat
sc create "TuPig Synergy" binPath= "<install dir>\synergy-daemon.exe" start= auto ^
   DisplayName= "TuPig Synergy"
sc description "TuPig Synergy" "Runs the Core process on secure desktops (UAC prompts, login screen, etc)."
sc start "TuPig Synergy"
```

To remove it:

```bat
sc stop "TuPig Synergy"
sc delete "TuPig Synergy"
```

> The service name is not hardcoded in the daemon — the Service Control Manager passes
> it in at startup — so any name pointing at the right binary works. The MSI happens to
> use `TuPig Synergy` (the project's proper name), which is what the commands above use.
>
> The **portable** 7Z package deliberately omits the daemon, so portable builds cannot
> provide UAC/login-screen support at all. Use the MSI for that.

---

### TLS / Security Issues

#### Certificate Errors

When TLS is enabled, both computers present a certificate and check the other's SHA-256
fingerprint. Each app can create its own certificate. Compare the prompt with the local
fingerprint shown on the other computer before accepting it.
The legacy **Require client certificates** control does not change this TLS requirement.
If you dismissed or declined a fingerprint, use **Start** or **Restart** to review it again.

| Error | Fix |
|-------|-----|
| `peer has no tls certificate` | Confirm TLS is enabled on both computers and each can read its own certificate and private key |
| `failed to verify peer certificate fingerprint` | Compare the fingerprint with the actual peer. If a certificate was replaced, verify its new fingerprint before trusting it |
| `key too small` | Use an RSA key of at least 2048 bits |

---

### Performance Tuning

| Setting | Recommendation |
|---------|----------------|
| `heartbeat` | 5000ms (LAN), 10000ms (WAN) |
| `switchDelay` | 250ms (default), 100ms (fast) |
| `clipboardSharingSize` | 1024KB (text), 5120KB (images) |
| `[log] level` | `INFO` for normal use, `DEBUG` while diagnosing. There is no `--debug` flag |
| TLS session reuse | Enabled by default in OpenSSL 3.0 |

---

### Debugging Tools

| Tool | Purpose |
|------|---------|
| `[log] level=DEBUG` in `TuPig Synergy.conf` | Verbose logging. `synergy-core` has no `--debug` flag |
| Wireshark | Capture port 22211, decode as Synergy |
| `strace -p <pid>` | Linux syscall trace |
| `dtruss -p <pid>` | macOS syscall trace |
| Process Monitor | Windows file/reg/net activity |
| `lsof -i :22211` | Check port usage |

---

### Log Analysis

**Log line shape** (`src/lib/base/Log.cpp`):

```text
[2026-10-09T16:26:00.000] INFO: QtNetworkTransport: connected (TLS)
[2026-10-09T16:26:01.000] ERROR: QtDataSocket: peer has no tls certificate
```

The level token is `INFO`, `WARNING`, `ERROR`, `DEBUG`, or `VERBOSE`. The second sample line is an error: a TLS peer with no certificate is disconnected, not accepted. File logging is off until `[log] toFile=true`. The default path is `~/synergy.log` (`kAppId`), not `synergy-debug.log`. The GUI log panel shows the same lines when file logging is off.

---

### Getting Help

| Channel | Best For |
|---------|----------|
| **GitHub Issues** | Bugs and crashes. Use the bug-report template |
| **Feature request issue** | Setup questions. Discussions are not enabled |
| **This directory** | Build, configuration, and this file. The wiki is only a short index |
| **Logs** | Attach the GUI log, or `~/synergy.log` after `[log] toFile=true` |

**Minimal bug report template:**
```markdown
**OS/Version**: Windows 11 23H2 / TuPig Synergy (see `cmake/Version.cmake`)
**Role**: Server (Desktop) ↔ Client (Laptop)
**Config**: [paste relevant config sections]
**Logs**: [GUI log, or ~/synergy.log when toFile is true]
**Steps**: 1. Start server 2. Start client 3. Move mouse to edge
**Expected**: Cursor moves to client
**Actual**: Cursor stops at edge, log shows [error]
```

---

## 中文

### 快速诊断

| 现象 | 可能原因 | 首要检查 |
|------|----------|----------|
| 无法连接 | 防火墙 / IP 错误 / 端口 | `telnet <server-ip> 22211` |
| 连上但无法控制 | 屏幕布局/链接缺失 | 检查 `links` 节 |
| 鼠标跳跃/卡住 | 角落大小/延迟 | 调整 `switchCornerSize`, `switchDelay` |
| 剪贴板不同步 | `clipboardSharing=false` | 在 `[server]` 或 options 启用 |
| 高 CPU/卡顿 | 调试日志 / 无 TLS | 设 `level=info`、启用 TLS |
| Wayland 无光标 | 缺 libei/Portal | 安装 `libei`, `libportal` |
| macOS 无输入 | 无辅助功能权限 | 系统设置 → 隐私与安全性 → 授权 |
| Windows UAC 失败 | 守护进程未提权 | 以管理员运行，检查 `daemon` 节 |

---

### 连接问题

#### 无法连接到服务端

启动服务端，在日志中核对实际监听的地址和端口。客户端填写可访问的服务端地址或主机名，以及日志中的端口。
如果日志显示监听所有网卡，客户端填写服务端的局域网地址。TCP 已连通时，
继续检查服务端布局中是否有客户端的电脑名或对应别名，以及两端是否都验证了对方的证书指纹。
Windows 服务模式发出启动请求却没有监听日志时，请查看后台服务日志。

如果客户端提示连接被拒绝，先核对服务端实际监听的地址和端口。提示超时时，再检查两台电脑之间的网络和防火墙规则。

```bash
# 1. 验证服务端监听
netstat -an | grep 22211
# 或
ss -tlnp | grep 22211

# 2. 测试 TCP 连通性
telnet <server-ip> 22211
# 或
nc -zv <server-ip> 22211

# 3. 检查防火墙
# Windows:
netsh advfirewall firewall add rule name="TuPig Synergy" dir=in action=allow protocol=TCP localport=22211
# Linux (ufw):
sudo ufw allow 22211/tcp
# Linux (firewalld):
sudo firewall-cmd --permanent --add-port=22211/tcp
sudo firewall-cmd --reload
# macOS:
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --add /path/to/synergy-core
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --unblock /path/to/synergy-core
```

#### 频繁断连

| 原因 | 解决方案 |
|------|----------|
| 网络不稳定 | 增加 `heartbeat` (默认 5000ms) |
| 服务端休眠 | 设置 `preventSleep=true` |
| 客户端超时 | 检查保活：`CALV` 每 3 秒 |
| TLS 握手失败 | 在两台电脑核对各自的证书、私钥和已保存的对方指纹 |

**调试**：打开文件日志。不写 `file` 时默认是 `~/synergy.log`。
```ini
[log]
level=DEBUG
toFile=true
```

#### "版本不兼容" 错误

```text
EICV: Protocol version mismatch
```

**修复**：双端必须支持共同版本。服务端日志显示协商版本。双端升级到同一主版本。

---

### 输入问题

#### 鼠标不动 / 卡住

1. **检查角落设置**：
   ```ini
   [options]
   switchCornerSize = 6
   switchDelay = 250
   ```

2. **验证链接双向**：
   ```ini
   section: links
       desktop:
           right = laptop
       laptop:
           left = desktop
   end
   ```

3. **分辨率不匹配**：确保 `DINF` 上报正确尺寸。

#### 键盘无响应

1. **修饰键卡住**：两台机器分别按 `Ctrl`/`Alt`/`Shift` 重置。
2. **布局不匹配**：客户端启用 `languageSync=true`。
3. **半双工键**：CapsLock/NumLock 行为异常时：
   ```ini
   section: screens
       laptop:
           halfDuplexCapsLock = true
           halfDuplexNumLock = true
   end
   ```

#### 剪贴板不同步

```ini
# 服务端配置
[server]
clipboardSharing = true
clipboardSharingSize = 2048  # KB

# 或 GUI [server] 节
clipboardSharing=true
clipboardSharingSize=2048
```

**要求**：双端 v1.6+，大文件传输需 TLS。

---

### 平台专属问题

#### 🐧 Linux

| 问题 | 解决方案 |
|------|----------|
| **Wayland 无光标/输入** | 安装 `libei`, `libportal`；确保 `xdg-desktop-portal` 运行 |
| **X11 光标不限制** | 客户端设 `xtestIsXineramaUnaware = true` |
| **剪贴板为空** | Wayland 装 `wl-clipboard`，X11 装 `xclip`/`xsel` |
| **高 DPI 模糊** | 设 `QT_AUTO_SCREEN_SCALE_FACTOR=1` 或逐显示器 DPI |
| **SELinux 拦截** | `setsebool -P allow_execstack=1` 或自定义策略 |
| **想用 systemd** | 本仓库不带 unit 文件，`synergy-core` 也没有 `--no-daemon`。前台运行 `synergy-core server` |

**Wayland 调试**：
```bash
# 检查 Portal
busctl --user call org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.ScreenCast GetVersion

# 检查 libei
pkg-config --modversion libei
```

#### 🍎 macOS

| 问题 | 解决方案 |
|------|----------|
| **完全无输入** | 系统设置 → 隐私与安全性 → **辅助功能** + **输入监控** 授权 |
| **已授权但仍反复提示** | 从「应用程序」打开已安装的副本，在「系统设置 → 隐私与安全性 → 辅助功能」检查对应条目。旧版临时签名的授权可能绑定到旧版的代码哈希。如果签名升级后开关已打开却仍弹窗，请记录本次点击「启动」时的 macOS 权限日志和实际运行的应用路径。 |
| **公证失败** | 签名加 `--options runtime`，`xcrun notarytool` 公证 |
| **Apple Silicon Rosetta** | 原生构建 `arm64`；避免 Rosetta |
| **菜单栏图标缺失** | 授予 **屏幕录制** 权限 (托盘用) |
| **钥匙串弹窗** | 证书存入钥匙串：`security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain cert.pem` |

只清理 TuPig Synergy 的旧授权时，请参见 [macOS 签名与辅助功能授权](macos-signing.md#中文)。

#### 🪟 Windows

| 问题 | 解决方案 |
|------|----------|
| **UAC 提示 / 登录界面未捕获** | daemon 必须注册为 Windows 服务 —— 见 [注册守护服务](#注册守护服务-windows) |
| **服务模式失败** | 同上；可执行文件无法自行安装服务，必须由 MSI 或手工 `sc create` 完成 |
| **杀毒软件拦截** | 加白 `synergy_<版本>.exe`、`synergy-core.exe` 与 `synergy-daemon.exe` |
| **高 DPI 缩放** | 清单设 DPI 感知；逐显示器 v2 |
| **端口被占用** | `netstat -ano | findstr :22211`，结束 PID |
| **FIPS 模式破坏 TLS** | 关闭 FIPS 或用 OpenSSL FIPS provider |

**服务调试**：
```powershell
# 查看服务状态
Get-Service -Name "TuPig Synergy"

# 查看日志
Get-WinEvent -LogName Application -ProviderName "TuPig Synergy" -MaxEvents 50
```

<a id="注册守护服务-windows"></a>
**注册守护服务（Windows）**

UAC 提示与登录界面支持要求 `synergy-daemon.exe` 以 Windows 服务身份运行：它在会话 0 中复制
`winlogon.exe` / `logonui.exe` 的令牌，才能把 core 启动到安全桌面。用户会话中的进程无法做到
这一点，因此在服务注册完成之前，该能力不可用。

可执行文件**无法自行注册服务** —— 代码中不存在 `--install-service` 选项，也没有任何
`CreateService` 调用。因此注册只有两种来源：

1. **MSI（推荐）**：它为 `synergy-daemon.exe` 声明了 `ServiceInstall`（`Start="auto"`），
   并配 `ServiceControl` 在安装时启动服务、卸载时停止并删除服务。装完 MSI 即可。
2. **手工注册**（需管理员权限）：

```bat
sc create "TuPig Synergy" binPath= "<安装目录>\synergy-daemon.exe" start= auto ^
   DisplayName= "TuPig Synergy"
sc description "TuPig Synergy" "Runs the Core process on secure desktops (UAC prompts, login screen, etc)."
sc start "TuPig Synergy"
```

移除服务：

```bat
sc stop "TuPig Synergy"
sc delete "TuPig Synergy"
```

> 服务名在 daemon 中并未硬编码 —— 由服务控制管理器在启动时传入 —— 因此任何指向正确可执行
> 文件的名称都可用。MSI 使用的是 `TuPig Synergy`（项目正式名），上文命令即沿用该名称。
>
> **便携版** 7Z 包有意不含 daemon，因此便携构建完全无法提供 UAC/登录界面支持。该能力请用 MSI。

---

### TLS / 安全问题

#### 证书错误

开启 TLS 后，两台电脑都提供证书，并校验对方的 SHA-256 指纹。应用可以各自生成证书。
确认信任前，先把弹窗里的指纹与另一台电脑显示的本机指纹核对一致。
旧版的「要求客户端证书」选项也不能关闭这项 TLS 要求。
如果关闭或拒绝过指纹提示，可以点「启动」或「重启」重新核对。

| 错误 | 修复 |
|------|------|
| `peer has no tls certificate` | 确认两端都开启 TLS，且各自可以读取自己的证书和私钥 |
| `failed to verify peer certificate fingerprint` | 核对实际对端指纹。更换过证书时，先确认新指纹再信任 |
| `key too small` | 使用至少 2048 位的 RSA 密钥 |

---

### 性能调优

| 设置 | 建议 |
|------|------|
| `heartbeat` | 5000ms (局域网), 10000ms (广域网) |
| `switchDelay` | 250ms (默认), 100ms (极速) |
| `clipboardSharingSize` | 1024KB (文本), 5120KB (图片) |
| `[log] level` | 平时用 `INFO`，排查时用 `DEBUG`。没有 `--debug` 参数 |
| TLS 会话复用 | OpenSSL 3.0 默认开启 |

---

### 调试工具

| 工具 | 用途 |
|------|------|
| 在 `TuPig Synergy.conf` 里设 `[log] level=DEBUG` | 详细日志。`synergy-core` 没有 `--debug` 参数 |
| Wireshark | 抓包端口 22211，解析为 Synergy |
| `strace -p <pid>` | Linux 系统调用跟踪 |
| `dtruss -p <pid>` | macOS 系统调用跟踪 |
| Process Monitor | Windows 文件/注册表/网络 |
| `lsof -i :22211` | 检查端口占用 |

---

### 日志分析

**日志行格式**（`src/lib/base/Log.cpp`）：

```text
[2026-10-09T16:26:00.000] INFO: QtNetworkTransport: connected (TLS)
[2026-10-09T16:26:01.000] ERROR: QtDataSocket: peer has no tls certificate
```

级别标记是 `INFO`、`WARNING`、`ERROR`、`DEBUG` 或 `VERBOSE`。示例第二行是错误：TLS 对端没有证书时会断开，不会接受这条连接。`[log] toFile=true` 之前不会写文件。默认路径是 `~/synergy.log`（`kAppId`），不是 `synergy-debug.log`。未写文件时，GUI 日志面板里是同样的行。

---

### 获取帮助

| 渠道 | 适用场景 |
|------|----------|
| **GitHub Issues** | Bug 和崩溃，使用 bug 报告模板 |
| **功能请求 issue** | 安装和配置问题。Discussions 未开启 |
| **本目录文档** | 编译、配置和本文。Wiki 只是简短索引 |
| **日志** | 附上 GUI 日志，或在 `[log] toFile=true` 后附上 `~/synergy.log` |

**最小 Bug 报告模板：**
```markdown
**OS/版本**: Windows 11 23H2 / TuPig Synergy（见 `cmake/Version.cmake`）
**角色**: 服务端(台式机) ↔ 客户端(笔记本)
**配置**: [粘贴相关配置节]
**日志**: [GUI 日志，或 toFile 打开后的 ~/synergy.log]
**步骤**: 1. 启动服务端 2. 启动客户端 3. 移动鼠标到边缘
**预期**: 光标进入客户端
**实际**: 光标停在边缘，日志显示 [error]
```
