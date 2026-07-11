# 2026-04 Lessons Learned

## Session 2026-04-28 - SDL native fullscreen on macOS needs post-request state verification and foreground enforcement

- Problem: Even with correct render/backbuffer sizing, macOS could intermittently remain in a pseudo-borderless state (Dock/menu visible) after fullscreen request, with degraded focus/cursor behavior.
- Root cause:
	- `SDL_SetWindowFullscreen(true)` is asynchronous relative to compositor/window-manager transitions.
	- A successful call does not guarantee the window is already flagged as fullscreen in the immediate next code path.
	- If the window stays foreground-inconsistent during that transition, input behavior can degrade in practice.
- Fix:
	- After requesting fullscreen, verify window fullscreen flag and retry a few times when needed.
	- Raise the window once transition is requested to reinforce foreground focus state.
	- Apply the same logic in both `GeneralsMD` and `Generals` SDL display paths to keep behavior aligned.
- Validation:
	- macOS build and deploy completed successfully with the hardened fullscreen path.
- Prevention: Treat fullscreen entry as a transition state machine on macOS/SDL; validate resulting state, do not rely on a single call as terminal state.

## Session 2026-04-27 - Custom map pipelines must treat path separators and map-name encoding as cross-platform invariants

- Problem: User custom maps under `~/Library/Application Support/GeneralsX/GeneralsZH/Maps` could fail discovery/metadata flow when code assumed Windows-only separators and mixed path composition styles.
- Root cause:
	- Shared map scan/cache paths used `reverseFind('\\')` and Windows-only suffix checks.
	- `map.str` lookup composition used `mapDir + fname` assumptions that are fragile when `fname` already carries rooted/qualified path semantics.
	- Special-character map names rely on quoted-printable conversion; locale-sensitive `isalnum` in ASCII encoding can produce inconsistent behavior.
- Fix:
	- Added separator-agnostic path handling (`\\` or `/`) in shared map scanning (`MapUtil.cpp`) and in map cache parsing (`INIMapCache.cpp` for both ZH and Generals).
	- Built map string-file path from the actual map filepath and separator style instead of recomposing from mixed fragments.
	- Switched ASCII quoted-printable checks to locale-independent ASCII classification (`isAsciiAlphaNumeric`) in both game variants.
- Validation:
	- Custom user-path maps load successfully on macOS after patch.
	- Stress dataset with names containing spaces, quotes, punctuation, and accented Unicode characters completed without crash in successful run.
- Prevention: Treat map-name handling as a pipeline (filesystem scan -> cache serialization -> cache parse -> localized display load). Any separator/encoding change must be validated end-to-end, not only at one layer.

## Session 2026-04-27 - SDL fullscreen on macOS must size the initial DXVK backbuffer from the current window, not the target display mode

- Problem: After the ultrawide/pillarbox merge, macOS fullscreen could look like a low-resolution image stretched to fill the screen, with a visible small-window-then-fullscreen transition.
- Root cause:
	- Non-Windows builds force DXVK presentation through the windowed path and apply native fullscreen later through SDL.
	- The new pillarbox code prefilled `D3DPRESENT_PARAMETERS.BackBufferWidth/Height` with the native display size while the SDL window was still at its pre-fullscreen size.
	- On macOS this could leave the swapchain created/reset at the old window dimensions, while later logic incorrectly believed the backbuffer already matched fullscreen, so the frame was upscaled and blurred.
- Fix:
	- For SDL-managed fullscreen on non-Windows builds, initialize/reset the DXVK backbuffer from the current window pixel size first.
	- Keep native display sizing only as a fallback when the current SDL window size is unavailable.
	- Delay the SDL fullscreen transition until after the render device has applied the final game resolution, and resize the SDL window before creating/resetting the device.
	- Compute pillarbox/backbuffer fit from active present/backbuffer dimensions (source of truth), not from display-mode assumptions.
- Validation:
	- Runtime fullscreen behavior on macOS was confirmed after synchronizing SDL fullscreen transition, present backbuffer reset, and pillarbox sizing.
- Prevention: When fullscreen is entered asynchronously through SDL or the platform window manager, do not size the initial swapchain from the target mode unless the window has already completed the transition.

## Session 2026-04-27 - LAN network pacing must keep consistent time units across platforms

- Problem: LAN matches on Linux could run dramatically faster than intended, while non-network pacing paths looked normal.
- Root cause:
	- `Network::timeForNewFrame()` computes `frameDelay` as `m_perfCountFreq / m_frameRate`.
	- Linux network path used `clock_gettime(CLOCK_MONOTONIC)` but converted timestamps to microseconds while the configured counter frequency drifted from the expected high-resolution domain.
	- This mismatch made frame scheduling thresholds too small, allowing network logic frames to advance far too quickly.
- Fix:
	- Standardized Linux network pacing to nanoseconds in `Network.cpp` by setting `m_perfCountFreq = 1000000000` and using nanosecond timestamp conversion in both stall checks and frame pacing checks.
	- Kept Windows `QueryPerformanceCounter` / `QueryPerformanceFrequency` behavior unchanged.
- Prevention: For any multiplayer pacing code, treat counter frequency and timestamp conversions as a coupled invariant; never change one unit without updating all callsites that compare or accumulate those values.

## Session 2026-04-27 - OpenAL stream fixes must account for different producer lifecycles

- Problem: After restoring briefing-video audio in commit `c0ce9f0`, mission intro narrator lines and victory speech stopped playing on OpenAL builds.
- Root cause:
	- The shared `OpenALAudioStream::update()` logic was adjusted around the video use case, where FFmpeg queues audio before or during explicit stream maintenance.
	- Generic `AT_Streaming` speech uses a different lifecycle: the stream can enter `processPlayingList()` with no queued buffers, fill during `update()`, and still be seen as stopped in the same frame if playback is not retriggered after refill.
	- The manager then releases the stream immediately, so narrator speech never starts.
- Fix:
	- Added a post-refill restart check in `OpenALAudioStream::update()` so newly buffered speech data is started before stopped-stream teardown runs.
	- Preserved the video-specific FFmpeg/OpenAL path introduced for issue #38.
- Prevention: When hardening shared stream classes for video/audio regressions, always validate both producer models: push-queued video streams and lazy-filled gameplay speech streams.

## Session 2026-04-23 - Campaign river-water rendering must guard shroud sampling boundaries

- Problem: Linux `GeneralsXZH` could crash with `SIGSEGV` when opening campaign, with stack traces landing in `W3DShroud::getShroudLevel()` via `W3DWater::getRiverVertexDiffuse()`.
- Root cause:
	- `getShroudLevel(Int x, Int y)` validated only upper bounds (`x < m_numCellsX`, `y < m_numCellsY`) and assumed texture data was always valid.
	- During campaign transitions, water rendering can request shroud samples with transient invalid state (negative cell coordinates and/or unavailable source texture data).
	- This allowed invalid pointer arithmetic into `m_srcTextureData` and a segmentation fault.
- Fix:
	- Added defensive early return in `getShroudLevel()` for both Zero Hour and Generals paths when source texture/data is missing or coordinates are negative.
	- Kept existing upper-bound behavior unchanged, preserving normal shroud shading logic.
- Validation:
	- Linux Docker build for `GeneralsXZH` completed successfully after patch.
	- Static diagnostics (`get_errors`) reported no errors in modified shroud files.
- Prevention: For rendering-path sampling helpers in legacy code, always guard both pointer validity and full signed coordinate bounds before indexing raw texture/buffer memory.

## Session 2026-04-20 - Overlord portable riders must not go through fire-point redeploy on turret rotation

- Problem: Overlord portable upgrades (Gatling Cannon, Propaganda Tower, BattleBunker) could disappear intermittently during force-attack / heavy turret turning on macOS.
- Root cause:
	- `OverlordContain::containReactToTransformChange()` executes on turret orientation changes.
	- Base `OpenContain::redeployOccupants()` moved portable riders through fire points (`putObjAtNextFirePoint`).
	- Portable `setTransformMatrix` triggered rider `reactToTransformChange`, which could route into `GameLogic::destroyObject`.
	- BattleBunker loss cascaded to its contained infantry, making the issue appear broader than only one upgrade type.
- Fix:
	- Added `OverlordContain::redeployOccupants()` override to skip fire-point redeploy for `KINDOF_PORTABLE_STRUCTURE` riders.
	- Kept portable transform sync in `syncPortablePosition()` after transform updates.
	- Preserved base redeploy behavior for non-portable occupants.
- Validation insight: Removal traces observed during score/reset were expected cleanup (`GameLogic::reset`/`clearGameData`) and should not be treated as gameplay regression.
- Prevention: For Overlord portable upgrades, never route rider positioning through generic occupant fire-point redeploy; keep portable movement tied to host transform synchronization only.

## Session 2026-04-19 - Wide printf `%s`/`%S` mismatch causes one-character UI and replay metadata truncation on POSIX

- Problem: Multiple UI texts on macOS/Linux showed only the first character (construction requirements, under-construction/completed labels, replay list fields such as date/time/version).
- Root cause:
	- Legacy code used Windows/MSVC wide `printf` semantics (`%s` for wide string and `%S` for narrow string).
	- On POSIX libc (`vswprintf`), `%s` expects narrow strings unless `l` modifier is present (`%ls`), so many `UnicodeString::format` callsites truncated output.
	- Replay headers also wrote wide strings through `LocalFile::writeFormat(L"%s", ...)`, which could persist truncated metadata in newly recorded `.rep` files.
- Fix:
	- Added POSIX format normalization in `UnicodeString::format_va` to map MSVC-style wide specifiers safely (`%s` -> `%ls`, `%S` -> `%hs` when no explicit length modifier exists).
	- Updated `LocalFile::writeFormat(const WideChar*)` to route formatting via `UnicodeString::format_va`, ensuring identical normalization for replay/header serialization paths.
- Validation: Static diagnostics (`get_errors`) reported no issues in modified files after patching.
- Prevention: For cross-platform wide formatting, avoid relying on CRT-specific `%s`/`%S` behavior; normalize format strings or use explicit `%ls` / `%hs` in new code.

## Session 2026-04-16 - A single issue can hide multiple EXC_BAD_ACCESS root causes

- Problem: A macOS crash issue initially looked like one intermittent defect but later reports in the same thread showed a second stack signature.
- Root causes:
	- AI path: `AITNGuardOuterState::update()` could dereference `m_attackState` when it had not been initialized on certain state-entry paths.
	- Audio path: `OpenALAudioFileCache::freeEnoughSpaceForSample()` could dereference `m_eventInfo` during priority eviction for filename-only cache loads.
- Fix:
	- Added lazy attack-state reconstruction plus safer team/prototype checks in tunnel-network guard update logic.
	- Added null-safe priority selection for OpenAL cache eviction (`AudioEventInfo` optional entries now handled explicitly).
- Validation: Static diagnostics reported no errors in modified files; issue thread updated with code anchors and retest request.
- Prevention: Split triage by crash signature (top frame + fault address + subsystem) before assuming a shared cause across reports in the same issue.

## Session 2026-04-13 - Flatpak must build inside SDK, not package host binaries/libraries

- Problem: Flatpak packaging mixed host-built binaries with manually copied shared libraries, causing fragile runtime behavior and Wayland/Vulkan instability risks.
- Root cause: Packaging flow treated Flatpak as a generic container bundle instead of a runtime+SDK build pipeline.
- Fix:
	- Migrated manifests to compile the game inside `org.freedesktop.Sdk` (25.08) with `flatpak-builder`.
	- Declared dependency sources in manifest (`SDL3`, `SDL3_image`, `openal-soft`, `dxvk-native`) and wired them through `FETCHCONTENT_SOURCE_DIR_*` to avoid in-build network fetches.
	- Removed host library staging logic from the build script and switched to manifest-driven build/export only.
	- Kept runtime environment/path logic centralized in `/app/bin/run.sh`, with wrappers acting as pass-through entrypoints.
- Validation:
	- `flatpak-builder --show-manifest` succeeds for both `com.fbraz3.GeneralsX.yml` and `com.fbraz3.GeneralsXZH.yml`.
	- `flatpak-builder --user --build-only` now advances to SDK update/install phase (instead of failing on missing system remote refs).
- Prevention: For Flatpak work, never copy `.so` files from host system paths into app runtime. Build inside SDK and ship only artifacts produced by the manifest modules.

## Session 2026-04-12 - LAN lobby visibility must be traced at render and prune points, not only at announce receipt

- Problem: Logs could show `handleGameAnnounce` success while the user still reported that the LAN lobby list did not visibly retain the remote game.
- Code finding: The LAN lobby callback does not apply extra compatibility filtering; it forwards `m_games` directly into `LANDisplayGameList`.
- Remaining suspects: a parsed game can still fail to appear usefully if its row content is unexpected, or it can disappear shortly after due to `lastHeard`-based pruning.
- Process improvement: For LAN lobby regressions, instrument both row rendering and timeout-driven removal in addition to message receive/parse logs.

## Session 2026-04-11 - LAN auto-IP and global broadcast can hide hosts cross-platform

- Problem: Linux and macOS builds failed to discover each other in the LAN lobby, and same-platform behavior remained uncertain without multiple test machines per OS.
- Code finding: LAN discovery sends to a fixed global broadcast target (`INADDR_BROADCAST` / `255.255.255.255`) and LAN menu startup can auto-pick the first enumerated non-loopback IP.
- Risk: On macOS/Linux systems with VPN/Docker/virtual NICs, first-IP selection can bind sockets to a non-game interface, and global broadcast may not reach peers as expected for the active subnet.
- Process improvement: For LAN regressions, always log selected bind IP, broadcast destination, bind/send error codes, and incoming source addresses before judging protocol-level compatibility.
- Prevention: Prefer interface-aware LAN discovery (default-route NIC + subnet broadcast calculation) and keep manual IP override visible/easy in options.

## Session 2026-04-12 - LAN join accept must stay unicast to avoid host-only false positives

- Problem: In direct-connect tests, the host sometimes showed the remote player in the game lobby while the joining machine still timed out and never entered the lobby.
- Evidence: Fresh `[LAN86]` logs showed the host receiving the join request and then emitting `MSG_JOIN_ACCEPT` as broadcast (`255.255.255.255`) instead of sending it back directly to the joining client IP.
- Root cause: `LANAPI::handleRequestJoin()` zeroed the response target after adding the player locally, so `sendMessage()` fell through to the broadcast path for the join-accept packet.
- Fix: Keep the requester IP as the response target for join accept/deny packets; only local game-state updates should be host-local.
- Prevention: For request/response handshake packets (`REQUEST_JOIN`, `JOIN_ACCEPT`, `JOIN_DENY`), never repurpose the destination selection based on local slot mutation; log both sender and final response target during debugging.

## Session 2026-04-12 - In-game LAN control packets need directed delivery on mixed OS networks

- Problem: Even after direct-connect join success, clients could miss host-driven updates (game options, game start, and host leave effects), producing partial desync UX.
- Evidence: Join handshake traffic succeeded with unicast, but follow-up control behavior matched packet classes still emitted as broadcast without explicit destination.
- Fix: Prefer directed fan-out to known human slots for in-game control/state packets, keeping broadcast only as fallback when no slot target exists.
- Prevention: In cross-platform LAN paths, treat broadcast as discovery-only by default; use explicit per-slot delivery for game/session control events.

## Session 2026-04-12 - LAN discovery should prefer subnet broadcast over global broadcast

- Problem: LAN lobby discovery remained unreliable across macOS/Linux even when direct-connect worked.
- Evidence: Discovery still depended on global broadcast `255.255.255.255`, which may not be forwarded/handled consistently on mixed-network setups.
- Fix: In POSIX builds, collect broadcast addresses from active IPv4 interfaces matching the selected local LAN IP and send broadcast packets to those subnet addresses first; keep global broadcast as fallback.
- Prevention: For multi-platform LAN discovery, avoid single global broadcast as the only path; use interface-scoped subnet broadcast to reduce network-policy sensitivity.

## Session 2026-04-09 - libxcb Flatpak PoC needs newer source libs, not host baseline copy

## Session 2026-04-11 - 8-player macOS crash points to AI guard-state null dereference path

- Problem: A user reported an intermittent crash during 8-player skirmish on macOS (Apple Silicon), while the same broad scenario could not be reproduced on another Apple Silicon machine.
- Evidence: The crash report pinned thread 0 to `AITNGuardOuterState::update()` with `EXC_BAD_ACCESS` at `0x0000000000000038`, indicating a likely null-pointer dereference with field offset access.
- Code finding: `AITNGuardOuterState::update()` dereferences `m_attackState` without a null guard, but `AITNGuardOuterState::onEnter()` has early-success paths that can leave `m_attackState` unset.
- Scope insight: The crash location is in gameplay AI logic (tunnel-network guard state), not in rendering/audio backends, so hardware generation differences (M1 vs newer Apple Silicon) likely change trigger timing rather than root cause.
- Prevention: For legacy state-machine code, treat early-success state transitions and lazily initialized sub-states as high-risk paths and add explicit invariant checks before dereferencing nested state objects.

## Session 2026-04-11 - Dynamic shadow draws need explicit stream rebinding

- Problem: Animated object shadows (flags, rotor parts) in Zero Hour rendered incorrectly, while static shadows looked fine and the issue disappeared when `3D shadows` was disabled.
- Root cause: `W3DVolumetricShadow::RenderDynamicMeshVolume` in Zero Hour missed the `SetStreamSource` rebind, so dynamic volume draws could use stale vertex stream state.
- Fix: Restored vertex stream rebinding before dynamic shadow draw calls when the active buffer differs from `lastActiveVertexBuffer`.
- Validation: Static diagnostics reported no errors in the edited file; local non-docker build path was not usable due existing cache path mismatch, so full runtime verification is pending smoke test.
- Prevention: Keep dynamic and static shadow render paths behaviorally aligned between `Generals` and `GeneralsMD`, and audit render-state rebinding when cleaning refactor artifacts.

## Session 2026-04-09 - AppImage can bypass current Flatpak Vulkan/XCB coupling blockers

- Problem: Flatpak remained blocked by Vulkan ICD/XCB symbol incompatibilities despite multiple runtime workarounds.
- Action: Implemented an AppImage packaging PoC for `GeneralsXZH` bundling userspace runtime libs (DXVK, SDL3, SDL3_image, OpenAL, GameSpy) with a dedicated launcher.
- Result: AppImage launched successfully and progressed beyond Vulkan window creation and early engine initialization, where Flatpak path previously failed.
- Insight: For short-term Linux distribution, AppImage is currently lower-risk and faster to stabilize than Flatpak in this codebase state.
- Prevention: Keep Flatpak as a parallel track for longer-term sandbox goals, but prioritize AppImage for immediate user-facing releases.

## Session 2026-04-09 - Flatpak packaging needs explicit runtime source destination and compliant metadata/icon rules

- Problem: Local Flatpak packaging failed first with missing `runtime/.` during module build, then with AppStream metadata validation, and finally with icon export rejection.
- Root cause:
	- The `type: dir` source in manifest did not guarantee the expected `runtime/` folder name for the build command.
	- Metainfo files missed required AppStream `metadata_license`.
	- Source icon assets were 650x650, while Flatpak export enforces max 512x512 icon size.
- Fix:
	- Set `dest: runtime` for staging runtime dir source in both manifests.
	- Added `<metadata_license>CC0-1.0</metadata_license>` to both metainfo files.
	- Generated 512x512 icons for Flatpak packaging and switched manifests to install them under `hicolor/512x512`.
	- Added local script preflight to install Flatpak SDK/runtime (`org.freedesktop.Platform//23.08`, `org.freedesktop.Sdk//23.08`) under `--user` when missing.
- Validation: Local builds completed for both targets and produced:
	- `build/GeneralsX-linux64-deploy.flatpak`
	- `build/GeneralsXZH-linux64-deploy.flatpak`
- Prevention: For every new Flatpak app module, validate early that sources map to expected in-build paths, metainfo passes `appstreamcli compose`, and icon dimensions satisfy export limits.

## Session 2026-04-09 - Flatpak runtime library bundling must preserve SONAME links and avoid self-symlink overwrite

- Problem: `flatpak run com.fbraz3.GeneralsXZH` failed with missing `libSDL3_image.so.0` and later codec libraries such as `libavcodec.so.60` / `libx264.so.164`.
- Root cause:
	- Runtime staging copied only regular files, dropping symlink chains required by SONAME resolution.
	- Manifest fallback symlink logic could overwrite already-correct `lib*.so.<major>` files into self-symlinks.
	- FFmpeg-linked binary required a larger codec dependency set than the initial package list.
- Fix:
	- Staging now copies runtime libs with `cp -a` globs to preserve symlinks.
	- Manifest symlink fallback now creates `lib*.so.<major>` links only when source has a minor suffix.
	- Flatpak build script now includes FFmpeg codec libs and uses `ldd`-based dependency closure copy for FFmpeg roots.
- Validation: Dynamic loader errors for SDL3_image/FFmpeg libs were eliminated; runtime progressed to Vulkan initialization stage.
- Prevention: For Flatpak packaging of dynamically linked binaries, validate `NEEDED`/SONAME closure and inspect `/app/lib` for accidental self-symlink artifacts.

## Session 2026-04-01 - User-facing path migrations need runtime fallback, not just docs updates

- Problem: Zero Hour user-facing scripts and docs exposed the internal `GeneralsMD` path, which leaks implementation details and conflicts with product naming (`GeneralsZH`).
- Root cause: Runtime/deploy/bundle scripts hardcoded `~/GeneralsX/GeneralsMD` as the only default path.
- Fix: Switched user-facing defaults to `~/GeneralsX/GeneralsZH` and added automatic fallback to `~/GeneralsX/GeneralsMD` when legacy installs are detected.
- Validation: Shell syntax checks (`bash -n`) and diagnostics validation (`get_errors`) passed for all modified scripts/docs.
- Prevention: For user-visible path renames, implement resolution order (`new path` then `legacy path`) directly in launch/deploy logic instead of relying on migration notes alone.

## Session 2026-04-01 - SDL3 key events are not enough for GUI text entry

- Problem: Text-entry gadgets on SDL3 builds (Linux/macOS) could gain focus but did not insert typed characters.
- Root cause: The SDL3 backend forwarded only key up/down events (`GWM_CHAR`) while the text-entry pipeline inserts printable characters through `GWM_IME_CHAR`.
- Fix: Added an SDL text-input bridge in `SDL3GameEngine` (both Zero Hour and Generals) that enables `SDL_StartTextInput` only when a `GWS_ENTRY_FIELD` has focus and forwards `SDL_EVENT_TEXT_INPUT` UTF-8 codepoints as `GWM_IME_CHAR`.
- Validation: macOS `GeneralsXZH` build completed successfully and the mirrored `g_generals` target built with `EXIT:0`.
- Prevention: On SDL platforms, treat physical key events and text composition as separate channels; always wire `SDL_EVENT_TEXT_INPUT` for editable widgets.

## Session 2026-04-01 - SDL text bridge still needs editing key scan-code coverage

- Problem: After wiring `SDL_EVENT_TEXT_INPUT`, printable characters worked but `Backspace` and navigation/editing keys did not trigger expected GUI behavior.
- Root cause: `SDL3Keyboard::translateScanCodeToKeyVal` was missing mappings for editing/navigation keys (notably `SDL_SCANCODE_BACKSPACE`, plus `Delete/Home/End/PageUp/PageDown/KP Enter`).
- Fix: Added the missing SDL scancode mappings to both Zero Hour and Generals SDL3 keyboard backends.
- Validation: Incremental macOS build (`z_generals`) finished with `EXIT:0`; static diagnostics reported no errors in edited files.
- Prevention: When introducing IME/text-input paths, verify scancode coverage for non-printable editing/navigation keys to preserve legacy `GWM_CHAR` behavior.

## Session 2026-04-01 - Double Backspace from mixed repeat sources

- Problem: Pressing Backspace once deleted two characters in SDL3 builds.
- Root cause: Keyboard repeat happened in two places at once: SDL repeated `KEY_DOWN` events and the engine also generated autorepeat in `Keyboard::checkKeyRepeat()`.
- Fix: In SDL3 keyboard backends (Zero Hour and Generals), ignore SDL repeated keydown events (`event->key.repeat`) and set `keyDownTimeMsec` using `timeGetTime()` so repeat timing uses the same clock domain as engine logic.
- Validation: Incremental macOS `z_generals` build completed with `EXIT:0`.
- Prevention: Keep a single repeat authority (engine side) and avoid mixing timestamp domains when feeding input timing into legacy repeat logic.

## Session 2026-04-02 - SaveLoad crashes can be environment-specific (M1 vs M3, local state)

- Problem: `Menus/SaveLoad.wnd` opened through main menu could exit immediately on one macOS machine while another user (M3) could not reproduce.
- Root cause: Non-Windows save-file enumeration and load flow depended on local filesystem/user state, so missing or invalid save directories could fail during iteration and load could proceed without a valid selection.
- Fix: Hardened non-Windows `GameState::iterateSaveFiles()` to guard directory switch + iteration + restore, and added a load selection guard so the load path bails out safely when no valid save entry is selected.
- Validation: Static diagnostics (`get_errors`) reported no issues in edited files; macOS build task completed with warnings only in unrelated code.
- Prevention: For cross-platform save/load flows, guard filesystem-dependent save enumeration and validate selected save entries before starting load operations when local user state may differ between machines.

## Session 2026-04-02 - Save/load data paths must preserve separator and case semantics

- Problem: Save files were listed and selected correctly, but loading failed with `SC_INVALID_DATA` during snapshot transfer on macOS.
- Root cause: The portable-to-real map path conversion in save-load logic assumed Windows-style path semantics (`\\` separator and forced lowercase output), which can corrupt valid paths on case-sensitive platforms.
- Fix: Updated map path helpers to accept both `\\` and `/` separators and preserved real path casing on non-Windows builds (keep lowercase normalization only on Windows).
- Backport: Applied the same path-semantics fix to both Zero Hour and Generals base game codepaths.
- Validation: Manual macOS Zero Hour flow completed (create save + load save) and process exited cleanly with code 0.
