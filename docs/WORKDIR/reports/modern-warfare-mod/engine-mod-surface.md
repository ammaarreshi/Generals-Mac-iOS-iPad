# Engine Mod Surface (GeneralsXZH) — what a data-only mod can express

Source-verified map of mod loading, INI modules, enums and hard limits. Compiled 2026-10-22 for the modern-warfare mod proposal. Paths relative to repo root unless absolute.

## 1. Mod loading

- `FileSystem::openFile` tries the local filesystem first, then archives (`Core/GameEngine/Source/Common/System/FileSystem.cpp:175-220`). Loose files under the cwd beat any `.big`.
- `-mod <path>` (`GeneralsMD/Code/GameEngine/Source/Common/CommandLine.cpp:1060-1105`, used at 1179):
  - Relative paths are prefixed with the user-data dir (`~/Library/Application Support/GeneralsX/GeneralsZH/`). Absolute paths are used as-is.
  - A directory mounts only the `*.big` files inside it, not loose INIs (`ArchiveFileSystem.cpp:231-236`). A file mounts that `.big` (`ArchiveFileSystem.cpp:213-229`).
  - Mods mount with `overwrite=TRUE` and are prepended, so a mod `.big` beats every base `.big`.
- Base archives mount with `overwrite=FALSE`, so the first-loaded copy wins, in case-insensitive sorted order (`StdBIGFileSystem.cpp:278`, `FileSystem.h:66`).
  - `Data/INI/INIZH.big` is deliberately skipped (`StdBIGFileSystem.cpp:660-663`). The root `INIZH.big` plus `PatchINI.big` are authoritative.
- **Timing trap:** `GameData.ini` loads (`GameEngine.cpp:468`) before `-mod` archives mount (528-530). GameData changes in a `-mod` `.big` are ignored. Ship them loose or via `SagePatch.ini`.
- `INI::loadFileDirectory` (`Core/.../INI/INI.cpp:193-243`) loads `X.ini` and then every `*.ini` under directory `X`. Root files load first, then subdirectories, sorted. New files in `Data/INI/Object/` are picked up automatically.
- GeneralsX quirks:
  - Loose INIs resolve relative to the cwd, so launch via `run.sh`, which does `cd` into the install dir.
  - Loose directory recursion and `.big`-in-subfolder discovery work only one level deep (`StdLocalFileSystem` around 340-350, `StdBIGFileSystem.cpp:651`).
  - `SagePatch.ini` (user-data dir) loads after GameData with a null xfer, so it is not in the INI CRC.
- **Redefining objects:** a second `Object X` block for an existing name silently re-parses in release and asserts "Duplicate" in debug (`ThingFactory.cpp:396-404`). Replace the whole original file, or use `ObjectReskin`.
  - `AddModule`, `RemoveModule` and `ReplaceModule` work only in override loads (map `map.ini` / `solo.ini`) (`GameLogic.cpp:2563-2578`).
- **Recommended shipping path:** develop with loose files in `<install>/Data/INI/...`. Ship one `.big` via `-mod /abs/path/Mod.big`, plus loose or SagePatch GameData edits.

## 2. Module catalog (`GeneralsMD/.../Common/Thing/ModuleFactory.cpp:318-512`)

The Surrender/POW/Prison/PropagandaCenter/Demoralize modules are **not compiled** (`ALLOW_SURRENDER` / `ALLOW_DEMORALIZE` are undefined).

- **Behavior:** AutoHeal, GrantStealth, NeutronBlast, Countermeasures, DumbProjectile, Physics, InstantDeath, SlowDeath, HelicopterSlowDeath, Overcharge, PropagandaTower, BunkerBuster, FireWeaponWhenDamaged, FireWeaponWhenDead, GenerateMinefield, ParkingPlace, FlightDeck, Poisoned, RebuildHole, TechBuilding, Minefield, BattleBusSlowDeath, JetSlowDeath, SpawnBehavior, Bridge*.
- **Contain:** Open, Cave, Heal, Garrison, InternetHack, Transport, RiderChange, RailedTransport, MobNexus, Tunnel, Overlord, Helix, Parachute.
- **Die:** Destroy, FXList, Crush, CreateCrate, CreateObject, EjectPilot, SpecialPowerCompletion, RebuildHoleExpose, Upgrade, KeepObject.
- **Body:** Inactive, Active, Highlander, Immortal, Structure, HiveStructure, Undead.
- **AI:** AIUpdateInterface, JetAIUpdate, MissileAIUpdate, NeutronMissileUpdate, ChinookAIUpdate, DeliverPayloadAIUpdate, TransportAIUpdate, AssaultTransportAIUpdate, DeployStyleAIUpdate, WanderAIUpdate, Worker/Dozer/SupplyTruck AI, HackInternetAIUpdate.
- **Update:**
  - Weapons and targeting: PointDefenseLaser, Laser, ProjectileStream, SmartBombTargetHoming, AssistedTargeting, WeaponBonus, FireWeapon, FireOCLAfterWeaponCooldown, StickyBomb.
  - Unit and base support: Horde, EnemyNear, BattlePlan, AutoFindHealing, BaseRegenerate, RepairDock, CleanupHazard.
  - Spawning and lifetime: Lifetime, Deletion, HeightDie, OCL.
  - Vision and abilities: DynamicShroudClearingRange, SpyVision, SpecialAbility, MissileLauncherBuilding, CommandButtonHunt, Radar, EMPUpdate, StealthUpdate, StealthDetectorUpdate, SlavedUpdate.
- **Special power modules:** OCLSpecialPower, FireWeaponPower, SpecialAbility, SpyVisionSpecialPower, CashHack, Defector, CashBounty, CleanupArea.
- **Upgrades:** CostModifier, ActiveShroud, Armor, CommandSet, GrantScience, PassengersFire, StatusBits, SubObjects, Stealth, Radar, PowerPlant, LocomotorSet, ObjectCreation, ReplaceObject, ModelCondition, UnpauseSpecialPower, WeaponBonus, WeaponSet, ExperienceScalar, MaxHealth.
- **Create:** LockWeapon, Preorder, SupplyCenter, SupplyWarehouse, SpecialPower, GrantUpgrade, VeterancyGain.

### Key mechanics for modern-warfare features

| Mechanic | Fields and notes |
|---|---|
| SpawnBehavior (drones, slaves) | `SpawnNumber`, `SpawnTemplateName`, `SpawnReplaceDelay`, `OneShot`, `InitialBurst`, `ExitByBudding`, `AggregateHealth`, `SlavesHaveFreeWill`, `SpawnedRequireSpawner`, `CanReclaimOrphans`. With `KINDOF_SPAWNS_ARE_THE_WEAPONS`, the spawns act as the weapon. |
| SlavedUpdate | `ScoutRange`, `AttackRange`, `GuardMaxRange`, repair fields, `DistToTargetToGrantRangeBonus` (the DRONE_SPOTTING bonus). |
| MissileAIUpdate | `FuelLifetime`, `IgnitionDelay`, `InitialVelocity`, `DistanceToTargetBeforeDiving`, `DistanceToTargetForLock`, **`DistanceScatterWhenJammed`**, `DetonateOnNoFuel`, `TryToFollowTarget`. |
| JetAIUpdate | `NeedsRunway`, lock-on fields, `OutOfAmmoDamagePerSecond`. |
| DeliverPayloadAIUpdate | Strafe and drop fields. Used for airstrike and loitering-munition deliveries via OCL. |
| StealthUpdate | `StealthDelay`, `StealthForbiddenConditions`, `InnateStealth`, `RevealDistanceFromTarget`, `DisguisesAsTeam`, `GrantedBySpecialPower`. |
| StealthDetectorUpdate | `DetectionRate`, `DetectionRange`, `ExtraRequiredKindOf`. |
| GrantStealthBehavior | Growing stealth radius; this is the GPS Scrambler mechanism. |
| PointDefenseLaserUpdate | Generic point defense with any `WeaponTemplate`: `PrimaryTargetTypes`, `SecondaryTargetTypes`, `ScanRate`, `ScanRange`. Usable for C-UAS and APS. |
| CountermeasuresBehavior | Flares: `VolleySize`, `EvasionRate`, `ReloadTime`, `MustReloadAtAirfield`. |
| EMPUpdate | `EffectRadius`, `DisabledDuration`, `VictimRequiredKindOf`, `VictimForbiddenKindOf`. Sets `DISABLED_EMP`. |
| Subdual damage | `DAMAGE_SUBDUAL_*` fills subdual HP. At max the object becomes `DISABLED_SUBDUED`. Needs `SubdualDamageCap > 0` on the body. Fits jamming and HPM disable. |
| `DAMAGE_STATUS` | Weapon `Damage` is the duration in ms. `DamageStatusType` sets an ObjectStatus. |
| WeaponBonusUpdate | Area aura granting a WeaponBonusCondition. |
| OCL nuggets | CreateObject, CreateDebris, ApplyRandomForce, DeliverPayload, FireWeapon, Attack. |
| Locomotor sets | NORMAL, NORMAL_UPGRADED, FREEFALL, WANDER, PANIC, TAXIING, SUPERSONIC, SLUGGISH. |

**Special powers are a fixed enum of 68 names** (`SpecialPower.cpp:52-134`). Examples: SPECIAL_EMP_PULSE, SPECIAL_SPY_DRONE, SPECIAL_GPS_SCRAMBLER, SPECIAL_SPECTRE_GUNSHIP, SPECIAL_PARTICLE_UPLINK_CANNON, plus the AIRF_/SUPW_/NUKE_/INFA_/SLTH_/LAZR_/EARLY_ variants. New powers must reuse an existing enum slot. Generic modules like OCLSpecialPower let one slot drive arbitrary payloads.

## 3. Enums (fixed; cannot add values)

- **DamageType** (ZH): EXPLOSION CRUSH ARMOR_PIERCING SMALL_ARMS GATTLING RADIATION FLAME LASER SNIPER POISON HEALING UNRESISTABLE WATER DEPLOY SURRENDER HACK KILL_PILOT PENALTY FALLING MELEE DISARM HAZARD_CLEANUP PARTICLE_BEAM TOPPLING INFANTRY_MISSILE AURORA_BOMB LAND_MINE JET_MISSILES STEALTHJET_MISSILES MOLOTOV_COCKTAIL COMANCHE_VULCAN SUBDUAL_MISSILE SUBDUAL_VEHICLE SUBDUAL_BUILDING SUBDUAL_UNRESISTABLE MICROWAVE KILL_GARRISONED STATUS
- **DeathType:** NORMAL NONE CRUSHED BURNED EXPLODED POISONED TOPPLED FLOODED SUICIDED LASERED DETONATED SPLATTED POISONED_BETA EXTRA_2…EXTRA_8 POISONED_GAMMA. EXTRA_* are free for modders.
- **WeaponBonusCondition:** GARRISONED HORDE CONTINUOUS_FIRE_MEAN CONTINUOUS_FIRE_FAST NATIONALISM PLAYER_UPGRADE DRONE_SPOTTING DEMORALIZED_OBSOLETE ENTHUSIASTIC VETERAN ELITE HERO BATTLEPLAN_* SUBLIMINAL SOLO_* TARGET_FAERIE_FIRE FANATICISM FRENZY_ONE/TWO/THREE. Bonus fields: DAMAGE RADIUS RANGE RATE_OF_FIRE PRE_ATTACK.
- **KindOf (116):** notable modding tags are DRONE, BALLISTIC_MISSILE, SMALL_MISSILE, EMP_HARDENED, SPAWNS_ARE_THE_WEAPONS, CAN_SEE_THROUGH_STRUCTURE, REVEALS_ENEMY_PATHS, PRODUCED_AT_HELIPAD, AIRCRAFT_CARRIER, BOAT, CANNOT_RETALIATE, TECH_BASE_DEFENSE, HERO, DISGUISER, BOOBY_TRAP, MINE.
- **Disabled:** HACKED EMP HELD PARALYZED UNMANNED UNDERPOWERED FREEFALL AWESTRUCK BRAINWASHED SUBDUED SCRIPT_DISABLED SCRIPT_UNDERPOWERED.
- **WeaponSet conditions:** VETERAN ELITE HERO PLAYER_UPGRADE CRATEUPGRADE_ONE/TWO VEHICLE_HIJACK CARBOMB MINE_CLEARING_DETAIL WEAPON_RIDER1-8.
- **ArmorSet conditions:** VETERAN ELITE HERO PLAYER_UPGRADE WEAK_VERSUS_BASEDEFENSES SECOND_LIFE CRATE_UPGRADE_ONE/TWO.

## 4. Hard limits

- 3 weapon slots and 2 turrets per object.
- Command sets have 18 internal slots and 14 visible. The index check is debug-only, so an overflow in release corrupts memory.
- Science purchase buttons: 4 / 15 / 4 by rank. Upgrade cameos: 5 per unit and 5 on the HUD.
- Upgrades max 512. AI skillset sciences max 20. Players max 16 (8 lobby slots).
- Garrison max 40 points. OpenContain max 32 fire points.
- All the enums above are fixed-size bitsets.

## 5. Compatibility

- **INI CRC** covers GameData, Object, Weapon, Armor, Locomotor, SpecialPower, Upgrade, Science, Rank, PlayerTemplate, FXList, OCL, Crate, AIData, Multiplayer, Terrain, Roads, DamageFX, Water and Weather. It does **not** cover CommandButton, CommandSet, ControlBarScheme, InGameUI, Eva, ParticleSystem or SagePatch.
- **Multiplayer:** online lobbies refuse an INI CRC mismatch. The ZH LAN check is compiled out, but mismatched data will still desync.
- **Replays** store the INI CRC. Mod replays play back only under the identical mod.
- **Saves** reference templates by name, so a mod likely breaks old saves.
- **Golden rule 6** (retail compatibility) is preserved only if the mod is opt-in via `-mod` and never replaces base archives.

## 6. Data locations on the dev machine

- Install: `~/GeneralsX/GeneralsZH`, the default deploy target (binary `GeneralsXZH`, launched via `run.sh`).
- Retail INIs extracted read-only for reference into the session scratchpad. Re-extract with a BIG reader; no BIG tool exists in the repo yet.
