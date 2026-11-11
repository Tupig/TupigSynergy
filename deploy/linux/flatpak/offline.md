# Offline Flatpak distribution / Flatpak 离线交付

## English

This archive contains the application, its declared platform runtime and the
related extensions selected in a fresh installation, including all locales
and the default Mesa graphics extension. It contains no SDK or build cache.
`refs.tsv` records every original commit and origin; `SHA256SUMS` covers the
repository and installer. Verify the archive's published provenance/checksum
before extracting it, then run `bash install-offline.sh` from the extracted
directory. Flatpak and the host operating system must already be installed.

The installer uses only the included `file://` repository. This local snapshot
repository is unsigned; GPG checking is disabled only for its uniquely named
local remote. No network remote is added or globally weakened. Original
upstream runtime commits are retained, but this archive is not a new upstream
signature. Keep the directory available while its local remote is in use.

The host provides the kernel, Flatpak sandbox machinery, display services,
fonts and hardware-specific drivers. Proprietary GPU drivers are a host
requirement; they are not silently downloaded by this installer. CI verifies
installation into an empty installation with networking disabled, compares
all commits, and runs the core and an offscreen GUI smoke check. This does not
replace real display, portal or multi-machine input tests.

## 中文

本归档包含应用、应用声明的平台运行时以及在全新安装环境中解析出的关联扩展，
包括全部语言和默认 Mesa 图形扩展；不包含 SDK 或构建缓存。`refs.tsv`
记录每个原始提交与来源，`SHA256SUMS` 覆盖仓库和安装器。先核对发布的归档
来源证明及校验值，再解压并运行 `bash install-offline.sh`。宿主需要已安装
Flatpak 及操作系统。

安装器只使用归档中的 `file://` 仓库。该本地快照仓库未签名，仅对其独立命名
的本地 remote 关闭 GPG 检查；不添加网络 remote，不降低全局校验要求。
上游运行时的原始提交保持不变，但归档不是新的上游签名。使用该 remote 时
请保留解压后的目录。

宿主提供内核、Flatpak 沙箱设施、显示服务、字体和硬件驱动；专有 GPU 驱动
属于宿主要求，安装器不会静默联网下载。CI 在空安装目录和断网环境下检查安装、
逐项核对提交，并运行核心与 offscreen 图形冒烟测试；该结果不替代实际显示、
portal 或多机输入验收。
