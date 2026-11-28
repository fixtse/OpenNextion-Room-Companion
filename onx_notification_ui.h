#pragma once
// Notification centre UI for the touch panels (see onx_notification::Center).
//   - bell on the main carousel (top right, no background) with the unread count;
//     every screen opens the centre by pulling down from the top edge
//   - arrival banner at the top; tap to open
//   - list of recent notifications (accent bar while unread, time, x to
//     dismiss), Clear all in the header, swipe up (or tap the handle) to close
//   - detail view with a small title thumbnail, Back and Dismiss
// Built on LVGL's top layer; sizes follow onx_geom so both panels scale.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include "lvgl.h"
#include "onx_render.h"
#include "esphome/core/hal.h"
#include "onx_geom.h"
#include "onx_display_settings.h"
#include "onx_notification.h"
#include "onx_presets.h"
#include "onx_emoji.h"
#include <map>
#include <memory>

namespace onx_notification_ui {
using onx_geom::S;

struct Fonts {
  const lv_font_t *title{nullptr}, *body{nullptr}, *small{nullptr}, *icon{nullptr}, *symbol{nullptr};
};
struct Hooks {
  std::function<void(const std::string &)> load_image;  // Start downloading a picture.
  std::function<void()> release_image;                  // Free the picture.
  std::function<void()> activity;                       // Keep the screen awake.
  // Saves a new screen orientation (quarter turns); the panel then restarts.
  std::function<bool(uint8_t)> set_orientation;
  std::function<uint8_t()> orientation;
};

inline Fonts fonts;
inline Hooks hooks;
inline int width = 240, height = 240;
inline lv_obj_t *bell = nullptr, *badge = nullptr, *badge_label = nullptr;
inline lv_obj_t *toast = nullptr, *toast_title = nullptr, *toast_body = nullptr, *toast_more = nullptr;
// The banner's body clip: grows up to several full-width lines; when a
// multi-line conversation overflows, it scrolls to the latest lines.
inline lv_obj_t *toast_clip = nullptr;
inline bool scroll_toast = false;
inline lv_obj_t *panel = nullptr, *list = nullptr, *empty_label = nullptr, *header_label = nullptr,
                *count_label = nullptr, *clear_button = nullptr,
                *handle = nullptr, *standby_button = nullptr, *preset_button = nullptr, *preset_list = nullptr,
                *rotate_button = nullptr, *rotate_dialog = nullptr;
// An opened notification returns to the centre when it was opened from there.
inline bool detail_from_panel = false;
inline const void *clock_tile_image = nullptr, *preset_tile_image = nullptr;
inline bool presets_open = false;
inline std::vector<std::string> preset_names;
inline std::string preset_signature, preset_error;
inline std::function<bool()> controls_blocked;
inline bool standby_timeout_enabled = true;
inline std::function<void()> toggle_standby_timeout;
inline const void *bell_image = nullptr;  // Bell glyph (an MDI image), set before init().
inline lv_obj_t *detail = nullptr, *detail_title = nullptr, *detail_age = nullptr, *detail_image = nullptr,
                *detail_body = nullptr;
struct Thumbnail {
  lv_image_dsc_t image{};
  std::shared_ptr<uint8_t> pixels;
};
inline std::map<uint32_t, Thumbnail> thumbnails;
inline std::map<lv_obj_t *, uint32_t> image_widgets;
inline uint32_t loading_id = 0;
inline bool release_pending = false;
inline lv_obj_t *toast_icon = nullptr, *toast_bar = nullptr;
inline uint32_t detail_id = 0, built_revision = 0;
// Set while a list card is removed in place, so refresh() skips the rebuild.
inline bool keep_list = false;
inline bool panel_open = false;
inline bool bell_wanted = false;  // Only the main carousel shows the bell.
// Status row on the main carousel: running timers (Assist's own and Home
// Assistant ones, with or without a page) as a countdown chip, and a music
// chip while a player plays. Tapping opens the timer screen or the player.
// With Wi-Fi and Home Assistant online the row takes the place of their status
// icons (top centre); otherwise it sits left of the bell so the warnings stay.
inline lv_obj_t *status_row = nullptr, *timer_chip = nullptr, *timer_label = nullptr, *media_chip = nullptr;
inline std::string timer_text;
inline bool media_playing = false, status_centered = true;
inline const void *music_image = nullptr, *rotate_tile_image = nullptr;  // MDI images, set before init().
inline std::function<void()> on_timer_chip, on_media_chip;
constexpr int STATUS_CHIP_H = 28;  // Fixed inside the 40 px MAIN_TOP header (6 px gap above cards on 2.8" and 3.5").
inline bool has_status_chips() { return !timer_text.empty() || media_playing; }
inline bool status_row_shown() { return bell_wanted && has_status_chips(); }
inline void update_timer_chip() {
  if (status_row == nullptr) return;
  auto show = [](lv_obj_t *o, bool on) { if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); };
  show(timer_chip, !timer_text.empty());
  show(media_chip, media_playing);
  show(status_row, status_row_shown());
  const int y = (40 - STATUS_CHIP_H) / 2;
  if (status_centered) lv_obj_align(status_row, LV_ALIGN_TOP_MID, 0, y);
  else lv_obj_align(status_row, LV_ALIGN_TOP_RIGHT, -(4 + 44 + S(4)), y);
}
inline void set_status_centered(bool centered) {
  if (centered == status_centered) return;
  status_centered = centered;
  update_timer_chip();
}
inline void set_media_chip(bool playing) {
  if (playing == media_playing) return;
  media_playing = playing;
  update_timer_chip();
}
inline void set_timer_chip(const std::string &text) {
  if (text == timer_text) return;
  timer_text = text;
  if (timer_label != nullptr) lv_label_set_text(timer_label, text.c_str());
  update_timer_chip();
}

constexpr uint32_t BG = 0x1A1D21, CARD = 0x272B30, CONTROL = 0x3A3F45, TEXT = 0xFFFFFF, MUTED = 0xB8B8B8;
inline uint32_t accent() { return onx_display_settings::accent(); }
// The list follows the panel's saved palette.
inline uint32_t bg() { return onx_display_settings::background(); }
inline uint32_t surface() { return onx_display_settings::surface(); }
inline uint32_t control() { return onx_display_settings::control_surface(); }
inline uint32_t foreground() { return onx_display_settings::foreground(); }
inline uint32_t muted() { return onx_display_settings::muted_text(); }
// "now", "5 min", then the time of day (or hours before the clock is set).
inline std::string when(uint32_t received_ms, uint32_t now_ms) {
  const uint32_t seconds = (now_ms - received_ms) / 1000;
  if (seconds < 60) return "now";
  if (seconds < 3600) return std::to_string(seconds / 60) + " min";
  const time_t now = std::time(nullptr);
  if (now > 1600000000) {
    const time_t at = now - time_t(seconds);
    struct tm local{};
    localtime_r(&at, &local);
    char out[16];
    strftime(out, sizeof(out), seconds < 86400 ? onx_display_settings::time_pattern() : "%d %b", &local);
    return out;
  }
  return seconds < 86400 ? std::to_string(seconds / 3600) + " h" : std::to_string(seconds / 86400) + " d";
}

inline void hide(lv_obj_t *obj, bool hidden) {
  if (obj == nullptr) return;
  if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
// The dark design colours above stand for theme roles; resolve them to the
// current panel palette so light themes get light cards and dark text.
inline uint32_t themed(uint32_t c) {
  if (c == BG) return bg();
  if (c == CARD) return surface();
  if (c == CONTROL) return control();
  if (c == TEXT) return foreground();
  if (c == MUTED) return muted();
  return c;
}
inline lv_obj_t *box(lv_obj_t *parent, uint32_t color, int radius) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_set_style_bg_color(obj, lv_color_hex(themed(color)), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}
inline lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *value) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  // As the palette sweep: readable ink on accent-filled buttons.
  lv_obj_set_style_text_color(label, lv_color_hex(onx_render::panel_property_color(label, LV_STYLE_TEXT_COLOR, themed(color))), 0);
  onx_emoji::set_label(label, value);
  return label;
}
inline lv_obj_t *button(lv_obj_t *parent, const char *caption, uint32_t color, lv_event_cb_t cb, void *user) {
  lv_obj_t *btn = box(parent, color, S(18));
  lv_obj_set_height(btn, S(36));
  lv_obj_set_width(btn, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(btn, S(14), 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t *label = text(btn, fonts.body, TEXT, caption);
  lv_obj_center(label);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
  return btn;
}
inline void touched() {
  if (hooks.activity) hooks.activity();
}

// True while the centre covers the native UI or shows its banner; the native UI
// and the dashboard stream then give way.
inline bool owns_screen() {
  return panel_open || detail_id != 0 || onx_notification::center.toast_visible(esphome::millis());
}

enum class SheetMode { NONE, OPENING, CLOSING };
inline lv_obj_t *sheet = nullptr;
inline int sheet_edge = 0;
inline SheetMode sheet_mode = SheetMode::NONE;
// LVGL always redraws the active screen under the top and system layers, so a
// grid of cards under the centre or the sheet was repainted on every frame.
// Page content (cover_root, set by the panel) fully covered by them is hidden
// while covered and shown again as it is uncovered. Not with the translucent
// dashboard backdrop.
inline lv_obj_t *cover_root = nullptr;
inline int covered_rows = 0;
inline bool backdrop = false;
inline void cover_walk(lv_obj_t *obj, int rows, int depth) {
  const uint32_t n = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < n; ++i) {
    lv_obj_t *c = lv_obj_get_child(obj, i);
    lv_area_t a;
    lv_obj_get_coords(c, &a);
    const bool ours = lv_obj_has_flag(c, LV_OBJ_FLAG_USER_4);
    if (a.y2 < rows) {
      if (!ours && !lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(c, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_USER_4));
      continue;
    }
    if (ours) lv_obj_remove_flag(c, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_USER_4));
    if (depth < 3 && (a.y1 < rows || rows == 0)) cover_walk(c, rows, depth + 1);
  }
}
inline void apply_cover(int rows) {
  if (backdrop) rows = 0;
  if (cover_root == nullptr || rows == covered_rows) return;
  covered_rows = rows;
  cover_walk(cover_root, rows, 0);
}
inline void set_dashboard_backdrop(bool visible) {
  if (!panel || !detail || !toast) return;
  const auto opacity = visible ? LV_OPA_80 : LV_OPA_COVER;
  backdrop = visible;
  if (visible) apply_cover(0);
  lv_obj_set_style_bg_opa(panel, opacity, 0);
  lv_obj_set_style_bg_opa(detail, opacity, 0);
  if (visible) {
    lv_obj_move_foreground(panel_open ? panel : detail_id ? detail : toast);
    if (rotate_dialog) lv_obj_move_foreground(rotate_dialog);
  }
}

inline void refresh();
inline lv_obj_t *notification_icon(lv_obj_t *parent, uint32_t id, int size = 28) {
  auto *icon = lv_image_create(parent);
  lv_obj_set_size(icon, S(size), S(size));
  lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CONTAIN);
  lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  auto found = thumbnails.find(id);
  lv_image_set_src(icon, found == thumbnails.end() || !found->second.pixels ? bell_image : &found->second.image);
  image_widgets[icon] = id;
  lv_obj_add_event_cb(icon, [](lv_event_t *e) {
    image_widgets.erase(static_cast<lv_obj_t *>(lv_event_get_target(e)));
  }, LV_EVENT_DELETE, nullptr);
  return icon;
}
inline void open_detail(uint32_t id);

inline void close_detail() {
  if (detail_id == 0) return;
  detail_id = 0;
  hide(detail, true);
  if (detail_from_panel) panel_open = true;
  detail_from_panel = false;
  refresh();
}
inline void open_panel() {
  touched();
  onx_notification::center.hide_toast();
  panel_open = true;
  built_revision = 0;
  refresh();
  apply_cover(height);
}
inline int drag_offset = 0;  // Panel lift while a swipe up is in progress (<= 0).
inline lv_obj_t *handle_hint = nullptr;
inline bool drag_closes(int dy) { return -dy >= height / 5; }
inline void set_drag_offset(int offset) {
  if (panel == nullptr || offset == drag_offset) return;
  drag_offset = offset;
  lv_obj_set_style_translate_y(panel, offset, 0);
  if (handle_hint != nullptr) {
    const char *want = drag_closes(offset) ? "Release to close" : "Swipe up to close";
    if (std::strcmp(lv_label_get_text(handle_hint), want) != 0) lv_label_set_text_static(handle_hint, want);
    lv_obj_set_style_text_color(handle_hint, lv_color_hex(drag_closes(offset) ? accent() : muted()), 0);
  }
}
inline void close_panel() {
  lv_anim_delete(panel, nullptr);
  set_drag_offset(0);
  panel_open = false;
  presets_open = false;
  preset_error.clear();
  refresh();
  if (sheet_mode == SheetMode::NONE) apply_cover(0);
}

inline void on_bell(lv_event_t *) { panel_open ? close_panel() : open_panel(); }
inline void on_toast(lv_event_t *) {
  touched();
  const uint32_t id = onx_notification::center.toast_id;
  onx_notification::center.hide_toast();
  if (id) open_detail(id);
}
inline void on_close_panel(lv_event_t *) {
  if (controls_blocked && controls_blocked()) return;
  touched();
  close_panel();
}
// The touch handler's swipe up closes the centre unless a list could still
// scroll down (then the swipe scrolls it, as before).
inline bool swipe_closes() {
  if (!panel_open || detail_id != 0 || rotate_dialog != nullptr) return false;
  for (lv_obj_t *l : {list, preset_list})
    if (l != nullptr && !lv_obj_has_flag(l, LV_OBJ_FLAG_HIDDEN) && lv_obj_get_scroll_bottom(l) > 0) return false;
  return true;
}
// The handle at the bottom of the open centre (grip line and "Swipe up to
// close"): the only place a swipe up closes it. It used to be a fixed 84 px
// band, which also covered the end of the list, so scrolling the list closed
// the centre.
inline bool close_zone(int, int y) {
  if (!panel_open || detail_id != 0 || rotate_dialog != nullptr || handle == nullptr) return false;
  lv_area_t a;
  lv_obj_get_coords(handle, &a);
  return y >= a.y1 - S(6);
}
// Drag sheet. Moving the real centre redraws the whole screen for every move
// (80-200 ms a frame on this SPI panel), so while a finger drags it open or
// closed a plain sheet in the centre's colour follows the finger instead. The
// sheet never moves or resizes: it draws itself down to `sheet_edge`, and each
// move only invalidates the strip between the old and new edge plus the grip
// band, so a frame costs a few milliseconds and keeps up with the finger.
inline uint32_t sheet_color = 0;
inline int sheet_band() { return S(48); }
inline bool sheet_releases(int travel) { return travel >= height / 5; }
inline void sheet_invalidate(int a, int b) {
  if (sheet == nullptr) return;
  lv_area_t area{0, std::max(0, std::min(a, b) - sheet_band()), width - 1, std::min(height - 1, std::max(a, b) + 2)};
  if (area.y2 >= area.y1) lv_obj_invalidate_area(sheet, &area);
}
inline void set_sheet_edge(int edge) {
  edge = std::clamp(edge, 0, height);
  if (edge == sheet_edge) return;
  const int old = sheet_edge;
  sheet_edge = edge;
  sheet_invalidate(old, edge);
  apply_cover(edge);
}
inline void draw_sheet(lv_event_t *e) {
  if (sheet_edge <= 0) return;
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_draw_rect_dsc_t rect;
  lv_draw_rect_dsc_init(&rect);
  rect.bg_color = lv_color_hex(sheet_color);
  rect.bg_opa = LV_OPA_COVER;
  rect.radius = 0;
  const lv_area_t body{0, 0, width - 1, sheet_edge - 1};
  lv_draw_rect(layer, &rect, &body);
  // Grip at the moving edge, like the centre's own handle.
  const int gw = S(36), gh = std::max(2, S(4)), gy = sheet_edge - S(12);
  if (gy > 0) {
    lv_draw_rect_dsc_t grip;
    lv_draw_rect_dsc_init(&grip);
    grip.bg_color = lv_color_hex(muted());
    grip.bg_opa = LV_OPA_COVER;
    grip.radius = LV_RADIUS_CIRCLE;
    const lv_area_t g{width / 2 - gw / 2, gy, width / 2 + gw / 2 - 1, gy + gh - 1};
    lv_draw_rect(layer, &grip, &g);
  }
  const lv_font_t *font = fonts.small;
  if (font == nullptr) return;
  const int ty = sheet_edge - S(16) - lv_font_get_line_height(font);
  if (ty <= 0) return;
  const bool armed = sheet_mode == SheetMode::CLOSING ? sheet_releases(height - sheet_edge) : sheet_releases(sheet_edge);
  lv_draw_label_dsc_t label;
  lv_draw_label_dsc_init(&label);
  label.font = font;
  label.color = lv_color_hex(armed ? accent() : muted());
  label.align = LV_TEXT_ALIGN_CENTER;
  label.text = sheet_mode == SheetMode::CLOSING ? (armed ? "Release to close" : "Swipe up to close")
                                                : (armed ? "Release to open" : "Notifications");
  const lv_area_t t{0, ty, width - 1, ty + lv_font_get_line_height(font)};
  lv_draw_label(layer, &label, &t);
}
inline void ensure_sheet() {
  if (sheet != nullptr) return;
  // Always present and transparent at edge 0, so showing it never triggers a
  // full-screen redraw; it sits above the centre and the toast.
  sheet = lv_obj_create(lv_layer_sys());
  lv_obj_remove_style_all(sheet);
  lv_obj_set_pos(sheet, 0, 0);
  lv_obj_set_size(sheet, width, height);
  lv_obj_remove_flag(sheet, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_event_cb(sheet, draw_sheet, LV_EVENT_DRAW_MAIN, nullptr);
}
inline void animate_sheet(int to, lv_anim_completed_cb_t done) {
  ensure_sheet();
  lv_anim_delete(sheet, nullptr);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, sheet);
  lv_anim_set_values(&a, sheet_edge, to);
  lv_anim_set_duration(&a, std::clamp(std::abs(to - sheet_edge) / 3, 60, 180));
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void *, int32_t v) { set_sheet_edge(v); });
  if (done) lv_anim_set_completed_cb(&a, done);
  lv_anim_start(&a);
}
inline uint32_t panel_color() {
  return panel != nullptr ? (lv_color_to_u32(lv_obj_get_style_bg_color(panel, LV_PART_MAIN)) & 0xFFFFFF) : bg();
}

// Pull down from the top edge: the sheet follows the finger and the centre
// opens on release once pulled a fifth of the way (or flicked).
inline void drag_open(int dy) {
  if (panel_open || panel == nullptr) return;
  ensure_sheet();
  if (sheet_mode != SheetMode::OPENING) {
    lv_anim_delete(sheet, nullptr);
    sheet_mode = SheetMode::OPENING;
    sheet_color = panel_color();
  }
  set_sheet_edge(std::max(0, dy));
}
inline bool release_open(int dy, uint32_t ms) {
  if (sheet_mode != SheetMode::OPENING) return !panel_open && dy >= S(50);
  const bool open = sheet_releases(dy) || (ms > 0 && ms < 350 && dy >= S(36));
  if (open) {
    animate_sheet(height, [](lv_anim_t *) {
      open_panel();
      // The centre now covers the screen; drop the sheet without a redraw of its own.
      sheet_edge = 0;
      sheet_mode = SheetMode::NONE;
    });
  } else {
    animate_sheet(0, [](lv_anim_t *) { sheet_mode = SheetMode::NONE; });
  }
  return false;  // Opening is handled here.
}

// Swipe up: the centre is replaced by the sheet, which follows the finger and
// closes on release past a fifth of the screen (or a flick). A shorter drag
// springs back and shows the centre again.
inline void drag_panel(int dy) {
  if (!panel_open || panel == nullptr) return;
  ensure_sheet();
  if (sheet_mode != SheetMode::CLOSING) {
    lv_anim_delete(sheet, nullptr);
    sheet_mode = SheetMode::CLOSING;
    sheet_color = panel_color();
    sheet_edge = height;           // One full redraw: centre -> sheet over the page.
    lv_obj_invalidate(sheet);
    hide(panel, true);
  }
  // Downward drags barely move it, to show that direction does nothing.
  set_sheet_edge(height + (dy < 0 ? dy : 0));
}
inline bool release_panel_drag(int dy, uint32_t ms) {
  if (sheet_mode != SheetMode::CLOSING) return false;
  const bool flick = ms > 0 && ms < 350 && -dy >= S(36);
  if (sheet_releases(-dy) || flick) {
    close_panel();
    animate_sheet(0, [](lv_anim_t *) { sheet_mode = SheetMode::NONE; });
  } else {
    animate_sheet(height, [](lv_anim_t *) {
      sheet_mode = SheetMode::NONE;
      if (panel_open) hide(panel, false);
      sheet_edge = 0;
    });
  }
  return false;  // Closing is handled here.
}
inline void on_clear(lv_event_t *) {
  if (controls_blocked && controls_blocked()) return;
  touched();
  onx_notification::center.clear();
  close_panel();
}
inline void style_tile(lv_obj_t *button,bool selected) {
  lv_obj_set_style_bg_color(button,lv_color_hex(selected?accent():surface()),0);
  auto *icon=lv_obj_get_child(button,0);
  const uint32_t selected_ink=onx_display_settings::light_color(accent())?0x161B22:0xFFFFFF;
  lv_obj_set_style_image_recolor(icon,lv_color_hex(selected?selected_ink:foreground()),0);
}
inline void update_standby_button() {
  if(standby_button)style_tile(standby_button,standby_timeout_enabled);
}
inline void choose_preset(lv_event_t *event) {
  if(controls_blocked && controls_blocked())return;
  const auto index=reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  if(index>=preset_names.size() || !onx_presets::applying.empty())return;
  touched();
  preset_error=onx_presets::apply(preset_names[index]);
}
inline void update_presets() {
  if(!preset_list)return;
  hide(preset_list,!presets_open);
  hide(list,presets_open);
  style_tile(preset_button,presets_open);
  if(!panel_open || !presets_open)return;
  onx_presets::load();
  std::string signature=onx_presets::active+'\n'+onx_presets::applying+'\n'+preset_error;
  for(const auto &preset:onx_presets::summaries)signature+='\n'+preset.name;
  if(signature==preset_signature)return;
  preset_signature=std::move(signature);
  lv_obj_clean(preset_list);
  preset_names.clear();
  if(!onx_presets::applying.empty())
    text(preset_list,fonts.small,foreground(),("Applying "+onx_presets::applying+"…").c_str());
  if(!preset_error.empty())text(preset_list,fonts.small,foreground(),preset_error.c_str());
  if(onx_presets::summaries.empty())text(preset_list,fonts.body,muted(),"No saved presets");
  for(const auto &preset:onx_presets::summaries) {
    const auto name=preset.name==onx_presets::active?preset.name+" · Active":preset.name;
    auto *row=button(preset_list,name.c_str(),surface(),choose_preset,reinterpret_cast<void *>(preset_names.size()));
    lv_obj_set_width(row,lv_pct(100));
    lv_obj_set_height(row,S(40));
    auto *label=lv_obj_get_child(row,0);
    lv_obj_set_width(label,lv_pct(100));
    lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    if(!onx_presets::applying.empty())lv_obj_add_state(row,LV_STATE_DISABLED);
    preset_names.push_back(preset.name);
  }
}
// Quick toggles share the full width in one compact row.
inline lv_obj_t *quick_tile(lv_obj_t *parent,const void *src,lv_event_cb_t cb) {
  auto *tile=box(parent,surface(),S(18));
  lv_obj_set_size(tile,1,S(36));
  lv_obj_set_flex_grow(tile,1);
  lv_obj_add_flag(tile,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(tile,cb,LV_EVENT_CLICKED,nullptr);
  auto *icon=lv_image_create(tile);
  lv_image_set_src(icon,src);
  lv_obj_set_style_image_recolor_opa(icon,LV_OPA_COVER,0);
  lv_obj_remove_flag(icon,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_center(icon);
  return tile;
}
inline void on_item(lv_event_t *event) {
  if (controls_blocked && controls_blocked()) return;
  touched();
  open_detail(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))));
}
inline void on_item_dismiss(lv_event_t *event) {
  lv_event_stop_bubbling(event);
  if (controls_blocked && controls_blocked()) return;
  touched();
  // Drop just this card at once instead of rebuilding every card (slow on the
  // panel). The card owns this button, so delete it after the event finishes.
  lv_obj_t *card = lv_obj_get_parent(static_cast<lv_obj_t *>(lv_event_get_current_target(event)));
  hide(card, true);
  lv_obj_delete_async(card);
  lv_async_call([](void *user) {
    keep_list = true;
    onx_notification::center.dismiss(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(user)));
    keep_list = false;
  }, lv_event_get_user_data(event));
}
inline void on_back(lv_event_t *) { touched(); close_detail(); }
inline void on_detail_dismiss(lv_event_t *) {
  touched();
  // Removing the item closes the detail in refresh(); closing first rebuilt the
  // list twice (once still holding the item).
  onx_notification::center.dismiss(detail_id);
}

// Orientation: the next quarter turn, confirmed in a dialog because the
// screen size changes and the panel has to restart.
inline const char *orientation_name(uint8_t turns) {
  return turns == 0 ? "portrait" : turns == 1 ? "landscape (90°)" : turns == 2 ? "portrait, upside down" : "landscape (270°)";
}
inline void close_rotate_dialog() {
  if (rotate_dialog == nullptr) return;
  lv_obj_t *dialog = rotate_dialog;
  rotate_dialog = nullptr;
  lv_obj_delete_async(dialog);
}
inline void open_rotate_dialog() {
  if (rotate_dialog != nullptr || !hooks.set_orientation) return;
  const uint8_t next = ((hooks.orientation ? hooks.orientation() : 0) + 1) % 4;
  rotate_dialog = box(lv_layer_top(), 0x000000, 0);
  lv_obj_set_size(rotate_dialog, width, height);
  lv_obj_set_style_bg_opa(rotate_dialog, LV_OPA_60, 0);
  lv_obj_add_flag(rotate_dialog, LV_OBJ_FLAG_CLICKABLE);  // Blocks touches behind it.
  lv_obj_t *card = box(rotate_dialog, surface(), S(16));
  lv_obj_set_size(card, width - 2 * S(20), LV_SIZE_CONTENT);
  lv_obj_center(card);
  lv_obj_set_style_pad_all(card, S(16), 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(card, S(10), 0);
  text(card, fonts.title, foreground(), "Rotate the screen?");
  lv_obj_t *body = text(card, fonts.body, muted(), "");
  lv_obj_set_width(body, lv_pct(100));
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_label_set_text(body, (std::string("Switching to ") + orientation_name(next) +
                           " restarts the panel. It is back in a few seconds.").c_str());
  lv_obj_t *row = box(card, surface(), 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, S(10), 0);
  lv_obj_t *cancel = button(row, "Cancel", onx_display_settings::control_surface(), [](lv_event_t *) { touched(); close_rotate_dialog(); }, nullptr);
  lv_obj_t *accept = button(row, "Restart", accent(), [](lv_event_t *event) {
    touched();
    const uint8_t turns = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    if (hooks.set_orientation && hooks.set_orientation(turns)) {
      // panel.yaml restarts once the saved orientation differs from the running one.
      lv_obj_t *label = lv_obj_get_child(static_cast<lv_obj_t *>(lv_event_get_current_target(event)), 0);
      if (label) lv_label_set_text(label, "Restarting…");
    } else close_rotate_dialog();
  }, reinterpret_cast<void *>(uintptr_t(next)));
  for (auto *b : {cancel, accept}) { lv_obj_set_width(b, 1); lv_obj_set_flex_grow(b, 1); }
  lv_obj_set_style_text_color(lv_obj_get_child(accept, 0),
                              lv_color_hex(onx_display_settings::light_color(accent()) ? 0x161B22 : 0xFFFFFF), 0);
}
inline void build_header(lv_obj_t *parent, const char *title, lv_obj_t **out_row) {
  lv_obj_t *row = box(parent, BG, 0);
  lv_obj_set_size(row, lv_pct(100), S(44));
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  text(row, fonts.title, TEXT, title);
  *out_row = row;
}

inline void init(int screen_width, int screen_height, Fonts f, Hooks h) {
  width = screen_width;
  height = screen_height;
  fonts = f;
  hooks = std::move(h);
  lv_obj_t *top = lv_layer_top();

  // Bell, top-right corner of the main carousel: a plain icon in the slim
  // header row (onx_panel_layout::MAIN_TOP), no circular background.
  bell = box(top, CARD, 0);
  lv_obj_set_style_bg_opa(bell, LV_OPA_TRANSP, 0);
  lv_obj_set_size(bell, 44, 36);
  lv_obj_align(bell, LV_ALIGN_TOP_RIGHT, -4, 2);
  lv_obj_add_flag(bell, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(bell, S(8));
  if (bell_image != nullptr) {
    lv_obj_t *glyph = lv_image_create(bell);
    lv_image_set_src(glyph, bell_image);
    lv_obj_set_style_image_recolor(glyph, lv_color_hex(themed(TEXT)), 0);
    lv_obj_set_style_image_recolor_opa(glyph, LV_OPA_COVER, 0);
    lv_obj_remove_flag(glyph, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(glyph);
  } else {
    lv_obj_center(text(bell, fonts.body, TEXT, "!"));
  }
  lv_obj_add_event_cb(bell, on_bell, LV_EVENT_CLICKED, nullptr);
  status_row = box(top, CARD, 0);
  lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
  lv_obj_set_size(status_row, LV_SIZE_CONTENT, STATUS_CHIP_H);
  lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(status_row, S(6), 0);
  lv_obj_remove_flag(status_row, LV_OBJ_FLAG_CLICKABLE);
  timer_chip = box(status_row, CARD, STATUS_CHIP_H / 2);
  lv_obj_set_size(timer_chip, LV_SIZE_CONTENT, STATUS_CHIP_H);
  lv_obj_set_style_pad_hor(timer_chip, S(10), 0);
  lv_obj_add_flag(timer_chip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(timer_chip, S(6));
  timer_label = text(timer_chip, fonts.body, TEXT, "");
  lv_obj_remove_flag(timer_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_center(timer_label);
  lv_obj_add_event_cb(timer_chip, [](lv_event_t *) { if (on_timer_chip) on_timer_chip(); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(timer_chip, LV_OBJ_FLAG_HIDDEN);
  media_chip = box(status_row, CARD, STATUS_CHIP_H / 2);
  lv_obj_set_size(media_chip, std::min(44, S(40)), STATUS_CHIP_H);
  lv_obj_add_flag(media_chip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(media_chip, S(6));
  if (music_image != nullptr) {
    lv_obj_t *note = lv_image_create(media_chip);
    lv_image_set_src(note, music_image);
    lv_obj_set_style_image_recolor(note, lv_color_hex(accent()), 0);
    lv_obj_set_style_image_recolor_opa(note, LV_OPA_COVER, 0);
    lv_obj_remove_flag(note, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(note);
  } else {
    lv_obj_center(text(media_chip, fonts.body, TEXT, "\xE2\x99\xAA"));
  }
  lv_obj_add_event_cb(media_chip, [](lv_event_t *) { if (on_media_chip) on_media_chip(); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(media_chip, LV_OBJ_FLAG_HIDDEN);
  // The row is content sized; update_timer_chip() re-aligns it as it grows.
  lv_obj_add_flag(status_row, LV_OBJ_FLAG_HIDDEN);
  badge = box(top, 0xE5484D, S(9));
  lv_obj_set_size(badge, LV_SIZE_CONTENT, S(18));
  lv_obj_set_style_min_width(badge, S(18), 0);
  lv_obj_set_style_pad_hor(badge, S(4), 0);
  lv_obj_align_to(badge, bell, LV_ALIGN_TOP_RIGHT, 0, 0);
  badge_label = text(badge, fonts.small, TEXT, "");
  // White count on the red badge in every theme.
  lv_obj_set_style_text_color(badge_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_add_flag(badge, onx_render::FIXED_PALETTE);
  lv_obj_center(badge_label);
  lv_obj_remove_flag(badge, LV_OBJ_FLAG_CLICKABLE);

  // Arrival banner: compact icon + title header on top, full-width body below
  // so message content has room to breathe and read clearly at a glance.
  toast = box(top, CARD, S(12));
  lv_obj_set_size(toast, width - 2 * S(8), LV_SIZE_CONTENT);
  lv_obj_align(toast, LV_ALIGN_TOP_LEFT, S(8), S(6));
  lv_obj_set_style_pad_left(toast, S(12), 0);
  lv_obj_set_style_pad_right(toast, S(10), 0);
  lv_obj_set_style_pad_ver(toast, S(8), 0);
  lv_obj_set_style_border_width(toast, 1, 0);
  lv_obj_set_style_border_color(toast, lv_color_hex(control()), 0);
  lv_obj_set_flex_flow(toast, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(toast, S(5), 0);
  lv_obj_set_style_clip_corner(toast, true, 0);
  lv_obj_add_flag(toast, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(toast, LV_OBJ_FLAG_GESTURE_BUBBLE);
  toast_bar = box(toast, accent(), S(2));
  lv_obj_add_flag(toast_bar, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_pos(toast_bar, -S(8), 0);
  lv_obj_set_size(toast_bar, S(3), lv_pct(100));
  lv_obj_remove_flag(toast_bar, LV_OBJ_FLAG_CLICKABLE);
  auto *toast_header = box(toast, CARD, 0);
  lv_obj_set_style_bg_opa(toast_header, LV_OPA_TRANSP, 0);
  lv_obj_set_size(toast_header, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(toast_header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(toast_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(toast_header, S(6), 0);
  lv_obj_remove_flag(toast_header, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(toast_header, LV_OBJ_FLAG_EVENT_BUBBLE);
  toast_icon = notification_icon(toast_header, 0, 22);
  toast_title = text(toast_header, fonts.title, MUTED, "");
  lv_obj_set_flex_grow(toast_title, 1);
  lv_label_set_long_mode(toast_title, LV_LABEL_LONG_DOT);
  lv_obj_set_height(toast_title, lv_font_get_line_height(fonts.title));
  lv_obj_remove_flag(toast_title, LV_OBJ_FLAG_CLICKABLE);
  toast_clip = box(toast, CARD, 0);
  lv_obj_set_style_bg_opa(toast_clip, LV_OPA_TRANSP, 0);
  lv_obj_set_size(toast_clip, lv_pct(100), LV_SIZE_CONTENT);
  const int body_line_h = std::max(1, static_cast<int>(lv_font_get_line_height(fonts.body)));
  const int max_toast_lines = std::max(3, std::min(7, (height * 48 / 100) / body_line_h));
  lv_obj_set_style_max_height(toast_clip, max_toast_lines * body_line_h, 0);
  lv_obj_set_scrollbar_mode(toast_clip, LV_SCROLLBAR_MODE_OFF);
  // Scrolled only here (taps and swipes belong to the banner).
  lv_obj_remove_flag(toast_clip, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  toast_body = text(toast_clip, fonts.body, TEXT, "");
  lv_obj_set_width(toast_body, lv_pct(100));
  lv_label_set_long_mode(toast_body, LV_LABEL_LONG_WRAP);
  lv_obj_remove_flag(toast_body, LV_OBJ_FLAG_CLICKABLE);
  toast_more = text(toast, fonts.small, accent(), "Tap to read more");
  lv_obj_set_width(toast_more, lv_pct(100));
  lv_obj_set_style_text_align(toast_more, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_remove_flag(toast_more, LV_OBJ_FLAG_CLICKABLE);
  hide(toast_more, true);
  lv_obj_add_event_cb(toast, on_toast, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(toast, [](lv_event_t *) {
    auto *indev=lv_indev_active();
    if(!indev)return;
    const auto direction=lv_indev_get_gesture_dir(indev);
    if(direction!=LV_DIR_LEFT && direction!=LV_DIR_RIGHT && direction!=LV_DIR_TOP)return;
    touched();
    lv_indev_wait_release(indev);
    onx_notification::center.hide_toast();
  }, LV_EVENT_GESTURE, nullptr);

  // List panel: header with the count and Clear all, the cards, and a handle
  // along the bottom (swipe up or tap it to close).
  panel = box(top, BG, 0);
  lv_obj_set_size(panel, width, height);
  lv_obj_set_style_pad_hor(panel, S(10), 0);
  lv_obj_set_style_pad_top(panel, S(10), 0);
  lv_obj_set_style_pad_bottom(panel, S(4), 0);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(panel, S(8), 0);
  // Quick toggles first, then the notifications with their count and Clear all.
  auto *tiles=box(panel,BG,0);
  lv_obj_set_style_bg_opa(tiles,LV_OPA_TRANSP,0);
  lv_obj_set_size(tiles,lv_pct(100),S(36));
  lv_obj_set_flex_flow(tiles,LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(tiles,S(8),0);
  standby_button=quick_tile(tiles,clock_tile_image,[](lv_event_t *) {
    if(controls_blocked && controls_blocked())return;
    touched();
    if(toggle_standby_timeout)toggle_standby_timeout();
  });
  preset_button=quick_tile(tiles,preset_tile_image,[](lv_event_t *) {
    if(controls_blocked && controls_blocked())return;
    touched();
    presets_open=!presets_open;
    preset_error.clear();
    preset_signature.clear();
    refresh();
  });
  rotate_button=box(tiles,surface(),S(18));
  lv_obj_set_size(rotate_button,1,S(36));
  lv_obj_set_flex_grow(rotate_button,1);
  lv_obj_add_flag(rotate_button,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(rotate_button,[](lv_event_t *) {
    if(controls_blocked && controls_blocked())return;
    touched();
    open_rotate_dialog();
  },LV_EVENT_CLICKED,nullptr);
  if(rotate_tile_image!=nullptr){
    auto *icon=lv_image_create(rotate_button);
    lv_image_set_src(icon,rotate_tile_image);
    lv_obj_set_style_image_recolor(icon,lv_color_hex(foreground()),0);
    lv_obj_set_style_image_recolor_opa(icon,LV_OPA_COVER,0);
    lv_obj_remove_flag(icon,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(icon);
  } else lv_obj_center(text(rotate_button,fonts.symbol,foreground(),LV_SYMBOL_REFRESH));
  update_standby_button();
  lv_obj_t *header = box(panel, BG, 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
  lv_obj_set_size(header, lv_pct(100), S(36));
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(header, S(6), 0);
  lv_obj_set_style_pad_left(header, S(4), 0);
  header_label = text(header, fonts.title, TEXT, "Notifications");
  count_label = text(header, fonts.title, MUTED, "");
  lv_obj_t *spacer = box(header, BG, 0);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_size(spacer, 1, 1);
  lv_obj_set_flex_grow(spacer, 1);
  clear_button = button(header, "Clear all", CARD, on_clear, nullptr);
  lv_obj_set_height(clear_button, S(30));
  lv_obj_set_style_radius(clear_button, S(15), 0);
  lv_obj_set_style_pad_hor(clear_button, S(12), 0);
  lv_obj_set_style_text_font(lv_obj_get_child(clear_button, 0), fonts.small, 0);
  preset_list=box(panel,BG,0);
  lv_obj_set_style_bg_opa(preset_list,LV_OPA_TRANSP,0);
  lv_obj_set_width(preset_list,lv_pct(100));
  lv_obj_set_flex_grow(preset_list,1);
  lv_obj_set_flex_flow(preset_list,LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(preset_list,S(8),0);
  lv_obj_add_flag(preset_list,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(preset_list,LV_DIR_VER);
  lv_obj_remove_flag(preset_list,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_CHAIN));
  hide(preset_list,true);
  list = box(panel, BG, 0);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_width(list, lv_pct(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, S(8), 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  // Without elastic/chained scrolling a swipe past the end becomes a gesture.
  lv_obj_remove_flag(list, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_CHAIN));
  empty_label = text(list, fonts.body, MUTED, "No notifications");
  lv_obj_set_width(empty_label, lv_pct(100));
  lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_top(empty_label, S(40), 0);
  handle = box(panel, BG, 0);
  lv_obj_set_style_bg_opa(handle, LV_OPA_TRANSP, 0);
  lv_obj_set_size(handle, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(handle, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(handle, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(handle, S(4), 0);
  lv_obj_set_style_pad_ver(handle, S(4), 0);
  lv_obj_add_flag(handle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(handle, on_close_panel, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *grip = box(handle, MUTED, S(2));
  lv_obj_set_size(grip, S(36), S(4));
  lv_obj_remove_flag(grip, LV_OBJ_FLAG_CLICKABLE);
  handle_hint = text(handle, fonts.small, MUTED, "Swipe up to close");
  lv_obj_remove_flag(handle_hint, LV_OBJ_FLAG_CLICKABLE);

  // Detail view.
  detail = box(top, BG, 0);
  lv_obj_set_size(detail, width, height);
  lv_obj_set_style_pad_all(detail, S(10), 0);
  lv_obj_set_flex_flow(detail, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(detail, S(8), 0);
  lv_obj_add_flag(detail, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(detail, LV_DIR_VER);
  lv_obj_t *detail_header;
  build_header(detail, "Notification", &detail_header);
  button(detail_header, "Back", CARD, on_back, nullptr);
  auto *title_row = box(detail, BG, 0);
  lv_obj_set_size(title_row, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(title_row, S(8), 0);
  detail_image = notification_icon(title_row, 0);
  detail_title = text(title_row, fonts.title, TEXT, "");
  lv_obj_set_flex_grow(detail_title, 1);
  detail_age = text(detail, fonts.small, accent(), "");

  detail_body = text(detail, fonts.body, TEXT, "");
  for (auto *label : {detail_body}) {
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  }
  lv_obj_t *dismiss = button(detail, "Dismiss", CONTROL, on_detail_dismiss, nullptr);
  lv_obj_set_width(dismiss, lv_pct(100));

  for (auto *obj : {toast, panel, detail}) hide(obj, true);
  onx_notification::center.enabled = true;
  onx_notification::center.changed = []() { refresh(); };
  refresh();
}

inline void rebuild_list() {
  // Keep the empty-state label (the first child); drop the cards.
  while (lv_obj_get_child_count(list) > 1) lv_obj_delete(lv_obj_get_child(list, 1));
  const uint32_t now = esphome::millis();
  const int pad_l = S(16), pad_r = S(6), thumb = S(44), close_w = S(26);
  for (const auto &item : onx_notification::center.items) {
    lv_obj_t *card = box(list, surface(), S(12));
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(card, pad_l, 0);
    lv_obj_set_style_pad_right(card, pad_r, 0);
    lv_obj_set_style_pad_ver(card, S(10), 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(card, S(10), 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(card, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE));
    void *user = reinterpret_cast<void *>(static_cast<uintptr_t>(item.id));
    lv_obj_add_event_cb(card, on_item, LV_EVENT_CLICKED, user);
    // Accent bar along the left edge while unread.
    lv_obj_t *bar = box(card, item.read ? control() : accent(), S(2));
    lv_obj_add_flag(bar, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(bar, -pad_l + S(7), 0);
    lv_obj_set_size(bar, S(3), lv_pct(100));
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    auto *metadata = box(card, surface(), 0);
    lv_obj_set_style_bg_opa(metadata, LV_OPA_TRANSP, 0);
    lv_obj_set_size(metadata, thumb, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(metadata, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(metadata, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(metadata, S(4), 0);
    lv_obj_remove_flag(metadata, LV_OBJ_FLAG_CLICKABLE);
    notification_icon(metadata, item.id, 44);
    auto *age = text(metadata, fonts.small, muted(), when(item.received_ms, now).c_str());
    lv_obj_set_width(age, lv_pct(100));
    lv_obj_set_style_text_align(age, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(age, LV_LABEL_LONG_DOT);
    lv_obj_t *column = box(card, surface(), 0);
    lv_obj_set_style_bg_opa(column, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(column, 1);
    lv_obj_set_height(column, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(column, S(2), 0);
    lv_obj_remove_flag(column, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(column, LV_OBJ_FLAG_EVENT_BUBBLE);
    // Title uses the full text column; age is beneath the icon.
    lv_obj_t *top_row = box(column, surface(), 0);
    lv_obj_set_style_bg_opa(top_row, LV_OPA_TRANSP, 0);
    lv_obj_set_size(top_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(top_row, S(6), 0);
    lv_obj_remove_flag(top_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(top_row, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_t *title = text(top_row, fonts.title, item.read ? muted() : foreground(), item.title.c_str());
    lv_obj_set_flex_grow(title, 1);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_height(title, lv_font_get_line_height(fonts.title));
    // Up to two lines of the message.
    const int text_w = width - 2 * S(10) - pad_l - pad_r - close_w - S(10) - (thumb + S(10));
    // Two visible lines do not need to retain/queue an entire chat transcript.
    auto preview = onx_notification::clipped(item.body, 192);
    if (preview.size() < item.body.size()) preview += "...";
    lv_point_t size;
    lv_text_get_size(&size, preview.c_str(), fonts.body, 0, 0, std::max(40, text_w), LV_TEXT_FLAG_NONE);
    const int body_lines = std::clamp(int((size.y + lv_font_get_line_height(fonts.body) - 1) / lv_font_get_line_height(fonts.body)), 1, 2);
    lv_obj_t *body = text(column, fonts.body, muted(), preview.c_str());
    lv_obj_set_size(body, lv_pct(100), body_lines * lv_font_get_line_height(fonts.body));
    lv_label_set_long_mode(body, LV_LABEL_LONG_DOT);
    hide(body, item.body.empty());
    for (auto *label : {title, age, body}) lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    // Dismiss.
    lv_obj_t *close = box(card, surface(), S(13));
    lv_obj_set_style_bg_opa(close, LV_OPA_TRANSP, 0);
    lv_obj_set_size(close, close_w, close_w);
    lv_obj_add_flag(close, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(close, S(10));
    lv_obj_center(text(close, fonts.body, muted(), "\xC3\x97"));  // ×
    lv_obj_add_event_cb(close, on_item_dismiss, LV_EVENT_CLICKED, user);
  }
}

inline void open_detail(uint32_t id) {
  auto *item = onx_notification::center.find(id);
  if (item == nullptr) return;
  touched();
  detail_from_panel = panel_open;
  panel_open = false;
  detail_id = id;
  onx_emoji::set_label(detail_title, item->title.c_str());
  lv_label_set_text(detail_age, onx_notification::age_text(item->received_ms, esphome::millis()).c_str());
  onx_emoji::set_label(detail_body, item->body.c_str());
  image_widgets[detail_image] = id;
  auto found = thumbnails.find(id);
  lv_image_set_src(detail_image, found == thumbnails.end() || !found->second.pixels ? bell_image : &found->second.image);
  lv_obj_scroll_to_y(detail, 0, LV_ANIM_OFF);
  onx_notification::center.mark_read(id);  // Refreshes.
  refresh();
}

// Copy the shared downloader buffer into a stable, PSRAM-backed thumbnail.
inline void image_ready(const lv_image_dsc_t *dsc) {
  if (loading_id && dsc && dsc->data) {
    auto *pixels = static_cast<uint8_t *>(heap_caps_malloc(dsc->data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pixels) {
      std::memcpy(pixels, dsc->data, dsc->data_size);
      auto &thumb = thumbnails[loading_id];
      thumb.pixels = std::shared_ptr<uint8_t>(pixels, [](uint8_t *p) { heap_caps_free(p); });
      thumb.image = *dsc;
      thumb.image.data = pixels;
      for (auto &widget : image_widgets)
        if (widget.second == loading_id) lv_image_set_src(widget.first, &thumb.image);
    }
  }
  release_pending = true;
}
inline void image_failed() { release_pending = true; }

inline void refresh() {
  if (bell == nullptr) return;
  auto &center = onx_notification::center;
  const uint32_t now = esphome::millis();
  if (detail_id != 0 && center.find(detail_id) == nullptr) close_detail();
  const size_t unread = center.unread();
  // Bell only on the main carousel (dimmed when empty); every screen can pull
  // down from the top edge to open the centre.
  const bool bell_shown = bell_wanted && !panel_open && detail_id == 0 && !center.toast_visible(now);
  hide(bell, !bell_shown);
  lv_obj_set_style_opa(bell, center.items.empty() ? LV_OPA_50 : LV_OPA_COVER, 0);
  hide(badge, unread == 0 || !bell_shown);
  char count[8];
  snprintf(count, sizeof(count), "%u", static_cast<unsigned>(unread > 99 ? 99 : unread));
  lv_label_set_text(badge_label, count);
  lv_obj_align_to(badge, bell, LV_ALIGN_TOP_RIGHT, 0, 0);
  // Accent colour follows the device setting.
  lv_obj_set_style_border_color(toast, lv_color_hex(control()), 0);
  lv_obj_set_style_text_color(detail_age, lv_color_hex(accent()), 0);
  if (toast_more != nullptr) lv_obj_set_style_text_color(toast_more, lv_color_hex(accent()), 0);
  char count_text[16] = "";
  if (!center.items.empty()) snprintf(count_text, sizeof(count_text), "\xC2\xB7 %u", static_cast<unsigned>(center.items.size()));
  const char *heading = presets_open ? "Presets" : "Notifications";
  if (std::strcmp(lv_label_get_text(header_label), heading) != 0)
    lv_label_set_text_static(header_label, heading);
  lv_label_set_text(count_label, count_text);
  hide(count_label, presets_open);
  hide(clear_button, presets_open || center.items.empty());
  lv_obj_set_style_bg_color(panel, lv_color_hex(bg()), 0);
  lv_obj_set_style_bg_color(clear_button, lv_color_hex(surface()), 0);
  update_standby_button();
  update_presets();

  const bool show_toast = center.toast_visible(now) && !panel_open && detail_id == 0;
  onx_notification::CenterItem *toast_item = show_toast ? center.find(center.toast_id) : nullptr;
  if (toast_item != nullptr) {
    onx_emoji::set_label(toast_title, toast_item->title.c_str());
    if (image_widgets[toast_icon] != toast_item->id) {
      image_widgets[toast_icon] = toast_item->id;
      auto found = thumbnails.find(toast_item->id);
      lv_image_set_src(toast_icon, found == thumbnails.end() || !found->second.pixels ? bell_image : &found->second.image);
    }
    static uint32_t scrolled_id = 0;
    static std::string scrolled_body;
    if (scrolled_id != toast_item->id || scrolled_body != toast_item->body) {
      scrolled_id = toast_item->id;
      scrolled_body = toast_item->body;
      lv_obj_scroll_to_y(toast_clip, 0, LV_ANIM_OFF);  // A shorter text starts at the top.
      onx_emoji::set_label(toast_body, toast_item->body.c_str());
      hide(toast_more, true);
      scroll_toast = true;
    }
    lv_obj_set_style_bg_color(toast_bar, lv_color_hex(accent()), 0);
  }
  hide(toast, toast_item == nullptr);
  // Once the banner is shown and laid out, check whether the body overflows:
  // show the "Tap to read ..." hint, and for multi-line conversations scroll
  // to the latest lines. Start/update the timeout bar after final sizing.
  if (toast_item != nullptr) {
    if (scroll_toast) {
      scroll_toast = false;
      lv_obj_update_layout(toast);
      const int32_t below = lv_obj_get_scroll_bottom(toast_clip);
      if (below > 0) {
        const bool multiline = toast_item->body.find('\n') != std::string::npos;
        if (multiline) lv_obj_scroll_to_y(toast_clip, lv_obj_get_scroll_y(toast_clip) + below, LV_ANIM_OFF);
        lv_label_set_text_static(toast_more, multiline ? "Tap to read all" : "Tap to read more");
        hide(toast_more, false);
        lv_obj_update_layout(toast);
      }
    }
    // Timeout bar: one LVGL animation shrinks it smoothly every frame (a
    // 3 px strip, so each step redraws almost nothing). Restarted only when
    // the banner or its deadline changes.
    static uint32_t animated_id = 0, animated_until = 0;
    if (animated_id != toast_item->id || animated_until != center.toast_until) {
      animated_id = toast_item->id;
      animated_until = center.toast_until;
      lv_obj_update_layout(toast);
      const int full = std::max<int>(1, lv_obj_get_content_height(toast));
      const uint32_t total = std::max<uint32_t>(1, center.toast_until - toast_item->received_ms);
      const uint32_t left = center.toast_until > now ? center.toast_until - now : 0;
      lv_anim_delete(toast_bar, nullptr);
      lv_obj_set_height(toast_bar, int(uint64_t(full) * left / total));
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, toast_bar);
      lv_anim_set_values(&a, int(uint64_t(full) * left / total), 0);
      lv_anim_set_duration(&a, std::max<uint32_t>(1, left));
      lv_anim_set_path_cb(&a, lv_anim_path_linear);
      lv_anim_set_exec_cb(&a, [](void *bar, int32_t h) { lv_obj_set_height(static_cast<lv_obj_t *>(bar), std::max<int32_t>(1, h)); });
      lv_anim_start(&a);
    }
  }

  if (sheet_mode != SheetMode::CLOSING) hide(panel, !panel_open);
  // Rebuilt on changes, and once a minute while open so the times stay current.
  static uint32_t built_minute = 0;
  const uint32_t minute = now / 60000;
  if (keep_list) built_revision = center.revision;
  if (panel_open && (built_revision != center.revision || built_minute != minute)) {
    built_revision = center.revision;
    built_minute = minute;
    rebuild_list();
  }
  hide(empty_label, !center.items.empty());
  hide(detail, detail_id == 0);
  if (panel_open || detail_id != 0 || show_toast) lv_obj_move_foreground(panel_open ? panel : detail_id ? detail : toast);
  lv_obj_move_foreground(bell);
  lv_obj_move_foreground(badge);
}

// A touch anywhere else hides the arrival banner (the notification stays in
// the centre) and still reaches whatever was touched.
inline void hide_toast_unless(int x,int y);
inline bool toast_hit(int x,int y) {
  if(!toast || !lv_obj_is_visible(toast))return false;
  lv_area_t a;lv_obj_get_coords(toast,&a);
  return x>=a.x1 && x<=a.x2 && y>=a.y1 && y<=a.y2;
}
inline void hide_toast_unless(int x,int y) {
  if(!onx_notification::center.toast_visible(esphome::millis()) || toast_hit(x,y))return;
  onx_notification::center.hide_toast();
  refresh();
}
// Touches on the visible bell go to LVGL rather than the gesture router.
inline bool bell_hit(int x, int y) {
  if (bell == nullptr || !lv_obj_is_visible(bell)) return false;
  lv_area_t a;
  lv_obj_get_coords(bell, &a);
  // Match lv_obj_set_ext_click_area(bell, S(8)) on every side. A smaller
  // passthrough area lets an edge tap also press the carousel entity below.
  const int pad = S(8);
  return x >= a.x1 - pad && x <= a.x2 + pad && y >= a.y1 - pad && y <= a.y2 + pad;
}
// The timer chip is also an LVGL target. Otherwise its tap first enters the
// underlying entity through the virtual button before opening the timer overlay.
inline bool chip_hit(lv_obj_t *chip, int x, int y) {
  if (chip == nullptr || !lv_obj_is_visible(chip)) return false;
  lv_area_t a;
  lv_obj_get_coords(chip, &a);
  const int pad = S(6);
  return x >= a.x1 - pad && x <= a.x2 + pad && y >= a.y1 - pad && y <= a.y2 + pad;
}
// Timer and music chips: their taps go to LVGL, not the gesture router.
inline bool timer_chip_hit(int x, int y) { return chip_hit(timer_chip, x, y) || chip_hit(media_chip, x, y); }
// Called by the panel's UI tick: the bell belongs to the main carousel only.
inline void set_bell_visible(bool visible) {
  if (bell == nullptr || visible == bell_wanted) return;
  bell_wanted = visible;
  update_timer_chip();
  refresh();
}

// Periodic: banner timeout and list ages.
inline void tick() {
  if (release_pending) {
    if (hooks.release_image) hooks.release_image();
    release_pending = false;
    loading_id = 0;
  }
  // Drop widgets before freeing their descriptors. Retained thumbnails are PSRAM-backed.
  for (auto it = thumbnails.begin(); it != thumbnails.end();) {
    if (!onx_notification::center.find(it->first)) {
      for (auto &widget : image_widgets)
        if (widget.second == it->first) lv_image_set_src(widget.first, bell_image);
      lv_image_cache_drop(&it->second.image);
      lv_image_header_cache_drop(&it->second.image);
      it = thumbnails.erase(it);
    } else ++it;
  }
  if (!loading_id && owns_screen() && hooks.load_image) {
    for (const auto &item : onx_notification::center.items) {
      const bool needed = panel_open || detail_id == item.id ||
          (detail_id == 0 && onx_notification::center.toast_visible(esphome::millis()) &&
           onx_notification::center.toast_id == item.id);
      if (!needed || item.image_url.empty() || thumbnails.count(item.id)) continue;
      // One attempt per notification; network failure keeps the bell fallback.
      thumbnails.try_emplace(item.id);
      loading_id = item.id;
      hooks.load_image(item.image_url);
      break;
    }
  }
  if (onx_notification::center.toast_visible(esphome::millis())) refresh();
  static uint32_t minute = 0;
  if (panel_open && esphome::millis() / 60000 != minute) {
    minute = esphome::millis() / 60000;
    refresh();
  }
  static bool toast_was_visible = false;
  const bool visible = onx_notification::center.toast_visible(esphome::millis());
  if (visible != toast_was_visible) {
    toast_was_visible = visible;
    refresh();
  }
}

}  // namespace onx_notification_ui
