#include "ui.h"

#include <stdio.h>

#include "theme.h"
#include "ui_internal.h"

constexpr int SCREEN_W = 320;
constexpr int STATUS_H = 22;
constexpr int TABBAR_H = 40;
constexpr int CONTENT_H = 240 - STATUS_H - TABBAR_H;

static lv_obj_t* s_tabview;
static lv_obj_t* s_overlay;
static uint32_t s_activeTab = 0;
static lv_obj_t* s_ota;
static lv_obj_t* s_otaLabel;
static lv_obj_t* s_otaBar;
static lv_obj_t* s_toast;
static lv_timer_t* s_toastTimer;

static void tabChanged(lv_event_t* e) {
  s_activeTab = lv_tabview_get_tab_active(s_tabview);
}

// Página do tabview: sem padding e sem rolagem, para o deslize ir direto
// para o tabview trocar de aba.
static lv_obj_t* addPage(const char* name) {
  lv_obj_t* page = lv_tabview_add_tab(s_tabview, name);
  lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_all(page, 0, 0);
  return page;
}

static void styleTabBar() {
  lv_obj_t* bar = lv_tabview_get_tab_bar(s_tabview);
  lv_obj_set_style_bg_color(bar, lv_color_hex(col::tabbar_bg), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_border_color(bar, lv_color_hex(col::tabbar_border), 0);
  lv_obj_set_style_pad_all(bar, 4, 0);
  lv_obj_set_style_pad_column(bar, 4, 0);

  uint32_t count = lv_obj_get_child_count(bar);
  for (uint32_t i = 0; i < count; i++) {
    lv_obj_t* btn = lv_obj_get_child(bar, i);
    lv_obj_add_style(btn, &st_tab, 0);
    lv_obj_add_style(btn, &st_tab_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &st_tab_checked, LV_STATE_CHECKED);
    // com 5 abas o botão tem ~59x31; a área extra cobre o padding e a célula inteira de 64x40
    lv_obj_set_ext_click_area(btn, 4);
  }
}

static void createOverlay() {
  // provisório até a tela "Aguardando o Mac..." ser desenhada (seção 10)
  s_overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(s_overlay, 0, STATUS_H);
  lv_obj_set_size(s_overlay, SCREEN_W, CONTENT_H);
  lv_obj_set_style_bg_color(s_overlay, lv_color_hex(col::bg), 0);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_90, 0);

  lv_obj_t* label = lv_label_create(s_overlay);
  lv_obj_add_style(label, &st_f10_t2, 0);
  lv_obj_set_style_text_font(label, F12, 0);
  lv_label_set_text(label, "Aguardando o Mac...");
  lv_obj_center(label);
}

void ui_init(lv_display_t* disp, uint32_t initialTab) {
  theme_init();

  lv_obj_t* scr = lv_display_get_screen_active(disp);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, lv_color_hex(col::bg), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(scr, lv_color_hex(col::text_1), 0);
  lv_obj_set_style_text_font(scr, F12, 0);

  status_bar_create(scr);

  s_tabview = lv_tabview_create(scr);
  lv_obj_set_pos(s_tabview, 0, STATUS_H);
  lv_obj_set_size(s_tabview, SCREEN_W, CONTENT_H + TABBAR_H);
  lv_tabview_set_tab_bar_position(s_tabview, LV_DIR_BOTTOM);
  lv_tabview_set_tab_bar_size(s_tabview, TABBAR_H);

  tab_overview_create(addPage("Geral"));
  tab_history_create(addPage("Histórico"));
  tab_cores_create(addPage("Núcleos"));
  tab_network_create(addPage("Rede"));
  tab_claude_create(addPage("Claude"));

  styleTabBar();
  lv_obj_add_event_cb(s_tabview, tabChanged, LV_EVENT_VALUE_CHANGED, nullptr);
  if (initialTab < lv_tabview_get_tab_count(s_tabview)) {
    lv_tabview_set_active(s_tabview, initialTab, LV_ANIM_OFF);
    s_activeTab = initialTab;
  }

  createOverlay();
  ui_set_online(false);
}

void ui_update(const Metrics& m) {
  // todas as abas são atualizadas, visíveis ou não, para os gráficos acumularem
  status_bar_update(m);
  tab_overview_update(m);
  tab_history_update(m);
  tab_cores_update(m);
  tab_network_update(m);
}

void ui_update_claude(const ClaudeUsage& c) {
  tab_claude_update(c);
}

void ui_set_online(bool online) {
  status_bar_set_online(online);
  if (online) {
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  }
}

void ui_tick_offline() {
  // abre um vão nas linhas dos gráficos; o resto fica com o último valor
  tab_history_gap();
  tab_network_gap();
}

// Tela inteira, por cima da barra de status e do overlay "Aguardando o Mac..."
static void createOtaPanel() {
  s_ota = lv_obj_create(lv_layer_top());
  lv_obj_remove_flag(s_ota, LV_OBJ_FLAG_SCROLLABLE);  // clicável: bloqueia o toque na tela
  lv_obj_set_size(s_ota, SCREEN_W, 240);
  lv_obj_set_style_bg_color(s_ota, lv_color_hex(col::bg), 0);
  lv_obj_set_style_bg_opa(s_ota, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_ota, 0, 0);
  lv_obj_set_style_radius(s_ota, 0, 0);
  lv_obj_set_flex_flow(s_ota, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_ota, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_ota, 12, 0);

  s_otaLabel = ui_label(s_ota, &st_f12_t1, "");

  s_otaBar = lv_bar_create(s_ota);
  lv_obj_remove_flag(s_otaBar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(s_otaBar, 200, 8);
  lv_bar_set_range(s_otaBar, 0, 100);
  lv_obj_set_style_radius(s_otaBar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_radius(s_otaBar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s_otaBar, lv_color_hex(col::track), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_otaBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_otaBar, lv_color_hex(col::cpu), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s_otaBar, LV_OPA_COVER, LV_PART_INDICATOR);
}

void ui_ota_progress(int pct) {
  if (!s_ota) createOtaPanel();
  lv_obj_remove_flag(s_ota, LV_OBJ_FLAG_HIDDEN);
  char buf[32];
  snprintf(buf, sizeof(buf), "Atualizando firmware %d%%", pct);
  ui_set_text(s_otaLabel, buf);
  lv_bar_set_value(s_otaBar, pct, LV_ANIM_OFF);
}

void ui_ota_message(const char* text) {
  if (!s_ota) createOtaPanel();
  lv_obj_remove_flag(s_ota, LV_OBJ_FLAG_HIDDEN);
  ui_set_text(s_otaLabel, text);
}

void ui_ota_hide() {
  if (s_ota) lv_obj_add_flag(s_ota, LV_OBJ_FLAG_HIDDEN);
}

static void hideToast(lv_timer_t* t) {
  lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
}

void ui_toast(const char* text) {
  if (!s_toast) {
    s_toast = lv_label_create(lv_layer_top());
    lv_obj_add_style(s_toast, &st_f12_t1, 0);
    lv_obj_set_style_bg_color(s_toast, lv_color_hex(col::tab_active_bg), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_toast, 6, 0);
    lv_obj_set_style_pad_hor(s_toast, 12, 0);
    lv_obj_set_style_pad_ver(s_toast, 8, 0);
    lv_obj_center(s_toast);
    s_toastTimer = lv_timer_create(hideToast, 1500, nullptr);
    lv_timer_set_auto_delete(s_toastTimer, false);
  }
  lv_label_set_text(s_toast, text);
  lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_toast);
  // um disparo só, reiniciado a cada chamada
  lv_timer_set_repeat_count(s_toastTimer, 1);
  lv_timer_reset(s_toastTimer);
  lv_timer_resume(s_toastTimer);
}

uint32_t ui_active_tab() {
  return s_activeTab;
}
