#include "mango/layout/panel.h"

#include "mango/common/server.h"
#include "mango/config/parse_config.h"
#include "mango/dispatch/bind.h"
#include "mango/draw/text-node.h"
#include "mango/layout/layout.h"
#include "mango/manage/monitor.h"
#include <linux/input-event-codes.h>
#include <scenefx/types/wlr_scene.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PANEL_WIDTH 450
#define PANEL_HEIGHT 274
#define PANEL_MARGIN 24

enum panel_control {
	PANEL_LAYOUT_TILE,
	PANEL_LAYOUT_SCROLLER,
	PANEL_LAYOUT_VERTICAL,
	PANEL_GAP_DOWN,
	PANEL_GAP_UP,
	PANEL_SNAP,
	PANEL_CLOSE,
	PANEL_CONTROL_COUNT,
};

struct layout_panel_state {
	Monitor *mon;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *background;
	struct wlr_scene_rect *border;
	MangoJumpLabel *title;
	MangoJumpLabel *layout_label;
	MangoJumpLabel *layout_buttons[3];
	MangoJumpLabel *spacing_label;
	MangoJumpLabel *gap_down;
	MangoJumpLabel *gap_value;
	MangoJumpLabel *gap_up;
	MangoJumpLabel *snap_label;
	MangoJumpLabel *snap_button;
	MangoJumpLabel *close;
	MangoJumpLabel *hint;
	int focus;
	int32_t x;
	int32_t y;
};

static struct layout_panel_state panel;

static const char *const panel_layout_names[] = {"tile", "scroller",
												 "vertical_scroller"};
static const char *const panel_layout_labels[] = {"Tiled", "Scroller",
												  "Vertical"};
static const float panel_text[4] = {0.92f, 0.90f, 0.86f, 1.0f};

static bool panel_layout_name_allowed(const char *name) {
	for (size_t i = 0; i < sizeof(panel_layout_names) /
												  sizeof(panel_layout_names[0]);
		 i++) {
		if (strcmp(name, panel_layout_names[i]) == 0)
			return true;
	}
	return false;
}

static bool panel_state_paths(char *state_dir, size_t state_dir_size,
							  char *state_path, size_t state_path_size) {
	const char *state_home = getenv("XDG_STATE_HOME");
	const char *home = getenv("HOME");
	char parent[PATH_MAX];

	if (state_home && *state_home) {
		if (snprintf(state_dir, state_dir_size, "%s/mango", state_home) >=
			(int)state_dir_size)
			return false;
		if (snprintf(state_path, state_path_size, "%s/layout-panel.conf",
					 state_dir) >= (int)state_path_size)
			return false;
		return true;
	}

	if (!home || !*home ||
		snprintf(parent, sizeof(parent), "%s/.local/state", home) >=
			(int)sizeof(parent) ||
		snprintf(state_dir, state_dir_size, "%s/mango", parent) >=
			(int)state_dir_size ||
		snprintf(state_path, state_path_size, "%s/layout-panel.conf",
				 state_dir) >= (int)state_path_size)
		return false;
	return true;
}

static bool panel_ensure_state_dir(void) {
	char state_dir[PATH_MAX], state_path[PATH_MAX];
	const char *state_home = getenv("XDG_STATE_HOME");
	const char *home = getenv("HOME");
	char parent[PATH_MAX];

	if (!panel_state_paths(state_dir, sizeof(state_dir), state_path,
						   sizeof(state_path)))
		return false;

	if (state_home && *state_home) {
		if (mkdir(state_home, 0700) < 0 && errno != EEXIST)
			return false;
	} else {
		if (!home || !*home ||
			snprintf(parent, sizeof(parent), "%s/.local", home) >=
				(int)sizeof(parent) ||
			(mkdir(parent, 0700) < 0 && errno != EEXIST))
			return false;
		if (snprintf(parent, sizeof(parent), "%s/.local/state", home) >=
			(int)sizeof(parent) ||
			(mkdir(parent, 0700) < 0 && errno != EEXIST))
			return false;
	}

	if (mkdir(state_dir, 0700) < 0 && errno != EEXIST)
		return false;
	return true;
}

static bool panel_parse_int(const char *value, int32_t *result) {
	char *end = NULL;
	long parsed;

	errno = 0;
	parsed = strtol(value, &end, 10);
	if (errno == ERANGE || end == value || *end != '\0' ||
		parsed < INT32_MIN || parsed > INT32_MAX)
		return false;
	*result = (int32_t)parsed;
	return true;
}

static void panel_persist_state(void) {
	char state_dir[PATH_MAX], state_path[PATH_MAX], temp_path[PATH_MAX];
	if (!panel_ensure_state_dir() ||
		!panel_state_paths(state_dir, sizeof(state_dir), state_path,
						   sizeof(state_path)) ||
		snprintf(temp_path, sizeof(temp_path), "%s/.layout-panel.XXXXXX",
				  state_dir) >= (int)sizeof(temp_path))
		return;

	int fd = mkstemp(temp_path);
	if (fd < 0)
		return;
	FILE *file = fdopen(fd, "w");
	if (!file) {
		close(fd);
		unlink(temp_path);
		return;
	}

	bool ok = fprintf(file,
				  "# Mango layout panel state\n"
				  "layout=%s\n"
				  "gappoh=%u\n"
				  "gappov=%u\n"
				  "gappih=%u\n"
				  "gappiv=%u\n"
				  "floating_snap=%d\n",
				  config.layout_panel_default, config.gappoh, config.gappov,
				  config.gappih, config.gappiv,
				  config.enable_floating_snap) >= 0;
	if (ok)
		ok = fflush(file) == 0 && fsync(fd) == 0;
	if (fclose(file) != 0)
		ok = false;
	if (ok && rename(temp_path, state_path) == 0)
		return;
	unlink(temp_path);
}

void layout_panel_load_persisted_config(void) {
	char state_dir[PATH_MAX], state_path[PATH_MAX];
	char line[128], key[32], value[80];
	FILE *file;

	if (!panel_state_paths(state_dir, sizeof(state_dir), state_path,
						   sizeof(state_path)))
		return;
	file = fopen(state_path, "r");
	if (!file)
		return;

	while (fgets(line, sizeof(line), file)) {
		int32_t parsed;
		if (sscanf(line, "%31[^=]=%79[^\n]", key, value) != 2)
			continue;
		if (strcmp(key, "layout") == 0 && panel_layout_name_allowed(value)) {
			snprintf(config.layout_panel_default,
					 sizeof(config.layout_panel_default), "%.31s", value);
			config.layout_panel_override = 1;
		} else if (strcmp(key, "gappoh") == 0 &&
				   panel_parse_int(value, &parsed)) {
			config.gappoh = CLAMP_INT(parsed, 0, 1000);
		} else if (strcmp(key, "gappov") == 0 &&
				   panel_parse_int(value, &parsed)) {
			config.gappov = CLAMP_INT(parsed, 0, 1000);
		} else if (strcmp(key, "gappih") == 0 &&
				   panel_parse_int(value, &parsed)) {
			config.gappih = CLAMP_INT(parsed, 0, 1000);
		} else if (strcmp(key, "gappiv") == 0 &&
				   panel_parse_int(value, &parsed)) {
			config.gappiv = CLAMP_INT(parsed, 0, 1000);
		} else if (strcmp(key, "floating_snap") == 0 &&
				   panel_parse_int(value, &parsed)) {
			config.enable_floating_snap = CLAMP_INT(parsed, 0, 1);
		}
	}
	fclose(file);
}

static DecorateDrawData panel_text_data(bool button) {
	DecorateDrawData data = {0};
	memcpy(data.fg_color, panel_text, sizeof(data.fg_color));
	memcpy(data.focus_fg_color, panel_text, sizeof(data.focus_fg_color));
	memcpy(data.bg_color, button ? config.bordercolor : (float[4]){0, 0, 0, 0},
		   sizeof(data.bg_color));
	memcpy(data.focus_bg_color, config.focuscolor, sizeof(data.focus_bg_color));
	memcpy(data.border_color, config.focuscolor, sizeof(data.border_color));
	data.border_width = button ? 1 : 0;
	data.corner_radius = 6;
	data.padding_x = button ? 10 : 0;
	data.padding_y = button ? 7 : 0;
	data.font_desc = "Sans 14";
	return data;
}

static MangoJumpLabel *panel_make_label(const char *text, bool button,
										int32_t x, int32_t y) {
	MangoJumpLabel *label =
		mango_jump_label_node_create(panel.tree, panel_text_data(button));
	if (!label)
		return NULL;
	mango_jump_label_node_update(label, text, panel.mon->wlr_output->scale);
	wlr_scene_node_set_position(&label->scene_buffer->node, x, y);
	return label;
}

static void panel_set_label(MangoJumpLabel *label, const char *text,
							bool focused) {
	if (!label)
		return;
	label->focused = focused;
	mango_jump_label_node_update(label, text, panel.mon->wlr_output->scale);
}

static int panel_active_layout(void) {
	const Layout *layout = panel.mon->pertag->ltidxs[get_mon_curtag(panel.mon)];
	if (!layout)
		return -1;
	for (int i = 0; i < 3; i++) {
		uint32_t id = i == 0 ? TILE : i == 1 ? SCROLLER : VERTICAL_SCROLLER;
		if (layout->id == id)
			return i;
	}
	return -1;
}

static void panel_refresh(void) {
	if (!panel.tree || !panel.mon)
		return;

	char text[64];
	int active = panel_active_layout();
	for (int i = 0; i < 3; i++) {
		snprintf(text, sizeof(text), "%s%s", panel_layout_labels[i],
				 active == i ? " *" : "");
		panel_set_label(panel.layout_buttons[i], text, panel.focus == i);
	}

	snprintf(text, sizeof(text), "%d px", panel.mon->gappih);
	panel_set_label(panel.gap_value, text,
					panel.focus == PANEL_GAP_DOWN ||
						panel.focus == PANEL_GAP_UP);
	panel_set_label(panel.gap_down, "-", panel.focus == PANEL_GAP_DOWN);
	panel_set_label(panel.gap_up, "+", panel.focus == PANEL_GAP_UP);
	snprintf(text, sizeof(text), "Floating snap: %s",
			 config.enable_floating_snap ? "On" : "Off");
	panel_set_label(panel.snap_button, text, panel.focus == PANEL_SNAP);
	panel_set_label(panel.close, "Close", panel.focus == PANEL_CLOSE);
}

static void panel_activate(int control) {
	Arg arg = {0};
	switch (control) {
	case PANEL_LAYOUT_TILE:
	case PANEL_LAYOUT_SCROLLER:
	case PANEL_LAYOUT_VERTICAL:
		arg.v = (char *)panel_layout_names[control];
		snprintf(config.layout_panel_default,
				 sizeof(config.layout_panel_default), "%s", arg.v);
		config.layout_panel_override = 1;
		set_layout(&arg);
		break;
	case PANEL_GAP_DOWN:
		arg.i = -2;
		increase_gaps(&arg);
		break;
	case PANEL_GAP_UP:
		arg.i = 2;
		increase_gaps(&arg);
		break;
	case PANEL_SNAP:
		config.enable_floating_snap = !config.enable_floating_snap;
		break;
	case PANEL_CLOSE:
		layout_panel_close();
		return;
	default:
		return;
	}
	if (server.selected_monitor && control >= PANEL_GAP_DOWN &&
		control <= PANEL_GAP_UP) {
		config.gappoh = server.selected_monitor->gappoh;
		config.gappov = server.selected_monitor->gappov;
		config.gappih = server.selected_monitor->gappih;
		config.gappiv = server.selected_monitor->gappiv;
	}
	panel_persist_state();
	panel_refresh();
}

bool layout_panel_is_active(void) { return panel.tree != NULL; }

void layout_panel_close(void) {
	if (!panel.tree)
		return;
	MangoJumpLabel *labels[] = {panel.title,
								panel.layout_label,
								panel.layout_buttons[0],
								panel.layout_buttons[1],
								panel.layout_buttons[2],
								panel.spacing_label,
								panel.gap_down,
								panel.gap_value,
								panel.gap_up,
								panel.snap_label,
								panel.snap_button,
								panel.close,
								panel.hint};
	for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++)
		mango_jump_label_node_destroy(labels[i]);
	wlr_scene_node_destroy(&panel.tree->node);
	memset(&panel, 0, sizeof(panel));
}

void layout_panel_monitor_destroyed(Monitor *mon) {
	if (panel.mon == mon)
		layout_panel_close();
}

int32_t toggle_layout_panel(const Arg *arg) {
	(void)arg;
	if (layout_panel_is_active()) {
		layout_panel_close();
		return 0;
	}
	if (server.session_locked || !server.selected_monitor ||
		server.selected_monitor->isoverview)
		return 0;

	panel.mon = server.selected_monitor;
	panel.focus = PANEL_LAYOUT_TILE;
	panel.x = panel.mon->m.x + (panel.mon->m.width - PANEL_WIDTH) / 2;
	panel.y = panel.mon->m.y + (panel.mon->m.height - PANEL_HEIGHT) / 2;
	panel.tree = wlr_scene_tree_create(server.layers[LyrOverlay]);
	if (!panel.tree) {
		memset(&panel, 0, sizeof(panel));
		return 0;
	}
	wlr_scene_node_set_position(&panel.tree->node, panel.x, panel.y);

	float panel_color[4] = {config.rootcolor[0], config.rootcolor[1],
							config.rootcolor[2], 0.98f};
	panel.border = wlr_scene_rect_create(panel.tree, PANEL_WIDTH, PANEL_HEIGHT,
										 config.focuscolor);
	panel.background = wlr_scene_rect_create(panel.tree, PANEL_WIDTH - 4,
													 PANEL_HEIGHT - 4, panel_color);
	if (!panel.border || !panel.background) {
		layout_panel_close();
		return 0;
	}
	wlr_scene_node_set_position(&panel.background->node, 2, 2);

	panel.title = panel_make_label("Layout controls", false, PANEL_MARGIN, 18);
	panel.layout_label = panel_make_label("Layout", false, PANEL_MARGIN, 58);
	for (int i = 0; i < 3; i++)
		panel.layout_buttons[i] = panel_make_label(panel_layout_labels[i], true,
												   PANEL_MARGIN + i * 134, 82);
	panel.spacing_label = panel_make_label("Spacing", false, PANEL_MARGIN, 132);
	panel.gap_down = panel_make_label("-", true, PANEL_MARGIN, 156);
	panel.gap_value = panel_make_label("0 px", true, PANEL_MARGIN + 58, 156);
	panel.gap_up = panel_make_label("+", true, PANEL_MARGIN + 146, 156);
	panel.snap_label = panel_make_label("Snapping", false, PANEL_MARGIN, 206);
	panel.snap_button =
		panel_make_label("Floating snap", true, PANEL_MARGIN + 106, 200);
	panel.close = panel_make_label("Close", true, PANEL_WIDTH - 86, 18);
	panel.hint =
		panel_make_label("Tab / arrows move, Enter selects, Esc closes", false,
						 PANEL_MARGIN, 244);
	panel_refresh();
	return 0;
}

static int panel_control_at(double lx, double ly) {
	double x = lx - panel.x;
	double y = ly - panel.y;
	if (y >= 78 && y < 122) {
		for (int i = 0; i < 3; i++)
			if (x >= PANEL_MARGIN + i * 134 && x < PANEL_MARGIN + i * 134 + 118)
				return i;
	}
	if (y >= 150 && y < 198) {
		if (x >= PANEL_MARGIN && x < PANEL_MARGIN + 48)
			return PANEL_GAP_DOWN;
		if (x >= PANEL_MARGIN + 136 && x < PANEL_MARGIN + 194)
			return PANEL_GAP_UP;
	}
	if (y >= 194 && y < 238 && x >= PANEL_MARGIN + 96 &&
		x < PANEL_WIDTH - PANEL_MARGIN)
		return PANEL_SNAP;
	if (y >= 12 && y < 54 && x >= PANEL_WIDTH - 90)
		return PANEL_CLOSE;
	return -1;
}

bool layout_panel_handle_button(double lx, double ly, uint32_t button,
								uint32_t state) {
	if (!layout_panel_is_active())
		return false;
	if (state != WL_POINTER_BUTTON_STATE_PRESSED)
		return true;
	if (button != BTN_LEFT) {
		if (button == BTN_RIGHT)
			layout_panel_close();
		return true;
	}
	int control = panel_control_at(lx, ly);
	if (control >= 0) {
		panel.focus = control;
		panel_activate(control);
	} else if (lx < panel.x || lx >= panel.x + PANEL_WIDTH || ly < panel.y ||
			   ly >= panel.y + PANEL_HEIGHT) {
		layout_panel_close();
	}
	return true;
}

bool layout_panel_handle_key(xkb_keysym_t sym, uint32_t state, uint32_t mods) {
	if (!layout_panel_is_active())
		return false;
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
		return true;
	if (mods != 0 && sym != XKB_KEY_Escape)
		return false;
	if (sym == XKB_KEY_Escape) {
		layout_panel_close();
		return true;
	}
	if (sym == XKB_KEY_Tab || sym == XKB_KEY_Right || sym == XKB_KEY_Down) {
		panel.focus = (panel.focus + 1) % PANEL_CONTROL_COUNT;
		panel_refresh();
		return true;
	}
	if (sym == XKB_KEY_Left || sym == XKB_KEY_Up) {
		panel.focus =
			(panel.focus + PANEL_CONTROL_COUNT - 1) % PANEL_CONTROL_COUNT;
		panel_refresh();
		return true;
	}
	if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter ||
		sym == XKB_KEY_space) {
		panel_activate(panel.focus);
		return true;
	}
	return true;
}
