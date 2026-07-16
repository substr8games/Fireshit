#include "frametap/worker/cli.hpp"

namespace frametap::worker
{
std::string help_text()
{
    return R"HELP(# ============================================================
# FrameTap 0.6 Cheat Sheet
# ============================================================

# Help / version / discovery
frametap -h
frametap -help
frametap --help
frametap --version
frametap --list
frametap --list-outputs

# ------------------------------------------------------------
# User configuration
# ------------------------------------------------------------

frametap --config-path
frametap --show-config
frametap --write-default-config

# Config path:
#   $XDG_CONFIG_HOME/frametap/config.ini
#   fallback: ~/.config/frametap/config.ini
#
# The default file is created on the first real capture or by
# --write-default-config. Help, version, and discovery stay read-only.
# CLI options always override the user config.
#
# Built-in recording destination:
#   ~/Captures/recordings/[frametap]-{timestamp}.mkv
#
# Screenshot path/name are reserved now for the screenshot pass:
#   ~/Captures/screens/[frametap]-{timestamp}.png

# ------------------------------------------------------------
# Whole-output recording
# ------------------------------------------------------------

# Default path + complete system audio
frametap --output DP-1

# Explicit path
frametap --output DP-1 --record recordings/desktop.mkv

# Silent recording
frametap --output DP-1 --audio=off

# ------------------------------------------------------------
# Snipping-tool region recording
# ------------------------------------------------------------

# Draw on the output under the pointer; default path + system audio
frametap --region

# Draw on a named output
frametap --region --output DP-1

# Reuse an exact logical rectangle: x,y,width,height
frametap --region=120,80,1280,720 --output DP-1

# Region selection controls:
#   left-drag  select
#   right-click or Escape  cancel
# Regions are currently contained by one output.

# ------------------------------------------------------------
# Application capture
# ------------------------------------------------------------

# Existing-view selectors record to the configured path by default.
frametap --focused
frametap --app-id onewingedangel
frametap --view-id 93876872831824

# Launch-wrapper mode records with application-only audio by default.
frametap -- ./owa
frametap -- zen-browser

# Explicit path still wins.
frametap --record recordings/owa.mkv -- ./owa

# Viewport-only application capture; no file is written.
frametap --no-record --viewport=on --focused
frametap --no-record --viewport=min -- ./owa

# ------------------------------------------------------------
# Defaults and overrides
# ------------------------------------------------------------

--record PATH      # explicit recording path
--no-record        # disable configured automatic recording
--audio=automatic  # app for launch; system for output/region
--audio=app        # launch-wrapper mode only
--audio=system     # output/region mode only
--audio=off        # silent capture
--viewport=off     # no FrameTap Output window
--viewport=min     # create then minimize application viewport
--viewport=on      # visible application viewport

# Output/region capture always requires viewport=off to prevent recursion.
# Existing-view capture cannot isolate app audio; automatic audio is off.

# ------------------------------------------------------------
# Stop / inspect
# ------------------------------------------------------------

# Press Ctrl+C. Launch mode also stops when the launched app exits.
ffprobe -hide_banner /path/printed/by/frametap.mkv

# Expected recording streams:
#   Video: H.264, Intel VA-API, exact 60/1 metadata
#   Audio: Opus, 48 kHz stereo, 160 kbps when enabled

# ------------------------------------------------------------
# Build and install
# ------------------------------------------------------------

./scripts/build.sh

sudo install -Dm755 build/frametap \
  /usr/local/bin/frametap

# FrameTap 0.6 is worker-only. The protocol and compositor plugin remain 5.
# No plugin reinstall or Wayfire restart is required for this pass.
)HELP";
}
}
