// Claude: uso do Claude Code (docs/claude/claude-code-dados-agente.md)
// Arco do bloco de 5 h, card com tokens e ritmo, barras de tokens por hora de hoje.

#include <Arduino.h>
#include <stdio.h>

#include "format.h"
#include "theme.h"
#include "ui_internal.h"

namespace {

// Posições relativas à página (320x178, a tela começa em y = 22)
constexpr int TOP_Y = 6;
constexpr int LEFT_X = 10, LEFT_W = 98;
constexpr int ARC = 76;
constexpr int ARC_Y = 2;                    // dentro da coluna da esquerda
constexpr int CARD_X = 118, CARD_W = 192;
constexpr int TOP_H = 100;                  // coluna da esquerda e card
constexpr int CAPTION_Y = 110;
constexpr int BARS_X = 10, BARS_W = 300;
constexpr int BARS_Y = 127;
constexpr int BARS_H = 34;                  // a maior hora do dia ocupa a altura toda
constexpr int STUB_H = 2;                   // hora sem uso
constexpr int HOURS_Y = BARS_Y + BARS_H + 3;

constexpr uint32_t STALE_MS = 60000;        // sem linha "cc" há 60 s: dado velho

const char* const WEEKDAYS[7] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};

// As 24 barras e a linha de base são desenhadas por um objeto só (drawBars),
// para não gastar 24 lv_obj do heap de 48 KB do LVGL
struct Bar {
  int h;
  uint32_t color;
};

lv_obj_t* body;
lv_obj_t* notFound;
lv_obj_t* arc;
lv_obj_t* number;
lv_obj_t* pctSign;
lv_obj_t* subLabel;
lv_obj_t* blockValue;
lv_obj_t* dayValue;
lv_obj_t* rateValue;
lv_obj_t* lastName;    // "Semana" ou, sem dado da semana, "Sessões ativas"
lv_obj_t* lastValue;
lv_obj_t* hourLabel;
lv_obj_t* barsObj;
Bar bars[24];

ClaudeUsage data;
bool received = false;
uint32_t receivedAt = 0;
int arcPct = -1;
uint32_t arcColor = col::claude;

// Coluna i das barras: 24 colunas em 302 px (300 + o espaço de 2 px da última)
int barX(int i) {
  return BARS_X + (i * (BARS_W + 2) + 12) / 24;
}

void arcAnim(void* obj, int32_t value) {
  lv_arc_set_value((lv_obj_t*)obj, value);
}

void setArc(int pct) {
  if (pct == arcPct) return;
  arcPct = pct;
  uint32_t color = value_color(pct, col::claude);
  if (color != arcColor) {
    arcColor = color;
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
  }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, arc);
  lv_anim_set_exec_cb(&a, arcAnim);
  lv_anim_set_values(&a, lv_arc_get_value(arc), pct);
  lv_anim_set_duration(&a, 300);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

void drawBars(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t coords;
  lv_obj_get_coords(barsObj, &coords);

  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_opa = LV_OPA_COVER;
  dsc.radius = 2;
  for (int i = 0; i < 24; i++) {
    lv_area_t a;
    a.x1 = coords.x1 + barX(i) - BARS_X;
    a.x2 = coords.x1 + barX(i + 1) - BARS_X - 3;  // 2 px de espaço até a próxima
    a.y2 = coords.y1 + BARS_H - 1;
    a.y1 = a.y2 - bars[i].h + 1;
    dsc.bg_color = lv_color_hex(bars[i].color);
    lv_draw_rect(layer, &dsc, &a);
  }

  lv_area_t base = {coords.x1, coords.y1 + BARS_H, coords.x2, coords.y1 + BARS_H};
  dsc.radius = 0;
  dsc.bg_color = lv_color_hex(col::baseline);
  lv_draw_rect(layer, &dsc, &base);
}

void createLeft(lv_obj_t* parent) {
  lv_obj_t* left = ui_box(parent);
  lv_obj_set_pos(left, LEFT_X, TOP_Y);
  lv_obj_set_size(left, LEFT_W, TOP_H);

  arc = lv_arc_create(left);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(arc, ARC, ARC);
  lv_obj_set_pos(arc, (LEFT_W - ARC) / 2, ARC_Y);
  lv_arc_set_bg_angles(arc, 135, 45);  // 270° com a abertura para baixo, como na tela Geral
  lv_arc_set_range(arc, 0, 100);
  lv_arc_set_value(arc, 0);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lv_color_hex(col::track), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, lv_color_hex(col::claude), LV_PART_INDICATOR);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);

  // número em F20 e "%" em F10 na mesma linha de base (mesma técnica da tela Geral)
  lv_obj_t* value = ui_row(left, 1);
  lv_obj_set_flex_align(value, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  number = ui_label(value, &st_f12_t1, "--");
  lv_obj_set_style_text_font(number, F20, 0);
  pctSign = ui_label(value, &st_f10_t2, "%");
  lv_obj_set_style_pad_bottom(pctSign, F20->base_line - F10->base_line, 0);
  lv_obj_add_flag(pctSign, LV_OBJ_FLAG_HIDDEN);
  lv_obj_align(value, LV_ALIGN_TOP_MID, 0, ARC_Y + ARC / 2 - F20->line_height / 2);

  // na abertura do arco
  lv_obj_t* caption = ui_label(left, &st_caption, "BLOCO 5 H");
  lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 70);

  subLabel = ui_label(left, &st_f10_t2, "sem dados");
  lv_obj_set_style_text_color(subLabel, lv_color_hex(col::text_1), 0);
  lv_obj_align(subLabel, LV_ALIGN_TOP_MID, 0, 85);
}

// Linha "rótulo ........ valor" do card; devolve o label do valor
lv_obj_t* createCardRow(lv_obj_t* card, const char* name, lv_obj_t** nameLabel = nullptr) {
  lv_obj_t* row = ui_box(card);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* label = ui_label(row, &st_f10_t2, name);
  if (nameLabel) *nameLabel = label;
  return ui_label(row, &st_f12_t1, "--");
}

void createCard(lv_obj_t* parent) {
  lv_obj_t* card = ui_box(parent);
  lv_obj_add_style(card, &st_card, 0);
  lv_obj_set_pos(card, CARD_X, TOP_Y);
  lv_obj_set_size(card, CARD_W, TOP_H);
  lv_obj_set_style_pad_hor(card, 12, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  blockValue = createCardRow(card, "Bloco atual");
  dayValue = createCardRow(card, "Hoje");
  rateValue = createCardRow(card, "Ritmo agora");
  lastValue = createCardRow(card, "Semana", &lastName);
}

void createHourLabel(lv_obj_t* parent, int hour, lv_align_t align) {
  char buf[4];
  snprintf(buf, sizeof(buf), "%dh", hour);
  lv_obj_t* label = ui_label(parent, &st_f10_t3, buf);
  if (align == LV_ALIGN_TOP_LEFT) {
    lv_obj_set_pos(label, BARS_X, HOURS_Y);
  } else if (align == LV_ALIGN_TOP_RIGHT) {
    lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -(320 - BARS_X - BARS_W), HOURS_Y);
  } else {
    // centralizado na barra da hora
    int center = (barX(hour) + barX(hour + 1) - 2) / 2;
    lv_obj_align(label, LV_ALIGN_TOP_MID, center - 160, HOURS_Y);
  }
}

void createChart(lv_obj_t* parent) {
  lv_obj_t* header = ui_box(parent);
  lv_obj_set_pos(header, BARS_X, CAPTION_Y);
  lv_obj_set_width(header, BARS_W);
  ui_label(header, &st_caption, "TOKENS POR HORA · HOJE");
  hourLabel = ui_label(header, &st_f10_t2, "");
  lv_obj_set_style_text_color(hourLabel, lv_color_hex(col::text_1), 0);
  lv_obj_align(hourLabel, LV_ALIGN_TOP_RIGHT, 0, 0);

  barsObj = ui_box(parent);
  lv_obj_set_pos(barsObj, BARS_X, BARS_Y);
  lv_obj_set_size(barsObj, BARS_W, BARS_H + 1);  // + linha de base
  lv_obj_add_event_cb(barsObj, drawBars, LV_EVENT_DRAW_MAIN, nullptr);
  for (Bar& b : bars) b = {STUB_H, col::track};

  createHourLabel(parent, 0, LV_ALIGN_TOP_LEFT);
  createHourLabel(parent, 6, LV_ALIGN_TOP_MID);
  createHourLabel(parent, 12, LV_ALIGN_TOP_MID);
  createHourLabel(parent, 18, LV_ALIGN_TOP_MID);
  createHourLabel(parent, 23, LV_ALIGN_TOP_RIGHT);
}

void renderBars(bool fresh) {
  long peak = 1;
  if (fresh) {
    for (long v : data.hours) peak = v > peak ? v : peak;
  }
  bool changed = false;
  for (int i = 0; i < 24; i++) {
    long v = fresh ? data.hours[i] : 0;
    int h = v > 0 ? (int)((v * BARS_H + peak / 2) / peak) : 0;
    if (h < STUB_H) h = STUB_H;
    uint32_t color;
    if (!fresh || i > data.hour) {
      color = col::track;
    } else if (i == data.hour) {
      color = col::claude;
    } else {
      color = v > 0 ? col::claude_dim : col::bar_idle;
    }
    if (h != bars[i].h || color != bars[i].color) {
      bars[i] = {h, color};
      changed = true;
    }
  }
  if (changed) lv_obj_invalidate(barsObj);

  if (!fresh) {
    ui_set_text(hourLabel, "");
    return;
  }
  char tokens[16];
  char buf[32];
  fmt_tokens(tokens, sizeof(tokens), data.hours[data.hour] * 1000);
  snprintf(buf, sizeof(buf), "%dh · %s", data.hour, tokens);
  ui_set_text(hourLabel, buf);
}

void setTokens(lv_obj_t* label, long tokens) {
  char num[16];
  char buf[24];
  fmt_tokens(num, sizeof(num), tokens);
  snprintf(buf, sizeof(buf), "%s tok", num);
  ui_set_text(label, buf);
}

// Redesenha a partir da última linha "cc"; chamada na chegada e 1x por segundo,
// para descontar o reinício e perceber o dado velho. Só mexe no que mudou.
void render() {
  if (received && !data.ok) {
    lv_obj_add_flag(body, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(notFound, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_remove_flag(body, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(notFound, LV_OBJ_FLAG_HIDDEN);

  uint32_t elapsed = millis() - receivedAt;
  bool fresh = received && elapsed <= STALE_MS;
  renderBars(fresh);

  char buf[32];
  if (!fresh) {
    setArc(0);
    ui_set_text(number, "--");
    lv_obj_add_flag(pctSign, LV_OBJ_FLAG_HIDDEN);
    ui_set_text(subLabel, "sem dados");
    ui_set_text(blockValue, "--");
    ui_set_text(dayValue, "--");
    ui_set_text(rateValue, "--");
    ui_set_text(lastName, "Semana");
    ui_set_text(lastValue, "--");
    return;
  }

  // Bloco de 5 h
  if (data.hasBlock && data.blockPct >= 0) {
    setArc(data.blockPct);
    snprintf(buf, sizeof(buf), "%d", data.blockPct);
    ui_set_text(number, buf);
    lv_obj_remove_flag(pctSign, LV_OBJ_FLAG_HIDDEN);
    long left = data.blockRst - (long)(elapsed / 1000);
    if (left > 0) {
      char dur[12];
      fmt_duration(dur, sizeof(dur), left);
      snprintf(buf, sizeof(buf), "reinicia em %s", dur);
      ui_set_text(subLabel, buf);
    } else {
      ui_set_text(subLabel, "reiniciando");
    }
  } else {
    // sem o percentual oficial não inventa número: estimado ou sem bloco
    setArc(0);
    ui_set_text(number, "--");
    lv_obj_add_flag(pctSign, LV_OBJ_FLAG_HIDDEN);
    ui_set_text(subLabel, data.src == ClaudeUsage::SRC_EST && data.hasBlock ? "estimado" : "sem bloco ativo");
  }

  // Card
  setTokens(blockValue, data.tokBlock);
  setTokens(dayValue, data.tokDay);
  fmt_token_rate(buf, sizeof(buf), data.rate);
  ui_set_text(rateValue, buf);

  if (data.hasWeek) {
    ui_set_text(lastName, "Semana");
    if (data.weekDay >= 0 && data.weekDay < 7 && data.weekHour >= 0) {
      snprintf(buf, sizeof(buf), "%d%% · %s %dh", data.weekPct, WEEKDAYS[data.weekDay], data.weekHour);
    } else {
      snprintf(buf, sizeof(buf), "%d%%", data.weekPct);
    }
  } else {
    ui_set_text(lastName, "Sessões ativas");
    snprintf(buf, sizeof(buf), "%d", data.sessions);
  }
  ui_set_text(lastValue, buf);
}

void tick(lv_timer_t*) {
  render();
}

}  // namespace

void tab_claude_create(lv_obj_t* page) {
  body = ui_box(page);
  lv_obj_set_size(body, LV_PCT(100), LV_PCT(100));
  createLeft(body);
  createCard(body);
  createChart(body);

  // provisório até o estado "não encontrado" ser desenhado
  notFound = ui_label(page, &st_f10_t2, "Claude Code não encontrado");
  lv_obj_set_style_text_font(notFound, F12, 0);
  lv_obj_center(notFound);
  lv_obj_add_flag(notFound, LV_OBJ_FLAG_HIDDEN);

  render();
  lv_timer_create(tick, 1000, nullptr);
}

void tab_claude_update(const ClaudeUsage& c) {
  data = c;
  received = true;
  receivedAt = millis();
  render();
}
