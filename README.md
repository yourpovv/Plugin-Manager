![banner](assets/banner.png)

# Equicord Plugin Manager
**Auto installer for unofficial Equicord plugins** — by [yourpov.dev](https://yourpov.dev)

Equicord comes with its own plugins, This installs the ones it doesn't. easily install plugins people post on GitHub or send you as a zip. Normally you'd move folders and run build commands yourself, this does it in one click.

> Note: You need Equicord already installed.

## Usage
1. `File > Settings`: set your Equicord folder path and pick your Discord (Stable, PTB, Canary etc)
2. Drag the plugin on the window (supports `folders`, `.zip`, `.rar`, `.7z` and `github links`)
3. Press **Install**

Discord closes, plugin gets added, Equicord rebuilds, Discord reopens. (the run time takes a minute or two)

## Warning
Unofficial plugins are unauthorized code from strangers running inside your Discord. Only install ones you trust. This app doesn't check what a plugin does.

## Notes
- GitHub `tree` links fetch that folder directly from the repo
- Only copies `.ts` / `.tsx` / `.css`
- Installs `node` and `pnpm` for you if its missing
- Installs plugins to `src/userplugins/`

## Build
Needs VS2022, CMake, and Qt 6.8:
```bash
pip install aqtinstall
aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir C:\Qt

cmake -S . -B build/qt -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build/qt --config Release
```
Output: `build/EquicordPluginManager.exe` + Qt DLLs

## License

[MIT](LICENSE) © [YourPOV](https://github.com/yourpov) - Qt is LGPLv3, see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
