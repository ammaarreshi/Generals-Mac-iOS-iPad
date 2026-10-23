# Modern Warfare Mod: Design Proposal (working title "Zero Hour: Kill Chain")

**Status: proposal only, revision 4 (2026-10-23).** No game file, INI, `.big`, asset, engine code or install directory was touched.
- **Revision 4** applies the user's decisions (§9):
  - Modern conventional superweapons (§2.h).
  - A true `TOP_ATTACK` damage type as an in-scope GeneralsX engine change (§6.1). The mod is therefore **no longer data-only**.
  - A skirmish/AI-first release plan.
- Revision 2 answered `PROPOSAL-REVIEW.md`.
- Revision 3 adds an in-scope, script-made Art & Audio track (§7) and uses it to restore design that placeholders had compromised.
- The Revision log at the end lists every change.

## Sources

| Tag | Source |
|---|---|
| `[USA §n]` | `docs/WORKDIR/reports/modern-warfare-mod/research-usa.md` |
| `[CHN §n]` | `docs/WORKDIR/reports/modern-warfare-mod/research-china.md` |
| `[IRR §n]` | `docs/WORKDIR/reports/modern-warfare-mod/research-irregular-and-trends.md`; Part B trend numbers are written `§B.n` |
| `[ENG §n]` | `docs/WORKDIR/reports/modern-warfare-mod/engine-mod-surface.md` |
| `[FC #n]` | `docs/WORKDIR/reports/modern-warfare-mod/fact-check.md` |
| `[REV #n]` | `docs/WORKDIR/planning/modern-warfare-mod/PROPOSAL-REVIEW.md` |

- **Verification.** Template names were checked by grep against the retail ZH INIs extracted to the session scratchpad. Engine behavior was checked in `GeneralsMD/Code/GameEngine/Source`. New templates use the `MW_` prefix.
- **Fact-check alignment.** The design depends on no claim the fact-check marked Contradicted or Unconfirmed.
- **Tuning values are design choices.** They are not real-world specs; for example, fiber-optic range is a balance choice `[FC #21]` and HPM effectiveness is an assumption `[USA §2]`.
- **Asset grounding.** Asset requirements (§7) were checked against the retail Draw, MappedImages and Audio INIs, the W3D format header `Core/Tools/WW3D/pluglib/w3d_file.h`, and the W3D, OpenAL and texture loaders in this repo.
- **"Prototype first".** Items with this tag rest on engine paths that retail does not exercise in exactly this way. Each must pass a one-unit test in Phase 1 before the design commits to it.

---

## 1. Executive summary

**Zero Hour: Kill Chain** is an opt-in, skirmish/AI-first `-mod`: INI data, script-made art and audio, and one small GeneralsX engine change (a `TOP_ATTACK` damage type). It modernizes all three factions with real designations.

**Design pillars**
1. **A drone layer.** USA: few and hardened. China: mass. GLA: improvised.
2. **Cost exchange is gameplay.** Cheap counters beat mass; saturation beats expensive defenses.
3. **The spectrum is terrain.** China jams, GLA spoofs and goes fiber-optic, and the USA hardens and detects.
4. **Nowhere to hide.** Cheap detection; top-attack answered by APS, cages or scrap.
5. **Deep strike is finite.** Caps, magazines and long reloads. Superweapons become conventional, interceptable missile strikes.
6. **Identity first.** Every faction has things the others cannot do (§3.0).
7. **Readable at a glance.** Blender-scripted models, FX, sounds and cameos, gated by a Phase 0 W3D-export spike (§7).

**Headlines**
- **USA:**
  - Units: Abrams, M1E3 with APS, HIMARS, Switchblade teams, CCA.
  - The richest counter-drone (C-UAS) layer: LIDS, Stryker M-SHORAD and Leonidas.
  - SIGINT and anti-jam navigation instead of jammers.
  - Dark Eagle hypersonic battery replaces the particle cannon.
- **China:**
  - Type 99A and a modeled Type 100 with add-ons.
  - The only mobile area jamming (Type 5).
  - DF-26 conventional strike complex replaces the nuke.
  - FPV mass, drone carrier, PHL-16, mothership, visible cages.
- **GLA:**
  - FPV pilots, recon drones, a workshop van, exclusive fiber-optic.
  - Inflatable decoys, fake structures and tunnels.
  - Gun C-UAS; toxins kept.
  - Scud Storm becomes a camouflaged, dispersed launch complex.

---

## 2. Cross-cutting systems

**Rule zero: rework in place.** Where possible, change the *behavior* of an existing template but keep its template, upgrade and button names. Players see new names through `generals.csf`. This preserves skirmish-AI scripts, hotkeys, sciences and general variants. Brand-new templates are additive and need AI work (§5, §6).

### 2.0 Shared data primitives

| Primitive | Implementation | Verified basis |
|---|---|---|
| **TOP_ATTACK damage type** | A **new engine enum value `DAMAGE_TOP_ATTACK`**, INI name `TOP_ATTACK` (engine change, §6.1). The mod's `Armor.ini` gives **every** armor an explicit `TOP_ATTACK` line (see the profile below the table). | `Damage.h:47-94` (enum), `Damage.cpp` name table; analysis in §6.1 |
| **Drone projectiles** | FPVs and loitering munitions are projectiles (`KindOf = PROJECTILE SMALL_MISSILE`) driven by `MissileAIUpdate`. `TryToFollowTarget = Yes`; `DistanceToTargetBeforeDiving` gives the dive; `FuelLifetime` sets range. The body is `ActiveBody` with `MaxHealth 60` and `SubdualDamageCap 120`. `RadiusDamageAffects = ENEMIES NEUTRALS`. Model: new `MWFPVQuad` / `MWLoiter` (§7 A1, A3); the placeholder until Phase 0 passes is `AVScoutDr`/`AVHellDrone` at `Scale 0.5–0.8`. | `TunnelDefenderMissile` (`PROJECTILE SMALL_MISSILE`, `SubdualDamageCap 200`); `ThingTemplate.cpp:219` (`Scale`) |
| **Drone units** | Recon quadcopters are `KindOf = VEHICLE DRONE`. They use a hover locomotor with `AirborneTargetingHeight` 20 and `PreferredHeight` 50, the retail drone values, so AA can engage them. Armor is `MW_SmallDroneArmor`. | `BattleDroneLocomotor`; `AIUpdate.cpp:2296` |
| **Drone armors** | Three new armors, specified after this table. | `ProjectileArmor` (`DEFAULT 25%`, `LASER 100%`, `SUBDUAL_MISSILE 100%`) [REV #11] |
| **Soft-kill (jamming)** | Uses **`SUBDUAL_MISSILE` only**. Its effect depends on the target (see after this table). | `Armor.ini` audit; `ActiveBody.cpp:1285-1323`, `:1327`; `SubdualDamageHelper.cpp` (heal countdown reset on every hit) [REV #1] |
| **Hard-kill HPM** | Uses `HAZARD_CLEANUP`. Only `HazardousMaterialArmor` (poison and radiation fields) takes it in retail. Side effects: an HPM pulse also cleans toxin fields, and ambulance/CBRN cleanup bursts can clip drones. Both are acceptable. | `Armor.ini` audit; no code special case (`grep DAMAGE_HAZARD_CLEANUP` = 0) |
| **Point defense (PDL)** | `PointDefenseLaserUpdate` takes any `WeaponTemplate` and a KindOf target mask. It does not use a weapon slot. It ignores `ClipSize`/`ClipReloadTime` because it uses a fresh weapon per shot, so only `DelayBetweenShots` matters. `ScanRange` must exceed the weapon range or debug builds assert. It cannot be upgrade-gated, and it stops while its owner is disabled. | `PointDefenseLaserUpdate.cpp:115, 205-221`; `GameLogic.cpp:3852-3858` [REV #15a] |
| **Mobile spawners** | `SpawnBehavior` needs an exit interface. Retail precedent for a **mobile** spawner: `GLAInfantryAngryMobNexus` uses `SpawnBehavior`, `QueueProductionExitUpdate` and `ExitByBudding = Yes`. Any object with an `OpenContain`-family module also has an exit interface, but spawning through a transport contain is untested. | `SpawnBehavior.cpp:596-606`; `Object.cpp:2635-2650`; Mob Nexus in `GLAInfantry.ini` [REV #2] |
| **Build caps** | `MaxSimultaneousOfType` (+ `MaxSimultaneousLinkKey`). Airfields also cap jets at 4 parking spots (`NumRows 2 × NumCols 2`). | `ThingTemplate.cpp:243-244`; `AmericaAirfield` [REV #15b] |
| **Per-weapon bonuses** | The `WeaponBonus` field on a `Weapon` block (119 retail uses) means doctrine upgrades need no `GameData.ini` edits. | `Weapon.cpp:225` |

**TOP_ATTACK armor profile (rev 4).** Every armor needs an explicit `TOP_ATTACK` line, because about 20 retail armors carry a `DEFAULT x%` line that would otherwise also set `TOP_ATTACK` (`Armor.cpp:88-95`).

| Armor group | TOP_ATTACK |
|---|---|
| Infantry, vehicle and aircraft armors | 100 % |
| `DozerArmor` and `HazMatHumanArmor` | 100 %, so they are **vulnerable** (unlike the rev-2/3 `LAND_MINE` reuse) |
| All structure and base-defense armors | 10 % (FPVs are a nuisance to buildings, not siege weapons) |
| Projectile, mine, tree and invulnerable armors | 0 % |
| `MW_CagedTankArmor` | 60 % |
| `MW_ScrapCagedArmor` | 70 % |

- Mines keep `LAND_MINE` unchanged, so **cages no longer affect mines**.
- Anti-structure drone strikes (one-way attack salvoes) use `EXPLOSION`.
- The Aurora/B-21 keeps `AURORA_BOMB` unchanged.
- Rejected alternatives: `AURORA_BOMB` (rev 1; 15 armors list it at 80–300 % vs structures [REV #3]) and `LAND_MINE` (rev 2–3; dozer and CBRN immunity, and cages also cut mine damage).

**Drone armors (new).**

| Armor | Specification |
|---|---|
| `MW_SmallDroneArmor` | GATTLING, SMALL_ARMS and COMANCHE_VULCAN 150 %; LASER, HAZARD_CLEANUP and SUBDUAL_MISSILE 100 %; SUBDUAL_VEHICLE 0 % |
| `MW_FPVProjectileArmor` | Copy of `ProjectileArmor` with explicit GATTLING, SMALL_ARMS and COMANCHE_VULCAN 100 % (instead of 25 %), so gun PDLs work; HAZARD_CLEANUP 100 %; SUBDUAL_MISSILE 100 % |
| `MW_FiberFPVProjectileArmor` | Same as `MW_FPVProjectileArmor`, but SUBDUAL_MISSILE 0 % |
| `MW_CagedTankArmor` / `MW_ScrapCagedArmor` | Copies of `TankArmor`/`TruckArmor` with TOP_ATTACK 60 % / 70 % (all other types unchanged) |

**How `SUBDUAL_MISSILE` jamming works.**
- In retail, only `ProjectileArmor` and `BallisticMissileArmor` take it at a non-zero rate. Every human, vehicle and structure armor takes it at 0 %, and only `ECMTankMissileJammer` deals it.
- A projectile whose subdual damage reaches its `MaxHealth` is *jammed* and scatters.
- A non-projectile drone in `MW_SmallDroneArmor` is `DISABLED_SUBDUED` (link lost).
- Tanks and infantry are unaffected by construction, whatever the pulse rate.
- Inside a bubble, healing is starved because every hit resets the heal countdown. So a drone stays jammed until it leaves the bubble.
- There is **no** area `SUBDUAL_VEHICLE` pulse anywhere in the mod.

### 2.a Drone layer: an asymmetric toolkit

- **Real-world basis.** FPVs and the "kill zone" `[IRR §B.1]`. Commercial drones used by irregulars `[IRR §2]`. US drone-dominance goals, with Switchblade and LUCAS fielded `[USA §2, FC #11, FC #24]`. PLA drones at every echelon `[CHN §3]`.
- **Common rules.**
  - **Range rule:** strike drones fire to 200, which is also the team's own vision radius. Anything longer (loitering munitions 260–450) needs a spotter's vision.
  - **Damage rule:** an FPV hit is 120 TOP_ATTACK. An uncaged MBT (480–520 HP) takes 4–5 hits; a caged one takes 6–7.

| Faction | Drone character | Units and powers | Strength | Weakness |
|---|---|---|---|---|
| USA | Few, expensive, hardened, long-range | Switchblade Team (1 loitering munition, range 260 with spotter, 20 s reload); vehicle drone escorts (retail Scout/Battle/Hellfire upgrades reworked to Recon / PD-escort / Launched-Effect); CCA from the Airfield; LUCAS salvo power | Precision, jam resistance (Alt-PNT), escort APS | Low volume; expensive losses |
| China | Mass and carrier-launched, all radio-linked | FPV Team (T1, ClipSize 2, horde bonus); Type 100 Support Vehicle (drone carrier); Type 100 loitering pod; Drone-Mothership power; GJ-11 | Numbers, horde bonuses, fire-support integration | Every China drone is RF, so GLA's spoof field and USA HPM/C-UAS punish it |
| GLA | Commercial and improvised; **fiber-optic is GLA-only** | FPV Pilot (RF; Fiber-Optic Spools upgrade); Commercial Drones (recon on Technicals and Kell); Drone Workshop Van; One-Way Drone Swarm power | Cheapest; jam-immune with fiber | Short range (fiber 170); fragile operators; no air cover |

**Implementation.**
- Projectiles and drones live in a new `Object/MW_Drones.ini`. Weapons go into the replaced `Weapon.ini`.
- Operator teams carry the strike drone in PRIMARY and a sidearm in SECONDARY.
- Fiber-optic is a `WeaponSetUpgrade` (PLAYER_UPGRADE weaponset) to a weapon whose projectile uses `MW_FiberFPVProjectileArmor`.
- **Carriers** come in two tiers:
  - **Certain:** the carrier fires drones as a *weapon* (projectile launcher with clip and reload). Retail missile pattern.
  - **Prototype first:** a visible drone swarm using `SpawnBehavior` + `QueueProductionExitUpdate` + `ExitByBudding` on a vehicle, copying the Mob Nexus. Spawned kamikaze units use the `TerroristSuicideWeapon` pattern (`LeechRangeWeapon`, `DamageDealtAtSelfPosition`, `RadiusDamageAffects = SELF SUICIDE`) with a `TOP_ATTACK` payload delivered by `FireWeaponWhenDeadBehavior`.
  - If the spawner prototype fails, ship the certain tier.

**Counters and knobs.**
- Counters: see §2.c by faction. Operators are infantry, so artillery and snipers kill them.
- Knobs: FPV damage, team `ClipReloadTime`, projectile `MaxHealth`, `FuelLifetime`, carrier reload or `SpawnReplaceDelay`, and team cost.

### 2.b Electronic warfare: China jams, GLA spoofs, USA hardens and detects

- **Real-world basis.** EW and spoofing are everywhere, and drones answer with better links `[IRR §B.3]`. Fiber-optic drones are immune to RF jamming `[IRR §B.2]`; the range penalty here is a balance choice `[FC #21]`. Irregular drones with simple links are easy to jam `[IRR §4]`. The US relies on GPS and data links, and alt-PNT fielding is under way `[USA §2–3]`. The PLA fields EW vehicles and J-16D jammers `[CHN §2]`; Type 4/5 details are mock-up-level, so this is design-level only.

| Faction | EW tool | Mechanism |
|---|---|---|
| China (only mobile jammer) | Type 5 EW Vehicle = retail `ChinaTankECM` | The **existing** `FireWeaponUpdate` → `ECMTankMissileJammer` (100 SUBDUAL_MISSILE, radius 150, every 650 ms, `RadiusDamageAffects = ENEMIES NEUTRALS`). It now jams enemy drones and drone projectiles too, because their armors take SUBDUAL_MISSILE. **No new module is needed.** It keeps `ECMTankVehicleDisabler` (single-target SUBDUAL_VEHICLE) as the hard jam. Cap 3. Plus the retail EMP Pulse power. |
| GLA | Spoof & Jam Field (retail GPS Scrambler) | Add the same SUBDUAL_MISSILE `FireWeaponUpdate` to the scrambler's marker object, which already carries `GrantStealthBehavior`. Result: a temporary bubble of concealment plus jamming. |
| USA (no area jammer) | TLS SIGINT Vehicle + **Alt-PNT** upgrade | SIGINT: a large `StealthDetectorUpdate` (`DetectionRange` 350) and wide vision; it detects rather than jams. Alt-PNT: an `ArmorUpgrade` on every US drone and US projectile template, which swaps to a `PLAYER_UPGRADE` armor set with SUBDUAL_MISSILE 40 %. Projectiles have the precedent: `TunnelDefenderMissile` already carries an upgrade module. Effect: three pulses to jam instead of one, so fast munitions usually cross a bubble before they are jammed, while loitering drones are only delayed. |

- **What jamming does.** A jammed FPV scatters by `DistanceScatterWhenJammed`. With a blast radius of 10, that is effectively a guaranteed miss. Jam *resistance* is therefore modeled by time-to-jam (Alt-PNT) or immunity (fiber), not by scatter distance [REV #10].
- **Counters.**
  - Fiber-optic (GLA) and Alt-PNT (USA).
  - Unguided fire, which can't be jammed: tank guns, and artillery shells using `DumbProjectileBehavior`.
  - Killing the emitter.
  - HPM and lasers, which ignore jamming status.
- **Knobs.** Jammer radius and period, Type 5 cap, Alt-PNT percentage, projectile `MaxHealth`.

### 2.c Counter-UAS: three different answers to the cost exchange

- **Real-world basis.** Cheap drones force expensive intercepts; cheap interceptors, guns, lasers and HPM fix the exchange `[IRR §2 notes, §B.4, §B.10, §B.11, FC #19, FC #22]`. US layered C-UAS (LIDS, M-SHORAD, HPM) `[USA §2–3, FC #25]`. The PLA's gun, laser and HPM "iron triangle" `[CHN §2]`. No stockpile counts are used `[FC #4]`. Leonidas performance is a company claim, so its strength here is a design assumption.

**Hits-to-kill against a 60 HP drone or FPV.**

| Tier | Hits to kill |
|---|---|
| Gun PDL (30 damage, gun damage type) | 2 |
| Laser PDL (100 LASER) | 1 |
| Interceptor PDL (100 EXPLOSION; 25 % on projectiles, so the projectile is set to `MaxHealth 25`) | 1 |
| HPM pulse (40 HAZARD_CLEANUP per 500 ms, radius 100) | about 1 s inside the radius |

| Faction | C-UAS assets | Character |
|---|---|---|
| **USA (richest, priciest)** | LIDS Site (interceptor PDL, `ScanRange` 320, weapon range 300, 1500 ms); M-SHORAD (gun PDL + Stinger vs air); Leonidas HPM (**the only base-faction hard-kill that also kills fiber at range 100**); M1E3 APS; PD-escort drone. The retail Paladin and Avenger PDLs already shoot `SMALL_MISSILE`, so they kill FPV projectiles: an intended USA strength [REV #8]. | Expensive and deep; beaten only by saturation or by killing the sites |
| **China (guns + laser + jamming)** | PGL-625 (gun PDL); Type 100 C-UAS turret add-on; Gun/Missile site (retail gattling cannon + gun PDL); LY-1 Laser Site (static, infinite magazine). No interceptor drones and no HPM in the base faction (the Tank General gets HPM). | Mid-cost; strong when static and layered with Type 5 |
| **GLA (cheap guns only)** | C-UAS Gun Truck (retail Quad Cannon + gun PDL, `ScanRange` 160); MANPADS Team; MANPADS Nest. | Cheapest; short range; saturable; no answer to heavy loitering munitions except dispersion and decoys |

- **Implementation.**
  - Gun PDL: `PointDefenseLaserUpdate` firing a `ProjectileObject = NONE` gun weapon, with `PrimaryTargetTypes = SMALL_MISSILE DRONE` and `ScanRate` ≥ 250 ms (no `ScanRate 0`, for performance [REV #15d]).
  - Patriot: adds a PDL against `BALLISTIC_MISSILE` with a slow cycle.
  - Interceptor PDL (LIDS) fires the visible `MWCoyote` interceptor projectile (§7 A5). That path exists in code but no retail PDL uses it, so it is **prototype first** (fallback: a hitscan weapon with a missile FX trail).
- **Counters.** Saturation (more drones than PDL cycles), artillery and air against static sites, and decoys that draw PDL fire.

### 2.d Top-attack threat and armor protection (asymmetric)

- **Real-world basis.** FPVs are the leading armor killer. Cages give limited protection, and dispersion and EW matter more `[IRR §B.1, §B.9]`. APS is planned on M1E3 `[USA §2, FC #10]`. The Type 100 reportedly has APS (specs unconfirmed `[FC #13]`). PLA tanks have been seen with cages `[CHN §2]`.

| Faction | Protection | Implementation |
|---|---|---|
| USA | **Active (APS) only**, no cages | M1E3 (`AmericaTankPaladin`): innate PDL `MW_APSHardKill` (`ScanRange` 90, weapon range 60, one intercept per 4 s). Other US vehicles: the reworked `Upgrade_AmericaBattleDrone` spawns a slaved PD-escort drone, copied from `SupW_AmericaVehiclePointDefenseDrone`. Composite Armor stays a `MaxHealthUpgrade` as in retail. |
| China | **Passive cages, broad and cheap**; APS only on the Type 100 | `MW_Upgrade_ChinaSurvivabilityKit`: `ArmorUpgrade` → `MW_CagedTankArmor` on Type 99A, ZBD and PGL-625. Cages are **visible** through a `ModelConditionUpgrade` (`ConditionFlag = USER_1`) plus an attached cage draw module (§7 A12). Type 100 APS module is a 4th, mutually exclusive Overlord-style add-on (`ObjectCreationUpgrade`, `ChinaTankOverlordDefaultCommandSet` slot 7, which is free; §7 A7). |
| GLA | **Scrap cages**: cheapest, but slow the vehicle | `MW_Upgrade_GLAScrapCages`: `ArmorUpgrade` → `MW_ScrapCagedArmor` plus `LocomotorSetUpgrade` (−10 % speed) on Scorpion, Marauder and Technical. Visible scrap-cage draw module (§7 A12). `GLATankScorpion` already shows 5 upgrade cameos, so the cage works but shows no cameo. |

- **Constraints.**
  - `ArmorUpgrade` sets only the one `PLAYER_UPGRADE` flag. No retail vehicle uses `ArmorUpgrade`, so there is no collision.
  - PDLs cannot be gated by an upgrade, so APS is innate, an escort drone, or an add-on.
- **Counters.** Saturation beats APS (one intercept per cycle). Tank guns (ARMOR_PIERCING) ignore cages. Heavy loitering munitions overwhelm the 60–70 % reduction.

### 2.e ISR transparency and reduced stealth reliance

- **Real-world basis.** Cheap thermal drones shorten kill chains, though concealment and weather still open windows `[IRR §B.12]`. Decoys at scale `[IRR §B.13]`. Tunnels and terrain `[IRR §4]`. US and PLA space ISR `[USA §3, CHN §3]`.
- **Gameplay.**
  - **USA sees:** SIGINT vehicle, recon escorts, F-35 sensor fusion (a small `StealthDetectorUpdate`).
  - **China sees in bulk:** the Support Vehicle keeps the retail Listening Outpost detection (300).
  - **GLA hides:**
    - Camouflage (`Upgrade_GLACamouflage`) adds `MOVING` to the Rebel's `StealthForbiddenConditions` (retail precedent for `MOVING`).
    - **Decoys** and the retail **fake-structure system** become GLA-exclusive deception. The fake system is `GLAWorkerFakeBuildingsCommandSet` (fake CC, Barracks, Supply Stash, Arms Dealer and Black Market), reached from Worker slot 13 [REV #6].
- **Decoy implementation.**
  - `MW_GLADecoyVehicle` is a mobile, weaponless *unit* produced at the **Barracks** (slot 10). It is not worker-built, which avoids the untested non-`STRUCTURE` dozer path.
  - `KindOf = VEHICLE SELECTABLE SCORE`, 80 HP, slow locomotor.
  - Model: the new inflatable `MWDecoyTank` with a deflate death animation (§7 A13); the placeholder is `UVLiteTank`.
  - Enemy auto-acquire treats it as a vehicle.
  - **Prototype first:** check that AI target priority and PDLs actually spend on it.
- **Counters.** Jamming or killing recon drones blinds the enemy. Spoof & Jam fields. Tunnels.

### 2.f Long-range precision fires with limited stockpile

- **Real-world basis.** Deep precision strike is a US signature; PrSM has been used in combat `[USA §3, FC #3]`. Munitions and interceptors drain faster than they are rebuilt; this is the trend only, with no counts `[USA §3, IRR §B.11, FC #4]`. PLA mass precision fires `[CHN §3]`.
- **Gameplay: four stacked scarcity levers.**
  1. **Caps:** HIMARS 3, China TBM TEL 2, GLA missile launcher 3.
  2. **Magazines:** `ClipSize` 1–2 with `ClipReloadTime` 45–75 s.
  3. **Powers:** long `ReloadTime` with `SharedSyncedTimer`.
  4. **Price.**
- **Interception.** Long-range projectiles carry `BALLISTIC_MISSILE`, as the retail `TomahawkMissile` does. Patriot PDLs and USA PDLs can intercept them, and salvoes saturate.
- **Not possible in data** (Appendix A):
  - Per-shot ammunition purchase.
  - Ground rearming: `AutoReloadsClip = NO` strands a ground unit, and `RETURN_TO_BASE` works only for jets.

### 2.g Special-power enums, sciences and warnings

Shortcut lookups and validity checks are **by enum** (`ActionManager.cpp:1498-1570`; `findMostReadyShortcutSpecialPowerOfType`), so each enum must be unique within a player template.

| Power | Faction | Enum | Science slot | Warning |
|---|---|---|---|---|
| LUCAS one-way drone salvo (reworks the A-10 strike) | USA | `SPECIAL_A10_THUNDERBOLT_STRIKE` (in place) | retail A-10 1–3 | none (retail had none) |
| Long-Range Fires (PrSM/Tomahawk salvo) | USA | `SPECIAL_BATTLESHIP_BOMBARDMENT` (campaign-only; also allows non-ally object targets) | USA rank 3: 9 → 10 of 15 | none: mitigated by visible slow projectiles + `ViewObjectDuration` reveal |
| Drone Mothership swarm | China | `SPECIAL_CRATE_DROP` (unused in MP; has a passability check) | China rank 3: 14 → **15 of 15 (full)** | none: the carrier plane itself is visible and interceptable |
| One-Way Drone Swarm (new; the Anthrax Bomb stays) | GLA | `SPECIAL_TERROR_CELL` (`Command_TerrorCell` exists but is in no command set) | GLA rank 3: 9 → 10 of 15 | none: slow, interceptable projectiles from the map edge |
| Spoof & Jam Field | GLA | `SPECIAL_GPS_SCRAMBLER` (in place) | retail | retail EVA kept |
| Typhon Battery (Superweapon General only; rev 4) | USA SupW | `SUPR_SPECIAL_CRUISE_MISSILE` (cut-content `SupW_CruiseMissile`) | building | retail EVA does not cover it; public timer (`PublicTimer = Yes`, as the cut content already has) |
| Dark Eagle / DF-26 / Scud Storm superweapons | all | `SPECIAL_PARTICLE_UPLINK_CANNON` / `SPECIAL_NEUTRON_MISSILE` / `SPECIAL_SCUD_STORM` (**in place**, §2.h) | building | Retail detect/launch EVA fires by enum; audio is remapped for USA and China (§2.h) |

`SpecialPowerModule` announces EVA warnings only for particle cannon, neutron missile, Scud Storm, GPS Scrambler and Sneak Attack. The new powers rely on visible, slow, interceptable delivery instead [REV #12].

### 2.h Superweapons: modern conventional replacements (rev 4, decision 2)

**Rule: rework in place.** Each faction keeps its retail superweapon structure template, its `SpecialPower` template name and enum, `MaxSimultaneousLinkKey = Superweapon` (so the lobby superweapon restriction still works) and `PublicTimer = Yes`. Only the delivery modules, OCL, projectiles, Draw and strings change. Everything keyed on those names and enums keeps working:

| Mechanism | Keyed on | Where |
|---|---|---|
| "Superweapon launched" EVA | Enum | `SpecialPowerModule.cpp:563-610`: particle (`SPECIAL_`/`SUPW_`/`LAZR_PARTICLE_UPLINK_CANNON`), neutron (`SPECIAL_`/`NUKE_`/`SUPW_NEUTRON_MISSILE`), `SPECIAL_SCUD_STORM` |
| "Superweapon detected" EVA | `hasSpecialPower(enum)` | `Player.cpp:1663-1675` |
| Target validity | Enum | `ActionManager.cpp:1534-1554`, `:1800-1821`, `:1929-1951` |
| Skirmish AI | Power and template names | AI scripts |

**Verified:** every general's copy of the uplink (`AirF_`, `Lazr_`, `SupW_`, `Boss_`) and of the silo (`Tank_`, `Infa_`, `Nuke_`, `Boss_`) references the **base** `SuperweaponParticleUplinkCannon` / `SuperweaponNeutronMissile`.

| Faction | Retail (template → power → enum) | Modern replacement | Mechanic | Implementation | Interception and counters | Basis |
|---|---|---|---|---|---|---|
| USA | Particle Cannon (`AmericaParticleCannonUplink` → `SuperweaponParticleUplinkCannon` → `SPECIAL_PARTICLE_UPLINK_CANNON`; retail reload 240 s) | **Dark Eagle LRHW Battery** (all USA) | 2 hypersonic glide vehicles. Very fast terminal dive, precise (scatter ≈ 10), heavy point damage (each sized to destroy one production structure), small radius, no lingering effect. Precision where China has area and GLA has saturation | Remove `ParticleUplinkCannonUpdate`. Add `OCLSpecialPower` + `MissileLauncherBuildingUpdate` (door/launch pattern from `ChinaNuclearMissileLauncher`). New projectile `MW_DarkEagleGlider` (`MissileAIUpdate`, high `InitialVelocity`, `KindOf = PROJECTILE BALLISTIC_MISSILE`, high `MaxHealth`). Reload 300 s | Its speed leaves a short PDL window: a layered Patriot/LY-1 base may kill 1 of 2. Counters: dispersal, killing the battery (Spec Ops, Sneak Attack, Long-Range Fires), GLA rebuild holes and fake structures | Dark Eagle entering service, about 2,775 km, 4 TELs × 2 missiles per battery `[USA §2]` (battery-2 timing not checked `[FC]`) |
| China | Nuclear Missile (`ChinaNuclearMissileLauncher` → `SuperweaponNeutronMissile` → `SPECIAL_NEUTRON_MISSILE`; retail 360 s) | **DF-26 Conventional Strike Complex** | Salvo of 4 conventional ballistic warheads, each bursting into submunitions over the target. Largest area, moderate per-point damage, **no radiation field** | Keep `OCLSpecialPower` + `MissileLauncherBuildingUpdate` (silo doors). `SUPERWEAPON_NeutronMissile` OCL → `FireWeapon` of a new weapon firing 4 `MW_DF26Warhead` (`MissileAIUpdate`, `BALLISTIC_MISSILE`, scatter 80, cluster `FireWeaponWhenDeadBehavior`). Drop the retail radiation death effects. Reload 360 s | Retail `NeutronMissile` is `UNATTACKABLE` and uses `NeutronMissileUpdate`; the switch to `MissileAIUpdate` + `BALLISTIC_MISSILE` makes it interceptable. Patriot PDLs thin the salvo. Counters: dispersal, killing the complex | DF-26/DF-17, world-leading hypersonic arsenal, mass precision fires `[CHN §2–3, FC #17]` |
| GLA | Scud Storm (`GLAScudStorm` → `SuperweaponScudStorm` → `SPECIAL_SCUD_STORM`; retail 300 s, 9 missiles) | **Scud Storm: Dispersed Launch Complex** (name kept, so retail GLA EVA lines stay accurate) | Camouflaged when idle and revealed while launching. Salvo of 8 improvised ballistic missiles: inaccurate (scatter 120), mixed HE and cluster. **Decoy launch sites** in the fake-structure set | Add `StealthUpdate` (`InnateStealth = Yes`, `StealthForbiddenConditions = ATTACKING`). Re-enable `BALLISTIC_MISSILE` on `ScudStormMissile`, whose retail `KindOf` has it commented out, and tune `MaxHealth` so layered PDLs kill 2–3 missiles. New fake `MW_FakeGLAScudStorm` in `GLAWorkerFakeBuildingsCommandSet`, slot 6 (free). Rebuild hole kept | Interceptable but saturating. Detection (SIGINT, recon drones) is needed to find it before launch | Irregular ballistic and anti-ship missiles, smuggling and local manufacture `[IRR §2]`; dispersion and decoys `[IRR §B.13]` |

**Distinct from the GLA One-Way Drone Swarm** (`SPECIAL_TERROR_CELL`, §2.g):

| | Scud Storm | One-Way Drone Swarm |
|---|---|---|
| Speed | Fast ballistic | Slow |
| Origin | A fixed, hidden base complex | The map edge |
| Effect | Heavy area damage, superweapon timer | Cheap, light, rank-3 science power |

**Balance targets** (benchmark in Phase 2):
- Each strike is a game-ending threat to an undefended base: it destroys 1–2 key structures or a clumped army.
- A base with layered interceptors (USA Patriot + LIDS; China gun/missile sites, which gain a low-rate PDL vs `BALLISTIC_MISSILE` reflecting the HQ air-defense ladder `[CHN §2]`) loses about half the warheads.
- GLA has no ballistic interception by design; it survives through dispersal, decoys, fake structures and rebuild holes.
- Reloads: Dark Eagle 300 s, DF-26 360 s, Scud Storm 300 s, all with public timers.

**EVA audio.**
- The enum-keyed *events* keep firing, but the retail USA/China *audio* names the particle cannon and nuclear missile.
- `Eva.ini` (outside the INI CRC, client-side) remaps the USA and China superweapon detect, ready and launch events to synthesized alarm tones (§7 A24). No new voice acting.
- The superweapon timer label comes from `generals.csf`.
- GLA keeps its retail Scud Storm lines.


---

## 3. Faction proposals

Change types:

| Code | Meaning |
|---|---|
| **Ren** | Rename (CSF only) |
| **Bal** | Numbers only |
| **Rwk** | Modules or weapons change; template name kept |
| **Rep** | New role in the same template |

Cost tiers: T1 ≤ 300, T2 300–900, T3 900–1,600, T4 > 1,600.

### 3.0 Identity: what each faction cannot do [REV #7]

| | USA | China | GLA |
|---|---|---|---|
| **Unique to it** | APS; interceptor-drone C-UAS; HPM (base); CCA; SIGINT; Alt-PNT; space ISR | Mobile area jamming; drone mothership; drone carrier; horde-bonus FPV mass; laser site | Fiber-optic drones; decoys and fake structures; tunnels; spoof field; salvage; chemicals (retail) |
| **Cannot do** | No area jammer; no cheap FPV mass; no cages; no fiber | No fiber; no interceptor drones; no HPM (except Tank General); no decoys | No air force; no APS; no laser, HPM or interceptors; no mobile jammer; no precise long-range fires |
| **Wins by** | Information + precision; trades few for many | Mass + spectrum control + artillery | Cost exchange, deception, attrition |
| **Loses to** | Saturation; losing the expensive C-UAS/ISR nodes | Fiber-optic drones; HPM; precision strikes on Type 5 and artillery | Detection + area fires; armor that it cannot crack without drones |

### 3.1 USA: precision, networks and the glass jaw

**Identity.** The most capable, most expensive force. It wins on information, precision fires and air power, and fields few but hardened drones behind the deepest C-UAS layer. Saturation and stockpile limits punish it `[USA §1, §3]`.

| Retail (template) | Modern name | Type | What changes | Model |
|---|---|---|---|---|
| Ranger (`AmericaInfantryRanger`, 225) | Rifle Squad (XM7) | Bal | Range 150 → 165; flashbang and capture kept | `AIRngr_SKN` |
| Missile Defender (`AmericaInfantryMissileDefender`, 300) | Javelin Team | Rwk | Primary becomes TOP_ATTACK (range 200, slower ROF); AA at 50 %; laser-guided ability kept as "Designate" | `NITHNT_SKN` |
| Pathfinder (`AmericaInfantryPathfinder`) | Scout-Sniper | Bal | Detection 200 | retail |
| Col. Burton (`AmericaInfantryColonelBurton`) | SOF Operator (hero) | Ren | — | retail |
| Biohazard Tech (`AmericaInfantryBiohazardTech`) | CBRN Tech | Ren | — | retail |
| Humvee (`AmericaVehicleHumvee`, 700) | ISV | Rwk | 600 / 200 HP / +15 % speed; `Upgrade_AmericaTOWMissile` → "Javelin Mount" (TOP_ATTACK) | `AVHUMMER` |
| Crusader (`AmericaTankCrusader`, 900) | M1A2 SEPv3 Abrams | Bal | 1,000 / 520 HP | `AVLeopard` |
| Paladin (`AmericaTankPaladin`, 1,100) | M1E3 Abrams | Rwk | `PaladinPointDefenseLaser` → `MW_APSHardKill` vs `SMALL_MISSILE DRONE`; faster; still gated by `SCIENCE_PaladinTank` | `AVPaladin` |
| Tomahawk (`AmericaVehicleTomahawk`, 1,200) | HIMARS (PrSM) | Rwk | Range 450, ClipSize 2, reload 60 s, `BALLISTIC_MISSILE`, cap 3 | `AVTomahawk` |
| Avenger (`AmericaTankAvenger`, 2,000) | Stryker M-SHORAD | Rwk | 1,400; laser PDLs → gun PDL vs drones; 30 mm + Stinger vs air; designator kept | **new** `MWStrykerSHO` (§7 A8/A9); placeholder `AVAVNGER` |
| Microwave Tank (`AmericaTankMicrowave`, 800) | Stryker Leonidas HPM | Rwk | Keeps building disabler (SUBDUAL_BUILDING) and garrison clearer; adds HPM `FireWeaponUpdate` (HAZARD_CLEANUP + SUBDUAL_MISSILE, radius 100) + HPM pulse FX (§7 A15) | **new** `MWStrykerHPM` (§7 A8/A9); placeholder `AVTHUNDRBLT` |
| Sentry Drone (`AmericaVehicleSentryDrone`) | RCV-L | Rwk | Gun upgrade → 30 mm | `AVSENTRY` |
| Ambulance (`AmericaVehicleMedic`) | Medevac | Ren | — | `AVAmbulance` |
| Scout / Battle / Hellfire drone upgrades | Recon Quadcopter / **PD-Escort Drone** / **Launched Effect** | Rwk | Battle drone → copy of `SupW_AmericaVehiclePointDefenseDrone`; Hellfire drone fires `MW_LoiterMunition`; all three move to `MW_SmallDroneArmor`; retail `Upgrade_AmericaDroneArmor` (`MaxHealthUpgrade`) kept | **new** `MWRecon` / retail `AVBATTLEDR` / **new** `MWLoiter` (§7 A2, A3) |
| Comanche (`AmericaVehicleComanche`) | AH-64E | Rwk | `Upgrade_ComancheRocketPods` → "Air-Launched Effects" (tertiary fires 2 loitering munitions) | `AVComanche` |
| Raptor / Stealth Fighter / Chinook | F-22A / F-35A / CH-47F | Ren/Bal | F-35 gets a small `StealthDetectorUpdate` | retail |
| Aurora (`AmericaJetAurora`) | B-21 Raider | Rwk | Supersonic → subsonic + `InnateStealth` (`StealthForbiddenConditions = FIRING_PRIMARY`); **bombs stay `AURORA_BOMB`** | `AVAurora` |
| Patriot (`AmericaPatriotBattery`) | Patriot Battery | Rwk | +PDL vs `BALLISTIC_MISSILE` (slow) | `ABPatriot` |
| Fire Base / Strategy Center | Fire Base (M777) / Joint Operations Center | Ren | — | retail |
| Particle Cannon (`AmericaParticleCannonUplink`, `SuperweaponParticleUplinkCannon`) | **Dark Eagle LRHW Battery** | Rwk | §2.h: `ParticleUplinkCannonUpdate` → `OCLSpecialPower` + `MissileLauncherBuildingUpdate`; 2 hypersonic glide vehicles | **new** `MWDarkEagle` (§7 A20); placeholder `ABSDILink` (the retail uplink dish) |
| A-10 Strike (`SuperweaponA10ThunderboltMissileStrike`) | LUCAS One-Way Drone Salvo | Rwk | OCL: 6 slow `MW_OneWayDrone` (EXPLOSION, interceptable) | **new** `MWOneWay` (§7 A4) |
| Spy Drone, Spectre, Daisy Cutter/MOAB, Paradrop, Repair, Leaflets, Spy Satellite, CIA Intel | ISR Drone Sortie, AC-130J, BLU-82/GBU-43, —, —, Info-Ops Broadcast, Space ISR Pass, SIGINT Fusion | Ren | — | retail |

**New USA content.**

| New | Role | Counters | Tier | Implementation | Model |
|---|---|---|---|---|---|
| **Switchblade Team** (`MW_AmericaInfantrySwitchbladeTeam`) | 1 loitering munition (range 260; needs a spotter beyond 200), Alt-PNT-hardened | China guns/laser, snipers, artillery | T2 (500) | Infantry; PRIMARY = `MW_Switchblade300` (TOP_ATTACK 90 + splash vs infantry); innate recon quadcopter (`ObjectCreationUpgrade` granted at creation) | `NITHNT_SKN` |
| **Switchblade 600** (`MW_Upgrade_AmericaSwitchblade600`) | Heavy anti-armor TOP_ATTACK 200 | Cages, China C-UAS guns and laser | 1,000 | `WeaponSetUpgrade` | — |
| **TLS SIGINT Vehicle** (`MW_AmericaVehicleSIGINT`) | Detection 350, vision 400; no weapon | Anything that reaches it | T2 (700), cap 2 | `StealthDetectorUpdate`; `CANNOT_RETALIATE` | **new** `MWStrykerSIG` (§7 A8/A9) |
| **LIDS Site** (`MW_AmericaLIDSSite`) | Interceptor PDL (prototype first) | Saturation, artillery, jets | T2 (900), `POWERED` | Structure + PDL; radar spin loop; visible `MWCoyote` launches | **new** `MWLIDSSite` (§7 A10) |
| **CCA** (`MW_AmericaJetCCA`) | Unmanned escort/striker | AA, fighters; shares 4 parking spots per Airfield | T3 (900), cap 4 | Copy of `AmericaJetStealthFighter` without `EjectPilotDie` | **new** `MWCCA` (§7 B1); placeholder `AVStealth` at Scale 0.8 |
| **Alt-PNT** (`MW_Upgrade_AmericaAltPNT`) | US drones and munitions take SUBDUAL_MISSILE at 40 % | Guns and lasers still kill them; long bubbles still jam loitering drones | 1,500 | `ArmorUpgrade` on US drone + projectile templates | — |
| **Long-Range Fires** power (`MW_SuperweaponLongRangeFires`) | 4-missile salvo, interceptable | PDL, long reload | rank-3 science | `OCLSpecialPower`, `SPECIAL_BATTLESHIP_BOMBARDMENT`, 300 s | `AVTomahawk` missiles |

**USA button budgets** (visible slots 1–14).

| Building | Slots | Used |
|---|---|---|
| War Factory | 1 Abrams, 2 HIMARS, 3 ISV, 4 Medevac, 5 M1E3, 6 RCV, 7 M-SHORAD, 8 Leonidas, 9 RCV gun, **10 TLS SIGINT**, 11 Javelin Mount, 12 –, 13 Rally, 14 Sell | 13/14 |
| Barracks | 1 Rifle, 2 Javelin, 3 SOF, 4 Scout-Sniper, **5 Switchblade Team**, 6 CBRN, 7 Flashbang, 8 Capture, **9 Switchblade 600**, 13, 14 | 11/14 |
| Airfield | 1 F-22, 2 AH-64E, 3 B-21, 4 F-35, **5 CCA**, 7 Launched Effects, 8 Laser Missiles, 9 Countermeasures, 10 Bunker Busters, 13, 14 | 11/14 |
| Strategy Center | retail 1, 2, 3, 5–11, 13, 14 + **4 Alt-PNT** (retail slot 4 is commented out) | 13/14 |
| Dozer | retail 1–9, 11, 13, 14 + **10 LIDS** | 13/14 |
| Command Center | retail 1, 2, 4–10, 13, 14 + **3 Long-Range Fires** | 12/14 |

The `AirF_`, `Lazr_` and `SupW_` copies must mirror these sets.

### 3.2 China: mass, armor and the spectrum

**Identity.** Mass precision fires and heavy armor under the only mobile jamming umbrella in the game. FPVs come in hordes and from carriers. Conventional missiles are the superweapon; only the Nuke General keeps a nuclear option. Network dependence is the weakness: kill the Type 5 and the artillery, and the mass goes blind and loud `[CHN §1, §3]`.

| Retail (template) | Modern name | Type | What changes | Model |
|---|---|---|---|---|
| Red Guard (`ChinaInfantryRedguard`) | PLA Rifle Squad | Bal | −10 % cost; horde kept | `NICNSC_SKN` |
| Tank Hunter (`ChinaInfantryTankHunter`) | HJ-12 ATGM Team | Rwk | TOP_ATTACK (range 200); TNT → "Demolition Charge" | `NIMSST_SKN` |
| Hacker / Black Lotus | Cyberspace Force Operator / Special Ops Infiltrator | Ren | — | retail |
| Battlemaster (`ChinaTankBattleMaster`) | Type 99A | Bal | — | `NVBtMstr` |
| Overlord (`ChinaTankOverlord`, 2,000) | Type 100 | Rwk | 1,800 / 950 HP / faster. Add-ons: `Upgrade_ChinaOverlordGattlingCannon` → C-UAS turret (gun PDL); `…PropagandaTower` → Data-Link Node (same aura); `…BattleBunker` → Loitering Pod (2 loitering munitions / 30 s). Optional 4th add-on, APS module, needs a 4th `ObjectCreationUpgrade` and button (the Overlord's own command set has room). Specs are design-level only `[FC #13]`. **Re-added:** 4th add-on, APS module (visible, slot 7) | **new** `MWType100` + add-on models (§7 A6, A7); placeholder `NVOvrlrd` |
| Gattling Tank (`ChinaTankGattling`) | PGL-625 SPAAG | Rwk | + gun PDL; `Upgrade_ChinaChainGuns` → "Airburst Rounds" | `NVGattTank` |
| Dragon (`ChinaTankDragon`) | Thermobaric Rocket Vehicle | Rwk | Short-range FLAME rocket salvo; `Upgrade_ChinaBlackNapalm` → "Thermobaric Warheads" | `NVDragon` |
| Troop Crawler (`ChinaVehicleTroopCrawler`) | ZBD-04A IFV | Rwk | 1,100; + 30 mm; 6 passengers | `NVTCrawler` |
| Listening Outpost (`ChinaVehicleListeningOutpost`) | Type 100 Support Vehicle (drone carrier) | Rep | Certain tier: SECONDARY fires FPV projectiles (ClipSize 3, reload 25 s). Prototype tier: `SpawnBehavior` + `QueueProductionExitUpdate`, 2 kamikaze quadcopters. Keeps detection 300 and stationary stealth | **new** `MWT100Sup` with VLS hatch animation (§7 B4); placeholder `NVLOUTPOST` |
| Inferno Cannon (`ChinaVehicleInfernoCannon`) | PHL-16 MRL | Rwk | 6-rocket salvo, range 400, 40 s reload, cap 4 | `NVInferno` |
| Nuke Cannon (`ChinaVehicleNukeLauncher`) | Tactical Ballistic Missile TEL | Rwk | Conventional `BALLISTIC_MISSILE`, range 500, ClipSize 1, 75 s, cap 2 | `NVNukeCn` |
| ECM Tank (`ChinaTankECM`) | Type 5 EW Vehicle | Bal | §2.b: unchanged modules, new reach through drone armors; cap 3 | `NVBANSHEE` |
| Helix / MiG | Z-20T / J-16 | Ren/Bal | Helix add-ons as on the Type 100 | retail |
| Gattling Cannon (`ChinaGattlingCannon`) | Gun/Missile C-UAS Site | Rwk | + gun PDL | `NBGattling` |
| Propaganda / Internet Center / Speaker Tower / Bunker | ISF Center / Cyberspace Force Center / Info-Ops Relay / Fortified Position | Ren | — | retail |
| Nuclear Missile (`ChinaNuclearMissileLauncher`, `SuperweaponNeutronMissile`) | **DF-26 Conventional Strike Complex** | Rwk | §2.h: 4 conventional ballistic warheads with submunitions; no radiation; interceptable | retail silo `NBNMissle` kept + new warhead models (§7 A22); TEL pad is Tier B (B10) |
| Artillery Barrage | Joint Firepower Strike | Rwk | OCL → PHL-16 salvo (interceptable) | retail FX |
| Carpet Bomb / Napalm / Cluster Mines / Cash Hack / EMP / Frenzy / Repair | H-6K Strike / Thermobaric Strike / Scatterable Mines / Cyber Theft / HPM-EMP Strike / Intelligentized Targeting / Repair | Ren | — | retail |

**New China content.**

| New | Role | Counters | Tier | Implementation | Model |
|---|---|---|---|---|---|
| **FPV Team** (`MW_ChinaInfantryFPVTeam`) | Mass RF FPVs, horde bonus | GLA spoof field, USA C-UAS/HPM, artillery | T1 (250) | Infantry, ClipSize 2, reload 15 s | `NIMSST_SKN` |
| **GJ-11** (`MW_ChinaJetGJ11`) | Stealthy unmanned striker | AA; parking (4 per Airfield) | T3 (1,100), cap 2 | Copy of `ChinaJetMIG`, `InnateStealth` when not firing | **new** `MWGJ11` (§7 B2); placeholder `NVMIG` at Scale 0.85 |
| **LY-1 Laser Site** (`MW_ChinaLaserSite`) | Infinite-magazine C-UAS, `ScanRange` 260 / weapon 240 | Artillery, saturation | T2 (1,000) | Structure + LASER PDL; beam FX from `Lazr_PatriotLaserBeam` | **new** `MWLY1Site` (§7 A11) |
| **Drone Mothership** (`MW_SuperweaponDroneMothership`) | Swarm drop on an area (no count asserted `[FC #14]`) | C-UAS, AA vs the carrier | rank-3 science | `OCLSpecialPower`, `SPECIAL_CRATE_DROP`, `DeliverPayload` from the cargo plane, 240 s | **new** `MWMothship` (§7 B3); placeholder `NVCargoPln` |
| **Survivability Kit** (`MW_Upgrade_ChinaSurvivabilityKit`) | Cages and slat armor (TOP_ATTACK only) | Tank guns | 1,500 | `ArmorUpgrade` (§2.d) | — |
| **Intelligentized Kill Chain** (`MW_Upgrade_ChinaKillChain`) | +15 % range for PHL-16, TBM TEL and FPV teams | Kill the launchers | 2,000 | `WeaponBonusUpgrade` (PLAYER_UPGRADE) plus a per-weapon `WeaponBonus = PLAYER_UPGRADE RANGE 115%` in `Weapon.ini`. No aura, which avoids the `DRONE_SPOTTING` clash with `SlavedUpdate` [REV #15f] | — |

**China button budgets.**

| Building (+ its `…Upgrade` twin) | Slots | Used |
|---|---|---|
| War Factory | retail 1–14, all reworked in place | 14/14 |
| Barracks | 1 Rifle, 2 HJ-12, 3 Cyber, 4 Spec Ops, **5 FPV Team**, 7 Capture, 12 Mines, 13, 14 | 9/14 |
| Airfield | 1 J-16, 2 Aircraft Armor, 3 Z-20T, **4 GJ-11**, 12, 13, 14 | 7/14 |
| ISF Center | 1 Nationalism, **2 Kill Chain**, 3 Subliminal, **4 Survivability Kit**, 12, 14 | 6/14 |
| Dozer | retail 1–12, 14 + **13 LY-1 Site** | 14/14 |
| Command Center | retail 1–10, 12–14 + **11 Mothership** | 14/14 |

All 13 `…CommandSetUpgrade` twins must be mirrored, plus the `Tank_`, `Infa_`, `Nuke_` and `Boss_` copies.

### 3.3 GLA: cheap, hidden, improvised

**Identity.** A fictional under-equipped force; no real group is modeled or named. Its "air force" is commercial and fiber-optic drones. Its armor is salvaged and scrap-caged. It wins by cost exchange, deception, tunnels and spoofing, and loses when found and fixed `[IRR §4]`. The retail chemical arsenal stays as fiction (Toxin Tractor, Toxin Shells, Anthrax Bomb and Beta). The Terrorist becomes an FPV Pilot in the base faction, but the Demolition General keeps the retail `Demo_GLAInfantryTerrorist` [REV #7].

| Retail (template) | Modern name | Type | What changes | Model |
|---|---|---|---|---|
| Rebel (`GLAInfantryRebel`) | Fighter | Bal | Camouflage breaks on MOVING | `UIRGrd_SKN` |
| RPG Trooper (`GLAInfantryTunnelDefender`) | RPG Team | Bal | Stays INFANTRY_MISSILE; `Upgrade_GLAAPRockets` → "Tandem Warheads" | `UITunF_SKN` |
| Terrorist (`GLAInfantryTerrorist`) | FPV Pilot | Rep | PRIMARY = RF FPV (range 200, ClipSize 1, reload 12 s); SECONDARY = pistol. Voice set swapped from the suicide lines to retail Rebel lines | `UITRST_SKN` (retail skin; no new infantry rig, §7) |
| Angry Mob / Hijacker / Saboteur / Jarmen Kell | Militia Mob / Vehicle Thief / Saboteur / Jarmen Kell | Ren/Bal | Kell gets a recon quadcopter through Commercial Drones | retail |
| Technical, Scorpion, Marauder | (same) | Bal | Scrap Cages eligible | retail |
| Quad Cannon (`GLAVehicleQuadCannon`) | C-UAS Gun Truck | Rwk | + gun PDL | `UVQuadCann` |
| Radar Van (`GLAVehicleRadarVan`) | **Drone Workshop Van** | Rwk | Keeps `SpecialPowerRadarVanScan` (the OCL now spawns a temporary recon quadcopter). Gains PRIMARY: FPV launcher (ClipSize 3, reload 20 s; fiber with Spools). Prototype tier: `SpawnBehavior` swarm as on the China carrier. Voices stay retail | `UVRadarVan` + attached workshop kit (§7 B8) |
| Toxin Tractor (`GLAVehicleToxinTruck`) | Toxin Tractor | — | **Kept** | `UVToxinTrk` |
| Bomb Truck / Scud Launcher / Combat Cycle (+ terrorist variant) / Battle Bus / Rocket Buggy | Explosive Truck / Improvised Missile Launcher (`BALLISTIC_MISSILE`, 45 s, cap 3) / Combat Cycle (+ FPV rider) / — / — | Ren/Rwk | — | retail |
| Stinger Site / Demo Trap / Tunnel / Palace / Black Market | MANPADS Nest / IED / Tunnel Network / — / — | Ren | — | retail |
| Scud Storm (`GLAScudStorm`, `SuperweaponScudStorm`) | **Scud Storm: Dispersed Launch Complex** | Rwk | §2.h: camouflaged when idle; 8 improvised ballistic missiles (interceptable); decoy launch sites in the fake-structure set | retail model + decoy-site model (§7 A23) |
| GPS Scrambler | Spoof & Jam Field | Rwk | §2.b | retail |
| Anthrax Bomb, Ambush, Sneak Attack, Repair, Cash Bounty | (kept) | — | — | retail |

**New GLA content.**

| New | Role | Counters | Tier | Implementation | Model |
|---|---|---|---|---|---|
| **MANPADS Team** (`MW_GLAInfantryMANPADS`) | Mobile AA and drone killer | Infantry, artillery | T1 (200) | **New Object** copied from `GLAInfantryStingerSoldier`: remove `CLICK_THROUGH`, `SPAWNS_ARE_THE_WEAPONS` and `SlavedUpdate`; add `SELECTABLE`, a CommandSet and voices [REV #5] | `UISmsd_SKN` |
| **Decoy** (`MW_GLADecoyVehicle`) | Soaks FPVs, PDL cycles and AI fire | Dies to any hit; never fires | T1 (100) | Barracks-produced unit (§2.e); prototype first | **new** `MWDecoyTank` (§7 A13) |
| **Commercial Drones** (`MW_Upgrade_GLACommercialDrones`) | Recon quadcopter on Technicals and Kell only (bounded count) | Jamming, guns | 800 | `ObjectCreationUpgrade` | **new** `MWRecon` (§7 A2) |
| **Scrap Cages** (`MW_Upgrade_GLAScrapCages`) | §2.d | Tank guns | 800 | `ArmorUpgrade` + `LocomotorSetUpgrade` | — |
| **Fiber-Optic Spools** (`MW_Upgrade_GLAFiberSpools`) | **GLA-exclusive** jam immunity for FPV Pilots, Workshop Van and FPV rider; range 200 → 170 | Guns, lasers, HPM | 1,500 | `WeaponSetUpgrade` | — |
| **One-Way Drone Swarm** (`MW_SuperweaponDroneSwarm`) | 8 slow drones from the map edge (EXPLOSION; can hit structures) | All C-UAS, Patriot | rank-3 science | `OCLSpecialPower`, `SPECIAL_TERROR_CELL`, 240 s | **new** `MWOneWay` (§7 A4) |

**GLA button budgets.**

| Building | Slots | Used |
|---|---|---|
| Arms Dealer | retail 1–14, in place (3 = Workshop Van) | 14/14 |
| Barracks | 1 Fighter, 2 RPG, 3 FPV Pilot, 4 Militia, 5 Thief, 6 Kell, 7 Saboteur, 8 Booby Trap, **9 MANPADS**, **10 Decoy**, 11 Capture, 13, 14 | 13/14 |
| Palace | retail unchanged | 12/14 |
| Black Market | retail 1–6, **7 Commercial Drones**, **8 Scrap Cages**, **9 Fiber Spools**, 14 | 10/14 |
| Command Center | retail 1, 4–8, 13, 14 + **9 Drone Swarm** | 9/14 |
| Worker | retail unchanged (fake-structure set on slot 13) | 12/14 |

Retail `Slth_GLAArmsDealerCommandSet` defines an invisible slot 15. Clean it up rather than copy it.

---

## 4. Generals (sub-factions)

Each general has copied objects with a prefix (`AirF_`, `Lazr_`, `SupW_`, `Tank_`, `Infa_`, `Nuke_`, `Demo_`, `Slth_`, `Chem_`). Base changes are mirrored into the copies, then the general's twist is applied.

**Superweapon copies** (verified): all USA uplink copies (`AirF_`, `Lazr_`, `SupW_`, `Boss_ParticleCannonUplink`) and all China silo copies (`Tank_`, `Infa_`, `Nuke_`, `Boss_NuclearMissileLauncher`) reference the *base* `SuperweaponParticleUplinkCannon` / `SuperweaponNeutronMissile` templates. Reworking those two templates and their OCLs in place therefore modernizes every general at once; only the Draw and modules of each copy need mirroring. The Scud Storm copies (`Demo_`, `Slth_`, `Chem_`, `Boss_ScudStorm`, `GC_*`) are mirrored the same way. `Boss_` and `GC_` copies stay loadable but are unsupported.

- **Air Force General (Granger): CCA and loyal-wingman doctrine.** CCAs are built at the Airfield (no jet-spawner [REV #2]), with cap 8 instead of 4 and −25 % cost. Parking still limits them to 4 jets per Airfield, so he needs more Airfields. `AirF_AmericaJetRaptor` (King Raptor, PDL via `AirF_RaptorPointDefenseLaserBeam`) adds `DRONE` to its PDL targets. The Combat Chinook becomes the MV-75. No Abrams or M1E3; retail already omits Crusader and Paladin.
- **Laser General (Townes): directed-energy C-UAS.** Every Lazr PDL adds `DRONE`. `Lazr_AmericaTankAvenger` becomes the premier anti-swarm vehicle. `Lazr_AmericaTankCrusader` keeps its beam. He builds the standard Dark Eagle battery. His retail `Lazr_AmericaLaserCannonUplink` (`Lazr_LaserCannon`, `LAZR_SPECIAL_PARTICLE_UPLINK_CANNON`) is cut content (commented out of `Lazr_AmericaDozerCommandSet` slot 6) and stays unused. He never pays for interceptors but struggles against massed heavy armor.
- **Superweapon General (Alexander): long-range fires and integrated air/missile defense** (rev 4; Dark Eagle is now standard for every USA player).
  - Her `SupW_AmericaParticleCannonUplink` becomes the standard Dark Eagle, with a −20 % reload, matching her retail shorter timer (`SupW_SuperweaponParticleUplinkCannon` 180 s vs 240 s).
  - Her twist is a **second** strategic system: the **Typhon Battery**, revived from cut-content `SupW_AmericaCruiseMissileLauncher` (`SupW_CruiseMissile`, `SUPR_SPECIAL_CRUISE_MISSILE`).
    - 6 Tomahawk-class cruise missiles: slow, interceptable `BALLISTIC_MISSILE`, area damage. Reload 180 s.
    - Its own `MaxSimultaneousLinkKey` (not `Superweapon`), cap 1, so she can field both Typhon and Dark Eagle.
    - Model: the Phase 2 Typhon launcher reuses the A20 Dark Eagle script with a box-launcher kit. Cut content needs a smoke test [REV #15e].
  - `SupW_AmericaVehiclePointDefenseDrone` becomes standard on her vehicles. Patriot PDL rate ×1.5. Real basis: Typhon is fielded `[USA §2]`.
- **Tank General (Kwai): Type 100 drone-teamed armor.**
  - `Tank_ChinaTankEmperor` becomes the "Type 100 Command Tank" with all add-ons, plus **China's only HPM** add-on.
  - `Tank_Upgrade_ChinaTankAutoLoader` is kept.
  - `SPECIAL_TANK_PARADROP` becomes a Type 99A airlift.
- **Infantry General (Fai): mass mobilization.** `Infa_ChinaInfantryMiniGunner` becomes a Militia Squad. FPV Teams are −30 % cost with a stronger horde bonus. `Infa_ChinaVehicleTroopCrawler` carries FPV teams. `Upgrade_Fanaticism` becomes "Mobilization".
  - **Re-added: Armed UGV** (`MW_Infa_ChinaVehicleUGV`, T1 400, tracked, 12.7 mm). Model `MWUGV` (§7 B6). Goes in `Infa_ChinaWarFactoryCommandSet` slot 1 (and its `…Upgrade` twin); retail leaves slots 1, 2 and 5 free, so the set becomes 12/14.
  - Robot-dog squad (slot 2) is Tier C art (§7 C1).
- **Nuke General (Tao): the one nuclear holdout** (rev 4 decision 2, confirmed as decision 9 in §9).
  - **What he keeps.** His silo `Nuke_ChinaNuclearMissileLauncher` keeps a **nuclear** strike, reframed as a modern strategic missile (DF-5C/DF-31-class).
  - **How.** Retail points every silo copy at the base `SuperweaponNeutronMissile`. Rev 4 points his copy at the existing `Nuke_SuperweaponNeutronMissile` (`NUKE_SPECIAL_NEUTRON_MISSILE`) with the retail neutron OCL and 300 s reload. That enum is already in the nuke EVA branch (`SpecialPowerModule.cpp` `type == … NUKE_SPECIAL_NEUTRON_MISSILE`), so its retail nuke EVA lines stay correct.
  - **Why this fits "modernize all three".**
    - The decision replaces fictional or obsolete superweapons with modern equivalents. A Chinese strategic nuclear missile *is* a current, well-documented capability (warheads in the low 600s, rising, with 100+ new silos `[CHN §1, FC #17]`). It is modernized in fiction and kept in mechanics.
    - The base faction's superweapon becomes conventional (DF-26).
    - The nuke survives only as this general's identity, which keeps "nukes as backdrop" for China.
  - `Nuke_ChinaVehicleNukeLauncher` keeps its neutron shells. `Upgrade_ChinaNuclearTanks` and `Upgrade_ChinaIsotopeStability` stay.
- **Toxin General (Thrax): chemical specialist (fiction).**
  - `Chem_GLAVehicleToxinTruck` and `Chem_Upgrade_GLAAnthraxGamma` are kept and strengthened.
  - "Contaminated drones": Workshop drones leave `Chem_PoisonFieldGammaSmall` fields.
  - `Chem_GLAScudStorm` / `Chem_ScudStormMissile` get the modern complex but keep toxin warheads (fiction).
  - This is clearly fictional, not grounded in the research `[IRR §5]`.
- **Demolition General (Juhziz): suicide and explosive identity kept.**
  - He keeps the retail `Demo_GLAInfantryTerrorist` and the bomb-truck identity.
  - `Demo_GLAVehicleBombTruck` adds an "Explosive UGV" variant, model `MWExplUGV` (§7 B7).
  - `Demo_Upgrade_SuicideBomb` also boosts drone warheads.
  - The One-Way Drone Swarm gets +4 drones. `Demo_GLAScudStorm` / `Demo_ScudStormMissile` get the modern complex, keeping his high-explosive warhead flavor.
- **Stealth General (Prince Kassad): underground, decoys, fiber.**
  - Fiber-Optic Spools are free.
  - Two decoys per purchase.
  - `Upgrade_GLACamoNetting` is cheaper; tunnels get +50 % HP.
  - `Slth_SuperweaponGPSScrambler` gets a larger jamming radius.
  - No Scorpion or Marauder; retail already omits them.

---

## 5. Balance and matchup notes

| Matchup | Early | Mid | Late | Key interaction |
|---|---|---|---|---|
| **USA v China** | Few Switchblade teams vs many FPV teams. USA PDLs (Paladin, Avenger lineage) eat FPVs, so China must mass. | Type 5 bubbles vs Alt-PNT; HIMARS vs PHL-16 counter-battery | Dark Eagle vs DF-26 complex: both interceptable only by layered PDLs; Patriot and China's gun/missile sites duel the warheads; LY-1 vs CCA munitions | USA must kill the Type 5s and artillery; China must out-mass the USA C-UAS layer |
| **USA v GLA** | Quad cannons make USA escort drones costly; FPV pilots harass ISVs | SIGINT and F-35 detection hunt camouflage; decoys and fake structures waste USA precision | Swarm + Scud Storm complex (hidden, decoyed) vs layered C-UAS + Patriot; Dark Eagle vs a base that rebuilds from holes | Fiber-optic FPVs bypass nothing for the USA, which has no jammer: the HPM and LIDS cost decide it |
| **China v GLA** | FPV hordes vs GLA fighters | Type 5 jams RF GLA drones, so GLA must buy Spools; GLA's Spoof & Jam field jams China's all-RF drones | Caged Type 100 pushes vs tunnels, sneak attacks and swarms | A spectrum duel: whoever wins the jam/fiber race wins the drone layer |

**Rock-paper-scissors**
- FPVs beat armor and static defenders, and lose to C-UAS and (RF only) jamming. They barely hurt structures (TOP_ATTACK 10 %).
- C-UAS beats drones and loses to tanks, infantry and artillery.
- Jammers beat RF drones and guided munitions, and lose to fiber, unguided fire and emitter hunting.
- Tanks beat infantry, C-UAS and EW, and lose to FPV/ATGM teams unless protected.
- Long-range fires and superweapons beat static targets, and lose to PDL interception, dispersal, decoys and caps.

**Risks and mitigations**
1. **Drone spam.** Operator clips and reloads, a 200 range tied to own vision, 10 % vs structures, PDLs and cages. FPVs need 4–7 hits per MBT.
2. **C-UAS too strong.** PDL `DelayBetweenShots` ≥ 250 ms and short scan ranges let 3+ simultaneous drones leak. **Before Phase 2**, build a scripted benchmark matrix: FPV count (1/3/6/10) × C-UAS type and count (0/1/2/layered) × jammer present (yes/no) × fiber (yes/no). Record drones leaked, operator losses and cost ratio [REV #8].
3. **Spotter loop.** Only loitering munitions beyond 200 need spotters. FPV teams see their own range, so killing spotters blunts the long-range layer without blinding the whole drone game.
4. **Jamming over-reach.** This is solved structurally: SUBDUAL_MISSILE cannot affect vehicles, infantry or structures. The only remaining risk is that persistent drones stay subdued while inside a bubble. That is intended, and drones recover on exit.
5. **TOP_ATTACK engine change.** The risk is engine/version coupling, not side effects (the rev-3 dozer/CBRN/mine caveats no longer apply).
   - The mod needs a GeneralsX build with the change (§6.1).
   - On an older build, retail `Armor.cpp:99-100` and `DamageFX.cpp:167` write to index −1 when they meet an unknown damage name. That is memory corruption, not a clean error.
   - Mitigation: §6.1 adds bounds checks, and the launch script refuses to start the mod on a build without the feature.
6. **Skirmish AI (now the primary audience).**
   - In-place reworks, **including all three superweapons**, are used automatically: HIMARS, Type 5, C-UAS gun truck, FPV Pilot, Workshop Van, LUCAS salvo, Support Vehicle, Type 100 add-ons. AI scripts reference power and template names, which are unchanged.
   - New templates are AI-invisible until scripted. **Phase 4A** (§6) therefore puts the C-UAS sites and drone teams into AI build lists and team scripts **in Milestone 1**.
   - The remaining new templates (SIGINT, CCA, GJ-11, decoys, new powers) follow in Phase 4B.
11. **Superweapon balance.** They must stay a game-ending threat while counterable.
    - Targets (§2.h): an undefended base loses 1–2 key structures per strike; a base with layered PDLs loses about half the salvo.
    - Benchmarked in Phase 2 against each faction's defenses.
    - GLA has no ballistic interception by design. It counters with dispersal, decoys, fake structures and rebuild holes.
7. **Performance.** More projectiles and PDL scans: no `ScanRate 0`, and Commercial Drones are limited to Technicals and Kell. Profile on the GeneralsX DXVK path.
8. **Art pipeline (the #1 asset risk).** W3D export depends on the OpenSAGE Blender plugin.
   - Its support for current Blender versions is unconfirmed; the last activity seen was Feb 2024.
   - Mitigation: a time-boxed Phase 0 spike with a go/no-go gate and a defined fallback (§7.1, §7.5). The design never depends on new art for *function*. Every asset has a working retail placeholder, so a no-go degrades readability, not gameplay.
9. **Readability.**
   - Scripted, procedural textures will look plainer than the hand-painted retail art.
   - Mitigation:
     - Silhouette-first modeling (rotor arms, cage slats and VLS hatches read at RTS zoom).
     - Faction palettes and house color on every new model.
     - Cameos rendered in one consistent lighting rig.
     - An in-game readability check at default camera height for each Tier A asset before it is accepted.
10. **FX and audio performance.**
    - Jamming-bubble and HPM pulses fire every 500–650 ms per emitter.
    - Mitigation:
      - Low particle counts, with a `MinLODRequired = MEDIUM` precedent for optional detail.
      - FX on the pulse weapon's `FireFX` only, never per victim.
      - A `Limit` on every new `AudioEvent` (retail uses `Limit` on 478 of 743 events).
      - Positional sounds must be **mono**: the GeneralsX OpenAL path decodes through FFmpeg and falls back to non-spatial playback for multichannel files (`OpenALAudioCache.cpp:148-153`).

---

## 6. Implementation plan (not to be executed now)

**Packaging** `[ENG §1, §5]`
- Ship one `ModernWarfare.big`, launched with `-mod /abs/path/ModernWarfare.big`. Retail archives are never replaced.
- **Engine requirement (rev 4).** The mod is **not data-only**. It requires a GeneralsX build that includes the `DAMAGE_TOP_ATTACK` change (§6.1): the first GeneralsX release or commit after that change merges, for macOS (`macos-vulkan`) and Linux (`linux64-deploy`). Retail executables, other ports and older GeneralsX builds cannot run it. The launch script checks the build before mounting the `.big`.
- **Duplicate-object rule.** Every touched retail INI is replaced whole in the mod `.big`; new content goes in new `MW_*.ini` files.
- **`ObjectReskin`** only changes Draw, Geometry and `MaxSimultaneous*`, and must follow its original. Every unit that behaves differently is therefore a full Object that reuses a model.
- **No `GameData.ini` changes are needed.** Per-weapon `WeaponBonus` replaces global edits. Avoid `SagePatch.ini` for gameplay: it is outside the INI CRC.
- **Strings.** A modified `Data/English/generals.csf` goes in the mod `.big`. CSF is outside the CRC. Other languages show retail names.
- **Compatibility.**
  - Multiplayer and replays work only among players with the same mod *and* a TOP_ATTACK-capable build. Saves break.
  - Retail play on the new build is unchanged: retail data, saves and replays are byte-identical in behavior (§6.1).

| Phase | Scope | Files touched | Effort |
|---|---|---|---|
| **0 Tooling** | BIG pack/unpack (only an extract script exists, in the session scratchpad); CSF read/write; mod layout; `-mod` launch script; CRC smoke test (mod vs mod LAN, replay round trip) | none | 4–6 days |
| **0B Engine change: `DAMAGE_TOP_ATTACK`** (§6.1) | Enum + name table + bounds checks in both games; build on both presets; retail replay regression; mod parse test; upstream note | `Core/GameEngine/Include/GameLogic/Damage.h`, `Core/GameEngine/Source/GameLogic/System/Damage.cpp`, `{Generals,GeneralsMD}/Code/GameEngine/Source/GameLogic/Object/Armor.cpp`, `{…}/Source/Common/DamageFX.cpp`; dev blog | 2–4 days |
| **0C SCB tool** (moved up from Phase 4; AI-first) | Read/write `SkirmishScripts.scb` (binary), enough to add build-list entries and team templates | tooling only | 3–5 days |
| **0A Asset pipeline spike** (go/no-go, §7.5) | Pinned headless Blender + OpenSAGE plugin; Python W3D chunk validator (`wdump` is Windows/MFC-only); cameo renderer + 512² atlas packer + `MappedImages` generator (the repo's `ImagePacker` is a Windows tool); TGA→DDS step; SFX synth scripts. **Proof asset:** FPV quadcopter + cameo, loaded through a test `-mod` and screenshotted in-game | test `-mod` only: one Object, one `MappedImages` INI | 6–9 days (spike capped at 5) |
| **1 Systems + prototypes** | Primitives in §2.0; **prototype checklist**: (1) vehicle `SpawnBehavior` + `QueueProductionExitUpdate`; (2) PDL firing a projectile (LIDS); (3) SUBDUAL_MISSILE on drone units; (4) `TOP_ATTACK` armor audit: an explicit line in every armor, generated by script; (5) Barracks-built decoy targeting; (6) `ScanRange` > weapon range on every PDL; (7) `SupW_AmericaCruiseMissileLauncher` revival; (8) Alt-PNT `ArmorUpgrade` on projectiles; (9) benchmark matrix (§5) | `Armor.ini`, `Weapon.ini`, `Locomotor.ini`, `ObjectCreationList.ini`, light `FXList`/`ParticleSystem`, `Object/WeaponObjects.ini`, new `Object/MW_Drones.ini` | 12–18 days |
| **2 Factions** | All of §3 | `Object/America*`, `China*`, `GLA*`, `FactionBuilding.ini`, `FactionUnit.ini`, `SpecialPowerObjects.ini`, `Upgrade.ini`, `SpecialPower.ini`, `Science.ini`, `CommandSet.ini`, `CommandButton.ini`, `PlayerTemplate.ini` (science sets), `ObjectCreationList.ini` (superweapon OCLs), `Eva.ini` (superweapon audio remap), new `Object/MW_*.ini` | 26–38 days (incl. +2 MANPADS/Decoy and +4–6 superweapons) |
| **3 Generals** | Mirror base changes into 9 general files plus `BossGeneral.ini` and `GC_*` (keep loadable), then apply the twists (incl. the SupW Typhon and the Nuke General's holdout) | `Object/*General.ini`, `GC_*.ini`, `CommandSet.ini`, science sets | 14–20 days |
| **4A AI for Milestone 1** (AI-first, rev 4) | AI build lists and team scripts for C-UAS sites (LIDS, LY-1, gun sites), FPV/Switchblade teams, MANPADS; superweapon use checked; skill-set sciences incl. new rank-3 powers | `AIData.ini`, `Data/Scripts/SkirmishScripts.scb` | 8–12 days |
| **4B AI, strings, balance** | Remaining new templates in AI scripts; `generals.csf` (real designations, decision 3); balance passes | `AIData.ini`, `SkirmishScripts.scb`, `Data/English/generals.csf` | 5–11 days + playtests |
| **Art & Audio track** (parallel to 1–3) | See below and §7.3. Each asset ships behind its placeholder: swapping is a `Model =` / `ButtonImage =` change, so gameplay phases never wait on art. | Draw modules in the faction INIs; `MappedImages/TextureSize_512/MW_*.INI`; `SoundEffects.ini`, `FXList.ini`, `ParticleSystem.ini`; `Art/W3D`, `Art/Textures`, `Data/Audio/Sounds` in the mod `.big` | Tier A 39–47; Tier B 15–18; Tier C 5–8 days |

**Asset deliverables by phase**

| Phase | Asset deliverables |
|---|---|
| Phase 1 | Drone family A1–A5, jamming/HPM/spoof FX A14–A16 and the drone SFX: everything the §5 benchmark matrix needs to read on screen |
| Phase 2 | Vehicles and structures A6–A11, cage kits A12, decoy A13, faction texture sets A19, superweapons A20–A24, all Tier A cameos A18; Tier B B1–B4 |
| Phase 3 | General-specific B6–B8, B10; Tier C if time allows |

**Total**

| Scope | Developer-days |
|---|---|
| Gameplay phases 0–4 (incl. 0B engine, 0C SCB tool, superweapons, 4A/4B) | 74–115 |
| + Phase 0A spike | 6–9 |
| + Tier A art and audio | 39–47 |
| **Recommended scope (Tier A)** | **≈ 119–171** |
| + Tier B | 134–189 |
| + Tier C | 139–197 |

**Change vs rev 3:**
- +2–4: engine change.
- +4–6: superweapons.
- +2: general twists.
- +3–5: SCB tool, moved earlier.
- −2: Phase 4 net.
- +5–6: superweapon art.

With one developer the art is serial, so calendar time follows the totals. A second contributor on the art track recovers most of the art time.

**Milestone 1 (first release; decisions 4–5): rework in place, AI-first**
- **Contents:**
  - Phases 0, 0B, 0C and 1.
  - The in-place reworks of Phase 2, **including the three superweapons** (all in place).
  - Phase 4A AI.
  - Placeholders wherever art isn't ready, plus the Phase 0A proof asset.
- **New templates included:** only those the AI needs to stay competitive: FPV/Switchblade teams, MANPADS, and the LIDS / LY-1 sites. They are scripted into the AI in 4A.
- **Effort:** about 50–75 developer-days.

### 6.1 Phase 0B: `DAMAGE_TOP_ATTACK` engine change (in scope)

**Change**
1. `Core/GameEngine/Include/GameLogic/Damage.h:47-94`:
   - Append `DAMAGE_TOP_ATTACK` **after `DAMAGE_STATUS` and before `DAMAGE_NUM_TYPES`**.
   - No existing ordinal shifts:
     - ZH `STATUS` = 37, so `TOP_ATTACK` = 38.
     - Generals has the extra `DAMAGE_FLESHY_SNIPER` under `#if RTS_GENERALS`, so its value is one higher there; that is harmless because the name lookup is per build.
2. `Core/GameEngine/Source/GameLogic/System/Damage.cpp`:
   - Append `"TOP_ATTACK"` to `DamageTypeFlags::s_bitNameList[]` before `nullptr`.
   - The existing `static_assert(ARRAY_SIZE(s_bitNameList) == NumBits + 1)` enforces the sync at compile time.
3. Robustness (`@bugfix`, both games):
   - `Armor.cpp:99-100` (`m_damageCoefficient[dt] = pct` with `dt = -1` on an unknown name) and `DamageFX.cpp:165-168` (`damageFirst = getSingleBitFromName(...)`) do not check for −1.
   - Throw `INI_INVALID_DATA` instead. Weapon `DamageType` already throws through `INI::scanIndexList` (`BitFlagsIO.h:141-148`).
   - Retail data never takes this path.
4. Annotations, per AGENTS.md:
   - `// GeneralsX @feature <author> DD/MM/YYYY Add DAMAGE_TOP_ATTACK (appended; retail ordinals unchanged) for modern-warfare mod top-attack munitions`
   - `// GeneralsX @bugfix … Reject unknown damage type names instead of writing index -1`
5. Update `docs/DEV_BLOG/2026-10-DIARY.md`.

**Compatibility analysis (golden rules 6 and 7)**

| Path | Finding | Retail effect |
|---|---|---|
| Ordinals | Appended; every existing `DamageType` value is unchanged. Damage types are serialized as raw enum values (`Damage.cpp:143,147`, `ActiveBody.cpp:1585`) | none |
| Count-sized arrays | `Armor.h:65` `m_damageCoefficient[DAMAGE_NUM_TYPES]`; `DamageFX.h:125` `m_dfx[DAMAGE_NUM_TYPES][LEVEL_COUNT]`; loops `Armor.cpp:60,92`, `DamageFX.cpp:75,163`. `Default` lines also fill the new slot | Never read by retail, which never deals TOP_ATTACK |
| `DamageTypeFlags` (`BitFlags<DAMAGE_NUM_TYPES>`, `Damage.h:99`) | Save/load writes set bits **by name** (count + strings) (`BitFlagsIO.h:156-210`), so old saves load unchanged. XFER_CRC hashes the raw bitset (`sizeof(*this)`, or `sizeof(this)` under `RETAIL_COMPATIBLE_CRC`, `BitFlagsIO.h:213-221`). `std::bitset<38>`, `<39>` and `<40>` are all 8 bytes, verified with clang/libc++ arm64 on this machine; libstdc++ x86_64 also uses one 64-bit word. So the hashed size is unchanged | CRC bytes identical |
| ALL-flag masks | `DAMAGE_TYPE_FLAGS_ALL` / `parseDamageTypeFlags` defaults gain the new bit (`INI.cpp:1951-1981`, `BoneFXUpdate.cpp:66-68`, `TransitionDamageFX.cpp:76-78`, `FireWeaponWhenDamagedBehavior.h:67`). These are **module data**, are not xferred, and are only tested against incoming damage types | Behavior identical |
| The only instance-state damage mask that is xferred and CRC'd | `WeaponSet::m_totalDamageTypeMask` (`WeaponSet.cpp:303,344`) is built only from the damage types of equipped weapons; the new bit is never set by retail data | CRC identical |
| INI CRC | Computed over INI **text** (`INI.cpp:556-559`, `xferUser(m_buffer, strlen)`) | Retail INI CRC unchanged |
| EXE CRC | `GlobalData.cpp:1278-1332` hashes the executable (and has a fake CD-EXE CRC path), so any new build changes `m_exeCRC`. This is true of **every** GeneralsX build. It affects lobby/version matching between different builds, not retail-data determinism | Same as any engine update |

**Verdict:** retail-compatible. For retail data, the new build's game logic, INI CRC, save files and replay CRCs are byte-identical, so golden rules 6 and 7 hold. The only observable differences are the new accepted INI name, and a clean error instead of memory corruption for invalid names.

**Scope and backport.** The enum and name table are in **shared Core**, so the change lands in both Generals and Zero Hour automatically. `Armor.cpp` and `DamageFX.cpp` are per-game copies that need the same bounds-check fix. This fits the "shared Core → backport" rule.

**Upstream (decision 11: offer both).** Two separate TheSuperHackers PRs once Phase 0B lands in GeneralsX:
1. The bounds-check `@bugfix`: retail-safe, fixes memory corruption on any misspelled damage name, and stands on its own.
2. `DAMAGE_TOP_ATTACK` as a modding feature, with the §6.1 compatibility analysis as the PR rationale. If upstream declines it, it stays GeneralsX-only; the append-only change keeps sync-conflict risk low either way.

Splitting them lets the bugfix merge even if the feature is debated. Opening either PR is a separate, user-confirmed step.

**Test plan**
1. Build `macos-vulkan` and `linux64-deploy` (Docker) for both GeneralsXZH and GeneralsX. The `static_assert` must pass.
2. **Retail replay regression:** run the same set of retail ZH and Generals replays headless before and after (`-replay <file> -headless`, `CommandLine.cpp:444,1152,1158`), with `RTS_BUILD_OPTION_DEBUG=OFF`. Expect zero mismatches and identical final CRCs.
3. **Save round-trip:** load a pre-change save on the new build.
4. **Mod parse:**
   - A test INI with `TOP_ATTACK` loads on the new build.
   - The same INI on the old build is refused by the launch-script check.
   - An unknown damage name now raises `INI_INVALID_DATA`.
5. **Mod CRC:** two new-build clients run a mod-vs-mod LAN game and play back the replay with no mismatch.

---

## 7. Assets: Art & Audio track (in scope)

**Constraints**
- **No image generator.** Every asset is script-made:
  - Blender Python for geometry, rigid-bone animation and cameo renders.
  - Procedural textures.
  - Code-synthesized sound effects.
- **Expected quality by kind:**

| Kind | Expected quality |
|---|---|
| Hard-surface low-poly vehicles, drones and structures | Good |
| Simple bone animation (rotors, turrets, hatches, launch) | Good |
| Cameos rendered from the 3D models | Good |
| Synthesized SFX | Good |
| Procedural textures | Fair (plainer than retail) |
| New skinned infantry | Poor, so not made (§7.4) |
| Voice | Not made |

- **Placeholders keep everything working.** Every asset replaces a working retail placeholder that the design already uses (§3). Art improves readability and never gates function.

### 7.1 Pipeline

| Step | Tooling | Output | Notes |
|---|---|---|---|
| 1. Model and animate | Blender, headless (`blender -b -P build_<asset>.py`), one script per asset, parameterized (dimensions, palette, faction) | `.blend` (not shipped) | Rigid hierarchies only: every moving part is its own mesh on its own bone; no skinning |
| 2. Export | OpenSAGE Blender plugin (`github.com/OpenSAGE/OpenSAGE.BlenderPlugin`), run from the script | `.w3d` (HLOD + hierarchy + meshes; animation in the same file or a separate `.w3d`) | Pin one Blender version after the spike. Blender support is unconfirmed; last activity seen Feb 2024. **#1 risk** |
| 3. Validate | New Python chunk dumper/validator written from `Core/Tools/WW3D/pluglib/w3d_file.h`. The repo's `GeneralsMD/Code/Tools/wdump` is MFC (Windows-only) | Pass/fail report | Checks: version `W3D_CURRENT_VERSION` 3.0; chunks `W3D_CHUNK_HIERARCHY` (0x100), `W3D_CHUNK_MESH` (0x0), `W3D_CHUNK_HLOD`, `W3D_CHUNK_ANIMATION` (0x200) or the compressed variant; names ≤ 15 characters (`W3D_NAME_LEN 16`); bone names match the INI (§7.2) |
| 4. Textures | Python (PIL or numpy) procedural generator: base palette + panel lines + edge wear + grime, baked from Blender AO where useful | `.tga` (+ `.dds`) | The loader prefers DDS and converts from TGA when no DDS exists (`WW3D2/textureloader.cpp:249-257`). Ship TGA first and add DDS for size later. Power-of-two sizes |
| 5. House color | Model convention | — | The engine recolors meshes whose name (after the `.`) starts with `HOUSECOLOR`, and textures whose name starts with `ZHC` (`W3DAssetManager.cpp:883-905`). Plan: one `HOUSECOLOR…` mesh per model (stripes and panels) plus a neutral faction texture. INI keeps `OkToChangeModelColor = Yes` (1,319 retail uses) |
| 6. Cameos | Blender script renders each model with a fixed 3/4 camera, three-point light and faction-tinted backdrop. The new atlas packer writes `MW_UserInterface512_00N.tga` and a matching `MappedImages` INI | `Data/INI/MappedImages/TextureSize_512/MW_UserInterface512.INI` | Retail format verified (`SAUserInterface512.INI`): `MappedImage <Name>` / `Texture = …512_00N.tga` / `TextureWidth/Height = 512` / `Coords = Left Top Right Bottom` / `Status = NONE`. Button cameo = **60×48** (e.g. `SAPaladin` 187,101 → 247,149). Portrait `_L` = **120×96** (`SAPaladin_L`). Wired through Object `ButtonImage`/`SelectPortrait` and `CommandButton` `ButtonImage` |
| 7. FX | `ParticleSystem.ini` (1,087 retail systems; `Shader = ADDITIVE/ALPHA`; `ParticleName = <tga>`) + `FXList.ini` entries; procedural particle sprites | TGA sprites + INI | Pulses attach to the jammer/HPM weapon's `FireFX`, so they fire once per pulse, not per victim |
| 8. SFX | Python synthesis (numpy + scipy): oscillators, filtered noise, AM/FM, envelopes | **Mono** 16-bit PCM WAV, 44.1 kHz | `AudioSettings.ini`: `SoundsExtension = wav`, `OutputRate = 44100`, `OutputBits = 16`, `SoundsFolder = Sounds`. Retail `AudioEvent` fields: `Sounds` (variants), `Control = random/loop`, `Volume`, `VolumeShift`, `PitchShift`, `Limit`, `Priority`, `Type = world shrouded everyone`. GeneralsX decodes through FFmpeg, and positional sounds must be mono (§5 risk 10) |
| 9. Package | BIG packer (Phase 0) | `Art/W3D/*.w3d`, `Art/Textures/*.tga`, `Data/Audio/Sounds/*.wav` and INIs in `ModernWarfare.big` | Asset folder layout to be confirmed against a retail `.big` listing in Phase 0 |

**LOD and shadow decisions**
- One HLOD per model, plus a `_D` damaged variant for vehicles and structures. This copies the retail pattern `AVPaladin` / `AVPaladin_D` with `ConditionState = REALLYDAMAGED` and `RUBBLE`.
- Optional detail sub-objects are hidden at low LOD with `MinLODRequired = MEDIUM` (180 retail uses).
- Shadows: `SHADOW_VOLUME` for vehicles and structures (1,226 retail uses) and `SHADOW_DECAL` for small drones (177 retail uses).
- Volume shadows probably need clean closed meshes. This is not verified in code, so it is a spike check.

**Polycount and texture targets** (design targets, not measured from retail):

| Asset class | Triangles | Texture |
|---|---|---|
| Drones | ≤ 300 | 128² |
| Vehicles | 1.5–3k | 256² |
| Structures | 2–4k | 512² |

### 7.2 Engine requirements every model must meet (grounded)

| Requirement | Source | Applies to |
|---|---|---|
| **Weapon bones are numbered.** The INI gives a base name (`WeaponFireFXBone = PRIMARY TurretMS`), and the engine looks up `TurretMS01…TurretMS99` (also for `WeaponRecoilBone`, `WeaponMuzzleFlash`, `WeaponLaunchBone`) | `W3DModelDraw.cpp:755-795` (`%s%02d`) | Every armed model (A6–A11, B1–B5) |
| **Turrets.** `Turret = <bone>`, `TurretPitch = <bone>` (720 / 311 retail uses; e.g. the Overlord add-on uses `TURRET01` / `TURRETEL`); `TurretArtAngle` when the art faces backwards | retail Draw modules | A6, A7, A9–A11 |
| **Tracks.** `W3DTankDraw` scrolls sub-meshes whose name starts with `TREADS` and that use a linear-offset UV mapper | `W3DTankDraw.cpp:244-262` | A6, B6 |
| **Wheels.** `W3DTruckDraw` supports up to 10 wheels: `LeftFront`/`RightFront`/`LeftRear`/`RightRear` plus `MidLeftFront`, `MidRightFront`, `MidLeftRear`, `MidRightRear`, `MidLeftMid`, `MidRightMid` tire bones; also `CabBone`/`TrailerBone` | `W3DTruckDraw.cpp` field table | A8 (8×8 Stryker), B7 |
| **Looping rotor and radar animation.** `Animation = <HLOD>.<HLOD>` + `AnimationMode = LOOP` | retail `AVBATTLEDR` / `AVComanche` | A1–A5, A10, B1–B3 |
| **Visible upgrade parts, option 1: sub-objects.** `SubObjectsUpgrade` → `ShowSubObjects = <mesh>` | retail `GLAVehicleBombTruck` (`Bombload02`), `ChinaVehicleHelix` | B8 workshop kit, A1 fiber spool (alternatively `ShowSubObject` in `ConditionState = WEAPONSET_PLAYER_UPGRADE`; 79 retail states do this) |
| **Visible upgrade parts, option 2: attached draw module (cages on *retail* models without re-export).** Add a second `Draw = W3DModelDraw` with `AttachToBoneInAnotherModule = <bone>` (retail: Technical gunner on `Dum_Turret`), `DefaultConditionState` `Model = NONE` (2,649 retail uses of `NONE`), and `ConditionState = USER_1` with `Model = MWCage…`. Then `ModelConditionUpgrade` with `ConditionFlag = USER_1`, `TriggeredBy` = the cage upgrade (retail precedent: `ModelConditionUpgrade` + `ConditionFlag` on `GLAPalace`). `USER_1`/`USER_2` are used only by structures in retail (Palace, Internet Center), so they are free on vehicles | `BitFlags.cpp:170-171`; retail INIs | A12 |
| **Damage and construction states.** Vehicles: `REALLYDAMAGED`, `RUBBLE` (alias). Structures: copy the full condition-state set of the placeholder they replace (`ABPatriot`, `NBGattling`, `NBNMissle`), including construction and damage states | retail Draw modules | A10, A11, B5 |
| **Names.** HLOD, mesh, bone and hierarchy names ≤ 15 characters; models prefixed `MW` | `w3d_file.h:317` | all |
| **Cameo limit.** At most 5 `UpgradeCameoN` per unit; `GLATankScorpion`, `ChinaTankOverlord`, `AmericaTankPaladin` and `AmericaVehicleHumvee` already use 5 | retail INIs | A7, A12 |

### 7.3 Per-asset table

Effort is in developer-days and includes the model, animation, texture and in-game hookup. Cameos are counted in A18. About 20 % integration and QA is added to the tier totals in §6.

**Tier A: core readability**

| # | Asset | Replaces placeholder | Type | Complexity | Quality | Engine requirements | Effort | Phase |
|---|---|---|---|---|---|---|---|---|
| A1 | FPV quadcopter `MWFPVQuad` (+ fiber-spool sub-object) | `AVScoutDr` at Scale 0.5 | model, anim, texture | Low | Good | 4 rotor bones in a loop animation; `HOUSECOLOR` arm mesh; spool via `ShowSubObject` under `WEAPONSET_PLAYER_UPGRADE`; `Shadow = NONE`/decal | 1.5 | **0A proof asset**, then 1 |
| A2 | Recon quadcopter `MWRecon` (camera gimbal) | `AVScoutDr` | model, anim | Low | Good | Shares A1 parts; optional IR particle bone (`IRParticleSysBone` field exists on `StealthDetectorUpdate`) | 0.5 | 1 |
| A3 | Loitering munition `MWLoiter` (tube-launched, wing-unfold) | `AVHellDrone` | model, anim | Low | Good | One-shot unfold animation; projectile, no `_D` | 0.75 | 1 |
| A4 | One-way attack drone `MWOneWay` (delta, pusher prop) | `AVHellDrone` | model, anim | Low | Good | Prop loop | 0.75 | 1 |
| A5 | Interceptor `MWCoyote` | (none: hitscan fallback) | model | Low | Good | Projectile; exhaust from the weapon's `ProjectileExhaust` | 0.5 | 1 |
| A6 | Type 100 MBT `MWType100` + `_D` | `NVOvrlrd` | model, anim, texture | Med–High | Good | `W3DOverlordTankDraw` rider/add-on attach bones **must match `NVOvrlrd`'s** (import the retail model to read them); `TREADS*`; `Turret`/`TurretPitch`; numbered muzzle bones | 3.0 | 2 |
| A7 | Type 100 add-ons: C-UAS turret, data-link mast, loitering pod, **APS module (re-added)** | `NVOvrlrd_G` etc. | model ×4 | Low–Med | Good | `TURRET01`/`TURRETEL`, numbered fire bones; 4th add-on in `ChinaTankOverlordDefaultCommandSet` slot 7 (free). The cameo row is full, so the APS shows no cameo | 2.0 | 2 |
| A8 | Stryker 8×8 hull `MWStryker` + `_D` | `AVHUMMER`, `AVAVNGER`, `AVTHUNDRBLT` | model, texture | Medium | Good | `W3DTruckDraw` with 8 tire bones (`Tire01–08`), each assigned to one of the 8 corner and mid wheel fields (`LeftFrontTireBone` … `MidRightRearTireBone`; the INI names the bones); `TrackMarks = EXTireTrack.tga` | 2.0 | 2 |
| A9 | Stryker kits: M-SHORAD turret (30 mm + Stinger box), SIGINT masts, Leonidas HPM emitter | — | model ×3 | Low–Med | Good | M-SHORAD: `Turret`/`TurretPitch`, `MuzzleFX01`, launch bones `MissileLaunch01–04`. Kits are full models (hull + kit) exported from the same script with different `Model =` names | 2.25 | 2 |
| A10 | LIDS site `MWLIDSSite` (radar + Coyote launcher) | `ABPatriot` | model, anim | Medium | Good | Structure construction/damage states copied from `AmericaPatriotBattery`'s Draw; radar loop; launcher turret + numbered launch bones | 2.0 | 2 |
| A11 | LY-1 laser site `MWLY1Site` | `NBGattling` | model | Medium | Good | Structure states copied from `ChinaGattlingCannon`; turret; fire bone as the `W3DLaserDraw` beam origin | 2.0 | 2 |
| A12 | Cage/slat kits: Type 99A, Type 100, ZBD-04A, PGL-625 (China); scrap cages: Scorpion, Marauder/Technical (GLA) | (invisible upgrade) | model ×6 | Low | Good | Attached draw + `USER_1` (§7.2). Attach bones on *retail* models must be read by importing them; hull cages need a hull-level bone (unverified) | 3.0 | 2 |
| A13 | Inflatable decoy `MWDecoyTank` | `UVLiteTank` | model, anim | Low | Good | Puffy silhouette; deflate animation under `DYING`/death state | 0.75 | 2 |
| A14 | Jamming-bubble FX | (retail particles) | FX | Low | Good | Expanding ring sprite + faint dome, via `FireWeaponUpdate`'s weapon `FireFX` | 1.0 | 1 |
| A15 | HPM pulse + Spoof & Jam field FX | (retail GPS Scrambler FX) | FX | Low | Good | As A14 | 0.75 | 1 |
| A16 | Drone FX: rotor dust, FPV impact, jammed sparks (`JAMMED` model condition on projectiles), interceptor trail | — | FX | Low | Good | `ParticleSysBone` in condition states (21k retail uses) | 1.0 | 1 |
| A17 | SFX set (~12 sounds; list below) | retail sounds | SFX | Low | Good | Mono WAV; new `AudioEvent`s with `Limit` | 3.0 | 1–2 |
| A18 | Cameos: ~30 new (unit button + portrait; upgrade and power buttons) | retail cameos | cameo | Low | Good | 60×48 / 120×96 cells in a mod-owned 512² atlas (§7.1 step 6) | 4.5 | 1–3 |
| A19 | Faction texture sets (US, PLA, GLA palettes, wear masks) | — | texture | Low | Fair | Power-of-two TGA/DDS | 2.0 | 1–2 |

| A20 | **Dark Eagle battery `MWDarkEagle`** (all USA; was B5) | `ABSDILink` (retail uplink) | model, anim | Med–High | Good | Copy `ChinaNuclearMissileLauncher`'s `MissileLauncherBuildingUpdate` door/launch state set; erector-raise animation; numbered launch bones; construction and damage states | 3.0 | 2 |
| A21 | Hypersonic glide vehicle + booster projectiles | — | model | Low | Good | `MissileAIUpdate` projectile; exhaust via weapon `ProjectileExhaust` | 0.75 | 2 |
| A22 | DF-26-class warhead + submunition projectiles | retail neutron missile | model | Low | Good | As A21 | 0.75 | 2 |
| A23 | GLA decoy launch site (fake structure) | — | model | Low | Good | Reuses the A13 inflatable pipeline; structure states minimal (fake-building precedent) | 0.75 | 2 |
| A24 | Synthesized superweapon alarm tones (detect, ready, launch × 3 types) + Tier A superweapon cameos | retail EVA voice lines (USA/China) | SFX, cameo | Low | Good | New `AudioEvent`s referenced from `Eva.ini`; cameos as A18 | 0.8 | 2 |

**Tier A sum:** about 38 days (rev 3: 32.75, plus A20–A24) + 20 % integration ≈ **39–47 days**.

**Tier B: identity polish**

| # | Asset | Replaces placeholder | Type | Complexity | Quality | Engine requirements | Effort | Phase |
|---|---|---|---|---|---|---|---|---|
| B1 | CCA jet `MWCCA` | `AVStealth` at Scale 0.8 | model | Medium | Good | Copy the condition-state set from `AmericaJetStealthFighter`'s Draw; numbered launch bones | 1.5 | 2 |
| B2 | GJ-11 flying wing `MWGJ11` | `NVMIG` at Scale 0.85 | model | Medium | Good | As B1, from `ChinaJetMIG` | 1.5 | 2 |
| B3 | Drone mothership `MWMothship` | `NVCargoPln` | model, anim | Medium | Good | Copy the delivery-plane states and bones from the cargo plane (payload door if present) | 1.5 | 2 |
| B4 | Type 100 Support Vehicle `MWT100Sup` | `NVLOUTPOST` | model, anim | Medium | Good | VLS hatch animation; 30 mm turret; `TREADS*` | 2.5 | 2 |
| B5 | *(moved to Tier A as A20 in rev 4)* | | | | | | | |
| B6 | Armed UGV `MWUGV` (Infantry General, **re-added unit**) | — | model | Low | Good | `W3DTankDraw`, `TREADS*`, one turret | 1.5 | 3 |
| B7 | Explosive UGV `MWExplUGV` (Demo General) | `Demo_GLAVehicleBombTruck` | model | Low | Good | `W3DTruckDraw` 4 wheels | 1.0 | 3 |
| B8 | Workshop-van kit (drone racks, antennas) | `UVRadarVan` | model | Low | Good | Attached draw on a `UVRadarVan` bone (§7.2 option 2) | 0.75 | 2 |
| B9 | Tier B cameos (~8) | — | cameo | Low | Good | As A18 | 1.2 | 2–3 |
| B10 | China DF-26 TEL pad `MWDF26Pad` (optional replacement for the retail silo) | `NBNMissle` | model, anim | Medium | Good | Superweapon door/launch states copied from `ChinaNuclearMissileLauncher`'s Draw; erector-raise animation | 2.5 | 3 |

**Tier B sum:** ≈ 14.7 days (B5 moved out, B10 added) + 20 % ≈ **15–18 days**.

**Tier C: stretch**

| # | Asset | Type | Complexity | Quality | Notes | Effort |
|---|---|---|---|---|---|---|
| C1 | Robot-dog squad (Infantry General, `Infa_ChinaWarFactoryCommandSet` slot 2) | model, walk animation | High | **Fair–uncertain** | Rigid four-legged walk cycle, driven by locomotor `ConditionState = MOVING` | 3.0 |
| C2 | HIMARS truck | model | Medium | Good | Replaces `AVTomahawk`; 6 wheels | 2.5 |

**Tier C sum:** ≈ **5–8 days**.

**SFX list (A17)**

| Sound | Character |
|---|---|
| Small-drone buzz | Loop |
| FPV dive whine | — |
| Loitering-munition launch pop | — |
| Wing-unfold click | — |
| One-way drone engine | Loop, moped-like |
| Interceptor launch | — |
| Jammer hum | Loop, per pulse |
| HPM zap | — |
| Laser C-UAS burst | — |
| Jammed-drone sputter | — |
| Mothership drone release | — |
| Cage-fit clank (upgrade complete) | — |

All units keep **retail voice sets and EVA lines**. The exceptions are the USA and China superweapon events, whose retail lines name the particle cannon and nuke. Those are remapped to A24 alarm tones; GLA keeps its Scud Storm lines.

### 7.4 Won't make (and what we do instead)

| Won't make | Why | Instead |
|---|---|---|
| New skinned infantry (rigs, skins, animations) | Script-made skinned characters are poor quality | Reuse retail infantry models and skins (`NITHNT_SKN`, `NIMSST_SKN`, `UITRST_SKN`, …). Distinguish teams with attached props and cameos |
| Voice acting or TTS | No cloning of the original actors, and TTS sounds wrong | Reuse retail unit voices and EVA. Example: the FPV Pilot uses Rebel/RPG voices instead of the Terrorist's suicide lines |
| Cinematics, Bink/FFmpeg videos, campaign or challenge content | Out of scope | Skirmish/MP only. `GC_*` objects stay loadable but unsupported |
| Music, loading screens, general portraits, shell UI layout | Low value for the cost | Retail |
| Remodels of units whose retail model still reads correctly (Type 99A = `NVBtMstr`, PHL-16 = `NVInferno`, TBM TEL = `NVNukeCn`, Type 5 = `NVBANSHEE`, ISV = `AVHUMMER`) | Retail art is fine | Rename and recolor through CSF only |
| Retextures of retail models | Texture names are embedded in each W3D, and there is no Draw-level texture override (Appendix A) | Only through the W3D path: a chunk-aware texture-name patch on *copied* retail W3Ds (fallback, §7.5) |

### 7.5 Phase 0A spike: go/no-go

- **Timebox.** 5 working days, inside the 6–9-day Phase 0A budget.
- **Proof asset.** A1 FPV quadcopter plus its cameo, loaded through a throwaway test `-mod` and screenshotted in-game.

**Go only if all of these pass**

| # | Criterion |
|---|---|
| 1 | The plugin installs into one pinned Blender version on macOS and exports headless from a script |
| 2 | The exported `.w3d` passes the Python chunk validator (§7.1 step 3) |
| 3 | A GeneralsXZH **debug** build loads it through the test `-mod` with no assert. Scale and orientation are correct, the rotor loop plays, house color applies and the shadow renders. Logs are captured to `logs/` |
| 4 | The cameo appears in the command bar from the new `MappedImages` INI |
| 5 | A generated mono WAV plays positionally through a new `AudioEvent` |
| 6 | The screenshot is archived with the spike report |

**No-go fallback, in order**
1. **Own exporter.** A minimal Python W3D writer covering only the chunks the validator checks (mesh, hierarchy, HLOD, uncompressed animation), from `w3d_file.h` and the reference exporter source in `Core/Tools/WW3D/max2w3d`. A second go/no-go after 8–12 days; this adds to Phase 0A.
2. **No new geometry.** Ship placeholders plus everything that does not need W3D export:
   - FX (A14–A16), SFX (A17), and cameos made as 2D procedural icons or edits of retail cameos.
   - Retexture-only variants through chunk-aware texture-name patches on copied retail W3Ds.
   - Tier A shrinks to about 12–16 days.
   - Cages, Stryker and Type 100 stay placeholders; readability suffers but gameplay is unchanged.

---

## 8. Appendices

### Appendix A: Requires engine work (C++)

`DAMAGE_TOP_ATTACK` moved out of this appendix into in-scope Phase 0B (§6.1). The items below remain out of scope.

| Need | Why data can't do it | Rough change |
|---|---|---|
| Drone-only EMP | `EMPUpdate` parses `VictimRequiredKindOf`/`VictimForbiddenKindOf`, but the filter is commented out (`EMPUpdate.cpp:233-236`). It kills only airborne `AIRCRAFT`. | Re-enable the filter. Retail INIs never set it, so it is retail-safe. |
| Real ammunition and stockpiles | No ground rearm; no per-shot cost | A rearm-dock module, or a per-fire player resource |
| PDL magazines | A fresh weapon is created per shot, so clips are ignored | Keep a persistent `Weapon` in the module |
| Upgrade-gated APS | `PointDefenseLaserUpdate` has no upgrade mux | Add `TriggeredBy`/`StartsActive` |
| Weapons limited by target KindOf | Only PDLs filter by KindOf | `RequiredTargetKindOf` on `WeaponTemplate` |
| Drone link range, line of sight, weather vs lasers, home-on-jam | No hooks | New statuses and bonus conditions |
| EVA cues for new powers | EVA branches are hard-coded by enum in `SpecialPowerModule` | Make EVA events data-driven per power |
| AI understanding of drones and EW | No concept of "jammed" or "needs C-UAS" | AI heuristics or more script vocabulary |
| More than 5 upgrade cameos | Fixed UI arrays (`GLATankScorpion`, `ChinaTankOverlord`, `AmericaTankPaladin` and `AmericaVehicleHumvee` already use all 5) | UI work |
| Per-object texture override (retexture without re-exporting W3D) | `ObjectReskin`/Draw have no texture-swap field; texture names are embedded in the W3D | A `TextureOverride` field on `W3DModelDraw` |

Appendix B ("Stretch: needs new art") moved into §7 as the in-scope Art & Audio track.

---

## 9. Decisions (rev 4)

| # | Decision (user) | Consequence in this proposal |
|---|---|---|
| 1 | Realism vs balance: **70/30** | Tuning values stay design choices; armor stays reasonably durable (4–7 FPV hits per MBT) |
| 2 | Superweapons: **modernize all three** | Dark Eagle, DF-26 complex and Scud Storm launch complex, all reworked in place (§2.h); Nuke General keeps the single nuclear holdout (§4) |
| 3 | Names: **real designations** | `generals.csf` uses Abrams, HIMARS, Type 100, PHL-16, Dark Eagle, DF-26, …; GLA stays fictional |
| 4 | Audience: **skirmish/AI first** | Phase 0C SCB tool moved up; Phase 4A AI work ships in Milestone 1 |
| 5 | First release: **rework-in-place milestone** | Milestone 1 = in-place reworks + superweapons + engine change + 4A AI (≈ 50–75 days) |
| 6 | TOP_ATTACK: **engine change** | Phase 0B; mod needs a TOP_ATTACK-capable GeneralsX build; `LAND_MINE` workaround removed |
| 7 | Art style: **match 2003 retail** | Saturated, painterly palettes and chunky silhouettes in the §7 procedural textures and render rig |
| 8 | External art: **none** | Script-made only; Tier A/B/C unchanged as the plan of record |

**Follow-up decisions (the questions rev 4 raised, now answered)**

| # | Question | Decision | Consequence |
|---|---|---|---|
| 9 | Nuke General holdout | **Keep the nuke** | Tao keeps the single nuclear strike (DF-5C/DF-31-class) via `NUKE_SPECIAL_NEUTRON_MISSILE`, as in §2.h and §4 |
| 10 | Superweapon lobby default | **Keep the retail default** | No change to the lobby superweapon restriction; all three superweapons keep `MaxSimultaneousLinkKey = Superweapon` |
| 11 | Upstreaming | **Offer both** | After Phase 0B lands, propose two TheSuperHackers PRs: the bounds-check `@bugfix`, then `DAMAGE_TOP_ATTACK` as a modding feature (§6.1). Opening the PRs is a separate, user-confirmed step |

No open questions remain.

---

## Revision log (rev 2, responding to `PROPOSAL-REVIEW.md`)

| Review # | Severity | Change made |
|---|---|---|
| 1 | blocker | Removed every area `SUBDUAL_VEHICLE` pulse and the "heal rate > pulse rate" argument (healing resets on each hit, per `SubdualDamageHelper`). Soft-kill is now `SUBDUAL_MISSILE` only: an audit of all 53 armors shows only projectile armors take it, so drones get it through new armors and vehicles, infantry and structures are immune by construction. The retail `ECMTankMissileJammer` now jams drones without a new module. HPM hard-kill moved to `HAZARD_CLEANUP` (only hazard fields take it). The USA EW vehicle was dropped. |
| 2 | blocker | Found a retail **mobile** spawner precedent (`GLAInfantryAngryMobNexus`: `SpawnBehavior` + `QueueProductionExitUpdate` + `ExitByBudding`). Carriers now ship a **certain tier** (drones fired as projectile weapons) and a **prototype tier** (vehicle spawner). The Air Force CCA jet-spawner was dropped; CCAs are Airfield-built. The wrong Fire Base precedent was removed. |
| 3 | major | Dropped `AURORA_BOMB` (15 explicit armors, structures 80–300 %). TOP_ATTACK is now `LAND_MINE`, audited: structures 0 % (no FPV sieging), dozer/HazMat/mine 0 %. Side effects are listed (risk 5, OQ 5, Appendix A). The B-21 keeps `AURORA_BOMB`, so there is no nerf. |
| 4 | major | Removed "Drone Production" (`CostModifierUpgrade` doesn't affect upgrade prices or `ObjectCreationUpgrade` drones). War Factory is now 13/14. |
| 5 | major | MANPADS Team is a new Object (`MW_GLAInfantryMANPADS`) copied from the Stinger soldier, with `CLICK_THROUGH`, `SPAWNS_ARE_THE_WEAPONS` and `SlavedUpdate` removed. Effort raised. |
| 6 | major | Decoy is now a Barracks-produced unit (not worker-built), marked prototype first. The retail fake-structure system is now explicitly part of the GLA deception identity. |
| 7 | major | Rebuilt the drone, EW, C-UAS and armor-protection toolkits asymmetrically (§2.a–2.d tables) and added the §3.0 identity table. USA: no jammer, no cages, no FPV mass. China: the only mobile jammer, no fiber, HPM or interceptors. GLA: fiber-optic exclusive, decoys, guns-only C-UAS. Base GLA keeps the Toxin Tractor, Toxin Shells and Anthrax; the Demo General keeps the Terrorist. |
| 8 | major | FPV range cut 300 → 200 (equal to the team's own vision; fiber 170); single spotter rule (only beyond 200). Noted that retail USA PDLs already kill FPVs (intended). Added the benchmark matrix as a pre-Phase-2 gate. |
| 9 | minor | Alt-PNT is now an `ArmorUpgrade` on US drones and projectiles (projectile upgrade precedent: `TunnelDefenderMissile`). Retail `Upgrade_AmericaDroneArmor` stays a `MaxHealthUpgrade`. Removed the "subdual cap ×3" claim. |
| 10 | minor | Jam resistance is modeled by time-to-jam or immunity, not scatter distance. FPV `RadiusDamageAffects = ENEMIES NEUTRALS`. |
| 11 | minor | FPV/drone armors list gun damage types at 100–150 % explicitly. Hits-to-kill table added per C-UAS tier. |
| 12 | minor | EVA gap documented (§2.g); mitigated with visible slow delivery and a public timer for Dark Eagle. Appendix A item added. |
| 13 | minor | Fixed names: `Chem_Upgrade_GLAAnthraxGamma`, `Tank_Upgrade_ChinaTankAutoLoader`, `Upgrade_ChinaIsotopeStability`, `Demo_Upgrade_SuicideBomb`, `Slth_SuperweaponGPSScrambler`. All re-grepped against retail. |
| 14 | minor | Fiber range is a balance choice; Leonidas efficacy is a design assumption; Type 4/5 is design-level only. |
| 15 | minor | (a) `ScanRange` > weapon range stated per PDL and added to the Phase 1 checklist. (b) Airfield parking (4) noted for CCA and GJ-11. (c) Rank-3 science slots budgeted (China becomes full at 15/15). (d) No `ScanRate 0`; Commercial Drones limited to Technicals and Kell. (e) Dark Eagle cut-content smoke test. (f) Kill Chain switched from a `DRONE_SPOTTING` aura to a per-weapon `PLAYER_UPGRADE` bonus. |

## Revision log (rev 3: Art & Audio track)

| Request | Change made |
|---|---|
| Promote Appendix B to an in-scope track | New §7 "Assets: Art & Audio track", grounded in the retail INIs and the engine code:<br>• Pipeline (§7.1).<br>• Engine requirements (§7.2): numbered bone lookup `%s%02d`, `TREADS*` meshes, `HOUSECOLOR*` meshes and `ZHC*` textures, 15-character W3D names, `AttachToBoneInAnotherModule`, `USER_1`/`USER_2`, the 60×48 / 120×96 cameo cells in 512² atlases, the WAV/mono rule.<br>• A per-asset table with tiers (§7.3).<br>• A won't-make list (§7.4).<br>• The Phase 0A go/no-go spike and fallback (§7.5).<br>Appendix B was removed; Appendices became §8 and Open questions §9. |
| Recover design lost to placeholders | Restored or visualized:<br>• Stryker family (M-SHORAD, SIGINT, Leonidas).<br>• LIDS site with visible Coyote interceptors.<br>• Modeled Type 100 with a re-added 4th add-on, the APS module (Overlord command set slot 7 is free).<br>• Visible cages and slats (`ModelConditionUpgrade` `USER_1` + attached draw module).<br>• Inflatable decoy.<br>• Jamming, HPM and spoof FX.<br>• Infantry General Armed UGV, re-added in `Infa_ChinaWarFactoryCommandSet` slot 1 (now 12/14).<br>• Robot dog kept as Tier C.<br>No other button budget changed. |
| Implementation plan | Added Phase 0A (spike, capped at 5 days, go/no-go, fallback), a parallel art track with per-phase deliverables, and new totals: about 105–150 developer-days for Tier A scope (119–168 with Tier B). |
| Summary, risks, questions | Added pillar 7 "Readable at a glance". Added risks 8–10 (pipeline, readability, FX/audio performance and mono positional audio). Added open questions on art style and external art; three earlier questions were merged to stay at 6. |
| Also found | Cameo display is already full (5) on Scorpion, Overlord, Paladin and Humvee, so new upgrades there work but show no cameo. The FPV Pilot keeps the Terrorist skin but needs retail Rebel voices instead of the suicide lines. Appendix A gained a texture-override entry. |

## Revision log (rev 4: user decisions)

| Area | Change made |
|---|---|
| Open questions | §9 replaced by "Decisions (rev 4)", with a consequence for each of the 8 decisions and 3 new open questions |
| Superweapons | New §2.h with in-place modern conventional replacements:<br>• USA: Dark Eagle LRHW battery.<br>• China: DF-26 conventional strike complex.<br>• GLA: Scud Storm dispersed launch complex.<br>Same templates, enums, link key and public timers, so detect/launch EVA (enum-keyed, `SpecialPowerModule.cpp:563-610`, `Player.cpp:1663`), targeting (`ActionManager.cpp:1534-1554, 1800-1821, 1929-1951`) and AI scripts keep working. USA and China EVA audio is remapped to synthesized alarms; GLA keeps its lines. Warheads are `BALLISTIC_MISSILE` and interceptable (retail neutron missiles are `UNATTACKABLE`, and Scud Storm missiles had `BALLISTIC_MISSILE` commented out). |
| Superweapon knock-on | • All general copies share the base power templates (verified), so they modernize together.<br>• Superweapon General: Typhon second system.<br>• Laser General: the laser cannon was always cut content, which corrects the rev-3 text.<br>• Nuke General: single nuclear holdout via `NUKE_SPECIAL_NEUTRON_MISSILE`, with justification.<br>• Boss/GC: mirrored, unsupported.<br>• China gun/missile sites gain low-rate ballistic interception. |
| TOP_ATTACK | The `LAND_MINE` workaround is removed everywhere (§2.0, §2.d armors, risks, Phase 1 checklist, Appendix A). New explicit `TOP_ATTACK` armor profile. **The rev-3 caveats no longer apply:** dozers and CBRN techs are vulnerable to FPVs, and cages do not reduce mine damage. |
| Engine change | New §6.1 / Phase 0B covering:<br>• Append-only enum and name table.<br>• Bounds-check bugfix for an unknown-name index −1 write in `Armor.cpp:99-100` / `DamageFX.cpp:165-168` (found in this revision).<br>• Compatibility analysis with file:line evidence (ordinals, arrays, name-based save xfer, CRC bitset size verified at 8 bytes, INI CRC over text, EXE CRC note).<br>• Verdict: retail-compatible.<br>• Shared Core, so both games; per-game copies need the bugfix.<br>• Upstream stance and test plan. |
| Data-only claims | Header, executive summary, packaging and compatibility now state the minimum-build requirement and that retail executables and other engines cannot run the mod. |
| AI-first | Phase 0C SCB tool moved up; Phase 4 split into 4A (Milestone 1: C-UAS sites, drone teams, MANPADS, superweapon checks) and 4B. |
| Assets | B5 → A20 (Tier A, all USA); added A21–A24 and B10. Tier A 39–47, Tier B 15–18. |
| Effort | Gameplay 74–115; recommended scope (Tier A) ≈ 119–171; +Tier B 134–189; +Tier C 139–197; Milestone 1 ≈ 50–75. |
| Follow-up decisions | The three questions rev 4 raised are answered (§9, decisions 9–11): the Nuke General keeps the nuke; the lobby superweapon restriction keeps its retail default; both the bounds-check bugfix and `DAMAGE_TOP_ATTACK` will be offered upstream as separate PRs (§6.1). No open questions remain. |
