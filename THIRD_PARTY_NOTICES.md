# Third-Party Notices

**Equicord Plugin Manager** is MIT (see [LICENSE](LICENSE)).

## Qt 6

This app uses Qt 6 under the [LGPLv3](https://www.gnu.org/licenses/lgpl-3.0.html). Qt is © The Qt Company Ltd and contributors. This project is not affiliated with them.

Qt is dynamically linked - `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll` and friends sit next to the exe and can be swapped for any compatible Qt 6 build (same major version and MSVC ABI). Nothing is statically linked. Qt source is available at [qt.io](https://www.qt.io/).

**If you redistribute this:** ship the Qt DLLs `windeployqt` put next to the exe, keep this file and `LICENSE` with it, and don't statically link Qt. If you ship a modified Qt, you also have to provide its source (LGPLv3 §4-6).