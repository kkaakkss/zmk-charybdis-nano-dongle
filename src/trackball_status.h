// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
struct nano35_speed_status { bool visible, scroll; uint16_t value; };
void nano35_trackball_speed_snapshot(struct nano35_speed_status *out);
void nano35_trackball_speed_ui_changed(void);
