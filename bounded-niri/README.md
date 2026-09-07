# Mango — bounded Niri mode

A patched Mango 0.16.3 source tree that implements Niri's scrolling
mechanics with an explicit **bounded placement policy** on top.

| Pinned Mango | `efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc` |
| --- | --- |
| Pinned Niri  | `dd75865f547f0eac0e9b6c4d86d2cd00c0744252` |
| Mode flag    | `scroller_niri_view` (off by default; Mango behaves stock when off) |

This is **not** a stock-Niri port. It ports Niri's persistent-column,
viewport, focus, sizing, gesture, and drag-placement semantics, then
overlays the user's bounded policy:

- 50 % minimum column width (`scroller_min_proportion`).
- First window opens at 100 %, stored width is restored once a sibling
  arrives (Niri stores 50 % on first window).
- Two-client limit per column (Niri unbounded).
- Hover-focus never auto-pans partially off-screen windows.
- Singleton column fills the full monitor edge.
- `SUPER+A` restore-from-expel is bounded, not infinite.

## Repository layout

```
bounded-niri/
├── README.md                              ← this file
├── runbook.md                             ← full spec + observable-behavior table
├── config-bounded-niri.conf               ← Mango config for the patched build
└── config.pre-bounded-niri-stock-baseline ← last known-good stock config

src/, include/, mmsg/, meson.build         ← Mango source with bounded patches
docs/, README.md (upstream), etc.          ← upstream Mango documentation
```

## Patch shape

```
 include/mango/common/server.h       |   1 +
 include/mango/common/types.h        |   9 +++
 include/mango/config/parse_config.h |   9 +++
 include/mango/dispatch/bind.h       |   3 +
 include/mango/layout/scroll.h       |   5 ++
 include/mango/manage/client.h       |   2 +
 src/config/parse_config.c           |  57 ++++++++++
 src/dispatch/bind.c                 | 144 ++++++++++++++++++++++++++++++++-
 src/input/pointer.c                 |   4 +-
 src/input/tablet.c                  |   2 +-
 src/layout/scroll.c                 |  92 +++++++++++++++++++-
 src/manage/client.c                 |  43 +++++++++-
 12 files changed, 359 insertions(+), 12 deletions(-)
```

Branch: `feature/bounded-niri-scroller`.

## Build

```sh
meson setup build --prefix=$HOME/.local --buildtype=debugoptimized
meson compile -C build
meson install -C build --no-rebuild
```

The user-local build lands at `~/.local/bin/mango` (and `~/.local/bin/mmsg`)
and **does not** overwrite the system `/usr/bin/mango`.

## Run

Point a Wayland session at the patched binary + alternate config:

```sh
~/.local/bin/mango -c ~/src/mango-bounded-niri/bounded-niri/config-bounded-niri.conf
```

A pre-built session entry that does exactly that is installed at
`~/.local/share/wayland-sessions/mango-bounded-niri.desktop` (select
**Mango (bounded Niri)** in your login manager).

## Rollback

Stock Mango + stock config is unaffected. To go back, just log into the
stock session entry. The bounded build and config are fully isolated.

## License

Mango is GPL-3.0-or-later. Niri is GPL-3.0-or-later. License compatibility
is preserved. The bounded-Niri algorithms are derived from Niri at the
pinned commit; project notices are kept in-file. Any redistribution must
include or offer the corresponding source.
