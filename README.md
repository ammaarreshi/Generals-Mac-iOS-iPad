# Command & Conquer Generals: Zero Hour — macOS, iOS & iPadOS

<img width="500" height="281" alt="IMG_3457_500" src="https://github.com/user-attachments/assets/aeaf6692-36e6-40c8-b9f8-8066d014ec4b" />

**Zero Hour running natively on Apple Silicon Macs, iPhone, and iPad** — campaign,
skirmish, and Generals Challenge, with touch controls built for RTS (tap-select,
drag-box, long-press deselect, two-finger scroll, pinch zoom). No emulation: this
is the real 2003 engine compiled for ARM64, rendering DirectX 8 →
[DXVK](https://github.com/doitsujin/dxvk) → Vulkan →
[MoltenVK](https://github.com/KhronosGroup/MoltenVK) → Metal.

Built on EA's GPL v3 source release, standing on a chain of community work —
[TheSuperHackers](https://github.com/TheSuperHackers/GeneralsGameCode),
[Fighter19's original Unix port](https://github.com/Fighter19/CnC_Generals_Zero_Hour), and
[fbraz3/GeneralsX](https://github.com/fbraz3/GeneralsX) — this fork adds the iOS/iPadOS
port and a set of engine fixes. See [Lineage & credits](#lineage--credits) for who built
what. The original GeneralsX README lives on the `upstream-main` branch.

**No game assets are included or distributed.** You need your own copy
([Steam](https://store.steampowered.com/app/2732960/), ~$5 on sale).

## What this port actually involved

"Porting" undersells how weird this journey was, so here's the honest shape of it.
The lineage below built the foundation: EA's source release, the community's
modernization, Fighter19's original Unix port, GeneralsX's macOS/Linux work.
What did *not* exist was any of this on iOS — and iOS is a hostile place for a
2003 Windows RTS:

- **The engine assumes a writable filesystem wherever it lives.** iOS apps live in a
  read-only, code-signed bundle. Every config write, cache, and save path had to be
  rerouted — and the working directory bootstrapped from the bundle itself.
- **The renderer speaks DirectX 8. The iPad speaks Metal.** In between: DXVK
  translating D3D8→Vulkan, MoltenVK translating Vulkan→Metal — and DXVK had never
  been built for iPhoneOS. That took a Meson cross-build and a patch to its Vulkan
  loader, because iOS confines `dlopen` to the app bundle ([`Patches/dxvk-ios.patch`](Patches/dxvk-ios.patch)).
- **iOS owns your process.** Open the app switcher and the OS seizes the Metal
  drawable *without backgrounding you* — draw one more frame and you're dead on
  resume. The whole render/sim loop learned to hold its breath.
- **An RTS needs a mouse.** SDL3 (from the lineage below) delivers raw touch events;
  the RTS semantics on top are new. Taps defer until the 2003 GUI has processed
  hover (or menu buttons never highlight), a drag has to decide "selection box or
  camera pan," long-press became right-click, and a cancelled touch must never
  ghost-click a rally point.
- **And then the bug hunts** — the best part. The minimap that rendered black
  because a 2003 texture-format fallback silently dropped the alpha channel. The
  EVA voice that went randomly mute because one zombie audio stream held a global
  "don't talk over speech" flag while chirping forever. Every one chased to root
  cause on a real device, fixed, and offered upstream.

**→ The war stories: [Porting Playbook §8 — the bug archaeology](docs/port/PORTING_PLAYBOOK.md#8-post-ship-bug-hunts-junejuly-2026--the-archaeology-section)**
**→ The complete engineering log: [docs/port/PORTING_PLAYBOOK.md](docs/port/PORTING_PLAYBOOK.md)**
**→ How to do this to another game: [docs/port/PORTING_PATTERNS.md](docs/port/PORTING_PATTERNS.md)**

Worth saying plainly: this was a **human + AI collaboration**. The engineering —
the C++, the cross-builds, the device debugging — was done by
[Claude Code](https://claude.com/claude-code) (Anthropic's Claude, Fable model),
directed and playtested by a human who described symptoms like *"the minimap is
black"* and *"I hear chirping"* and owned every decision. Neither half ships this
alone: one of us can't write C++, and the other can't hear the chirping.

## Quick start — macOS

Prerequisites (one time):

```sh
# Toolchain
xcode-select --install
brew install cmake ninja meson pkgconf
brew install --cask steamcmd

# vcpkg (full clone — a shallow clone breaks manifest baselines)
git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg          # add to your shell profile

# LunarG Vulkan SDK (NOT the Homebrew cask) — https://vulkan.lunarg.com/sdk/home
export VULKAN_SDK=$HOME/VulkanSDK/<version>/macOS   # add to your shell profile
```

Clone, build, get assets, play:

```sh
git clone https://github.com/ammaarreshi/Generals-Mac-iOS-iPad.git GeneralsX
cd GeneralsX
./scripts/build/macos/build-macos-zh.sh     # checks deps, configures, builds
./scripts/build/macos/deploy-macos-zh.sh    # creates ~/GeneralsX/GeneralsZH + run.sh
./scripts/get-assets.sh <your_steam_username>   # fetches game data you own
cd ~/GeneralsX/GeneralsZH && ./run.sh -win
```

## Quick start — iPhone / iPad

On top of the macOS prerequisites: full Xcode (signed into your Apple ID),
`brew install xcodegen`, and a (free or paid) Apple Developer team.

```sh
cd GeneralsX
git submodule update --init references/fbraz3-dxvk   # iOS DXVK is built from this + Patches/dxvk-ios.patch
./scripts/build/ios/fetch-moltenvk.sh                # pinned MoltenVK.framework (checksummed)
./scripts/build/ios/stage-fonts.sh                   # Liberation fonts, renamed as the game expects
cmake --preset ios-vulkan
cmake --build build/ios-vulkan --target z_generals
GX_TEAM_ID=<your-team-id> GX_BUNDLE_ID=com.you.generalszh \
    ./scripts/build/ios/package-ios-zh.sh --install  # assembles, signs, installs
```

Find your team id in Xcode → Settings → Accounts. Assets ship inside the app
bundle (self-contained install); `--dev` skips the ~2.7 GB copy for fast code
iteration.

## Where things are

| Path | What it is |
|---|---|
| [`docs/port/PORTING_PLAYBOOK.md`](docs/port/PORTING_PLAYBOOK.md) | The complete engineering log of this port: every failure mode, root cause, fix — start with [§8, the bug archaeology](docs/port/PORTING_PLAYBOOK.md#8-post-ship-bug-hunts-junejuly-2026--the-archaeology-section): the black minimap, the silent EVA lines, and the chirp |
| `docs/port/PORTING_PATTERNS.md` | Generalized methodology for porting classic Windows games to Apple platforms |
| `docs/port/RELEASE_CHECKLIST.md` | Gate for public release |
| `scripts/get-assets.sh` | Steam asset fetcher (your own copy; app 2732960) |
| `scripts/build/macos/`, `scripts/build/ios/` | Build, deploy, packaging pipelines |
| `ios/` | XcodeGen signing-stub project + `ios/config/` (staged Options.ini, dxvk.conf) |
| `Patches/dxvk-ios.patch` | DXVK changes the iOS d3d8/d3d9 dylibs are built from (applied via the local-fork build) |

## Known issues

- Long sessions on iPad can be killed by iOS for memory (~3 GB+ resident); the app
  exits to the home screen with no dialog. Session logs (current + previous) are in
  the Files app under the game's folder. Under investigation.
- Backgrounding mid-game can occasionally crash on iOS — the lifecycle pause covers
  the common paths; a rare race remains. Save often.

## What's next: Renegade 👀

Generals had a chain of giants to stand on. **Command & Conquer: Renegade** — EA's
2002 FPS from the same GPL source release — has far less: no native macOS or iOS
build of the W3D engine has ever shipped (Mac players today go through Wine-based
compatibility layers). The [OpenW3D](https://github.com/w3dhub/OpenW3D) community
project has real cross-platform groundwork — a DXVK wrapper scaffold and SDL3 build
plumbing — with Mac/Linux on its roadmap, and that groundwork is exactly what we
built on.

Same methodology as this repo, much deeper water: OpenW3D's Win32 compat scaffold
expanded by ~3,000 lines (the engine calls raw Windows APIs for file finding,
keyboard state, COM), a case-sensitivity strategy for twenty thousand asset paths,
the DXVK/MoltenVK renderer bring-up, the audio/video stack, and FPS touch controls.
It's playable today — campaign, cinematics, mission scripts — on a Mac and an
iPhone. For scale: this Generals port added ~2,200 lines on top of GeneralsX;
Renegade needed ~6,700 on top of the Windows-only source.

Repo drops soon, with the OpenW3D lineage credited the way this repo credits its
chain. Same rules: GPL v3, bring your own copy, full engineering log.

## Lineage & credits

This port is the newest link in a long chain, and the earlier links did foundational
work that this repo inherits everywhere:

- **Westwood / EA Pacific** — the game; **EA** — the GPL v3 source release
- **[TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode)** —
  the community mainline: build modernization, VC6→modern toolchain, and much of the
  cross-platform groundwork, including the FFmpeg video backend authored by
  **[feliwir](https://github.com/feliwir)** (of [OpenSAGE](https://github.com/OpenSAGE/OpenSAGE)),
  who also authored the OpenAL audio device work this port's audio stack builds on
- **[Fighter19/CnC_Generals_Zero_Hour](https://github.com/Fighter19/CnC_Generals_Zero_Hour)** —
  the original Unix/64-bit port: SDL3 platform management, C++17
  filesystem/threading, Freetype/Fontconfig text rendering, and the DXVK approach
  this renderer path descends from
- **[fbraz3/GeneralsX](https://github.com/fbraz3/GeneralsX)** — the macOS/Linux port
  this fork builds on directly, integrating and extending the above
- **This fork** — the iOS/iPadOS port (arm64-ios cross-build, DXVK-on-iOS, touch
  controls, app lifecycle, packaging) and engine fixes, offered upstream
- **DXVK, MoltenVK, SDL, OpenAL Soft, FFmpeg, Liberation Fonts** — the load-bearing walls

Engine code **[GPL v3](LICENSE.md)** (EA's source release → the chain above → this
fork). Game assets: not included, not licensed here.

EA has not endorsed and does not support this product. All trademarks are the
property of their respective owners.
