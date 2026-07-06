#!/bin/bash
# Download C&C Generals Zero Hour game files from your own Steam account.
# Usage: ./get-assets.sh <your_steam_username>
# Steam Guard: you'll be prompted for the code on first login.
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <steam_username>"
    exit 1
fi

STEAM_USER="$1"
DEST="$HOME/GeneralsX/GeneralsZH"
TMP_DIR="$HOME/GeneralsX/.steamcmd_zh"

mkdir -p "$TMP_DIR" "$DEST"

# macOS Gatekeeper quarantines steamcmd's unnotarized bundled frameworks
# (e.g. Breakpad.framework) when Homebrew installs the cask. On first run that
# pops a blocking "Apple could not verify ... malware" dialog that stops
# steamcmd dead. Clear the quarantine flag off the steamcmd install up front.
if [[ "$(uname)" == "Darwin" ]] && command -v brew >/dev/null 2>&1; then
    STEAMCMD_CASK="$(brew --prefix)/Caskroom/steamcmd"
    [[ -d "$STEAMCMD_CASK" ]] && xattr -dr com.apple.quarantine "$STEAMCMD_CASK" 2>/dev/null || true
fi

# steamcmd and the Steam desktop client share one data directory
# (~/Library/Application Support/Steam on macOS, ~/.steam on Linux). When the
# desktop client is running it holds a single-instance lock, so steamcmd stalls
# forever right after printing "Verifying installation..." — no error, just a
# silent hang. Fail fast with an actionable message instead.
if pgrep -x steam_osx >/dev/null 2>&1 \
    || pgrep -f 'Steam.AppBundle' >/dev/null 2>&1 \
    || pgrep -x steam >/dev/null 2>&1; then
    echo "Error: the Steam desktop client is running." >&2
    echo "steamcmd shares Steam's data directory, and the running client locks it —" >&2
    echo 'steamcmd would hang forever after "Verifying installation...".' >&2
    echo "Quit Steam completely (Steam > Quit Steam, or Cmd-Q on macOS), then re-run this script." >&2
    exit 1
fi

# App 2732960 = C&C Generals Zero Hour (Windows depot; assets are platform-independent)
steamcmd \
    +@sSteamCmdForcePlatformType windows \
    +force_install_dir "$TMP_DIR" \
    +login "$STEAM_USER" \
    +app_update 2732960 validate \
    +quit

echo "Moving game files into $DEST ..."
# Copy data files only; keep the deployed GeneralsX runtime (run.sh, dylibs) intact.
rsync -a --exclude="*.exe" --exclude="*.dll" "$TMP_DIR/" "$DEST/"

echo "Done. Assets in place:"
ls "$DEST"/*.big 2>/dev/null | head
echo
echo "Launch with: cd ~/GeneralsX/GeneralsZH && ./run.sh -win"
