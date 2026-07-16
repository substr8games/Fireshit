# Terminal Module Library

SUBSTR8 HUD installs default module records under the project data prefix and seeds the current user's active copies under:

```text
$XDG_CONFIG_HOME/substr8-hud/modules.d
```

When `XDG_CONFIG_HOME` is unset, this is `~/.config/substr8-hud/modules.d`.

Existing user files are preserved. Optional module files are seeded only when their declared commands are available. A previously seeded file that the user removes is treated as deliberately disabled and is not silently restored.

Files sharing a `layout_slot` form a module bank. `bank_order` defines the stable order inside that bank; lower numbers appear first. The first ordered module is visible by default.

Default side profile:

```text
TOP LEFT      TTY (read-only real virtual-console buffer)
TOP RIGHT     CONSOLE | WAYFIRE
BOTTOM LEFT   HTOP | BTOP | NVTOP | OPENSNITCHD | NETHOGS | IOTOP
```

Fullscreen adds `top-center` and `bottom-right-wide`, with three equal panels across the top and two equal panels across the bottom. The active arrangement is stored in `$XDG_CONFIG_HOME/substr8-hud/config.ini`. Drag a module header in `INTERACTIVE` to move or reorder it; module INI `layout_slot` and `bank_order` remain defaults.

`bottom-wide`, `bottom-left`, and `bottom-right` remain compatibility aliases. New module files use `bottom-left-wide` or `bottom-right-wide`.

The `TTY` module reads `/dev/vcsaN` through the installed `substr8-vt-mirror` helper and never accepts input.

`BTOP` uses the bundled compact passive-monitoring adapter. It holds an 82×16 canvas and rotates every ten seconds between CPU/Processes and Memory/Network. The HUD pane is deliberately noninteractive; ordinary `btop` remains untouched for full-screen use.

Lifecycle:

- `persistent` modules start with the HUD and retain state while another tab is visible.
- `visible` modules start when selected and stop when another tab replaces them.
- `manual` modules remain stopped until explicitly started.
- `restart_policy=always` restarts an unintentional exit after one second.

The HUD never launches `opensnitchd`, invokes `sudo`, grants capabilities, installs third-party telemetry programs, or writes firewall rules.
