# Inventory Bonus

Adds **+X inventory slots** on top of everything else. There are no milestones or unlocks, and you can change the number in-game whenever you like.

```
total slots = default + vanilla upgrades + other mods' upgrades + bonus
```

| Part | Where it comes from |
|---|---|
| Default | The game's starting slot count (`mDefaultPlayerInventorySlots`) |
| Vanilla upgrades | Every purchased vanilla schematic that unlocks slots: HUB milestones, MAM research, hard drive alternates |
| Mod upgrades | Every purchased schematic from a mod that unlocks slots, e.g. your `InventorySpace2` (+50) and `InventorySpace3` (+150) |
| Bonus | This mod. It starts at **+1000** and you can change it in the mod settings menu or with `/invbonus` |

Every part except the bonus is recalculated from the game's own data on spawn, on load, and whenever any schematic is unlocked. Buying upgrades or adding and removing other inventory mods keeps working.

## Settings menu
Open **Mods → Mod Configuration → Inventory Bonus** from the main menu or the pause menu, and set **Bonus inventory slots**.
- Changes made from the pause menu take effect within a second. You don't need to reload.
- **Reset to default** returns the bonus to +1000.
- This uses SML's built-in mod config system, so no other settings mod is required. SML saves the value to `<Satisfactory>\FactoryGame\Configs\InventoryBonus.cfg`.

## In-game commands (chat)
The chat command changes the same value as the settings menu.

| Command | Effect |
|---|---|
| `/invbonus` | Show your breakdown, e.g. `16 default + 32 vanilla upgrades + 200 mod upgrades + 1000 bonus = 1248 slots` |
| `/invbonus 500` | Set the bonus to +500 |
| `/invbonus +200` | Add 200 |
| `/invbonus -200` | Remove 200 |
| `/invbonus 0` | No bonus (back to the normal game) |

- **Removing slots never deletes items.** If a slot being removed still holds items, it's kept and the game tells you. Empty your last slots and run `/invbonus` again to finish shrinking.
- The bonus applies to every save. The range is 0–20000.
- In multiplayer, the bonus applies to every player and only the host can change it. Everyone needs the mod installed.

## Files
```
InventoryBonus/
  InventoryBonus.uplugin
  Config/AccessTransformers.ini         grants read access to the private default-slot field
  Source/InventoryBonus/
    InventoryBonus.Build.cs             from the official Alpakit C++ template
    Public/InventoryBonus.h             slot math + bonus storage
    Private/InventoryBonus.cpp          hooks: Resize, BeginPlay, GiveAccessToSchematic(s)
    Public|Private/InventoryBonusConfig.*    Mods -> Mod Configuration entry (SML config)
    Public|Private/InventoryBonusCommand.*   /invbonus chat command
    Public|Private/InventoryBonusModule.*    module entry point
```

## Building (one-time toolchain setup)
C++ mods must be compiled with Coffee Stain's custom Unreal Engine. Follow the official guide:
https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/dependencies.html

1. **Accounts:** GitHub, an Epic Games account linked to GitHub, and the ficsit.app Unreal Linker for access to the modding engine. You also need an Audiokinetic account for Wwise.
2. **Visual Studio 2022** (17.14), with the C++ game dev, C++ desktop, and .NET desktop workloads, plus MSVC v143 14.38.
3. **Unreal Engine 5.6.1-CSS** from the modding engine release.
4. **Wwise 2025.1.6.9117** (Authoring + SDK).
5. **Starter project**:
   ```bash
   git clone https://github.com/satisfactorymodding/SatisfactoryModLoader.git SatisfactoryModLoader
   ```

## Compile and install
1. Copy this `InventoryBonus` folder into `SatisfactoryModLoader/Mods/`.
2. Right-click `FactoryGame.uproject`, choose **Generate Visual Studio project files**, then build **Development Editor / Win64**.
3. Open the editor. In **Alpakit**, set the game path to your Satisfactory install folder, tick **InventoryBonus**, and click **Alpakit Dev**.
4. Launch the game and check **Mods → Mod Configuration → Inventory Bonus**, or type `/invbonus` in chat.

## Limitations
- Mods that add slots through schematics are counted, which is the normal way and how `InventorySpace2`/`3` work. A mod that resizes the inventory directly in code is overridden by this mod's total. If that happens, raise the bonus to compensate.
- If you uninstall the mod, the inventory may shrink back to the vanilla size. It's untested whether the game keeps the larger size in its save data. Back up your save and empty your extra slots first.
