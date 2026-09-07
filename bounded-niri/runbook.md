# Niri mechanics with bounded Dwindle placement in Mango

**Audience:** an AI coding agent operating the user's Linux laptop

**Research baseline:** 2026-09-07

**Pinned Niri source:** `dd75865f547f0eac0e9b6c4d86d2cd00c0744252`

**Pinned Mango source:** `efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc`

**Deliverable:** implement, configure, test, and troubleshoot Niri's scrolling mechanics inside Mango while retaining the user's explicit bounded placement policy.

## Read this first: literal Niri parity and the requested policy are different

It is not possible to enable this behavior with Mango configuration alone. Exact observable Niri-style scrolling requires a source change because current Mango has no independent per-tag viewport, no gesture/animation viewport state machine, and no persistent column object.

There is also no single behavior that is simultaneously stock Niri and the user's requested behavior. The implementation target in this document is therefore:

> **Niri mechanics plus the user's bounded placement policy.** Port Niri's persistent-column, viewport, focus, sizing, gesture, and drag-placement semantics. Override only the new-window policy, the two-client limit, the 50% minimum, hover-without-pan, singleton width, and `SUPER+A` restoration as explicitly requested.

Do not describe the result as an unmodified copy of Niri. Use the term **bounded Niri mode**. The exact differences are intentional and testable:

| Behavior | Stock Niri at pinned revision | Required bounded Niri mode |
| --- | --- | --- |
| First normal window | Default column width is 50% | Display at 100% while it is the only head; retain stored width 50% |
| Second normal window | New 50% column to the right | Same |
| Third and later normal windows | Every normal new window is a new column to the right of the active column | W3, W6, W9, ... join the head that was focused before mapping; other windows become new right-hand heads |
| Maximum members in a normal column | Unlimited | Two |
| Minimum column width | No 50% floor; defaults include 1/3 | 50%, so at most two normal heads fit on the primary axis |
| Focus follows mouse with zero scroll | Rejects the focus change if activation would scroll | Accepts focus but keeps the camera pixel-for-pixel fixed |
| Maximize a member of a normal multi-window column | Expels the member into a new column, then maximizes it | Temporarily show that member alone and restore its original column, slot, sizes, and camera on toggle-off |
| Maximize whole column | `maximize-column`; column may retain several windows | Provide separately as `toggle_full_width_column` |
| Width adjustment | Fixed pixels or percentages; no 50% policy floor | Preserve Niri's percentage-point semantics but clamp to 50%–100% |
| Vertical scrolling strip | Niri's strip is horizontal | Transpose the same model for Mango's `vertical_scroller` |

If the user later requests literal stock Niri policy, set the bounded overrides to zero, make normal new windows create columns, use a 0% lower bound, use reject-on-scroll pointer focus, and use stock Niri maximize-to-edges expulsion. Do not silently change modes.

## Exact bounded behavior contract

### Vocabulary

- **Primary axis:** strip direction: X for `scroller`, Y for `vertical_scroller`.
- **Cross axis:** direction of windows inside a head: Y for `scroller`, X for `vertical_scroller`.
- **Head/column:** a persistent item in the strip with its own stable ID, stored width/height, ordered tiles, active tile, and state flags.
- **Camera/view position:** the primary-axis content coordinate shown at the start of the work area. It must exist independently of the focused client's current geometry.
- **Full usable area:** Mango's work area after layer-shell reservations and the configured normal gaps/borders. It is not Wayland fullscreen.

### Horizontal sequence

| New tiled window | Result | Logical state |
| --- | --- | --- |
| W1 | Fills usable screen | `C1=[W1]`; stored width `0.50`, effective singleton width `1.00` |
| W2 | Opens on right; 50/50 visible | `C1=[W1] C2=[W2]` |
| W3 | Splits the focused W2 head top/bottom 50/50 | `C1=[W1] C2=[W2,W3]` |
| W4 | New 0.50 head after focused head; camera moves right enough to reveal it | `... C3=[W4]` |
| W5 | New 0.50 head to the right; camera reveals it | `... C3=[W4] C4=[W5]` |
| W6 | Splits **whichever head was focused before W6 mapped** | for normal focus, `... C4=[W5,W6]` |
| W7–W9 | Repeat new head, new head, split focused head | W9 is another split event |

W6 is not hard-coded to W5. If the user focuses W2 before opening W6, W6 must attempt to join W2's head. If that head already has two members, W6 remains a standalone 0.50 head after W2; it must not create a third member.

### Vertical sequence

Transpose the policy, do not create a separate algorithm:

- W1 fills the usable area.
- W2 is below W1 and both are 50% of usable height.
- W3 shares W2's head left/right at 50/50.
- W4 and W5 become new half-height heads below and scroll into view.
- W6 joins the head focused before it maps, splitting left/right, if capacity permits.
- `SUPER+<` and `SUPER+>` change head height in this mode.

### General placement invariants

1. Count mapped, visible, non-floating, non-minimized tiling clients on the same monitor and tag. Ignore dialogs, launchers, PIP, scratchpads, and clients made floating by rules.
2. Use the current live count including the new client. Split phases are `count % 3 == 0`.
3. Capture the focused target before mapping or focus is transferred to the new client.
4. A new standalone head is inserted immediately after the captured active/focused head, matching Niri's right-of-active insertion rather than always appending globally.
5. An automatic join is permitted only when target and new client are on the same monitor, tag, layout, and orientation, both are tiled, and target count is below two.
6. A failed join falls back to a standalone head after the intended target. Never lose, hide, or attach the client to a guessed target.
7. A new two-client normal head begins with equal automatic cross-axis weights. Later interactive resizing may make it unequal.
8. Closing, moving, floating, swallowing, minimizing, retagging, or destroying a client must update head membership exactly once.

### Focus contract

- Keyboard, IPC, activation, and new-window focus activate the destination head and run Niri's fit/center camera policy.
- Pointer/tablet hover changes keyboard focus and the active tile but must not alter the absolute camera position or any client geometry.
- Hover must not raise a floating window unless a separate Mango policy explicitly requests raising.
- Merely setting `edge_scroller_pointer_focus=0` is wrong: it prevents some focus changes instead of accepting focus without movement.
- Merely skipping `arrange()` is also incomplete once an explicit camera exists. Call the common activation path with `KEEP_CAMERA`, update active head/tile, and render from the unchanged camera.

### Maximize contract

- `SUPER+A` toggles a custom maximize-to-work-area state for the focused client.
- If the client is one member of a two-client head, maximizing hides the sibling from rendering and hit-testing but does not destroy membership, ordering, width, weights, or stable IDs.
- Toggle-off restores the same sibling, same before/after slot, same primary size, same cross-axis weights, and same camera. With unchanged output/configuration, geometry may differ by at most one physical pixel due to rounding.
- If the sibling closes or moves while the client is maximized, the head collapses safely to the remaining client. There is no wrong-sibling recovery.
- Keep Niri's whole-column full-width operation as a separate dispatcher. It changes the effective head size to 100% without overwriting the stored normal size.
- Wayland fullscreen remains distinct and must retain protocol semantics.

### Size contract

- Store a column size as either `PROPORTION` or `FIXED`, as Niri does. For this profile, new columns use `PROPORTION(0.50)`.
- `SUPER+<` applies `AdjustProportion(-5)`, meaning minus five percentage points. `SUPER+>` applies `AdjustProportion(+5)`.
- Clamp the resolved stored proportion to `0.50..1.00` in bounded mode. Clamp every mutation path, not only the binding.
- Adjustment operates on the whole focused head, including both of its members.
- As in Niri, changing column size clears full-width/maximized state. Thus pressing `SUPER+<` while maximized-to-work-area exits maximize and sets a 95% column; it is not a no-op.
- Cross-axis resize changes the focused tile. Other automatic tiles absorb the remaining area according to weights and minimum-size constraints.

## Source findings the implementation must respect

### Niri

The pinned implementation is not a binary tree. [`ScrollingSpace`](https://github.com/YaLTeR/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/layout/scrolling.rs) owns an ordered `columns` vector, `active_column_idx`, `ViewOffset`, removal focus state, and `view_offset_to_restore`. Each `Column` owns ordered tiles, an active tile, a stable `ColumnId`, stored `ColumnWidth`, automatic/fixed height data, full-width state, and pending maximize/fullscreen state.

The details that matter:

- `add_column()` inserts a normal new column immediately after the active column, activates it, and computes a new view offset.
- `add_tile_at()` inserts a new tile with automatic weight `1`; two unconstrained automatic tiles therefore divide available cross-axis space equally.
- Column width is persistent. Full-width or maximize uses an effective width of 100%, while the normal width remains available for restoration.
- `set_column_width()` supports `SetFixed`, `SetProportion`, `AdjustFixed`, and `AdjustProportion`; percentage arguments are percentage points, not fractions.
- `activate_column()` delegates to the view-offset calculation. Vertical focus within the same column changes only the active tile.
- `ViewOffset` has `Static`, `Animation`, and `Gesture` states. Gesture release predicts momentum and snaps to column edges or centers.
- `insert_position()` chooses the closest gap between columns versus the closest gap between tiles. This is more precise than Mango's current target quadrants.
- `focus-follows-mouse max-scroll-amount="0%"` rejects activation when it would scroll. It is not equivalent to the requested accept-focus/keep-camera behavior.
- `maximize-column` and `maximize-window-to-edges` are different. The latter expels a member of a regular multi-window column before maximizing, so the requested exact slot restoration is a custom extension.

Niri's documented default normal width is 50%, with presets 1/3, 1/2, and 2/3; it does not make the first window full width by default. See the [Niri layout reference](https://niri-wm.github.io/niri/Configuration%3A-Layout.html), [input reference](https://niri-wm.github.io/niri/Configuration%3A-Input.html), [gestures](https://niri-wm.github.io/niri/Gestures.html), and [fullscreen/maximize distinctions](https://niri-wm.github.io/niri/Fullscreen-and-Maximize.html).

### Mango

At the pinned Mango revision, [`TagScrollerState`](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/include/mango/layout/scroll.h) contains only an `all_first` node list and a count. A `ScrollerStackNode` combines client membership and head sizing. The head order is reconstructed from the global client list in [`src/layout/scroll.c`](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/src/layout/scroll.c), and the focused client's old geometry acts as the camera anchor.

Consequences:

- Mango has no stable head identity independent of a client.
- There is no explicit camera coordinate, so exact finger-tracked scrolling and focus-without-pan cannot be made robust with config.
- `scroller_insert_stack()` deletes/recreates the source node and has no size cap.
- `client_set_maximize_screen()` calls `exit_scroller_stack()`, which frees membership; toggle-off only arranges, so original sibling/slot data is gone.
- `set_proportion()` is absolute, and `resize_tile_scroller()` currently clamps primary proportions as low as 0.1.
- pointer/tablet sloppy focus calls `client_focus(c, 0)`, and scroller focus can cause `arrange()` to reposition the strip.

The older Mango source layout used implementation-heavy `.h` files. Current upstream uses `src/**/*.c` and declarations under `include/mango/**/*.h`. Do not apply an older patch against `src/layout/scroll.h`, `src/manage/client.h`, or `src/dispatch/bind_define.h`; those paths are obsolete for the pinned revision.

### License and attribution

The pinned Niri workspace declares `GPL-3.0-or-later`, and Mango's license grants GPL version 3 or later. The licenses are compatible for this source-level port, but distributing the patched Mango binary also requires distributing or offering its corresponding source under the GPL. Preserve both projects' notices, identify Niri-derived algorithms in comments and commit history, and include the exact Niri commit used. Do not copy code while stripping authorship or license information. This document is implementation guidance, not a substitute for project-specific legal review.

## Implementation architecture

This is a medium subsystem patch, not a small config patch. Refactor scroller state first, then add policy. Do not pile more flags onto the existing disposable linked-node model.

### Persistent per-tag model

Add an explicit ordered column list to `include/mango/layout/scroll.h`. Exact field names may follow Mango style, but the ownership and invariants must match this model:

```c
enum ScrollerSizeKind {
	SCROLLER_SIZE_PROPORTION,
	SCROLLER_SIZE_FIXED,
};

struct ScrollerSize {
	enum ScrollerSizeKind kind;
	double value; /* 0.50 for a proportion, logical pixels for fixed */
};

enum ScrollerViewKind {
	SCROLLER_VIEW_STATIC,
	SCROLLER_VIEW_ANIMATION,
	SCROLLER_VIEW_GESTURE,
};

struct ScrollerTile {
	struct wl_list link;
	Client *client;
	double auto_weight;
	double fixed_cross_size;
	bool cross_size_is_fixed;
};

struct ScrollerColumn {
	struct wl_list link;
	uint64_t id;
	struct wl_list tiles;
	size_t tile_count;
	struct ScrollerTile *active_tile;
	struct ScrollerSize normal_size;
	int32_t preset_size_index;
	bool full_width;
	Client *maximized_tile;
};

struct ScrollerView {
	enum ScrollerViewKind kind;
	double current;
	double start;
	double target;
	double velocity;
	uint64_t started_msec;
	/* 150 ms gesture history plus DnD edge-scroll state */
};

struct TagScrollerState {
	struct wl_list columns;
	struct ScrollerColumn *active_column;
	uint64_t next_column_id;
	struct ScrollerView view;
	bool activate_previous_on_removal;
	double previous_view_to_restore;
	bool has_previous_view_to_restore;
	double maximize_view_to_restore;
	bool has_maximize_view_to_restore;
};
```

Do not use a fixed two-element tile array. The core should support arbitrary Niri-style columns; `scroller_stack_max=2` is a bounded policy validation applied at insertion. This separation makes stock-parity tests possible and prevents the renderer from becoming the cap enforcer.

Every mapped scroller client belongs to exactly one live `ScrollerTile`; every tile belongs to exactly one live column; empty columns are destroyed; IDs never change while a column lives. Add debug assertions for these invariants.

### Axis abstraction

Use one layout engine with an axis descriptor rather than maintaining near-duplicate horizontal and vertical implementations:

```c
struct ScrollerAxis {
	bool vertical;
	int32_t work_start;
	int32_t work_extent;
	int32_t cross_start;
	int32_t cross_extent;
	int32_t primary_gap;
	int32_t cross_gap;
};
```

Helpers read/write primary and cross components of `wlr_box`. `scroller()` and `vertical_scroller()` construct the descriptor and call the same arrange, activation, sizing, hit-test, insertion, and gesture helpers. A horizontal-only fix that is then copied into vertical code is not acceptable.

### Independent camera

Compute each column's content coordinate from ordered resolved sizes and gaps. Render it at:

```text
screen_primary = work_start + column_content_primary - view.current
```

Never derive `view.current` from the selected client's old `geom`. Activation accepts an explicit policy:

```c
enum ScrollerActivateViewPolicy {
	SCROLLER_VIEW_FIT,       /* keyboard, IPC, map, activation */
	SCROLLER_VIEW_CENTER,    /* configured center mode */
	SCROLLER_VIEW_KEEP,      /* requested pointer hover */
	SCROLLER_VIEW_RESTORE,   /* maximize/removal restoration */
};
```

For `FIT`, reproduce Niri's `compute_new_view_offset_fit()` semantics: keep the camera if the column fits; otherwise move only enough to reveal the relevant edge, with normal gap padding. For `CENTER`, center a narrower column and align an oversized one to the working-area start. Support `never`, `always`, and `on-overflow` center policies. Round the final position at the physical-pixel boundary, not during intermediate accumulation.

When a column is inserted, removed, or resized before the active column, compensate `view.current` by the exact content-coordinate delta so the stationary content does not jump. When changing active column while policy is `KEEP`, leave the absolute camera unchanged. An implementation that changes active column and then reconstructs the camera from active-relative geometry will reintroduce the hover bug.

### Tile size distribution

Port Niri's observable normal-column algorithm:

1. Convert fixed window cross sizes to decorated tile sizes.
2. Subtract outer and inter-tile gaps.
3. Allocate exact-minimum clients first.
4. Iteratively allocate remaining space by automatic weights, pinning any client whose minimum is violated.
5. Round requested Wayland window sizes to logical pixels, then distribute the remainder without cumulative drift.
6. A new automatic tile has weight `1`. Reset the remaining tile to weight `1` when a two-tile column becomes one tile.

For this bounded policy only zero, one, or two tiles normally occur, but implement the general algorithm before enforcing the cap.

## Patch sequence for the laptop agent

### Phase 1: preflight and backups

Determine the binary, package, instance, config root, and includes without changing anything:

```bash
command -v mango
type -a mango
readlink -f "$(command -v mango)"
mango -v
mmsg get version
printf 'MANGO_INSTANCE_SIGNATURE=%s\n' "${MANGO_INSTANCE_SIGNATURE:-unset}"
ps -eo pid=,comm=,args= | rg '(^|[[:space:]])mango([[:space:]]|$)' || true
```

On Arch/CachyOS also record package ownership:

```bash
pacman -Qo "$(readlink -f "$(command -v mango)")" 2>/dev/null || true
pacman -Q | rg -i '^mango' || true
```

Find the active root config from the process arguments. Otherwise use `${XDG_CONFIG_HOME:-$HOME/.config}/mango/config.conf`, falling back to `/etc/mango/config.conf`. Recursively inspect `source=` and `source-optional=` files. Resolve relative includes from the including file.

```bash
config_path=/replace/with/actual/config.conf
rg -n '^(source|source-optional)=' "$config_path"
rg -n '^(tagrule|windowrule|keymode|bind|mousebind|axisbind|gesturebind|scroller_|sloppyfocus|warpcursor|drag_tile_to_tile)=' "$config_path"
mmsg get all-monitors
mmsg get all-tags
mmsg get all-clients

stamp=$(date +%Y%m%d-%H%M%S)
backup_path="${config_path}.pre-bounded-niri-${stamp}"
cp -a -- "$config_path" "$backup_path"
printf 'Backup: %s\n' "$backup_path"
```

Do not install/remove packages, overwrite `/usr/bin/mango`, close real applications, or replace the user's config wholesale. Retain a known-good session entry and reachable TTY.

### Phase 2: pin and branch current Mango

```bash
git clone https://github.com/mangowm/mango.git mango-bounded-niri
cd mango-bounded-niri
git fetch origin
git checkout --detach efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc
git switch -c feature/bounded-niri-scroller
git rev-parse HEAD
```

If implementing against newer upstream, first repeat the source audit and update the pinned baseline. Do not assume current file paths or call flows survived a rebase.

### Phase 3: replace scroller state without changing defaults

Primary files at the pinned Mango revision:

| Area | Declaration/config | Implementation |
| --- | --- | --- |
| Column, tile, camera state | `include/mango/layout/scroll.h` | `src/layout/scroll.c` |
| Arrange and pointer resize | `include/mango/layout/arrange.h` | `src/layout/arrange.c` |
| Focus, map, maximize, destroy | `include/mango/manage/client.h` | `src/manage/client.c` |
| Pointer drag/focus | `include/mango/input/pointer.h` | `src/input/pointer.c` |
| Tablet hover | relevant input header | `src/input/tablet.c` |
| Actions | `include/mango/dispatch/bind.h` | `src/dispatch/bind.c` |
| Config fields/parser | `include/mango/config/parse_config.h` | `src/config/parse_config.c` |
| IPC diagnostics | IPC headers | `src/ipc/ipc.c` |

First migrate current `ScrollerStackNode` state into persistent columns and tiles while keeping existing default placement and focus behavior. Required lifecycle hooks include map, unmap, destroy, retag, move-monitor, floating toggle, minimize/restore, swallow/unswallow, config reload, and monitor destruction.

Do not free a column merely because its active client leaves when another tile remains. Choose the next tile, preserve column size, and retain its stable ID. Do not store unvalidated raw pointers across a destruction callback.

Add a normalized diagnostic dump before adding the custom policy:

```json
{
  "tag": 2,
  "orientation": "horizontal",
  "active_column_id": 17,
  "view": {"kind":"static", "current":960.0, "target":960.0},
  "columns": [
    {"id":16,"size":{"kind":"proportion","value":0.5},"full_width":false,"tiles":[41]},
    {"id":17,"size":{"kind":"proportion","value":0.5},"full_width":false,"tiles":[42,43]}
  ]
}
```

Expose it as an `mmsg get` response or debug log gated behind an option. Geometry alone is insufficient to diagnose stable identity and camera state.

### Phase 4: port Niri camera and gestures

Port behavior from Niri's `ViewOffset`, `activate_column*`, `compute_new_view_offset*`, `animate_view_offset*`, and `view_offset_gesture_*` functions. Reimplement in C using Mango's frame clock; do not block input with sleeps.

Gesture constants/semantics at the pinned Niri revision:

- touchpad normalization: one work-area width per 1200 units of input movement;
- velocity history: most recent 150 ms;
- projected stopping point: `position - velocity / (1000 * ln(0.997))`;
- on release, generate snap points for left/right column edges, or centers when centering is enabled;
- select the closest projected snap, update the active column in the gesture direction, and animate from current position with the measured velocity;
- drag-and-drop edge scrolling has a configurable delay and maximum speed and must clamp broad off-strip bounds;
- cancellation behavior matches Niri: finish the gesture rather than attempting to restore an old active column.

Hook the gestures to Mango's equivalents:

- extend Mango's `moveresize` pointer-grab argument parser with `curview`, then make `SUPER` + middle-button drag move the camera;
- three-finger horizontal touchpad swipe moves the horizontal camera if Mango's input layer exposes swipe events;
- in `vertical_scroller`, transpose those deltas to the vertical camera;
- while moving a client or external DnD item, holding at a primary-axis output edge starts delayed edge scroll.

If the installed wlroots/Mango input path lacks a needed gesture event, report that compile-time/runtime capability explicitly. Do not claim exact continuous gestures when only discrete focus commands work.

At the pinned revision, Mango's normal `gesturebind` path recognizes a direction and dispatches an action; that is not sufficient for Niri parity. The scroller must receive gesture begin, every raw update delta with its monotonic timestamp, and end/cancel. Keep discrete `gesturebind` behavior for unrelated actions.

### Phase 5: exact insertion and drag placement

Replace target-quadrant placement in `scroller_drop_tile()` with Niri's closest-gap model:

1. Transform pointer primary coordinate from screen space to content space by adding `view.current`.
2. Aim at the center of configured gaps.
3. Find the nearest insertion gap between column boundaries.
4. Find the containing column and nearest gap between its tile boundaries.
5. Compare distances. Choose `NEW_COLUMN(index)` when the column-gap distance is smaller or equal; otherwise choose `IN_COLUMN(column_id, tile_index)`.
6. Draw an insertion hint from the selected position.
7. Validate the destination again at release because clients may close or move during the drag.

Use Niri's 8-pixel movement threshold before detaching a tiled client. Preserve the pointer's ratio within the client, its normal size, and original column information during the grab. On output transitions, update scale/transform and destination tag rules.

Bounded override: if `IN_COLUMN` targets a two-tile column, convert the result to the nearest valid `NEW_COLUMN` gap instead of rejecting the drop and leaving the client ambiguously floating. The client must become a visible standalone head.

Centralize all insertion through functions returning status:

```c
enum ScrollerInsertResult {
	SCROLLER_INSERT_OK,
	SCROLLER_INSERT_FULL,
	SCROLLER_INSERT_INVALID,
};

enum ScrollerInsertResult
scroller_insert_tile(struct TagScrollerState *state,
					 struct ScrollerTile *tile,
					 uint64_t column_id,
					 size_t tile_index);
```

Capacity must be checked before detaching from the source. Automatic map, keyboard consume/expel, pointer move, IPC move, and restoration all use this function.

### Phase 6: bounded `1,1,2` map policy

In `src/manage/client.c`, capture the selected scroller tile before the new client becomes selected. After rules decide that the new client is tiled and after it has a destination monitor/tag, call a policy helper.

```c
bool split_phase = live_tiled_count >= 3 && live_tiled_count % 3 == 0;

if (split_phase && captured_target_is_valid &&
	column_tile_count(target_column) < config.scroller_stack_max) {
	insert_new_tile_after_active_tile(target_column, new_client);
	set_all_cross_sizes_auto(target_column);
} else {
	insert_new_column_after(target_or_active_column, new_client,
						SCROLLER_SIZE_PROPORTION, 0.50);
}
activate_new_tile_with_policy(new_client, SCROLLER_VIEW_FIT);
```

`set_all_cross_sizes_auto()` should preserve existing visual ratios when converting manually sized members, as Niri's `convert_heights_to_auto()` does; for a brand-new split, set both weights to `1` to guarantee initial 50/50.

Do not use a historical window counter. Recompute the eligible live count. Do not let transient/floating windows shift W3/W6 phases.

### Phase 7: focus origins and hover-without-pan

Replace the ambiguous `lift`-only distinction with an origin and view policy:

```c
enum FocusOrigin {
	FOCUS_POINTER,
	FOCUS_TABLET,
	FOCUS_KEYBOARD,
	FOCUS_IPC,
	FOCUS_ACTIVATION,
	FOCUS_MAP,
};

void client_focus_with_origin(Client *c, bool raise,
					  enum FocusOrigin origin);
```

Keep `client_focus()` as a compatibility wrapper during the refactor, then migrate call sites deliberately. Pointer and tablet enter use `SCROLLER_VIEW_KEEP`; directional keyboard/IPC focus and new maps use `SCROLLER_VIEW_FIT`. `FOCUS_ACTIVATION` follows the user's existing `focus_on_activate` policy.

In keep-view activation:

1. Preserve `view.current`, animation target, and all column content coordinates.
2. Change `active_column` and `active_tile`.
3. Update seat keyboard focus, activation flags, border/opacity, focus history, input method, and foreign-toplevel state.
4. Do not raise solely because of hover.
5. Arrange only if needed to update focus visuals; arrangement must render from the unchanged camera.

Also implement Niri-compatible `reject-scroll` as another option for parity testing: precompute the required activation scroll distance and reject if it exceeds `max_scroll_amount`. The requested setting is `keep-view`, not `reject-scroll`.

### Phase 8: reversible maximize and full-width column

For bounded `SUPER+A`, do not call `exit_scroller_stack(c)`. The persistent column remains intact:

```text
normal column [A,B]
  -> save camera once
  -> column.maximized_tile = B
  -> effective primary size = work-area size
  -> render/hit-test/configure B only
  -> disable A's scene subtree and exclude A from directional hit testing
toggle off
  -> clear maximized_tile
  -> re-enable A
  -> restore normal_size, weights, active tile and saved camera
```

Handle cleanup centrally. If a hidden sibling is destroyed/moved/floated, remove its tile, re-enable any surviving tile, and clear invalid focus references. If the maximized tile leaves, clear maximize state before detaching. Never leave a disabled scene node after the state ends.

Add a separate `toggle_full_width_column` dispatcher matching Niri's `maximize-column`: toggle `column.full_width`, use effective `PROPORTION(1.0)`, preserve `normal_size`, and keep both tiles visible. If the column is custom maximized-to-work-area, treat full-width toggle as unmaximize and clear both temporary flags, mirroring Niri's intuitive state transition.

Changing size with a `SizeChange` clears full-width and custom maximize, re-enables hidden siblings, then applies the size. Store `maximize_view_to_restore` only for a normal-to-maximized transition; clear it after active-column changes that make restoration unsafe.

### Phase 9: Niri size actions plus the 50% floor

Add a parser for:

```text
set_column_size,50%
set_column_size,1280
adjust_column_size,+5%
adjust_column_size,-100
toggle_full_width_column
```

Represent parsed actions as the four Niri variants: set fixed, set proportion, adjust fixed, adjust proportion. A proportional adjustment converts a fixed current size into a work-area proportion first. Include gaps and decoration extra size consistently when converting, as Niri does.

In bounded mode clamp proportions to `0.50..1.00`. Apply the same lower bound in:

- action dispatch;
- pointer `resize_tile_scroller()` primary-axis branch;
- window rules and restored state;
- preset switching;
- config reload migration;
- arrangement as a final defensive assertion.

Do not clamp the effective singleton/full-width/maximized size down to 0.50. The stored normal size remains distinct from its effective display size.

### Phase 10: configuration plumbing

Add options in `include/mango/config/parse_config.h` and parse/default/clamp them in `src/config/parse_config.c`. Preserve current upstream behavior when the bounded mode is disabled.

| Option | Compatible default | Requested value | Purpose |
| --- | --- | --- | --- |
| `scroller_niri_view` | `0` | `1` | explicit persistent camera, animations, snapping |
| `scroller_stack_max` | `0` | `2` | zero means unlimited |
| `scroller_auto_stack_every` | `0` | `3` | zero means normal new-column placement |
| `scroller_min_proportion` | `0.0` | `0.5` | bounded primary-axis floor |
| `scroller_pointer_focus_mode` | `scroll` | `keep-view` | `scroll`, `reject-scroll`, or `keep-view` |
| `scroller_restore_stack_after_maximize` | `0` | `1` | custom reversible individual maximize |
| `scroller_niri_gap_drop` | `0` | `1` | closest-gap insert positions and hints |
| `scroller_view_gesture_fingers` | `0` | `3` | zero disables continuous view swipe; otherwise required finger count |
| `scroller_dnd_edge_scroll` | `0` | `1` | delayed primary-edge camera movement while dragging |

Reject invalid enums and malformed percentages with a config error. Do not silently interpret `5%` as `5.0` or `0.05` inconsistently. Document every option and add it to the example config.

## Configuration and bindings

Merge this block into the active config. Remove or update conflicting scalar settings; do not append duplicates blindly. Every custom option/action below requires the patched binary.

```ini
# BEGIN bounded-niri-scroller

scroller_structs=0
scroller_default_proportion=0.5
scroller_ignore_proportion_single=0
scroller_default_proportion_single=1.0
scroller_proportion_preset=0.5,1.0
scroller_prefer_overspread=0
scroller_focus_center=0
scroller_prefer_center=0

# Patched Niri-mechanics options.
scroller_niri_view=1
scroller_stack_max=2
scroller_auto_stack_every=3
scroller_min_proportion=0.5
scroller_pointer_focus_mode=keep-view
scroller_restore_stack_after_maximize=1
scroller_niri_gap_drop=1
scroller_view_gesture_fingers=3
scroller_dnd_edge_scroll=1

sloppyfocus=1
focus_on_activate=1
edge_scroller_pointer_focus=1
edge_scroller_focus_allow_speed=0.0
warpcursor=0

drag_tile_to_tile=1
drag_tile_small=1
mousebind=SUPER,btn_left,moveresize,curmove
mousebind=SUPER,btn_right,moveresize,curresize
mousebind=SUPER,btn_middle,moveresize,curview

focus_cross_monitor=0
focus_cross_tag=0
exchange_cross_monitor=0
tag_carousel=0

# Apply only to intended tags if the user has special-purpose tag rules.
tagrule=id:*,layout_name:scroller

keymode=default

bind=SUPER,h,focusdir,left
bind=SUPER,j,focusdir,down
bind=SUPER,k,focusdir,up
bind=SUPER,l,focusdir,right
bind=SUPER,Left,focusdir,left
bind=SUPER,Down,focusdir,down
bind=SUPER,Up,focusdir,up
bind=SUPER,Right,focusdir,right

bind=SUPER+SHIFT,h,exchange_client,left
bind=SUPER+SHIFT,j,exchange_client,down
bind=SUPER+SHIFT,k,exchange_client,up
bind=SUPER+SHIFT,l,exchange_client,right

bind=SUPER+CTRL+ALT,Left,scroller_stack,left
bind=SUPER+CTRL+ALT,Right,scroller_stack,right
bind=SUPER+CTRL+ALT,Up,scroller_stack,up
bind=SUPER+CTRL+ALT,Down,scroller_stack,down

# User-requested individual maximize with exact bounded-mode restoration.
bind=SUPER,a,togglemaximizescreen

# Literal Niri-style whole-column full width; choose another chord if occupied.
bind=SUPER,f,toggle_full_width_column

# Printed < and > on a common US XKB layout are Shift+comma/period.
bind=SUPER+SHIFT,comma,adjust_column_size,-5%
bind=SUPER+SHIFT,period,adjust_column_size,+5%

bind=SUPER,s,setlayout,scroller
bind=SUPER+SHIFT,s,setlayout,vertical_scroller

# END bounded-niri-scroller
```

### Effective controls

| Input | Horizontal `scroller` | Vertical `vertical_scroller` |
| --- | --- | --- |
| `SUPER+h/l` or arrows | focus previous/next head and fit it in view | focus left/right member inside a head |
| `SUPER+j/k` or arrows | focus top/bottom member | focus previous/next head and fit it in view |
| pointer hover | focus only; camera remains unchanged | focus only; camera remains unchanged |
| `SUPER+SHIFT+direction` | move/exchange directionally | transposed directional move/exchange |
| `SUPER+left drag` | Niri nearest-gap reinsert | same algorithm transposed |
| `SUPER+right drag` | primary or cross resize by grabbed edge | same algorithm transposed |
| `SUPER+A` | maximize one client; toggle restores same head/slot/view | same |
| `SUPER+F` | toggle whole head full width | toggle whole head full height |
| `SUPER+<` | subtract 5 percentage points from head width | subtract 5 points from head height |
| `SUPER+>` | add 5 percentage points to head width | add 5 points to head height |

Before merging, audit every included file for chord conflicts. Mango grammar is `bind[flags]=MODIFIERS,KEY,COMMAND,PARAMETERS`. Test the dispatcher through `mmsg` first, check `mmsg get keymode`, then use `wev` to confirm the actual keysym/modifiers. On non-US layouts, bind the observed symbol or a verified `code:NUMBER`; never guess a keycode.

## Build and static verification

Use current source paths:

```bash
git diff --check
clang-format -i \
  include/mango/layout/scroll.h \
  include/mango/layout/arrange.h \
  include/mango/manage/client.h \
  include/mango/dispatch/bind.h \
  include/mango/config/parse_config.h \
  src/layout/scroll.c \
  src/layout/arrange.c \
  src/manage/client.c \
  src/input/pointer.c \
  src/input/tablet.c \
  src/dispatch/bind.c \
  src/config/parse_config.c \
  src/ipc/ipc.c
git diff --check

meson setup build --prefix=/usr
meson compile -C build
./build/mango -v
./build/mango -c /path/to/test-config.conf -p
```

If `build/` already exists, use `meson setup --reconfigure build --prefix=/usr`. Do not delete an unknown build directory to cure a configuration problem.

Add focused unit tests for pure helpers even if Mango currently has little layout-test coverage:

- percentage/fixed `SizeChange` parsing and conversion;
- camera fit/center/keep calculations;
- content-coordinate compensation after insert/remove/resize;
- two automatic tile weights and minimum-size handling;
- nearest-gap `NEW_COLUMN` versus `IN_COLUMN` selection;
- two-client cap fallback;
- gesture history, velocity, projected endpoint, and snap selection;
- maximize state transitions and cleanup after either member disappears;
- horizontal/vertical transpose equivalence.

Run sanitizers in a nested/disposable session if the toolchain supports them. Exercise close/retag/output-removal during drag, gesture, and maximize; these are the highest-risk use-after-free paths.

## Runtime acceptance tests

Run with disposable terminals on an unused tag. Disable session restore. Wait for animations to settle before exact geometry comparisons. Capture both normal client IPC and the new scroller diagnostic state.

### Structure sequence

| Step | Horizontal acceptance | Vertical acceptance |
| --- | --- | --- |
| W1 | effective full usable width; stored `0.50` | effective full usable height; stored `0.50` |
| W2 | W1 left, W2 right at 50/50 | W1 above, W2 below at 50/50 |
| W3 | W2/W3 equal top/bottom members | W2/W3 equal left/right members |
| W4 | new half head to right; camera reveals it | new half head below; camera reveals it |
| W5 | another new half head right | another new half head below |
| W6 | joins the head focused before map | joins the head focused before map |
| W9 | repeats split phase | repeats split phase |

Explicit W6 focus test: before launching W6, focus a one-member older head rather than W5. W6 must split that focused head. Repeat with a full two-member head: W6 must fall back to a standalone head immediately after it.

### Camera and focus

1. Create enough heads to scroll.
2. Focus a partially visible head by hovering its visible portion.
3. Confirm keyboard input goes to it.
4. Compare diagnostic `view.current` and every client geometry before/after hover: they must be identical.
5. Use keyboard focus to select an off-screen head: it must animate into view using fit policy.
6. During a camera animation, hover another visible client: focus may change, but the current animation must not retarget because of hover.
7. Close a newly created active column without an intervening focus change: the previous column and its saved view should be restored, matching Niri removal behavior.

### Gestures

1. Drag the camera slowly; motion must track fingers continuously rather than jump per column.
2. Release with near-zero velocity; snap to the closest valid edge/center.
3. Flick in both directions; momentum projection must choose the expected farther snap without passing first/last bounds.
4. Change output scale and repeat; final physical positions must not accumulate gaps.
5. Hold a moving window at the primary-axis edge; edge scroll starts only after its delay and stops immediately after leaving the zone.
6. Repeat all tests in vertical mode with axes transposed.

### Drag rearrangement

- Move a standalone column before/between/after columns.
- Insert a client above and below a one-member horizontal column.
- Insert left/right in a one-member vertical column.
- Expel either member to a standalone column while preserving its normal size.
- Drop onto a full two-member column: it must choose a nearby standalone insertion, not create a third member or stay floating.
- Begin drag, close the proposed target, then release: validation must safely recompute or fall back.
- Verify the 8-pixel recognition threshold and insertion hint.

### Maximize and restoration

1. Create a two-member column, widen it to 75%, and make the cross split visibly unequal.
2. Put its head partially on screen using camera movement, then hover-focus the second member without moving the camera.
3. Capture column ID, tile IDs/order, normal size, weights, active tile, camera and pixel geometry.
4. Press `SUPER+A`: only the focused member fills the work area; sibling is not visible or hit-testable but remains in diagnostic membership.
5. Press `SUPER+A` again: all captured logical state must match, and pixel geometry must match within one physical pixel.
6. Repeat two cycles to detect stale snapshots.
7. While maximized, press `SUPER+<`: maximize must clear, sibling must reappear, and the column should become 95%.
8. Maximize again, then close/move/float the sibling before toggling off. The remaining client becomes a valid standalone column with no crash or disabled scene residue.
9. Test `SUPER+F` separately: both members remain visible while the whole column becomes full width/height, and toggle-off restores 75%.

### Width/height bounds

| Action | Result |
| --- | --- |
| at 50%, `SUPER+<` | remains 50% |
| at 50%, one `SUPER+>` | 55% |
| repeated `SUPER+>` | reaches 100%, never exceeds |
| repeated `SUPER+<` | returns to 50%, never below |
| pointer shrink | stops at 50% |
| preset/window-rule/config restore below 50% | clamps or rejects according to documented path |
| vertical mode | modifies height, not width |

At no point may three 50%-minimum normal heads be fully visible along the primary axis. A 2-by-2 grid is the densest normal fully visible arrangement.

### Differential Niri tests

For mechanics that are not intentional overrides, record the same abstract event trace in nested Niri and patched Mango:

```text
add columns -> focus left/right -> resize -> drag between gaps -> remove active
gesture begin -> deltas/timestamps -> gesture end
full-width on -> width adjust -> full-width off
```

Normalize output to ordered column IDs, tile indices, stored/resolved sizes, active indices, camera coordinate and chosen snap. Compare transitions, allowing only compositor-specific decoration pixels. Do not compare the six documented bounded overrides as if they were bugs.

## Troubleshooting

| Symptom | Cause to test first | Corrective action |
| --- | --- | --- |
| unknown custom config key/action | stock or wrong Mango binary | compare `type -a`, `readlink -f`, `mango -v`, `mmsg get version`; restart patched binary |
| W1 is half size | singleton effective size disabled | set singleton effective proportion to 1.0 without overwriting stored 0.50 |
| W3/W6 is standalone | wrong count, captured target lost, target full/invalid | inspect policy trace and column dump; full target fallback is expected |
| W6 always joins W5 | code uses newest/tail rather than pre-map focus | retain the captured focused column ID across map/rule processing |
| W5 joins too early | implementation fills any available column | require `live_count % 3 == 0` |
| a column has three members | cap applied only in map path | enforce centrally before every structural detach/insert |
| hover moves strip | camera still derived from focused geometry or activation always fits | use independent `view.current` and `KEEP_CAMERA` origin |
| hover refuses partially visible client | Niri `reject-scroll` mode accidentally enabled | configure `keep-view` |
| keyboard focus does not reveal target | all focus paths were changed to keep camera | keyboard/IPC/map must use `FIT` |
| camera jumps after insert/remove | content before active changed without compensation | add exact old/new active content-coordinate delta to view state |
| gesture jumps by columns | discrete focus bindings wired instead of continuous view state | connect raw deltas to `GESTURE`, then snap only on end |
| drag placement feels quadrant-based | old `drop_direction` path remains | compare closest column-gap and tile-gap distances |
| full column accepts dragged third client | cap checked after source detach | validate destination capacity first and redirect to `NEW_COLUMN` |
| unmaximize loses sibling/slot | `exit_scroller_stack()` still called | retain persistent column membership for bounded maximize |
| sibling remains invisible after unmaximize | scene disable cleanup incomplete | centralize state exit and assert every non-maximized tile is enabled |
| `SUPER+<` does nothing while maximized | old no-op rule retained | clear maximize, restore siblings, then apply Niri `SizeChange` from effective 100% |
| width drifts after repeated changes | size derived from rounded client geometry | mutate stored `ScrollerSize`; resolve/round only during arrange |
| three heads fit | only binding is clamped | clamp action, pointer, rules, restore, presets, reload and defensive arrange |
| vertical mode differs | duplicated algorithms diverged | use one axis-generic engine and transpose tests |
| update breaks patch | upstream refactor or package replacement | rebase from recorded commit, repeat source audit, rebuild custom package and rerun suite |

## Safe activation and rollback

Test from a TTY, a nested compositor, or a separate session entry. Prefer a user-local prefix or a package-managed custom build; never overwrite the running package-owned executable.

Validate the exact binary and config that will run:

```bash
/absolute/path/to/patched/mango -c "$config_path" -p
readlink -f /absolute/path/to/patched/mango
/absolute/path/to/patched/mango -v
```

A config reload cannot load compositor machine code. Restart into the patched session for initial testing.

Rollback configuration:

```bash
cp -a -- "$backup_path" "$config_path"
/absolute/path/to/known-good/mango -c "$config_path" -p
```

Then select the known-good session or reinstall the previously cached package version through the package manager. Keep the source branch until rollback is verified.

## Required completion report

The laptop agent must report all fields and must not call the result exact if any parity test is skipped:

```text
Installed Mango before:
Pinned/rebased Mango commit:
Niri reference commit:
Patched binary path/version:
Active config root and includes changed:
Backups:
Config validation:
Build/unit/sanitizer results:
Horizontal W1-W9:
Vertical W1-W9:
W6 splits pre-map focused head:
Two-member cap (map/key/drag/IPC):
Hover changes focus:
Hover leaves camera/geometry unchanged:
Keyboard/IPC focus still fits target:
Gesture continuous tracking/momentum/snap:
DnD edge scroll:
Nearest-gap drag and 8px threshold:
SUPER+A restores column ID/slot/weights/size/view:
SUPER+A sibling-close/move cleanup:
Whole-column full-width toggle:
SUPER+< and SUPER+> percentage-point behavior:
All 50% clamp paths:
Vertical transpose equivalence:
Known deviations from pinned Niri:
Intentional bounded-policy deviations:
Rollback command/path:
```

Hard failures include: focus changes with camera movement on hover; W6 joining a different one-member head than the pre-map focus; any normal three-member column; three minimum-size heads fully visible; maximize losing a still-live sibling or leaving it disabled; discrete rather than continuous gesture movement; or a claimed Niri parity result without a persistent camera.

## Primary sources

- [Pinned Niri scrolling implementation](https://github.com/YaLTeR/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/layout/scrolling.rs)
- [Pinned Niri move-grab implementation](https://github.com/YaLTeR/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/input/move_grab.rs)
- [Pinned Niri swipe tracker](https://github.com/YaLTeR/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/input/swipe_tracker.rs)
- [Pinned Niri focus-follows-mouse call path](https://github.com/YaLTeR/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/src/niri.rs)
- [Pinned Niri layout defaults](https://github.com/YaLTeR/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/niri-config/src/layout.rs)
- [Pinned Niri IPC `SizeChange` definitions](https://github.com/YaLTeR/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/niri-ipc/src/lib.rs)
- [Pinned Niri license metadata](https://github.com/YaLTeR/niri/blob/dd75865f547f0eac0e9b6c4d86d2cd00c0744252/Cargo.toml)
- [Niri layout documentation](https://niri-wm.github.io/niri/Configuration%3A-Layout.html)
- [Niri input and focus-follows-mouse documentation](https://niri-wm.github.io/niri/Configuration%3A-Input.html)
- [Niri gestures documentation](https://niri-wm.github.io/niri/Gestures.html)
- [Niri fullscreen and maximize documentation](https://niri-wm.github.io/niri/Fullscreen-and-Maximize.html)
- [Pinned Mango scroller declarations](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/include/mango/layout/scroll.h)
- [Pinned Mango scroller implementation](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/src/layout/scroll.c)
- [Pinned Mango arrange/resize implementation](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/src/layout/arrange.c)
- [Pinned Mango client focus/map/maximize implementation](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/src/manage/client.c)
- [Pinned Mango pointer implementation](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/src/input/pointer.c)
- [Pinned Mango binding implementation](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/src/dispatch/bind.c)
- [Pinned Mango config parser](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/src/config/parse_config.c)
- [Pinned Mango license](https://github.com/mangowm/mango/blob/efb5ed9bce19e0ae260ca34b35cfcdc9d4d8b2fc/LICENSE)
- [Mango layout documentation](https://mangowm.github.io/docs/window-management/layouts/)
- [Mango key dispatcher documentation](https://mangowm.github.io/docs/bindings/keys/)
- [Mango pointer/gesture binding documentation](https://mangowm.github.io/docs/bindings/mouse-gestures/)
