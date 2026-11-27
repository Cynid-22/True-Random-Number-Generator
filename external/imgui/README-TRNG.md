# Dear ImGui in TRNG

- **Upstream:** https://github.com/ocornut/imgui
- **Version:** v1.92.6 (tag `v1.92.6`, commit `6ded5230d043aa32c755e65c910c2af5002fb9f9`, 2026-02-17)
- **License:** MIT, in `LICENSE.txt`

Only the files TRNG compiles are kept: the core files, `imconfig.h`, the three `imstb_*` headers and the Win32 and DirectX 11 backends.

## Local changes

Every other file is exactly as in the tag. The two changes are marked with a `TRNG:` comment:

| File | Change |
|---|---|
| `backends/imgui_impl_dx11.cpp` | Loads `D3DCompile` from `d3dcompiler_47.dll` in System32 only (`LoadLibraryExW` with `LOAD_LIBRARY_SEARCH_SYSTEM32`) instead of linking `d3dcompiler`. |
| `backends/imgui_impl_win32.cpp` | Loads the three `dwmapi.dll` functions in `ImGui_ImplWin32_EnableAlphaCompositing` from System32 only instead of linking `dwmapi`. |

Both keep a DLL placed next to `TRNG.exe` from being loaded.

## Updating

1. Pick a release tag. Do not copy files from the `master` branch.
2. Copy the files listed above from the tag, and `LICENSE.txt`.
3. Apply the two changes again.
4. Update the version, commit and date on this page.
5. Check that `git diff <tag> -- <file>` in an upstream clone shows only the two changes.
6. Build, run the regression tests and check the UI.
