# Linux runtime boundary / Linux 运行环境边界

## English

The native package carries its non-glibc ELF runtime dependencies in the
application's private `libexec/synergy/lib` directory. Shared Qt builds also
carry the required Qt plugins; static Qt builds do not acquire an additional
shared Qt dependency. XKB layouts, application translations and third-party
license texts are included.

The operating system provides the Linux kernel, its glibc and ELF loader,
the POSIX shell and coreutils used by the launcher, display services, fonts
and hardware-specific GPU drivers. The package retains the minimum glibc ABI
of the distribution on which it was built. This package is not an operating
system image and does not replace the host's system libraries.

Package verification extracts the payload without installing dependencies.
It checks every packaged ELF dependency and relative RUNPATH, rejects broken
or external symlinks, and starts the core and GUI with temporary settings and
clean dependency search paths. Shared Qt verification requires the packaged
offscreen plugin to load. This smoke check does not replace a real display,
Wayland portal or multi-machine input-sharing test.

## 中文

原生安装包将 glibc 以外的实际 ELF 运行依赖放在应用私有的
`libexec/synergy/lib` 目录。动态 Qt 构建同时包含必要 Qt 插件；静态 Qt
构建不会额外引入动态 Qt。包内包含 XKB 布局、应用翻译及第三方许可证文本。

宿主操作系统提供 Linux 内核、glibc 与 ELF 加载器、启动器使用的 POSIX
shell 和 coreutils、显示服务、字体及硬件相关 GPU 驱动。安装包保留其构建
发行版所要求的最低 glibc ABI，不替换系统库，也不是操作系统镜像。

验收直接解包，不安装额外依赖；检查每个 ELF 的依赖与相对 RUNPATH，拒绝
损坏或指向包外的符号链接，再使用临时配置和清理后的依赖搜索路径启动核心与
图形程序。动态 Qt 必须实际加载包内 offscreen 插件。该冒烟检查不替代真实
显示环境、Wayland portal 或多机键鼠共享测试。
