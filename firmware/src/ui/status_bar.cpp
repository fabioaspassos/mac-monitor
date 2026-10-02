#include <stdio.h>

#include "../backlight.h"
#include "../settings.h"
#include "format.h"
#include "theme.h"
#include "ui.h"
#include "ui_internal.h"

static lv_obj_t* s_liveDot;
static lv_obj_t* s_liveLabel;
static lv_obj_t* s_pressDot;
static lv_obj_t* s_pressLabel;
static int s_press = -1;

static void dotOpaAnim(void* dot, int32_t opa) {
  lv_obj_set_style_bg_opa((lv_obj_t*)dot, (lv_opa_t)opa, 0);
}

// Toque na barra de status troca o modo de brilho
static void barClicked(lv_event_t* e) {
  settings_set_brightness_mode(backlight_cycle_mode());
  char buf[24];
  snprintf(buf, sizeof(buf), "Brilho: %s", backlight_mode_name());
  ui_toast(buf);
}

void status_bar_create(lv_obj_t* parent) {
  lv_obj_t* bar = ui_box(parent);
  lv_obj_add_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(bar, barClicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_size(bar, 320, 22);
  lv_obj_set_style_bg_color(bar, lv_color_hex(col::bg), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(bar, 10, 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* left = ui_row(bar, 5);
  s_liveDot = ui_dot(left, 6, col::text_3);
  s_liveLabel = ui_label(left, &st_f10_t2, "aguardando");

  lv_obj_t* right = ui_row(bar, 5);
  s_pressDot = ui_dot(right, 6, col::text_3);
  s_pressLabel = ui_label(right, &st_f10_t2, "");
}

void status_bar_set_online(bool online) {
  lv_anim_delete(s_liveDot, dotOpaAnim);
  lv_obj_set_style_bg_opa(s_liveDot, LV_OPA_COVER, 0);

  if (online) {
    lv_obj_set_style_bg_color(s_liveDot, lv_color_hex(col::ok), 0);
    lv_label_set_text(s_liveLabel, "ao vivo");

    // pulso: 100% → 30% → 100% em 2 s, sem fim
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_liveDot);
    lv_anim_set_exec_cb(&a, dotOpaAnim);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_30);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_playback_duration(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
  } else {
    lv_obj_set_style_bg_color(s_liveDot, lv_color_hex(col::text_3), 0);
    lv_label_set_text(s_liveLabel, "aguardando");
  }
}

void status_bar_update(const Metrics& m) {
  const char* pressText;
  uint32_t pressColor;
  switch (m.press) {
    case 1: pressText = "Pressão normal"; pressColor = col::ok; break;
    case 2: pressText = "Pressão em alerta"; pressColor = col::warn; break;
    case 4: pressText = "Pressão crítica"; pressColor = col::crit; break;
    default: pressText = "Pressão indisponível"; pressColor = col::text_3; break;
  }
  if (m.press != s_press) {
    s_press = m.press;
    lv_obj_set_style_bg_color(s_pressDot, lv_color_hex(pressColor), 0);
  }

  char swap[12];
  char buf[48];
  fmt_swap(swap, sizeof(swap), m.swap);
  snprintf(buf, sizeof(buf), "%s · swap %s", pressText, swap);
  ui_set_text(s_pressLabel, buf);
}
