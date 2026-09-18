#ifndef __LAYOUT_PANEL_H__
#define __LAYOUT_PANEL_H__ 1

#include "mango/dispatch/bind.h"
#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

bool layout_panel_is_active(void);
int32_t toggle_layout_panel(const Arg *arg);
void layout_panel_close(void);
bool layout_panel_handle_button(double lx, double ly, uint32_t button,
								uint32_t state);
bool layout_panel_handle_key(xkb_keysym_t sym, uint32_t state, uint32_t mods);

#endif
