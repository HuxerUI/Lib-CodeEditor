# SweetEdit Huxer

A cross-platform code editor implemented as a declarative HuxerUI component. The component reuses the SweetEditor editor core and the SweetLine syntax highlighting engine, exposing text editing, syntax highlighting, completion, search and replace, and more to the application layer the HuxerUI-native way.

## Features

- Declarative component: `SweetEditor(options, controller)` returns an ordinary HuxerUI `View`
- Retained `NodeExtension`: the editor core, document, highlighter, and input state are held by the mounted node
- Syntax highlighting: SweetLine JSON grammar rules, incremental analysis, bracket matching, indentation guides, rainbow brackets
- Editing: caret/selection, undo/redo, auto-indent, auto-closing brackets, Tab/space policy, read-only
- Code completion: provider-driven completion panel, snippet expansion, replacement ranges
- Search and replace: built-in declarative search bar, or programmatic control via `SweetEditorController`
- Decorations: diagnostics, inlay hints, code lens, gutter icons, diff presentation, phantom text
- Folding and links: code folding, clickable links
- Typed events: caret/selection/text/scroll/fold/gesture/click events are all emitted through the HuxerUI event system
- Platforms: Windows, macOS, Linux, Web, Android, iOS (following HuxerUI platform support)

## Quick Start

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

## Repository Layout

```text
sweetedit_huxer/
├── CMakeLists.txt                   # dependency library (sweetedit_core)
├── include/sweetedit_core/
│   └── sweet_editor.h               # public API: options, events, controller
├── src/sweet_editor/                # component implementation (retained extension)
│   ├── sweet_editor.cpp
│   ├── sweetline_highlighter.h
│   └── sweetline_highlighter.cpp
├── resources/                       # built-in component syntax rules
├── examples/preview/                # demo app that uses the library
│   ├── CMakeLists.txt
│   ├── src/app.cpp
│   ├── platform/                    # per-platform shells
│   └── resources/                   # demo sample files and grammars
├── docs/
│   └── sweet_editor.md              # detailed component documentation
└── 3dparty/
    ├── SweetEditor/                 # editor core (vendored, gitlink)
    └── SweetLine/                   # syntax highlighting engine (vendored, gitlink)
```

## Public API Overview

```cpp
// Declarative configuration + providers (document, font, editing behavior, completion, decorations, diff, display)
struct SweetEditorOptions;

// Document and search control
class SweetEditorController;
inline SweetEditorController UseSweetEditorController();

// Typed events (bound on the View returned by SweetEditor())
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

## HuxerUI Dependency and Installation

This repository is a **HuxerUI dependency library**: `sweetedit_core` is built on top of the HuxerUI SDK (it links `HuxerUI::huxerui`) and does not contain HuxerUI sources itself. **Machines without HuxerUI installed cannot build this project** — install it first as described in this section.

### Where HuxerUI Is Installed (HUXERUI_HOME)

The HuxerUI installer places the SDK in the following default locations and does two things at the same time:

- writes the `HUXERUI_HOME` environment variable (pointing at the SDK root);
- adds `$HUXERUI_HOME/bin` to `PATH` (providing the `huxerui` CLI).

| Platform | Default install location |
|---|---|
| Windows | `%LOCALAPPDATA%\HuxerUI` |
| macOS | `~/Library/Developer/HuxerUI` |
| Linux | `~/.local/share/HuxerUI` |
| Android (Termux) | `~/.local/share/HuxerUI` |

### Installing HuxerUI

Windows (PowerShell):

```powershell
irm https://github.com/HuxerUI/HuxerUI/releases/latest/download/install.ps1 | iex
```

macOS, Linux, and Android (works on Termux too):

```bash
curl -fsSL https://github.com/HuxerUI/HuxerUI/releases/latest/download/install.sh | sh
```

**Open a new terminal** after installation, then verify:

```bash
huxerui doctor
```

Explicit version or custom prefix:

```bash
curl -fsSLO https://github.com/HuxerUI/HuxerUI/releases/latest/download/install.sh
sh install.sh --version 0.1.0 --prefix "$HOME/Environment/HuxerUI" --yes
```

Offline install from a downloaded archive (place the matching `.sha256` file beside it):

```bash
sh install.sh --archive ./huxerui-sdk-0.1.0-android-arm64-v8a.tar.gz --yes
```

Uninstall (use the same prefix used for installation):

```bash
sh install.sh --uninstall --yes
```

> Installing HuxerUI does not include platform SDKs, compilers, or Android tooling (NDK, CMake, ninja, and so on); those still need to be prepared separately. Use `huxerui doctor` / `huxerui setup` to check.

### How This Repository Uses HuxerUI

The root `CMakeLists.txt` reads `HUXERUI_HOME` at configure time and supports two modes:

1. **Point at an installed SDK** (recommended): the SDK ships CMake package files and the project finds `HuxerUI::huxerui` through `find_package(HuxerUI CONFIG REQUIRED)`;
2. **Point at a HuxerUI source directory**: if `$HUXERUI_HOME` contains `CMakeLists.txt` and `include/huxerui/huxerui.h`, the project adds the SDK directly with `add_subdirectory` and builds it from source (useful when developing HuxerUI itself).

The component build also needs the host code generation tools `hrc`/`hcg`: an installed SDK provides them automatically under `share/huxerui/tools`; a source directory keeps them under `tools/prebuilt/<platform>/<architecture>/`. Termux-specific handling is described below.

## Building

### Android (on a normal computer)

`HUXERUI_HOME` is normally written by the HuxerUI installer, so it needs no manual export; this just shows how to override it with another SDK location:

```bash
cd examples/preview/platform/android
HUXERUI_HOME=/path/to/huxerui sh gradlew :app:assembleRelease --no-daemon
```

### No computer? Build in Termux (important)

This repository supports completing an Android build **using only a phone (Termux environment)**, but Termux differs a lot from a normal computer, so please note the following carefully.

#### 1. Environment preparation

First install the HuxerUI SDK (Termux supports the official installer; it defaults to `~/.local/share/HuxerUI` on the /data partition, where tools run directly):

```bash
curl -fsSL https://github.com/HuxerUI/HuxerUI/releases/latest/download/install.sh | sh
```

Then install the build tools:

```bash
pkg install clang cmake ninja openjdk-21 python
```

You also need the Android SDK and NDK (install them under the Termux private directory, not on shared storage):

```bash
export ANDROID_HOME=$HOME/android-sdk
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/29.0.14206865
```

And the HuxerUI host code generation tools (hrc/hcg). **If HuxerUI was installed with the installer on the /data partition, the tools are included and executable — skip this step**; only when `HUXERUI_HOME` points at a HuxerUI source directory on shared storage (/sdcard) do you need to **copy the tools to the /data partition and mark them executable**:

```bash
mkdir -p $HOME/huxerui-tools/host-tools/android/arm64-v8a
cp <huxerui>/tools/prebuilt/android/arm64-v8a/{hrc,hcg} $HOME/huxerui-tools/host-tools/android/arm64-v8a/
chmod 755 $HOME/huxerui-tools/host-tools/android/arm64-v8a/{hrc,hcg}
```

> The FUSE filesystem on shared storage (/sdcard) **does not support setting the executable bit**; only tools on the /data partition can run.

#### 2. Use the dedicated huxerui-termux tool

This repository provides `tools/huxerui-termux`, which wraps every Termux-specific step (working around the missing exec bit, injecting the cross toolchain, redirecting staging, overriding aapt2):

```bash
cd examples/preview
bash ../tools/huxerui-termux build android [--profile debug|release]
bash ../tools/huxerui-termux run android        # requires an adb-connected device
```

#### 3. Manual build (without the tool)

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

#### 4. Key Termux gotchas

- **gradlew cannot be executed directly**: shared storage has no exec bit, so use `sh gradlew ...`;
- **aapt2 must be overridden**: the SDK's bundled aapt2 is a Linux ELF and cannot run under Termux; `-Pandroid.aapt2FromMavenOverride=/data/data/com.termux/files/usr/bin/aapt2` points at the Termux-native version;
- **staging on a non-FUSE partition**: `-PtermuxCxxStaging=$HOME/...`, otherwise stale mtimes on shared storage make ninja rebuild/miss builds repeatedly;
- **host tool path**: newer HuxerUI resolves `android/arm64-v8a` on Termux; an installed SDK keeps the tools under `share/huxerui/tools/`, a source directory under `tools/prebuilt/`. If automatic resolution fails (for example `HUXERUI_HOME` points at shared storage), copy the tools to the /data partition as in step 1;
- **CMake cross toolchain**: `-PtermuxAndroidToolchain=platform/android/termux-android-toolchain.cmake` (shipped in the repository); Termux's Android userland cannot use the NDK toolchain;
- **HUXERUI_HOST_TOOL_ROOT**: newer HuxerUI needs this variable to point at the host tool directory (an installed SDK sets it automatically through the CMake package, so it only needs to be set manually for a source directory on shared storage; otherwise it falls back to a shared-storage path without exec permission);
- a full `assembleRelease` may fail under Termux because of system limits; prefer `buildCMakeRelease[arm64-v8a]` or the tool above.

### Desktop / Web / iOS

- Windows, Linux, macOS: `huxerui::RunApplication()` platform entry
- Web: Emscripten WebAssembly (`platform/web/`)
- iOS: the Xcode project under `platform/ios/`

## Third-Party Dependencies

- [SweetEditor](3dparty/SweetEditor) — cross-platform editor core (vendored, independent repo as gitlink)
- [SweetLine](3dparty/SweetLine) — cross-platform syntax highlighting engine (vendored, independent repo as gitlink)

Both third-party repositories are recorded as gitlinks and their contents are never modified.

## Documentation

- [Component documentation](docs/sweet_editor.md) — API, architecture, examples, limitations, and validation checklist

## License

Component code license is pending; SweetEditor and SweetLine follow the licenses of their respective upstream repositories.
