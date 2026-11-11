# AGENTS / 协作规则

> **Language / 语言**: [English](#english) | [中文](#中文)
>
> 本文件是本仓库对**所有 AI 协作代理与贡献者**的统一约束。与 GitHub Issues
> （问题追踪，<https://github.com/Tupig/Tupig_synergy/issues>；原台账
> `.github/ISSUE_TEMPLATE/security-quality-refactoring.md` 已于 2026-10-08 全量迁移并删除）配套使用。
>
> This file defines the binding rules for **all AI agents and contributors** working in this
> repository. It complements the issue tracker.

---

## English

### Scope

Applies to every change in this repository. When a rule here conflicts with an ad-hoc request, raise the
conflict instead of silently breaking the convention.

### Hard constraints

| # | Rule | Rationale |
|---|---|---|
| R1 | Windows automation is `.bat` only. **No `.ps1` files anywhere.** | One scripting runtime on Windows; avoids execution-policy failures. |
| R2 | `.bat` must work when invoked from both `cmd /c` and PowerShell. | Callers differ; the script must not assume either. |
| R3 | Script text is **ASCII-only**. | Console code pages (GBK/UTF-8) corrupt non-ASCII in batch files and can break parsing. |
| R4 | No machine-specific absolute paths, no required environment variables. | Every machine differs; scripts must self-locate VS, vcpkg and tools. |
| R5 | All third-party dependencies come from vcpkg in **manifest mode**, with one documented exception: **CI** installs Qt from a prebuilt Qt distribution and runs vcpkg in classic mode for OpenSSL. | Single declared dependency source for release builds; the CI exception trades exact parity for build time, and must stay version-compatible (see below). |
| R6 | The vcpkg baseline is pinned in `vcpkg.json` (`builtin-baseline`) and is the **single source of truth**; scripts must read it, never duplicate it. | Keeps clones reproducible. |
| R7 | `vendor/` (vcpkg checkout) and `build/` are never committed — they are bootstrapped locally. | Repository stays small and clonable. |
| R8 | Code must compile on Windows, macOS and Linux. Platform code lives under `src/lib/platform/<os>/`. | One source tree, conditional compilation. |
| R9 | Documentation is bilingual in a single file (sectioned `## English` + `## 中文`). | One document, two audiences. |
| R10 | Commit messages are Chinese Conventional Commits: `feat`, `fix`, `docs`, `refactor`, `test`, `ci`, `build`, or `chore`, then a Chinese subject. | Project convention. English subjects would break the existing history and the bilingual docs. |
| R11 | Every task is tracked on a live todo list before work starts. Update it on each status change and record every step. If the list is missing or stale, stop and repair it before continuing. Procedure: [Task progress](#task-progress-r11). | Progress stays visible. A finished claim with open items is invalid. |

### Task progress (R11)

The same procedure is injected every session from `.cursor/rules/todo-tracking.mdc`.

Before the first step, and again before the final answer:

1. Create one todo per work step. A one-step task still has two items: do the work, then check the result.
2. Mark exactly one item in progress before starting it.
3. In the same turn a step finishes, mark it completed or cancelled, then mark the next item in progress.
4. Add an item as soon as a new step appears. Work that exists only in the reply is a miss.

**Violation.** There is no separate checker. The agent stops itself: do not start the next step, and do not say the task is finished, until the missing items are written with the status they actually have. A finished-task reply is invalid while any item is still pending or in progress. Cancel only a step that will not be done, and keep the reason in the item text.

### Directory layout

The current tree is normative for new files:

- `src/apps/` — executables (`synergy-gui`/`synergy-core`/`synergy-daemon`) plus shared
  resources (`res/branding`, `res/icons`).
- `src/lib/` — libraries: `arch/`, `base/`, `client/`, `common/`, `gui/`, `io/`, `mt/`,
  `net/`, `platform/`, `server/`, and `synergy/` (the application library plus the
  TuPig overlay under `gui/`/`hooks/`).
- `src/unittests/` — one directory per tested module plus `gui/`.
- `cmake/` — CMake modules (`Synergy.cmake`, `Version.cmake`, …); `deploy/` — packaging;
  `config/` — tool and test configuration; `scripts/` — user entry points, with
  `scripts/ci/` for CI-only helpers.
- `extra/` no longer exists — do not reintroduce it; put assets at the location above
  that owns them.

### Build & dependency policy

- The user-visible entry points are `setup.bat` (one-time host toolchain) and `scripts/build.bat` /
  `scripts/build.sh` (per-platform build). Nothing else should be needed.
- `scripts/bootstrap-vcpkg.{bat,sh}` clones and bootstraps the repository-local vcpkg into
  `vendor/vcpkg`, reads the pinned baseline from `vcpkg.json`, and is idempotent.
- A C++ toolchain (MSVC / clang / gcc), CMake and Ninja are host prerequisites and are **not**
  vendored. Local macOS manifest builds additionally require a full, selected Xcode installation,
  pkgconf, Autoconf with autoconf-archive, Automake and GNU Libtool for the pinned ports. These are
  host build tools; Qt, OpenSSL and all application library dependencies are fetched by vcpkg.
- Build artifacts are 64-bit only (`vcpkg.json` `supports`: `x64 | arm64`). Do not add an x86 / Win32 target.
- On Windows, the MSVC environment must be activated (`vcvarsall.bat x64`) before CMake runs. vcpkg
  resolves its Visual Studio instance from the VC environment variables; without them it fails with
  *"Could not locate a complete Visual Studio instance"* on Build Tools installations whose instance
  metadata lacks an `isComplete` flag. `scripts/build.bat` does this automatically — do not remove it.
- Do not add `/MD` alongside the static vcpkg triplet, and do not reintroduce per-machine vcpkg
  discovery (`VCPKG_ROOT`, `C:\Users\<user>\vcpkg`).
- **The MSVC runtime must follow the vcpkg triplet, and `CMakeLists.txt` is what resolves it.**
  vcpkg does **not** set `CMAKE_MSVC_RUNTIME_LIBRARY` on a plain Windows configure: its
  `buildsystems/vcpkg.cmake` includes the platform toolchain only when
  `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` is set (`scripts/buildsystems/vcpkg.cmake:209`) and never
  defaults that variable, so `toolchains/windows.cmake` — the file that would derive the runtime
  from `VCPKG_CRT_LINKAGE` — does not run. `VCPKG_CRT_LINKAGE` is also empty in project scope
  (measured). The project therefore reads the linkage from the triplet file itself (falling back to
  vcpkg's `-static` naming convention) and sets `/MT` or `/MD` accordingly, logging which source
  answered. Do not hardcode either value: forcing `/MT` made every dynamic-CRT configuration —
  which is exactly what the CI Windows legs use — compile against `/MD` dependencies and fail with
  `LNK2038 RuntimeLibrary mismatch`. ASan is the only genuine constraint; it needs a dynamic
  triplet, which the `windows-msvc-asan` preset provides and the configure step now checks for.

**The CI dependency exception (R5), and its obligations.** `.github/actions/install-dependencies`
installs Qt via `jurplel/install-qt-action` and runs `johnwason/vcpkg-action` with `--classic` for
OpenSSL only, so CI's Qt does **not** come from `vcpkg.json`. This is deliberate: vcpkg builds Qt
from source, which is hours of CI time per leg, while the manifest remains the source of truth for
release builds and for every local build. The exception carries three requirements:

1. The CI Qt version must be **>= the project floor** (`REQUIRED_QT_VERSION` in `CMakeLists.txt`,
   currently 6.7.0, with a 5.13 fallback for RHEL 8). CI currently pins 6.9.3 / 6.10.3 / 6.11.1.
   When bumping it, re-check the floor rather than assuming.
2. CI's Qt is **shared**, whereas the manifest builds Qt **static**. So CI does not exercise the
   static-Qt path that releases ship. Treat a green CI as evidence about *logic*, not about the
   static link model; that path is covered by a local `scripts/build.bat release`.
3. Any divergence that has to be kept (for example a Qt-version-specific workaround) must be
   recorded here or in `docs/build.md` — an undocumented difference between CI and release is how
   the two drift apart silently.
4. On Windows, CI runs with a **dynamic CRT (`/MD`)**, while the manifest's Windows triplets use a
   **static CRT (`/MT`)**. CI's Windows legs never exercise `/MT`; verify that path locally with
   `scripts/build.bat release` (the runtime block in `CMakeLists.txt` derives the CRT from the
   triplet).

### Interactive test safety

Review host side effects before registering new tests. Default CTest execution must not inject real input or replace the live system clipboard. `SYNERGY_ENABLE_INTERACTIVE_TESTS` defaults to `OFF`: keep `OSXKeyStateTests`, `OSXClipboardTests`, `MSWindowsClipboardTests`, and X11's `XWindowsClipboardTests` registered and built when available, but set their CTest `DISABLED` property. The first injects keys into the focused application; the clipboard targets clear/replace clipboard contents or change X11 selection ownership. Keep pure Windows conversion, DropData, key-state and policy tests enabled. Report disabled targets as `Disabled` / `Not Run`, separately from passes.

Explicit `-DSYNERGY_ENABLE_INTERACTIVE_TESTS=ON` is reserved for a disposable test environment; native CI sets it in the common Configure command. Do not infer consent from CI environment variables, grant OS permissions, or treat opt-in as hardware/secure-desktop acceptance. Direct test-executable invocation bypasses this CTest safeguard.

### Package runtime completeness (RPACKAGE)

Every distributed MSI, portable archive, macOS app/DMG and native Linux package must contain the third-party libraries and plugins its applications need, including transitive dependencies and license notices. A build runner's Qt, OpenSSL, package manager or search path must not supply missing application dependencies at runtime. Static builds copy only dependencies that remain dynamically linked; static dependency license notices still ship.

The host supplies the supported OS kernel and ABI, Windows system APIs, macOS `/usr/lib` and `/System/Library`, or Linux glibc/loader and POSIX shell/coreutils. Display services, installed fonts and GPU drivers remain host components. These boundaries do not exempt Qt, OpenSSL, compiler redistributables or application plugins.

Package verification extracts a disposable payload without installing dependencies or services, checks every PE/Mach-O/ELF dependency and target architecture, and launches with isolated settings and dependency search paths. Report GUI platform-plugin startup, core loader startup, actual core connections and hardware permission acceptance separately. Prebuilt CI dependency checks do not establish that the local static release build passed. Flatpak delivery must use an offline OSTree repository archive containing the app, runtime and extensions, with hashes and bilingual installation instructions. A single app bundle is insufficient; complete offline delivery requires separate evidence for all components.

### Delivery constraints — do not "fix" these

This section is the authoritative list. The following are deliberate, not defects:

- **`synergy-daemon.exe` must stay a separate process.** It runs as a Windows service (session 0) and
  duplicates `winlogon.exe` / `logonui.exe` tokens to start the core on the secure desktop. Folding it
  into the GUI would silently drop UAC-prompt and login-screen support.
- **`synergy-core.exe` stays separate from the GUI.** The GUI launches it as a child process
  (`CoreProcess.cpp`). Merging them is possible for desktop mode only.
- **The portable package deliberately omits the daemon** (`deploy/windows/pre-cpack.cmake.in`); a
  portable archive cannot register a service. Do not "restore" it.
- **The Windows GUI file is `synergy_<X.Y.Z>.exe`.** `X.Y.Z` comes from `Version.cmake`
  (`SYNERGY_GUI_EXE_NAME`). Do not hardcode it, and do not put `-dev` or `Daemon` in that name.
  `synergy-core` and `synergy-daemon` stay unversioned: `Constants.h.in` `kCoreBinName` /
  `kDaemonBinName`, the Linux `.desktop` `Exec=`, and the WiX service/firewall component IDs
  look those two up by those exact names.
- **macOS ships a `.app` bundle inside a `.dmg`**, not a bare binary — signing and notarization require
  the bundle layout.
- **Nothing in this codebase registers the Windows service, and that is deliberate.**
  `synergy-daemon.exe` cannot install itself: there is no `--install-service` option and no
  `CreateService` call. The MSI provides registration via `ServiceInstall`. Do not implement a
  self-install option to satisfy older documentation — `docs/troubleshooting.md` documents the real
  (MSI or `sc create`) procedures.

### Modification workflow

1. Propose the approach before changing code; prefer the smallest correct change.
2. Keep `main` as the sole retained, long-lived branch. Use temporary `feature/<topic>` or `fix/<topic>` branches only to satisfy pull request and branch protection requirements, with one logical change per commit and Chinese Conventional Commit messages (R10). Open a pull request into `main`, merge after validation, and delete the temporary branch after integration. Do not push directly to protected `main`.
3. Verify with evidence (build log, command output, real run). Never report success on reasoning alone.
4. Keep the issue tracker in step with code changes.

### Known environment traps
- `vendor/vcpkg` is a shallow clone; the pinned baseline commit must be present before CMake
  configures.
- Deleting `vendor/` only costs a re-clone (~30 MB); dependency rebuild time is recovered by the vcpkg
  binary cache outside the repository.
- **Flaky or slow links to GitHub**: the first configure downloads large assets (Strawberry Perl is
  ~290 MB, required by OpenSSL; Qt 6 sources follow). On an unreliable connection these transfers can
  stall part-way. vcpkg stores them under `vendor/vcpkg/downloads/` and reuses anything already
  there, so simply re-running the build script resumes effectively — do not delete that directory to
  "start clean". vcpkg verifies every asset against a recorded SHA-512, so a partially written file is
  rejected rather than used.
- `vendor/vcpkg/downloads/` and the vcpkg binary cache are the only reasons a rebuild is cheap; keep
  them on disk between builds.

---

## 中文

### 适用范围

适用于本仓库的所有改动。若本文件的规则与临时要求冲突，应当**先提出冲突**，而不是默默破坏约定。

### 强制约束

| # | 规则 | 理由 |
|---|---|---|
| R1 | Windows 自动化脚本一律 `.bat`，**全仓库禁止出现任何 `.ps1`**。 | Windows 上只保留一种脚本运行时，规避执行策略问题。 |
| R2 | `.bat` 必须同时能被 `cmd /c` 与 PowerShell 调用。 | 调用方不固定，脚本不得假定其中一种。 |
| R3 | 脚本内容**只用 ASCII 字符**。 | 控制台代码页（GBK/UTF-8）会破坏批处理中的非 ASCII 字符，甚至导致解析错误。 |
| R4 | 不得出现机器专属绝对路径，不得要求预设环境变量。 | 每台机器环境不同；脚本须自行定位 VS、vcpkg 与工具链。 |
| R5 | 第三方依赖统一来自 vcpkg 的**清单（manifest）模式**，但有**一处已记录的例外**：**CI** 使用预编译 Qt 分发，并以 classic 模式用 vcpkg 仅装 OpenSSL。 | 发布构建的依赖来源唯一；CI 的例外以构建耗时换取了与发布构建的完全一致，故须保持版本兼容（见下）。 |
| R6 | vcpkg baseline 固定在 `vcpkg.json` 的 `builtin-baseline`，是**唯一真源**；脚本须读取它，不得复制一份。 | 保证克隆可复现。 |
| R7 | `vendor/`（vcpkg 检出）与 `build/` 永不入库，一律本地引导生成。 | 保持仓库轻量、可克隆。 |
| R8 | 代码必须能在 Windows、macOS、Linux 三平台编译。平台相关代码放在 `src/lib/platform/<os>/`。 | 单一源码树 + 条件编译。 |
| R9 | 文档在单一文件内中英双语（分节 `## English` + `## 中文`）。 | 一份文档，两类读者。 |
| R10 | 提交信息使用中文 Conventional Commits：`feat`、`fix`、`docs`、`refactor`、`test`、`ci`、`build` 或 `chore`，后面跟中文说明。 | 项目约定。英文说明会打断现有历史，也和双语文档不一致。 |
| R11 | 每项任务在动手前都用实时 todo 列表跟踪。状态一变就更新，每个环节都要有条目。列表缺失或过期时，先停下补记再继续。做法见 [任务进度](#任务进度-r11)。 | 进度必须可见。仍有未完成条目时，宣布完成无效。 |

### 任务进度 R11

同一做法写在 `.cursor/rules/todo-tracking.mdc`，每次会话都会注入。

第一步动手之前，以及给出最终答复之前：

1. 每个工作环节一条 todo。只有一步的任务也要两条：做这件事，再核对结果。
2. 开始某条之前先标成进行中。同一时刻只有一条处于进行中。
3. 某一步结束的同一轮里，把它标成已完成或已取消，再把下一条标成进行中。
4. 出现新环节就立刻补一条。只写在回复里、没有进列表的工作，算遗漏。

**违反时。** 没有另一套检查程序。代理自己停下：漏掉的条目还没按真实状态写上之前，不得开始下一步，也不得声称任务已完成。仍有待办或进行中的条目时，宣布完成的回复无效。只有确定不做的环节才标取消，并在条目文字里写明原因。

### 目录结构

新文件一律以下列现有结构为准：

- `src/apps/` —— 可执行程序（`synergy-gui`/`synergy-core`/`synergy-daemon`）与共享资源
  （`res/branding`、`res/icons`）。
- `src/lib/` —— 库：`arch/`、`base/`、`client/`、`common/`、`gui/`、`io/`、`mt/`、`net/`、
  `platform/`、`server/`，以及 `synergy/`（应用库本身 + TuPig overlay，见其 `gui/`/`hooks/`）。
- `src/unittests/` —— 每个被测模块一个目录，另有 `gui/`。
- `cmake/` —— CMake 模块（`Synergy.cmake`、`Version.cmake` 等）；`deploy/` —— 打包；
  `config/` —— 工具与测试配置；`scripts/` —— 用户入口，CI 专用辅助脚本放 `scripts/ci/`。
- `extra/` 已不存在 —— 不要恢复；资源放到上述真正拥有它的位置。

### 构建与依赖策略

- 用户可见入口只有两个：`setup.bat`（一次性安装宿主工具链）与 `scripts/build.bat` / `scripts/build.sh`
  （分平台构建）。不应再有其他前置步骤。
- `scripts/bootstrap-vcpkg.{bat,sh}` 负责克隆引导仓库内的 vcpkg 到 `vendor/vcpkg`，从 `vcpkg.json`
  读取固定 baseline，且可重复执行。
- C++ 工具链（MSVC / clang / gcc）、CMake、Ninja 是宿主前置条件，**不纳入仓库**。本地 macOS manifest
  构建还需要已选为活动开发者目录的完整 Xcode、pkgconf、Autoconf 及 autoconf-archive、Automake 和
  GNU Libtool，供固定端口构建使用。这些属于宿主构建工具；Qt、OpenSSL 及所有应用库依赖仍由 vcpkg 获取。
- 构建产物只有 64 位（`vcpkg.json` 的 `supports` 为 `x64 | arm64`）。不要增加 x86 / Win32 目标。
- Windows 上必须在 CMake 之前激活 MSVC 环境（`vcvarsall.bat x64`）。vcpkg 依赖 VC 环境变量来定位
  Visual Studio 实例；若缺少这些变量，在实例元数据没有 `isComplete` 标记的 Build Tools 上会报
  *"Could not locate a complete Visual Studio instance"*。`scripts/build.bat` 已自动处理，**不要删除**该步骤。
- 不要在静态 vcpkg triplet 下追加 `/MD`，也不要再引入依赖机器的 vcpkg 查找方式（`VCPKG_ROOT`、
  `C:\Users\<user>\vcpkg`）。
- **MSVC 运行库必须跟随 vcpkg triplet，而由 `CMakeLists.txt` 负责解析出它。**
  vcpkg 在普通 Windows 配置下**并不会**设置 `CMAKE_MSVC_RUNTIME_LIBRARY`：其
  `buildsystems/vcpkg.cmake` 仅在设置了 `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` 时才 include 平台工具链
  （`scripts/buildsystems/vcpkg.cmake:209`），且从不给该变量默认值，因此
  `toolchains/windows.cmake` —— 即「依据 `VCPKG_CRT_LINKAGE` 推导运行库」的那个文件 —— 根本不会
  执行；`VCPKG_CRT_LINKAGE` 在项目作用域亦为空（已实测）。故项目改为**直接读取 triplet 文件**
  中的该设置（退路是 vcpkg 的 `-static` 命名约定），据此设 `/MT` 或 `/MD`，并在配置日志中打印
  实际来源。**不要把任一方写死**：强行指定 `/MT` 会让所有动态 CRT 配置 —— 正是 CI 两条 Windows 腿
  所用的 —— 以 `/MT` 链接 `/MD` 依赖并报 `LNK2038 RuntimeLibrary mismatch`。唯一的真实约束是
  ASan：它需要动态 triplet，`windows-msvc-asan` 预设已提供，配置阶段亦会校验。

**CI 的依赖例外（R5）及其义务。** `.github/actions/install-dependencies` 通过
`jurplel/install-qt-action` 安装 Qt，并以 `--classic` 运行 `johnwason/vcpkg-action` 仅装 OpenSSL，
因此 CI 的 Qt **不来自** `vcpkg.json`。这是有意为之：vcpkg 从源码构建 Qt，每条 CI 腿要数小时，而
manifest 仍是发布构建与所有本地构建的唯一真源。该例外附带三项要求：

1. CI 所用 Qt 版本必须 **≥ 项目下限**（`CMakeLists.txt` 的 `REQUIRED_QT_VERSION`，当前 6.7.0，
   RHEL 8 回退 5.13）。CI 现固定为 6.9.3 / 6.10.3 / 6.11.1。**升级时须重新核对该下限**，不要凭假设。
2. CI 的 Qt 是**共享**的，而 manifest 构建的是**静态** Qt。因此 CI **并未覆盖**发布所用的静态 Qt 路径。
   把 CI 绿灯当作**逻辑**上的证据，而非静态链接模型的证据；后者由本地 `scripts/build.bat release` 覆盖。
3. 任何必须保留的差异（例如针对某个 Qt 版本的适配）都须记录在此处或 `docs/build.md` ——
   CI 与发布之间未记录的差异，正是二者静默漂移的成因。
4. Windows 上 CI 使用**动态 CRT（`/MD`）**，而 manifest 的 Windows triplet 使用**静态 CRT（`/MT`）**；
   CI 的 Windows 腿从不覆盖 `/MT`。该路径请用本地 `scripts\build.bat release` 验证
   （运行库由 `CMakeLists.txt` 的运行库块依据 triplet 推导）。

### 交互测试安全

注册新测试前须审查宿主副作用。默认 CTest 不得注入真实输入或替换用户实时系统剪贴板。`SYNERGY_ENABLE_INTERACTIVE_TESTS` 默认 `OFF`：当前平台存在的 `OSXKeyStateTests`、`OSXClipboardTests`、`MSWindowsClipboardTests` 及 X11 的 `XWindowsClipboardTests` 仍注册并编译，但设置 CTest `DISABLED` 属性。第一个会向焦点应用注入按键；剪贴板目标会清空/替换内容或改变 X11 selection 所有权。纯 Windows 转换、DropData、键状态和策略测试保持启用。禁用目标按 `Disabled` / `Not Run` 单独报告，不计为通过。

仅在一次性测试环境中显式设置 `-DSYNERGY_ENABLE_INTERACTIVE_TESTS=ON`；原生 CI 在公共 Configure 命令中设置。不得根据 CI 环境变量猜测同意、授予系统权限，或将开启选项当作硬件/安全桌面验收。直接运行测试程序会绕过这层 CTest 保护。

### 打包运行依赖完整性（RPACKAGE）

所有交付的 MSI、便携归档、macOS app/DMG 和 Linux 原生包都必须包含应用所需的第三方库及插件，包括传递依赖和许可声明。不能让构建机的 Qt、OpenSSL、包管理器或搜索路径补齐运行时缺失的应用依赖。静态构建只复制仍需动态链接的依赖；静态依赖的许可声明仍须随包交付。

宿主提供受支持的操作系统内核及 ABI、Windows 系统 API、macOS 的 `/usr/lib` 和 `/System/Library`，或 Linux 的 glibc/加载器及 POSIX shell/coreutils。显示服务、已安装字体和 GPU 驱动属于宿主组件。这些边界不能用于排除 Qt、OpenSSL、编译器可再分发运行库或应用插件。

包验证须解包到一次性目录，不安装依赖或服务；检查每个 PE/Mach-O/ELF 的依赖及目标架构，并在隔离设置与依赖搜索路径后启动。GUI 平台插件启动、core 加载启动、core 实际连接和硬件权限验收应分开报告。CI 预编译依赖检查不代表本机静态发布构建已通过。Flatpak 必须交付包含应用、runtime 和 extensions 的离线 OSTree 仓库归档，附带哈希及双语安装说明。单个应用包不足以交付，完整离线验收必须另有所有组件的证据。

### 交付约束 —— 不要把这些当缺陷「修复」

本节即权威清单。以下均为**有意设计**，不是缺陷：

- **`synergy-daemon.exe` 必须保持独立进程。** 它以 Windows 服务（会话 0）运行，通过复制
  `winlogon.exe` / `logonui.exe` 令牌把 core 启动到安全桌面。若合并进 GUI，会静默失去 UAC 提示与
  登录界面支持。
- **`synergy-core.exe` 与 GUI 保持分离。** GUI 以子进程方式启动它（`CoreProcess.cpp`）。仅在桌面
  模式下才谈得上合并。
- **便携包有意不含 daemon**（`deploy/windows/pre-cpack.cmake.in`）；便携归档无法注册服务。不要
  「恢复」它。
- **Windows GUI 文件名是 `synergy_<X.Y.Z>.exe`。** `X.Y.Z` 只来自 `Version.cmake`
  （`SYNERGY_GUI_EXE_NAME`），不要写死，也不要带 `-dev` 或 `Daemon`。
  `synergy-core` 与 `synergy-daemon` 仍不带版本号：`Constants.h.in` 的 `kCoreBinName` /
  `kDaemonBinName`、Linux `.desktop` 的 `Exec=`、WiX 服务与防火墙组件 ID 都按这两个固定名字查找。
- **macOS 交付的是 `.dmg` 内的 `.app` bundle**，而非裸二进制 —— 签名与公证都要求 bundle 结构。
- **本代码库中没有任何地方注册 Windows 服务，这是有意为之。**
  `synergy-daemon.exe` 无法自行安装：既无 `--install-service` 选项，也无 `CreateService` 调用。
  服务注册由 MSI 通过 `ServiceInstall` 提供。不要为了让旧文档「成真」而去实现自安装选项 ——
  `docs/troubleshooting.md` 记录的是真实做法（MSI 或 `sc create`）。

### 改动流程

1. 动手改代码前先给出方案，优先选择最小且正确的改动。
2. `main` 是唯一长期保留的分支。仅为满足 Pull Request 和分支保护要求使用临时 `feature/<主题>` 或 `fix/<主题>` 分支；一个提交只做一件逻辑独立的事，提交信息遵循中文 Conventional Commits（R10）。向 `main` 开 Pull Request，验证通过后合并，整合完成后删除临时分支。不要直接推送到受保护的 `main`。
3. 必须有证据验证（构建日志、命令输出、真实运行）。严禁仅凭推理声称通过。
4. 代码变更须同步更新 GitHub Issues（问题追踪；关联 issue 验证通过后关闭并附证据）。

### 已知环境陷阱

- `vendor/vcpkg` 是浅克隆；CMake 配置前必须确保固定 baseline 提交存在。
- 删除 `vendor/` 只需重新克隆（约 30 MB）；依赖重建耗时可由仓库外的 vcpkg 二进制缓存弥补。
- **GitHub 链路慢或抖动时**：首次配置需要下载大体积资源（Strawberry Perl 约 290 MB，为 OpenSSL
  所需；随后还有 Qt 6 源码）。链路不稳时传输可能中途停住。vcpkg 会把这些文件缓存于
  `vendor/vcpkg/downloads/`，已存在的文件会被复用，因此**重新运行构建脚本即可有效续传** —— 不要为了
  “干净重来”而删除该目录。vcpkg 会用记录的 SHA-512 校验每个资源，写入不完整的文件会被拒绝而非误用。
- `vendor/vcpkg/downloads/` 与 vcpkg 二进制缓存是重复构建成本低的关键，请在两次构建之间保留它们。
