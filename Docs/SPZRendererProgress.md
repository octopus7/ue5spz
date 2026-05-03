# SPZ Renderer Plugin Progress

## 2026-05-03 12:47:42 +09:00

- Checked the project layout. The project is a minimal UE 5.7 project with `Config`, `Content`, and no existing `Source` or `Plugins` directory.
- Decided to create a project plugin instead of modifying engine source.
- Target architecture:
  - Runtime module: cooked `USpzAsset`, BP-facing component/actor, lightweight preview render proxy.
  - Editor module: `.spz` importer, reimport support, official SPZ decode path.
  - SPZ/zstd decode dependencies stay editor-only so packaged runtime does not pay file decode cost.

## 2026-05-03 12:49:23 +09:00

- Confirmed the official SPZ v4 path uses ZSTD-compressed attribute streams.
- Confirmed UE 5.7 has a `zlib` third-party module, but no directly reusable standalone zstd C++ module for plugin import.
- Downloaded official `nianticlabs/spz` and `facebook/zstd` sources into temporary `.codex_tmp_*` folders for review and vendoring.

## 2026-05-03 12:51:23 +09:00

- Created `Plugins/SpzRenderer`.
- Added `SpzRuntime`:
  - `USpzAsset` stores cooked splat data, SH payload, metadata, and bounds.
  - `USpzComponent` is BP-spawnable and exposes sort policy settings.
  - `ASpzActor` wraps the component for level placement.
  - Current render path is a lightweight point preview scene proxy, intended as a safe runtime foundation before replacing the proxy with a GPU splat pass.
- Added `SpzEditor`:
  - `.spz` import factory and reimport handler.
  - Asset definition for Content Browser classification.
  - Official SPZ loader and zstd source are vendored under the editor module only.

## 2026-05-03 12:57:00 +09:00

- Built the plugin with:
  - `Build.bat UnrealEditor Win64 Development -Project="D:\github\spzdemo\spzdemo.uproject" -WaitMutex -NoLiveCoding -NoHotReloadFromIDE`
- Result: succeeded.
- Notes:
  - Initial build exposed an incorrect `AssetImportData` include and official SPZ shadow-variable warnings. Fixed both.
  - Live Coding was active, so the build required `-NoLiveCoding -NoHotReloadFromIDE`.

## 2026-05-03 12:58:28 +09:00

- Verified editor asset creation through `ImportAssets` commandlet using the official SPZ sample `hornedlizard.spz`.
- Import decoded `786,233` splats with `shDegree=3`.
- Result: commandlet completed with `0 error(s), 0 warning(s)` and saved `/Game/SPZImportTest/hornedlizard`.
- Removed the generated 400MB verification asset and temporary cloned source folders after recording the result. The actual plugin keeps only the required vendored SPZ/zstd source and license files.

## 2026-05-03 12:59:12 +09:00

- Removed generated plugin `Binaries` and `Intermediate` folders after successful build verification.
- Added `USpzActorFactory` so imported `USpzAsset` objects can be dragged into a level and placed as `ASpzActor` instances with the asset assigned to `USpzComponent`.

## 2026-05-03 13:01:14 +09:00

- Rebuilt after adding `USpzActorFactory`.
- Result: `UnrealEditor` build succeeded.
- Removed generated plugin `Binaries` and `Intermediate` again so the repository keeps source artifacts only.

## 2026-05-03 13:03:25 +09:00

- Reworked `USpzAsset` storage from an array of `FSpzSplat` USTRUCTs to packed arrays:
  - `Positions` as `FVector3f`
  - `Rotations` as `FVector4f`
  - `Scales` as `FVector3f`
  - `Colors` as `FColor`
  - SH data remains a float payload for future higher quality shading.
- Rebuilt successfully after the storage change.
- Re-ran the official sample import. Result: `0 error(s), 0 warning(s)`.
- The same sample asset dropped from about 400MB to about 176MB after the packed-array change.
- Removed the temporary sample import asset, temporary clone, and generated plugin build folders.
