# SweetEdit Huxer

HuxerUI 声明式组件实现的跨平台代码编辑器。组件复用 SweetEditor 编辑器内核与 SweetLine 语法高亮引擎，把文本编辑、语法高亮、补全、搜索替换等能力以 HuxerUI 原生方式暴露给应用层。

## 特性

- 声明式组件：`SweetEditor(options, controller)` 返回普通 HuxerUI `View`
- retained `NodeExtension`：编辑器内核、文档、高亮分析器与输入状态由挂载节点持有
- 语法高亮：SweetLine JSON 语法规则、增量分析、括号匹配、缩进线、彩虹括号
- 编辑能力：光标/选区、撤销重做、自动缩进、自动闭合括号、Tab/空格策略、只读
- 代码补全：Provider 驱动的补全面板、Snippet 展开、替换范围
- 搜索替换：内置声明式搜索栏，或经 `SweetEditorController` 程序化控制
- 装饰：诊断、Inlay hint、CodeLens、行号区图标、Diff 展示、Phantom text
- 折叠与链接：代码折叠、可点击链接
- typed events：光标/选区/文本/滚动/折叠/手势/点击事件全部通过 HuxerUI 事件系统发出
- 平台：Windows、macOS、Linux、Web、Android、iOS（随 HuxerUI 平台支持）

## 快速开始

```cpp
#include <huxerui/huxerui.h>

#include "sweet_editor/sweet_editor.h"

using namespace huxerui;
using namespace sweetedit_huxer;

View App() {
  SweetEditorOptions options;
  options.document_key = "hello.cpp";
  options.initial_text =
      "#include <iostream>\n"
      "int main() {\n"
      "  std::cout << \"Hello\";\n"
      "  return 0;\n"
      "}\n";

  return SweetEditor(std::move(options)).With(Grow{});
}

const Application application{
    App,
    {.window = {.title = "SweetEdit Huxer", .initial_size = {900.0F, 640.0F}}},
};
```

## 目录结构

```text
sweetedit_huxer/
├── CMakeLists.txt                   # 依赖库（sweetedit_core）
├── include/sweetedit_core/
│   └── sweet_editor.h               # 公共 API：配置、事件、controller
├── src/sweet_editor/                # 组件实现（retained extension）
│   ├── sweet_editor.cpp
│   ├── sweetline_highlighter.h
│   └── sweetline_highlighter.cpp
├── resources/                       # 组件内置语法规则
├── examples/preview/                # 引用库的 Demo 应用
│   ├── CMakeLists.txt
│   ├── src/app.cpp
│   ├── platform/                    # 各平台 shell
│   └── resources/                   # Demo 示例文件与语法
├── docs/
│   └── sweet_editor.md              # 组件使用文档（详细）
└── 3dparty/
    ├── SweetEditor/                 # 编辑器内核（vendored, gitlink）
    └── SweetLine/                   # 语法高亮引擎（vendored, gitlink）
```

## 公共 API 一览

```cpp
// 声明式配置 + providers（文档、字体、编辑行为、补全、装饰、Diff、显示）
struct SweetEditorOptions;

// 文档与搜索控制
class SweetEditorController;
inline SweetEditorController UseSweetEditorController();

// typed events（绑定在 SweetEditor() 返回的 View 上）
struct SweetEditorTextChanged;
struct SweetEditorCursorChanged;
struct SweetEditorSelectionChanged;
struct SweetEditorScrollChanged;
struct SweetEditorFoldToggled;
struct SweetEditorLongPressed;
struct SweetEditorDoubleTapped;
struct SweetEditorLinkClicked;
struct SweetEditorCodeLensClicked;
struct SweetEditorGutterIconClicked;
struct SweetEditorInlayClicked;

View SweetEditor(SweetEditorOptions options = {}, SweetEditorController controller = {});
```

## 构建

### Android（普通电脑）

```bash
cd examples/preview/platform/android
HUXERUI_HOME=/path/to/huxerui sh gradlew :app:assembleRelease --no-daemon
```

### 没有电脑？在 Termux 中构建（重要）

本仓库支持**只用手机（Termux 环境）**完成 Android 构建，但 Termux 与普通电脑差异很大，请务必注意以下事项。

#### 1. 环境准备

```bash
pkg install clang cmake ninja openjdk-21 python
```

还需要 Android SDK 与 NDK（建议装在 Termux 私有目录，不要放在共享存储）：

```bash
export ANDROID_HOME=$HOME/android-sdk
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/29.0.14206865
```

以及 HuxerUI 的 host 代码生成工具（hrc/hcg）。它们必须**复制到 /data 分区并加执行权限**，否则无法运行：

```bash
mkdir -p $HOME/huxerui-tools/host-tools/android/arm64-v8a
cp <huxerui>/tools/prebuilt/android/arm64-v8a/{hrc,hcg} $HOME/huxerui-tools/host-tools/android/arm64-v8a/
chmod 755 $HOME/huxerui-tools/host-tools/android/arm64-v8a/{hrc,hcg}
```

> 共享存储（/sdcard）的 FUSE 文件系统**不支持设置执行位**，工具必须放在 /data 分区。

#### 2. 使用专用工具 huxerui-termux

本项目提供 `tools/huxerui-termux`，已封装 Termux 所需的所有步骤（绕过无执行位、注入交叉工具链、staging 重定向、aapt2 覆盖）：

```bash
cd examples/preview
bash ../tools/huxerui-termux build android [--profile debug|release]
bash ../tools/huxerui-termux run android        # 需要 adb 连接设备
```

#### 3. 手动构建（不用工具时）

```bash
cd examples/preview/platform/android
export HUXERUI_HOME=/storage/emulated/0/资源/huxerui
export HUXERUI_HOST_TOOL_ROOT=$HOME/huxerui-tools/host-tools
sh gradlew :app:assembleRelease \
  -PtermuxAndroidToolchain=$PWD/termux-android-toolchain.cmake \
  -PtermuxCxxStaging=$HOME/sweetedit-android-cxx \
  -PhuxeruiAbis=arm64-v8a \
  -Pandroid.aapt2FromMavenOverride=/data/data/com.termux/files/usr/bin/aapt2 \
  --no-daemon
```

#### 4. Termux 关键注意点

- **gradlew 无法直接执行**：共享存储无执行位，必须用 `sh gradlew ...`；
- **aapt2 必须覆盖**：SDK 自带的 aapt2 是 Linux ELF，在 Termux 无法运行；`-Pandroid.aapt2FromMavenOverride=/data/data/com.termux/files/usr/bin/aapt2` 指向 Termux 原生版本；
- **staging 放非 FUSE 分区**：`-PtermuxCxxStaging=$HOME/...`，否则共享存储的 mtime 不更新会让 ninja 反复重编/漏编；
- **host 工具路径**：新版 HuxerUI 在 Termux 下从 `tools/prebuilt/android/arm64-v8a/` 解析，需按第 1 条复制到 /data 分区；
- **CMake 交叉工具链**：`-PtermuxAndroidToolchain=platform/android/termux-android-toolchain.cmake`（仓库自带），Termux 的 Android 用户态无法使用 NDK 自带工具链；
- **HUXERUI_HOST_TOOL_ROOT**：新版 HuxerUI 需要该变量指向 host 工具目录（否则回退到无执行权限的共享存储路径）；
- 完整 `assembleRelease` 在 Termux 可能因系统限制失败，优先用 `buildCMakeRelease[arm64-v8a]` 或上面的工具。

### 桌面 / Web / iOS

- Windows、Linux、macOS：`huxerui::RunApplication()` 平台入口
- Web：Emscripten WebAssembly（`platform/web/`）
- iOS：`platform/ios/` 的 Xcode 工程

## 第三方依赖

- [SweetEditor](3dparty/SweetEditor) — 跨平台编辑器内核（vendored，独立仓库 gitlink）
- [SweetLine](3dparty/SweetLine) — 跨平台语法高亮引擎（vendored，独立仓库 gitlink）

两个第三方仓库以 gitlink 形式记录，不修改其内容。

## 文档

- [组件使用文档](docs/sweet_editor.md) — API、架构、示例、限制与验证清单

## 许可

组件代码许可待定；SweetEditor 与 SweetLine 遵循各自上游仓库的许可。
