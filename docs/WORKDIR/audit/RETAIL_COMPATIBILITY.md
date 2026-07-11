# Retail Compatibility Macros Analysis

This document provides an overview of the various `RETAIL_COMPATIBLE_*` macros defined in `Core/GameEngine/Include/Common/GameDefines.h`. It explains what each macro does and outlines the advantages and disadvantages of setting them to `0` (ignoring retail compatibility).

Since the primary goal for this fork is *not* retail compatibility, it is highly recommended to disable these macros where applicable to benefit from the numerous engine fixes.

---

## 1. `RETAIL_COMPATIBLE_CRC`
**Definition:** Game is expected to be CRC compatible with retail Generals 1.08, Zero Hour 1.04.

* **What it does:** The original game relies on a deterministic frame-by-frame simulation (CRC checks) to ensure all players in a multiplayer match (and replays) stay synchronized. Many legacy bugs (like uninitialized variables, floating-point inconsistencies, and logic errors in particles/physics) were left untouched because fixing them would change the CRC and cause "Sync Errors" when playing against players using the original CD/Origin release.
* **Advantage of disabling (`0`):** Enables hundreds of bug fixes across the engine. Fixes undefined behaviors, logic errors, and makes the simulation much more robust.
* **Disadvantage of disabling:** Breaks multiplayer cross-play and replay compatibility with the original retail game.

---

## 2. `RETAIL_COMPATIBLE_XFER_SAVE`
**Definition:** Game is expected to be Xfer Save compatible with retail Generals 1.08, Zero Hour 1.04.

* **What it does:** Governs the serialization formats used for saving the game and transferring assets (e.g., when a player disconnects and their units are transferred to an ally).
* **Advantage of disabling (`0`):** Allows fixing struct alignments, memory layouts, and data serialization bugs. This improves memory safety and prevents crashes related to corrupted save states or network transfers.
* **Disadvantage of disabling:** Old retail save files will no longer load correctly, and transferring units with retail clients will crash or desync.

---

## 3. `RETAIL_COMPATIBLE_PATHFINDING`
**Definition:** Toggles between the retail-compatible pathfinding with its fallback mechanics and the pure fixed pathfinding mode.

* **What it does:** The original pathfinding system had "fallback" crutches that would trigger when units got stuck. These fallbacks were often inefficient and led to the infamous "dancing tanks" issue.
* **Advantage of disabling (`0`):** Activates a purely fixed pathfinding mode without the legacy crutches. Units move much more intelligently, reliably, and get stuck far less often.
* **Disadvantage of disabling:** Pathfinding directly affects unit movement, so disabling this will completely break replay synchronization for any retail replay.

---

## 4. `RETAIL_COMPATIBLE_PATHFINDING_ALLOCATION`
**Definition:** Toggles between the retail-compatible pathfinding memory allocation and the new static allocated data mode.

* **What it does:** Retail pathfinding constantly allocates and deallocates small chunks of memory on the heap every frame. This causes severe memory fragmentation, CPU overhead, and eventual memory leaks over long games.
* **Advantage of disabling (`0`):** Switches the pathfinder to use statically allocated memory pools. This provides a massive performance boost during heavy unit movement and eliminates related memory fragmentation/leaks.
* **Disadvantage of disabling:** Minor determinism changes in how paths are resolved could theoretically affect sync, but practically, it just breaks retail replay perfection.

---

## 5. `RETAIL_COMPATIBLE_CIRCLE_FILL_ALGORITHM`
**Definition:** Uses the original circle fill algorithm, which is more efficient but less accurate.

* **What it does:** Determines how the game calculates circular areas (used for shroud clearing, sight ranges, and area-of-effect damage).
* **Advantage of disabling (`0`):** Uses a more accurate mathematical circle algorithm. Fixes bugs where units on the very edge of an explosion wouldn't take damage, or sight ranges weren't perfectly round.
* **Disadvantage of disabling:** Slightly heavier on the CPU (negligible on modern hardware) and changes AoE/Sight determinism, breaking retail replays.

---

## 6. `RETAIL_COMPATIBLE_NETWORKING`
**Definition:** Disables non-retail fixes in the networking, such as putting more data per UDP packet.

* **What it does:** The retail netcode has a critical bug where command IDs (16-bit integers) overflow after 65,535 commands, causing the game to discard new packets and disconnect players in long matches. It also limits packet payload to 476 bytes.
* **Advantage of disabling (`0`):** Fixes the command ID overflow bug, completely eliminating the "long game disconnect" issue. It also increases the maximum UDP payload to 1100 bytes, which reduces network fragmentation and CPU overhead during intense battles.
* **Disadvantage of disabling:** Packets become incompatible with retail clients (they will drop the larger packets or fail to parse them).

---

## 7. `RETAIL_COMPATIBLE_AIGROUP`
**Definition:** AIGroup logic is expected to be CRC compatible with retail. Guards against fixing memory safety issues that would alter CRC.

* **What it does:** The `AIGroup` logic in the original game is notoriously unstable, riddled with use-after-free errors, double-frees, and memory leaks. Fixing these safely alters how AI decisions are processed.
* **Advantage of disabling (`0`):** Allows the engine to use robust memory-safety fixes (often described as "massive hacks" in the codebase) to prevent the AI from crashing the game. Vastly improves stability in Skirmish and Campaign modes.
* **Disadvantage of disabling:** AI behavior changes slightly, breaking synchronization if a human plays against a retail client with AI, or when watching retail AI replays.
