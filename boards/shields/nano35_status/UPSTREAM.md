# Nano35 status screen provenance

This is a local copy of the MIT-licensed `dongle_display` shield from
`englmaxi/zmk-dongle-display`, commit
`c6c4be11c5ede722086a2a1f355cec9c101b9390` (peeled `v0.3` tag).
Copyright notices and LICENSE are retained.

`kkaakkss/zmk-sofle-dongle` at
`d0845fd3569d415d74a94588cb9234373fe8e164` references that module's `v0.3`
in `config/west.yml` and includes the `dongle_display` shield in its build.
The bongo cat assets, animation thresholds, layer display and status layout
are reused from that actual dependency. Neither upstream repository is modified.

Local changes:

- Rename shield identification to `nano35_status`.
- Remove upstream hardware overlay; `nano35-oled` owns the SH1106 pins and geometry.
- Bongo WPM getter uses `zmk_wpm_get_state()` for NULL/unrelated events.
- HID indicator getter uses `zmk_hid_indicators_get_current_profile()` for
  NULL/unrelated events. Both widgets are initialized through a ZMK macro
  that calls the getter with NULL.
- Load LVGL before ZMK keycode definitions in the widget translation units to
  avoid expansion of the `C` drive letter in Zephyr 3.5's `LV_CONF_PATH`.
- Compile the WPM widget instance only when enabled, avoiding an unused-variable warning.

The Nano35 OLED snippet explicitly selects monochrome rendering, a complete
1024-byte 128x64 framebuffer, an 8192-byte LVGL object heap and a dedicated
4096-byte display work stack. Those settings apply only to the OLED build.
