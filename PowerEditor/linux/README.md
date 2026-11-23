# LinuxPad++ (Notepad++ for Linux, Qt 6)

LinuxPad++ is a native Linux front end for Notepad++. The Win32 user interface of
`PowerEditor/src` is replaced by Qt code in `PowerEditor/linux/src`. These parts
are used as they are:

- `scintilla/` (Qt platform layer) with Notepad++'s boost regex search engine (`boostregex/`)
- `lexilla/`, including the Notepad++ lexers (search results, UDL)
- the portable parts of `PowerEditor/src`: `EncodingMapper`, `uchardet`, `pugixml`,
  `menuCmdID.h`, the toolbar and tab bar icons
- the data files: `langs.model.xml`, `stylers.model.xml`, themes, `nativeLang`
  translations, auto-completion APIs

The menus, the default shortcuts and the language list are generated at build
time from `Notepad_plus.rc`, `Parameters.cpp`, `localization.cpp` and
`ScintillaEditView.cpp` (`tools/gen_tables.py`), so they follow the Windows sources.

## Build on Fedora

```bash
sudo dnf install gcc-c++ cmake ninja-build python3 qt6-qtbase-devel qt6-qt5compat-devel pcre2-devel libxml2-devel
```

Install `dotnet-sdk-10.0` to include JSONTools. Its headless engine runs with
`dotnet-runtime-10.0` on Fedora.

```bash
cmake -S PowerEditor/linux -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build-linux
```

```bash
./build-linux/bin/linuxpad++
```

Install (binary, data files, desktop entry and icon). The desktop entry gets the
absolute paths of the prefix chosen at configure time, so set the prefix with
`CMAKE_INSTALL_PREFIX` rather than `cmake --install --prefix`. For the current user only:

```bash
cmake -S PowerEditor/linux -B build-linux -DCMAKE_INSTALL_PREFIX="$HOME/.local"
```

```bash
cmake --install build-linux
```

System-wide, configure with `-DCMAKE_INSTALL_PREFIX=/usr/local` and run the
install step with `sudo`.

## Flatpak

The GitHub release provides a Flatpak bundle for Linux x86-64. Install the
downloaded `.flatpak` file with `flatpak install --user ./<bundle>.flatpak`.
The Flatpak manifest packages the bundled Linux plugins and the .NET runtime used by JSONTools.

To build the bundle from the repository root, install Flatpak Builder and the
following dependencies from Flathub:

```bash
flatpak install --user flathub org.kde.Platform//6.10 org.kde.Sdk//6.10 \
    org.freedesktop.Sdk.Extension.dotnet10//25.08
mkdir -p build-linux/flatpak
flatpak-builder --user --force-clean --repo=build-linux/flatpak/repo \
    --state-dir=build-linux/flatpak/state build-linux/flatpak/app \
    PowerEditor/linux/flatpak/io.github.SkeletonSkelettron.LinuxPadPlusPlus.yaml
flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    build-linux/flatpak/repo build-linux/flatpak/linuxpad-plus-plus-x86_64.flatpak \
    io.github.SkeletonSkelettron.LinuxPadPlusPlus stable
```

## Settings

User settings, the session and the backups of unsaved documents are stored in
`~/.config/linuxpad++/`. Put a `langs.xml` or `stylers.xml` there to override the
shipped models. Imported themes go to `~/.config/linuxpad++/themes/`.

## Plugins

The plugins are installed with LinuxPad++, but only the ones chosen in **Plugins > Plugins Admin…** are loaded
(none at the first start; saved as `[Plugins] enabled=` in `~/.config/linuxpad++/config.ini`, applied at the next
start). The [main README](../../README.md#plugins) explains how to choose and install plugins. See the
[bundled plugin list](PLUGINS_STATUS.md#bundled-modules) for their features. JSONTools builds when the .NET 10 SDK is present. Other plugins
need a source port that produces a Linux `.so` library.

- **Diff Checker > Compare…** compares two tabs, files from disk, or a tab against
  its saved copy. It includes unsaved changes, aligns lines and lets you jump
  between change blocks. Each input can be up to 8 MiB or 100,000 lines. Files
  from disk can be UTF-8 or have a Unicode BOM. Line endings are normalized for comparison.
- **JSONTools** supports pretty-printing, compression, syntax checks, tree
  navigation, RemesPath queries, JSON Lines, array sorting, schema validation
  and generation, YAML output, and JSON string conversion. It uses the upstream
  parser and query engine. API import, the original find/replace and CSV forms,
  filename-driven schema validation and the Windows settings dialog are still
  pending.

Build options are `-DNPP_BUILD_PLUGINS=OFF` and `-DNPP_BUILD_JSONTOOLS=OFF`.
Use `-noPlugin` at launch to skip loading plugins. For a standalone module,
use **Settings > Import > Import plugin(s)…** (it is chosen too) and restart. For modules with companion files,
put the complete plugin folder in the location shown by **Plugins > Open Plugins Folder…**.

The [plugin status](PLUGINS_STATUS.md) describes the Linux SDK and per-plugin coverage.
The [porting status](PORTING_STATUS.md) tracks editor features. With `-DNPP_BUILD_TESTS=ON`,
`npp_menu_coverage` lists the menu commands that are not implemented and
`ctest --test-dir build-linux --output-on-failure` runs the editor and plugin tests.
