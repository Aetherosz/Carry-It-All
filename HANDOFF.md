# InventoryBonus â€” handoff

Notes for picking this project up on a new PC.

## Status (2026-11-09)
- **v1.0.0 works in-game.** In a real save, `/invbonus` printed `18 default + 12 vanilla upgrades + 200 mod upgrades + 1000 bonus = 1230 slots`.
- The settings menu entry (Mods â†’ Mod Configuration â†’ Inventory Bonus) registers, and SML writes `FactoryGame\Configs\InventoryBonus.cfg`.
- The author is set to `Aetherosz`.
- **Not yet verified:**
  - Shrinking with `/invbonus 0` and growing back. Check the scrollbar length: 0 should give 230 slots.
  - Changing the value in the pause-menu settings live.
  - Multiplayer.
- **Known cosmetic issue:** `Resources/Icon128.png` is missing, so SML logs an error and shows a placeholder icon. Add a 128Ã—128 PNG at `Resources/Icon128.png` to fix it.

## What the mod does
total slots = default + vanilla schematic slot unlocks + other mods' schematic slot unlocks + bonus

- There are no milestones; the bonus applies by default. It is adjustable from 0 to 20000 (default 1000).
- Everything except the bonus is recomputed from purchased schematics on every resize. Schematics under `/Game/` count as vanilla; anything else counts as a mod upgrade.
- Shrinking never drops items: occupied trailing slots are kept, and `/invbonus` warns about them.
- See `README.md` for the user-facing docs.

## Code map
| File | Role |
|---|---|
| `InventoryBonus.uplugin` | Plugin descriptor. Depends on SML `^3.12.0`, GameVersion `>=491125` |
| `Config/AccessTransformers.ini` | `Friend` access so the mod can read the private `AFGCharacterPlayer::mDefaultPlayerInventorySlots` |
| `Private/InventoryBonus.cpp` | All hooks plus the slot math (see below) |
| `*Config.*` | `UInventoryBonusConfig`, the SML mod config with one integer, `Bonus` |
| `*Command.*` | `/invbonus` (alias `/inventorybonus`). With no args it shows the breakdown; only the host can set a value |
| `*Module.*` | Module startup calls `FInventoryBonus::RegisterHooks()` |

Hooks in `InventoryBonus.cpp`. All of them except the config registration are inside `#if !WITH_EDITOR`, so **the editor build never compiles them**. Only the Shipping/Alpakit build checks them.
- `UFGInventoryComponent::Resize` (before): for a player's main inventory, replaces NewSize with the computed total.
- `AFGCharacterPlayer::BeginPlay` (virtual, after):
  - Registers the chat command.
  - Starts a 1 s poll for config changes.
  - Applies the size now, and again at 2 s and 10 s.
- `AFGSchematicManager::GiveAccessToSchematic(s)` (after): re-applies the size to all players.
- `UConfigManager::MarkConfigurationDirty` (after): re-applies the size to all players.
- `FWorldDelegates::OnStartGameInstance`: registers the config, so it shows in the main menu.

## Rebuilding the toolchain on a new PC
Follow https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html. Exact versions used:

1. **Visual Studio 2022** with these workloads:
   - Game dev with C++
   - Desktop dev with C++
   - .NET desktop

   Also add the individual component **MSVC v143 14.38 (x64/x86)**. Not the ARM64 one.
2. **GitHub â†” Epic link** is already done for the account `Aetherosz`, and it is per account. On the new PC you only need to sign in to GitHub. **Unreal Linker** (https://linker.ficsit.app/link) is already done too.
3. **Unreal Engine 5.6.1-CSS:**
   - Download the 4 files from https://github.com/satisfactorymodding/UnrealEngine/releases/tag/5.6.1-css-83: `UnrealEngine-CSS-Editor-Win64.exe` and `-1.bin`, `-2.bin`, `-3.bin`.
   - Run the .exe. It installs to `C:\Program Files\Unreal Engine - CSS`.
4. **Wwise 2025.1.6.9117**, exactly this version and not 2026.x. Install it through the Audiokinetic Launcher:
   - Components: Authoring, SDK (C++).
   - Platforms: Windows (VS2022) and Linux.
   - **No plugins.** Untick even the two the launcher selects by default.
5. **Starter project:**
   ```
   git clone https://github.com/satisfactorymodding/SatisfactoryModLoader.git
   ```
   Use the `master` branch, which is SML 3.12.0 with EngineAssociation `5.6.1-CSS`.
6. **Short-path junction (required).** If the clone's path is long, UBT fails with "action paths are longer than 260 characters". Create a junction and always build through it:
   ```powershell
   New-Item -ItemType Junction -Path C:\SML -Target "<where you cloned SatisfactoryModLoader>"
   ```
7. **Integrate Wwise** in the Audiokinetic Launcher (you must be signed in):
   - Open the Unreal Engine tab and choose Open other â†’ `C:\SML\FactoryGame.uproject`.
   - Choose Integrate Wwise in Project.
   - Set Integration Version to All â†’ `2025.1.6.9117`.
   - Under Wwise Project, choose â–¸ New project.
   - Click Integrate.
8. **Generate soundbanks:**
   - Open `SatisfactoryModLoader_WwiseProject.wproj` in Wwise.
   - In SoundBanks, right-click the top folder and choose Generate for all platforms.
9. **Put this repo at** `C:\SML\Mods\InventoryBonus`. Either clone it there, or clone it elsewhere and create a junction into `Mods`.

## Build commands (PowerShell)
Generate the project files:
```powershell
& "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="C:\SML\FactoryGame.uproject" -game -rocket -progress
```
Build the editor. The first build takes about 10 minutes. It is needed before you open the editor or use Alpakit:
```powershell
& "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\Build.bat" FactoryEditor Win64 Development -project="C:\SML\FactoryGame.uproject" -WaitMutex -FromMsBuild
```
Package the mod. This is the same UAT call Alpakit Dev makes. It compiles the Shipping DLLs, including the hooks. The first run takes about 25 minutes:
```powershell
& "C:\Program Files\Unreal Engine - CSS\Engine\Build\BatchFiles\RunUAT.bat" -ScriptsForProject="C:\SML\FactoryGame.uproject" PackagePlugin -project="C:\SML\FactoryGame.uproject" -clientconfig=Shipping -serverconfig=Shipping -utf8output -DLCName=InventoryBonus -build -platform=Win64 -nocompileeditor -installed
```
The output is `C:\SML\Saved\ArchivedPlugins\InventoryBonus\InventoryBonus-Windows.zip`. It contains DLLs for both Steam and Epic.

**Don't** run `Build.bat FactoryGame Win64 Shipping` directly. It tries to compile engine modules and fails with `CoreNetTypes.generated.h` not found. Use the RunUAT command above.

## Installing and testing
1. Close Satisfactory Mod Manager completely, including the tray icon. It can remove mods it doesn't manage.
2. Unzip the package into `<Satisfactory>\FactoryGame\Mods\InventoryBonus\`.
3. Launch the game from Steam.
4. In a save, press Enter and type `/invbonus`.
5. Log: `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`. Search it for `InventoryBonus`.

## Possible next steps
- Add the icon (`Resources/Icon128.png`).
- Publish to ficsit.app:
  1. Run Alpakit Release, which builds the multi-target zip including the Linux dedicated server.
  2. Upload the zip.
  3. Clients and server both need the mod.
- **Next mod idea (Aetherosz's):** auto-supports.
  - Any buildable automatically extends supports down to the terrain, including through water.
  - Also buildable water blocks.
  - Neither exists on ficsit.app as of 2026-11. The closest is Structural Solutions.
