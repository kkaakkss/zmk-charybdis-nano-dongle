// SPDX-License-Identifier: MIT
#pragma once
#include "pomodoro_logic.h"

void nano35_pomodoro_snapshot(struct nano35_pomo_state *out);
// Called after releasing the model mutex. UI must queue any LVGL work.
void nano35_pomodoro_ui_changed(bool notify);
