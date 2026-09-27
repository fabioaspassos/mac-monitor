#pragma once

#include <lvgl.h>
#include <stdint.h>

// Regra de cor dos arcos (Geral) e das barras (Núcleos), seção 3.3
#define UI_COLOR_PER_METRIC 0
#define UI_COLOR_BY_RANGE 1
#ifndef UI_COLOR_MODE
#define UI_COLOR_MODE UI_COLOR_PER_METRIC
#endif

// Tokens de cor (seção 3.1)
namespace col {
constexpr uint32_t bg = 0x0B0E13;
constexpr uint32_t tabbar_bg = 0x0F131A;
constexpr uint32_t tabbar_border = 0x1B212B;
constexpr uint32_t tab_active_bg = 0x1D2432;
constexpr uint32_t tab_pressed_bg = 0x171D28;
constexpr uint32_t card = 0x131821;
constexpr uint32_t track = 0x1E2530;
constexpr uint32_t grid = 0x1A212B;
constexpr uint32_t baseline = 0x262E3A;
constexpr uint32_t text_1 = 0xE9EDF3;
constexpr uint32_t text_active = 0xF1F4F8;
constexpr uint32_t text_2 = 0x8D97A6;
constexpr uint32_t text_3 = 0x737D8C;
constexpr uint32_t text_core = 0xC9D0DA;
constexpr uint32_t cpu = 0x0090FF;
constexpr uint32_t mem = 0xA040FF;
constexpr uint32_t gpu = 0xFF8000;
constexpr uint32_t disk = 0x00E0A0;
constexpr uint32_t net_down = 0x00E0FF;
constexpr uint32_t net_up = 0xFF40FF;
constexpr uint32_t ok = 0x00E040;
constexpr uint32_t warn = 0xFFD000;
constexpr uint32_t crit = 0xFF2020;
}  // namespace col

// Fontes (seção 3.2), geradas em ui/fonts e declaradas no lv_conf.h
#define F9 (&font_f9)
#define F10 (&font_f10)
#define F12 (&font_f12)
#define F20 (&font_f20)

// Estilos compartilhados, criados uma vez em theme_init()
extern lv_style_t st_f10_t2;     // rótulos
extern lv_style_t st_f10_t3;     // eixos, índices
extern lv_style_t st_f12_t1;     // valores
extern lv_style_t st_caption;    // F10 / text_2 com espaçamento 1 (títulos)
extern lv_style_t st_card;
extern lv_style_t st_tab;
extern lv_style_t st_tab_checked;
extern lv_style_t st_tab_pressed;

void theme_init();

// Cor de um percentual já arredondado, pela regra de UI_COLOR_MODE
uint32_t value_color(int pct, uint32_t metric_color);

// Container transparente, sem padding, não clicável e sem rolagem
lv_obj_t* ui_box(lv_obj_t* parent);
lv_obj_t* ui_row(lv_obj_t* parent, int gap);  // ui_box em flex de linha, itens centralizados
lv_obj_t* ui_dot(lv_obj_t* parent, int size, uint32_t color);
lv_obj_t* ui_label(lv_obj_t* parent, lv_style_t* style, const char* text);

// lv_label_set_text só quando o texto muda, para não redesenhar à toa
void ui_set_text(lv_obj_t* label, const char* text);
