# Mango — bounded Niri mode

A patched Mango 0.17.2 source tree that implements Niri's scrolling
mechanics with an explicit **bounded placement policy** on top.

| Pinned Mango | `766da93fa6fa816140d2c22c9f8c3a644f3e666e` (upstream 0.17.2) |
| --- | --- |
| Fork integration | `963bcd49` |
| Pinned Niri  | `dd75865f547f0eac0e9b6c4d86d2cd00c0744252` |
| Mode flag    | `scroller_niri_view` (off by default; Mango behaves stock when off) |

This is **not** a stock-Niri port. It ports Niri's persistent-column,
viewport, focus, sizing, gesture, and drag-placement semantics, then
overlays the user's bounded policy:

- 50 % minimum column width (`scroller_min_proportion`).
- First window opens at 100 %; a second window snaps back to the bounded
  1,1 split without re-mapping.
- Two-client limit per column on the auto-stack path (manual joins
  beyond `scroller_stack_max` are **not** yet gated — see Status below).
- Hover-focus works on partially visible windows without panning; clicking a
  window brings it fully into the usable monitor area.
- Super+semicolon opens a small live panel for the active layout, gap size,
  and floating snap setting.
- Singleton column fills the full monitor edge.
- Whole-column maximize is reversible: the prior stack is preserved.
- `toggle_full_width_column` saves the prior proportion so a second
  toggle restores the original split.

## Status — what's wired vs deferred

Implementation status against the 10-phase plan in `runbook.md`:

| Phase | Scope | Status |
| --- | --- | --- |
| 1 | Preflight + backups | done |
| 2 | Pin Mango, branch repo | done (fork at `DrB0rk/mango-bounded-niri`) |
| 3 | Replace scroller state without changing defaults | done — `full_width`, `saved_scroller_proportion`, per-column `maximized` |
| 4 | Independent camera + viewport gestures | **partial** — state machine wired; touchpad/pointer gestures **deferred** (`scroller_view_gesture_fingers`, `scroller_dnd_edge_scroll` are accepted but unused) |
| 5 | Exact closest-gap insertion + drag-placement | **deferred** — only the auto-stack path inserts |
| 6 | Bounded `1,1,2` map policy | **partial** — auto-stack enforced; manual-join cap on `scroller_stack_max` not yet enforced |
| 7 | Focus origins + hover-without-pan | done — hover focuses without moving the viewport, while click focus reveals the target inside the usable monitor area |
| 8 | Reversible maximize + full-width column | done — `scroller_toggle_maximized` and `toggle_full_width_column` |
| 9 | Niri size actions + 50 % floor | done — `set_column_size`, `adjust_column_size`, `set_proportion` clamped |
| 10 | Configuration plumbing | done — 9 new `scroller_*` options in `config.def` and `override_config`; 3 new dispatch names registered |

### Concrete consequences of the deferred pieces

- Touchpad two-finger horizontal swipe / pinch does **not** pan the
  viewport (`scroller_view_gesture_fingers=3` is accepted but has no
  effect yet).
- Drag a window near the left/right edge of a column and release; it
  spawns a new column rather than inserting into the closest gap.
- Manually focusing a third client inside a column will currently stack
  it (the `scroller_stack_max=2` cap applies to the auto-stack path,
  not the focus-driven path).

The deferred work is purely additive — the patches that are wired are
complete and self-consistent.

## Repository layout

```
bounded-niri/
├── README.md                              ← this file
├── VALIDATION.md                          ← compile/config/symbol evidence
├── runbook.md                             ← full spec + observable-behavior table
├── NOTICE                                 ← license + attribution
├── config-bounded-niri.conf               ← Mango config for the patched build
├── config.pre-bounded-niri-stock-baseline ← last known-good stock config
└── dms/                                   ← bundled so config validates self-contained
    ├── binds.conf
    ├── colors.conf
    ├── cursor.conf
    ├── layout.conf
    ├── outputs.conf
    └── windowrules.conf

src/, include/, mmsg/, meson.build         ← Mango source with bounded patches
docs/, README.md (upstream), etc.          ← upstream Mango documentation
```

## Patch shape

```
 bounded-niri/                             ← new
 include/mango/common/server.h             ←   1 +
 include/mango/common/types.h              ←   9 ++
 include/mango/config/parse_config.h       ←   9 ++
 include/mango/dispatch/bind.h             ←   3 +
 include/mango/layout/scroll.h             ←  17 ++
 include/mango/manage/client.h             ←   2 +
 src/config/parse_config.c                 ←  57 +++++
 src/dispatch/bind.c                       ← 164 ++++++++
 src/input/pointer.c                       ←   4 +-
 src/input/tablet.c                        ←   2 +-
 src/layout/scroll.c                       ← 113 +++++++-
 src/manage/client.c                       ←  53 ++++++-
 12 source files changed; 4 doc files added
```

Branch: `main` of `DrB0rk/mango-bounded-niri` (initial push).

## Build

```sh
cd ~/src/mango-bounded-niri
meson setup build --prefix=$HOME/.local --buildtype=debugoptimized
meson compile -C build
meson install -C build --no-rebuild
```

The user-local build lands at `~/.local/bin/mango` (and `~/.local/bin/mmsg`)
and **does not** overwrite the system `/usr/bin/mango`.

After `meson install`, the installer overwrites
`~/.local/share/wayland-sessions/mango.desktop` with an
`Exec=/home/<user>/.local/bin/mango …` line. **Restore it to stock**
`Exec=mango` if you do not intend to switch sessions:

```sh
cat > ~/.local/share/wayland-sessions/mango.desktop <<'EOF'
[Desktop Entry]
Name=Mango
Comment=Mango Wayland Compositor
Exec=mango
Type=Application
DesktopNames=Mango;X-Wayland;
EOF
```

## Run (without switching the live session)

Validate the alternate config first:

```sh
cd ~/src/mango-bounded-niri/bounded-niri
mango -c ./config-bounded-niri.conf --validate 2>&1 | tail
```

When you do want to switch sessions, point a Wayland session at the
patched binary + bundled config:

```sh
~/.local/bin/mango -c ~/src/mango-bounded-niri/bounded-niri/config-bounded-niri.conf
```

**Live session was not switched** during the work that produced this
patch. The active compositor is still the system `/usr/bin/mango` with
the original `~/.config/mango/config.conf`.

## Rollback

Stock Mango + stock config is unaffected. To go back, log into the stock
session entry. The bounded build and config are fully isolated.

## License

Mango is GPL-3.0-or-later. Niri is GPL-3.0-or-later. License compatibility
is preserved. The bounded-Niri algorithms are derived from Niri at the
pinned commit; per-function attribution comments live next to the ports
and a project-wide `NOTICE` is included. Any redistribution must
include or offer the corresponding source — see `NOTICE`.
