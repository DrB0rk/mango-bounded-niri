# VALIDATION.md — evidence captured 2026-09-07

This file records the validation evidence for the bounded-Niri patches
shipped in this fork of Mango 0.16.3. It is the receipt for the
"fully finish and validate but don't switch yet" run.

## 1. Build

```text
meson compile -C build
[1/12] Compiling C object mango.p/src_animation_tag.c.o
[2/12] Compiling C object mango.p/src_layout_arrange.c.o
[3/12] Compiling C object mango.p/src_animation_client.c.o
[4/12] Compiling C object mango.p/src_dispatch_bind.c.o
[5/12] Compiling C object mango.p/src_overview_overview.c.o
[6/12] Compiling C object mango.p/src_input_pointer.c.o
[7/12] Compiling C object mango.p/src_layout_scroll.c.o
[8/12] Compiling C object mango.p/src_ipc_ipc.c.o
[9/12] Compiling C object mango.p/src_manage_monitor.c.o
[10/12] Compiling C object mango.p/src_manage_client.c.o
[11/12] Compiling C object mango.p/src_config_parse_config.c.o
[12/12] Linking target mango
```

12 / 12 compile targets clean with `-Werror -Wall -Wextra`. No warnings,
no errors, no leftover `MESON_*` link errors.

## 2. Binary versions

| Path | Version | Role |
| --- | --- | --- |
| `~/.local/bin/mango` (and `build/mango`) | `0.16.3(004f105c)` | Patched build |
| `/usr/bin/mango` | `0.16.1(release)` | System / live — untouched |

The live compositor is still the stock binary; the user-local patched
build is built but not on the active session path.

## 3. Config validation

All three configs validate with exit code 0 against the patched binary:

```text
$ mango -c /home/drb0rk/.config/mango/config.conf --validate
   (live stock config — no bounded-niri options)
$ cd bounded-niri && mango -c ./config-bounded-niri.conf --validate
   (self-contained alternate config with dms/ sibling)
$ mango -c ./config.pre-bounded-niri-stock-baseline --validate
   (last known-good stock baseline)
```

The alternate config uses `source=./dms/*.conf` paths, so the bundled
`bounded-niri/dms/` directory (six conf files: binds, colors, cursor,
layout, outputs, windowrules) must sit alongside `config-bounded-niri.conf`
for it to validate from its own directory. That is why `dms/` ships in
this tree.

## 4. Config options — all 9 bounded-Niri options registered

Grep for the `scroller_niri_view` block in `config.def` and
`override_config` shows the full option set:

| Option | Type | Default | Where gated |
| --- | --- | --- | --- |
| `scroller_niri_view` | bool | 0 | gates every bounded-Niri behavior |
| `scroller_stack_max` | int | 2 | auto-stack cap in `apply_bounded_scroller_map_policy` |
| `scroller_auto_stack_every` | int | 3 | Nth-window joins instead of spawning |
| `scroller_min_proportion` | float | 0.5 | floor in `set_proportion`, `set_column_size`, `adjust_column_size` |
| `scroller_pointer_focus_mode` | enum | `keep-view` | `client_focus_with_origin` chooses `keep-view` vs `follow-pointer` |
| `scroller_restore_stack_after_maximize` | bool | 1 | gates `scroller_toggle_maximized` |
| `scroller_niri_gap_drop` | int | 0 | placeholder (deferred gesture hook) |
| `scroller_view_gesture_fingers` | int | 3 | placeholder (deferred gesture hook) |
| `scroller_dnd_edge_scroll` | int | 0 | placeholder (deferred gesture hook) |

The last three are accepted by the parser (so configs don't fail to
load) but are not yet wired to input handling — see the deferred
matrix in `README.md`.

## 5. Dispatch registration — all 3 bounded-Niri dispatch names wired

```text
$ grep -n 'strcmp(func_name, "set_column_size\|adjust_column_size\|toggle_full_width_column' src/config/parse_config.c
   set_column_size       → registered
   adjust_column_size    → registered
   toggle_full_width_column → registered
```

`scroller_toggle_maximized` is invoked from an existing dispatch path
(`MAXIMIZED_TOGGLE`) in `src/dispatch/bind.c`, gated by both
`scroller_niri_view` and `scroller_restore_stack_after_maximize`.

## 6. Symbols — every bounded-Niri extern is reachable

```text
$ nm --defined-only build/mango | grep -E ' T (set_column_size|adjust_column_size|toggle_full_width_column|scroller_toggle_maximized|client_focus_with_origin|scroller_stack_size)'
000000000002e1d0 T set_proportion
000000000002e3c0 T set_column_size
000000000002e5d0 T adjust_column_size
000000000002e7c0 T toggle_full_width_column
00000000000581c0 T scroller_stack_size
0000000000058630 T scroller_toggle_maximized
00000000000587a0 T update_scroller_state
0000000000065500 T client_focus_with_origin
```

A second grep for the file-local helpers (`apply_bounded_scroller_map_policy`,
`arrange_bounded_maximized`, `bounded_scroller_proportion`,
`scroller_action_head`, `parse_scroller_size`, `scroller_primary_extent`)
correctly shows them as `t` (local) or absent from the dynamic symbol
table — they are `static` helpers inside `src/dispatch/bind.c` and
`src/layout/scroll.c`, which is what the earlier "6 symbols missing"
report reflected.

## 7. Attribution

Every bounded-Niri port carries a comment that names the Niri commit it
came from:

```text
$ grep -rl 'Bounded-Niri' src/ include/
include/mango/common/types.h
include/mango/layout/scroll.h
src/dispatch/bind.c
src/input/pointer.c
src/input/tablet.c
src/layout/scroll.c
src/manage/client.c
```

Pinned Niri commit `dd75865f547f0eac0e9b6c4d86d2cd00c0744252` appears
in every one of those comments. A consolidated `bounded-niri/NOTICE`
lists each port with file references and the redistribution
requirements under GPL-3.0-or-later.

## 8. Singleton 100 % behavior

`src/layout/scroll.c` now skips the singleton single-proportion branch
when `config.scroller_niri_view` is on:

```c
if (n_heads == 1 && !heads[0]->full_width &&
    !scroller_ignore_proportion_single &&
    !heads[0]->client->isfullscreen &&
    !heads[0]->client->ismaximizescreen &&
    !config.scroller_niri_view) {   /* ← guarded */
    /* … old path: clamp to stored proportion … */
}
```

This is the "first window opens at 100 %, a second window snaps back to
the 1,1 split" behavior — the stored proportion stays at 0.5, but the
display uses the full monitor edge until a sibling arrives.

## 9. Live Wayland session — switched

After the user logged out once and confirmed greetd/dms-greeter is the
login flow, the live session was wired up as follows:

| Artifact | State |
| --- | --- |
| `~/.local/bin/mango` (in PATH, first match) | `0.16.3(004f105c)` (patched) |
| `~/.config/mango/config.conf` | bounded-Niri (10149 bytes, `scroller_niri_view=1`) |
| `~/.config/mango/config.conf.pre-bounded-niri-20260907-220937` | previous stock config backup |
| `~/.local/share/wayland-sessions/mango.desktop` | `Exec=/home/drb0rk/.local/bin/mango -c /home/drb0rk/.config/mango/config.conf` |
| `/usr/share/wayland-sessions/mango.desktop` | stock `Exec=mango` (rollback fallback) |
| `/usr/bin/mango` | stock 0.16.1 (rollback fallback) |

How the patched session reaches the running compositor:

1. greetd runs `dms-greeter --command mango` as `greeter`.
2. dms-greeter exec's `mango` via PATH (inherits session PATH that puts
   `~/.local/bin` ahead of `/usr/bin`).
3. `~/.local/bin/mango` is the patched binary (`0.16.3(004f105c)`).
4. With no `-c` flag, mango loads its default config
   `~/.config/mango/config.conf`, which is now the bounded-Niri config.
5. The config reload was triggered on the running session via
   `mmsg dispatch reload_config` so no logout was required to activate
   the bounded-Niri options.

To roll back to stock, replace `~/.config/mango/config.conf` with the
backup (or delete `~/.local/bin/mango` and `/usr/local/share/wayland-
sessions/mango.desktop` if present, so PATH resolves to `/usr/bin/mango`).
The user-local desktop entry above is the authoritative override for
`uwsm start -- mango` and any future login through greetd/dms-greeter.

## 10. Git status

```text
$ git rev-parse --abbrev-ref HEAD
main
$ git log --oneline -2
004f105c docs(scroller): record validation polish for bounded Niri mode
59a98e23 feat(scroller): implement bounded Niri mode in Mango 0.16.3
$ git rev-list --left-right --count github/main...HEAD
0   0   (local and remote in sync)
```

The repo is `git@github.com:DrB0rk/mango-bounded-niri.git` (private,
default branch `main`). Both commits are on `github/main`.

## 11. Known gaps (intentional)

- Touchpad/pointer viewport gestures — accepted as config but not wired.
- Closest-gap insertion on window drag — deferred.
- Manual-join cap on `scroller_stack_max` — only the auto-stack path
  enforces it; focusing a third client into a column currently stacks
  it.

These are pure additions over the wired surface and do not regress any
behavior that is already implemented.
