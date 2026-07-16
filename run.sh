#!/usr/bin/env bash

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${FIRESHIT_BUILD_ROOT:-$ROOT_DIR/.build}"
PREFIX="${FIRESHIT_PREFIX:-/usr/local}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
HUD_CONFIG_DIR="$CONFIG_HOME/substr8-hud"

say() { printf 'Fireshit: %s\n' "$*"; }
fail() { printf 'Fireshit: %s\n' "$*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || fail "required command is unavailable: $1"; }
run_root() {
  if [ "$(id -u)" -eq 0 ]; then "$@"; else need sudo; sudo "$@"; fi
}

usage() {
  cat <<'EOF'
Fireshit 1.1 suite dispatcher

Usage: ./run.sh <command> [arguments]

  build                         Configure and compile all suite modules
  install                       Build and install all suite modules
  uninstall                     Remove files recorded by Meson install logs
  update [--no-run]             Fast-forward a Git checkout, optionally install
  layout [left|right|fullscreen] Show or change SUBSTR8 HUD placement
  module-check                  Inspect optional HUD module commands and TTY access
  console-status [--vt N]       Show shared-console service/session status
  console-enable --user U --vt N
  console-disable [--vt N]
  console-restart [--vt N]
  console-stop-session [--vt N]
  legal-check                   Check suite license and version alignment
  help

Environment:
  FIRESHIT_BUILD_ROOT   Build directory (default: ./.build)
  FIRESHIT_PREFIX       Meson install prefix (default: /usr/local)
EOF
}

meson_setup() {
  source_dir="$1"
  build_dir="$2"
  if [ -f "$build_dir/build.ninja" ]; then
    meson setup --reconfigure --prefix "$PREFIX" "$build_dir" "$source_dir" || return $?
  else
    meson setup --prefix "$PREFIX" "$build_dir" "$source_dir" || return $?
  fi
}

build_all() {
  need meson
  need ninja
  mkdir -p "$BUILD_ROOT" || return $?
  meson_setup "$ROOT_DIR/Firemod" "$BUILD_ROOT/firemod" || return $?
  meson compile -C "$BUILD_ROOT/firemod" || return $?
  meson_setup "$ROOT_DIR/FrameTap" "$BUILD_ROOT/frametap" || return $?
  meson compile -C "$BUILD_ROOT/frametap" || return $?
  meson_setup "$ROOT_DIR/Substr8HUD" "$BUILD_ROOT/substr8-hud" || return $?
  meson compile -C "$BUILD_ROOT/substr8-hud" || return $?
}

seed_user_config() {
  mkdir -p "$HUD_CONFIG_DIR/modules.d" "$HUD_CONFIG_DIR/btop" || return $?
  [ -f "$HUD_CONFIG_DIR/config.ini" ] || cp "$ROOT_DIR/Substr8HUD/config/config.ini" "$HUD_CONFIG_DIR/config.ini"
  [ -f "$HUD_CONFIG_DIR/tty.conf" ] || cp "$ROOT_DIR/Substr8HUD/config/tty.conf" "$HUD_CONFIG_DIR/tty.conf"
  for source in "$ROOT_DIR"/Substr8HUD/config/modules.d/*.ini; do
    target="$HUD_CONFIG_DIR/modules.d/$(basename "$source")"
    [ -f "$target" ] || cp "$source" "$target"
  done
  for source in "$ROOT_DIR"/Substr8HUD/config/btop/*; do
    target="$HUD_CONFIG_DIR/btop/$(basename "$source")"
    [ -f "$target" ] || cp "$source" "$target"
  done
}

install_all() {
  build_all || return $?
  run_root meson install -C "$BUILD_ROOT/firemod" || return $?
  run_root meson install -C "$BUILD_ROOT/frametap" || return $?
  run_root meson install -C "$BUILD_ROOT/substr8-hud" || return $?
  seed_user_config || return $?
  systemctl --user daemon-reload >/dev/null 2>&1 || true
  if command -v btop >/dev/null 2>&1; then
    systemctl --user enable --now substr8-btop.service >/dev/null 2>&1 || true
  fi
  say "installed Fireshit 1.1; existing user configuration was preserved"
}

uninstall_log() {
  log="$1/meson-logs/install-log.txt"
  [ -f "$log" ] || return 0
  awk 'NF { print length($0) "\t" $0 }' "$log" | sort -rn | cut -f2- | while IFS= read -r path; do
    [ -e "$path" ] || [ -L "$path" ] || continue
    run_root rm -f -- "$path" || return $?
  done
}

uninstall_all() {
  systemctl --user disable --now substr8-btop.service >/dev/null 2>&1 || true
  uninstall_log "$BUILD_ROOT/substr8-hud" || return $?
  uninstall_log "$BUILD_ROOT/frametap" || return $?
  uninstall_log "$BUILD_ROOT/firemod" || return $?
  systemctl --user daemon-reload >/dev/null 2>&1 || true
  run_root systemctl daemon-reload >/dev/null 2>&1 || true
  say "installed suite files removed; user configuration was preserved"
}

update_checkout() {
  [ -d "$ROOT_DIR/.git" ] || fail "update requires a Git checkout; downloaded release archives are immutable"
  need git
  git -C "$ROOT_DIR" pull --ff-only || return $?
  if [ "${1:-}" != "--no-run" ]; then install_all; fi
}

layout_command() {
  value="${1:-}"
  mkdir -p "$HUD_CONFIG_DIR" || return $?
  if [ ! -f "$HUD_CONFIG_DIR/config.ini" ]; then
    cp "$ROOT_DIR/Substr8HUD/config/config.ini" "$HUD_CONFIG_DIR/config.ini" || return $?
  fi
  if [ -z "$value" ]; then
    current="$(awk -F= '/^[[:space:]]*placement[[:space:]]*=/{gsub(/[[:space:]]/,"",$2); print $2; exit}' "$HUD_CONFIG_DIR/config.ini")"
    printf '%s\n' "${current:-right}"
    return 0
  fi
  case "$value" in left|right|fullscreen) ;; *) fail "layout must be left, right, or fullscreen" ;; esac
  tmp="$HUD_CONFIG_DIR/config.ini.tmp.$$"
  awk -v wanted="$value" '
    BEGIN { in_hud=0; written=0 }
    /^\[hud\][[:space:]]*$/ { in_hud=1; print; next }
    /^\[/ {
      if (in_hud && !written) { print "placement=" wanted; written=1 }
      in_hud=0
    }
    in_hud && /^[[:space:]]*placement[[:space:]]*=/ { if (!written) print "placement=" wanted; written=1; next }
    { print }
    END {
      if (!written) {
        if (!in_hud) print "\n[hud]"
        print "placement=" wanted
      }
    }
  ' "$HUD_CONFIG_DIR/config.ini" > "$tmp" || { rm -f "$tmp"; return 1; }
  mv "$tmp" "$HUD_CONFIG_DIR/config.ini" || return $?
  command -v wayfire-hudctl >/dev/null 2>&1 && wayfire-hudctl reload >/dev/null 2>&1 || true
  say "HUD layout set to $value"
}

module_check() {
  module_dir="$HUD_CONFIG_DIR/modules.d"
  [ -d "$module_dir" ] || module_dir="$ROOT_DIR/Substr8HUD/config/modules.d"
  missing=0
  for file in "$module_dir"/*.ini; do
    [ -f "$file" ] || continue
    id="$(awk -F= '/^[[:space:]]*id[[:space:]]*=/{print $2; exit}' "$file")"
    reqs="$(awk -F= '/^[[:space:]]*requires[[:space:]]*=/{sub(/^[^=]*=/,""); print; exit}' "$file")"
    absent=""
    old_ifs="$IFS"; IFS=';'
    for req in $reqs; do
      [ -n "$req" ] || continue
      command -v "$req" >/dev/null 2>&1 || absent="$absent $req"
    done
    IFS="$old_ifs"
    if [ -n "$absent" ]; then printf 'MISSING %-28s%s\n' "${id:-$(basename "$file")}" "$absent"; missing=1
    else printf 'READY   %s\n' "${id:-$(basename "$file")}"; fi
  done
  vt="${XDG_VTNR:-$(fgconsole 2>/dev/null || printf '1')}"
  if [ -r "/dev/vcsa$vt" ]; then printf 'READY   tty mirror source /dev/vcsa%s\n' "$vt"
  else printf 'NOTICE  tty mirror source /dev/vcsa%s is not readable\n' "$vt"; fi
  return "$missing"
}

parse_vt() {
  vt=8
  while [ "$#" -gt 0 ]; do
    case "$1" in
      --vt) [ "$#" -ge 2 ] || fail "--vt requires a number"; vt="$2"; shift 2 ;;
      *) fail "unknown console argument: $1" ;;
    esac
  done
  case "$vt" in ''|*[!0-9]*) fail "VT must be a positive number" ;; esac
  [ "$vt" -ge 1 ] || fail "VT must be a positive number"
}

console_config_user() {
  file="/etc/substr8-hud/console-$1.conf"
  [ -r "$file" ] || return 1
  awk -F= '/^SUBSTR8_HUD_CONSOLE_USER=/{print $2; exit}' "$file"
}

console_enable() {
  user=""; vt=8
  while [ "$#" -gt 0 ]; do
    case "$1" in
      --user) [ "$#" -ge 2 ] || fail "--user requires a username"; user="$2"; shift 2 ;;
      --vt) [ "$#" -ge 2 ] || fail "--vt requires a number"; vt="$2"; shift 2 ;;
      *) fail "unknown console argument: $1" ;;
    esac
  done
  [ -n "$user" ] || fail "console-enable requires --user"
  id "$user" >/dev/null 2>&1 || fail "unknown user: $user"
  case "$user" in *[!A-Za-z0-9_.-]*) fail "unsupported username" ;; esac
  case "$vt" in ''|*[!0-9]*) fail "VT must be a positive number" ;; esac
  state_dir="/var/lib/substr8-hud"; conf_dir="/etc/substr8-hud"
  enabled=no; active=no
  systemctl is-enabled "getty@tty$vt.service" >/dev/null 2>&1 && enabled=yes
  systemctl is-active "getty@tty$vt.service" >/dev/null 2>&1 && active=yes
  run_root mkdir -p "$state_dir" "$conf_dir" || return $?
  printf 'GETTY_ENABLED=%s\nGETTY_ACTIVE=%s\n' "$enabled" "$active" | run_root tee "$state_dir/console-$vt.state" >/dev/null || return $?
  printf 'SUBSTR8_HUD_CONSOLE_USER=%s\n' "$user" | run_root tee "$conf_dir/console-$vt.conf" >/dev/null || return $?
  run_root systemctl disable --now "getty@tty$vt.service" >/dev/null 2>&1 || true
  run_root systemctl daemon-reload || return $?
  run_root systemctl enable --now "substr8-hud-console@$vt.service" || return $?
  say "shared console enabled for $user on tty$vt"
}

console_disable() {
  parse_vt "$@"
  state="/var/lib/substr8-hud/console-$vt.state"
  enabled=no; active=no
  if [ -r "$state" ]; then
    enabled="$(awk -F= '/^GETTY_ENABLED=/{print $2}' "$state")"
    active="$(awk -F= '/^GETTY_ACTIVE=/{print $2}' "$state")"
  fi
  run_root systemctl disable --now "substr8-hud-console@$vt.service" >/dev/null 2>&1 || true
  run_root rm -f "/etc/substr8-hud/console-$vt.conf" "$state" || return $?
  [ "$enabled" = yes ] && run_root systemctl enable "getty@tty$vt.service" >/dev/null 2>&1 || true
  [ "$active" = yes ] && run_root systemctl start "getty@tty$vt.service" >/dev/null 2>&1 || true
  run_root systemctl daemon-reload >/dev/null 2>&1 || true
  say "shared console disabled on tty$vt; prior getty state restored when recorded"
}

console_status() {
  parse_vt "$@"
  systemctl --no-pager --full status "substr8-hud-console@$vt.service" 2>/dev/null || true
  user="$(console_config_user "$vt" 2>/dev/null || true)"
  if [ -n "$user" ]; then
    if [ "$(id -un)" = "$user" ]; then tmux -L substr8-hud has-session -t console 2>/dev/null
    else run_root runuser -u "$user" -- tmux -L substr8-hud has-session -t console 2>/dev/null; fi
    [ "$?" -eq 0 ] && say "tmux session is active for $user" || say "tmux session is not active for $user"
  fi
}

console_restart() { parse_vt "$@"; run_root systemctl restart "substr8-hud-console@$vt.service"; }
console_stop_session() {
  parse_vt "$@"
  user="$(console_config_user "$vt" 2>/dev/null || true)"
  [ -n "$user" ] || fail "no configured console user for tty$vt"
  if [ "$(id -un)" = "$user" ]; then tmux -L substr8-hud kill-session -t console
  else run_root runuser -u "$user" -- tmux -L substr8-hud kill-session -t console; fi
}

legal_check() {
  failed=0
  expected="$(cat "$ROOT_DIR/VERSION")"
  [ "$expected" = 1.1 ] || { echo "FAIL root VERSION is $expected"; failed=1; }
  for file in "$ROOT_DIR/Firemod/LICENSE" "$ROOT_DIR/FrameTap/LICENSE" "$ROOT_DIR/Substr8HUD/LICENSE" "$ROOT_DIR/LEGAL/FIRESHIT_CAPTURE_IMMUNITY_LICENSE.md"; do
    cmp -s "$ROOT_DIR/LICENSE" "$file" || { echo "FAIL license mismatch: ${file#$ROOT_DIR/}"; failed=1; }
  done
  grep -q "version: '1.1.0'" "$ROOT_DIR/Firemod/meson.build" || { echo 'FAIL Firemod version'; failed=1; }
  grep -q "version: '1.1.0'" "$ROOT_DIR/FrameTap/meson.build" || { echo 'FAIL FrameTap version'; failed=1; }
  grep -q "version: '1.1.0'" "$ROOT_DIR/Substr8HUD/meson.build" || { echo 'FAIL SUBSTR8 HUD version'; failed=1; }
  grep -q "license: 'LicenseRef-FCIL-0.1'" "$ROOT_DIR/Firemod/meson.build" || { echo 'FAIL Firemod license metadata'; failed=1; }
  grep -q "license: 'LicenseRef-FCIL-0.1'" "$ROOT_DIR/FrameTap/meson.build" || { echo 'FAIL FrameTap license metadata'; failed=1; }
  grep -q "license: 'LicenseRef-FCIL-0.1'" "$ROOT_DIR/Substr8HUD/meson.build" || { echo 'FAIL SUBSTR8 HUD license metadata'; failed=1; }
  grep -q '<project_license>LicenseRef-FCIL-0.1</project_license>' "$ROOT_DIR/Firemod/data/org.philabs.Firemod.metainfo.xml" || { echo 'FAIL AppStream project license'; failed=1; }
  if grep -RInE --exclude-dir=.git --exclude='FIRESHIT_THIRD_PARTY_POLICY.md' --exclude='FIRESHIT_LEGAL_FRAMEWORK.md' --exclude='RELEASE_LEGAL_GATE.md' --exclude='SPDX_AND_PACKAGE_METADATA.md' --exclude='run.sh' 'license:.*MIT|<project_license>MIT|^MIT License$' "$ROOT_DIR"; then
    echo 'FAIL unintended first-party MIT declaration'; failed=1
  fi
  [ "$failed" -eq 0 ] && say 'legal and suite-version alignment passed'
  return "$failed"
}

case "${1:-help}" in
  build) shift; build_all "$@" ;;
  install) shift; install_all "$@" ;;
  uninstall) shift; uninstall_all "$@" ;;
  update) shift; update_checkout "$@" ;;
  layout) shift; layout_command "$@" ;;
  module-check) shift; module_check "$@" ;;
  console-enable) shift; console_enable "$@" ;;
  console-disable) shift; console_disable "$@" ;;
  console-status) shift; console_status "$@" ;;
  console-restart) shift; console_restart "$@" ;;
  console-stop-session) shift; console_stop_session "$@" ;;
  legal-check) shift; legal_check "$@" ;;
  help|-h|--help) usage ;;
  *) usage >&2; fail "unknown command: $1" ;;
esac
