#pragma once

#include <stddef.h>
#include <stdint.h>

// Formatação de números da seção 3.4: vírgula decimal, sem separador de milhar.

// Percentual arredondado e limitado a 0..100
int fmt_pct(float value);

// Swap com uma casa decimal: 0.8 → "0,8%"
void fmt_swap(char* buf, size_t size, float swap);

// Taxa em base 1000: "0 B/s", "125 KB/s", "2,4 MB/s", "18 MB/s"
void fmt_rate(char* buf, size_t size, long bytes_per_sec);

// Rótulo do eixo Y da rede (valor em KB/s): "500K", "2,5M", "5M"
void fmt_axis_kb(char* buf, size_t size, int32_t kb);

// Tela Claude (docs/claude, seção 5)

// Tokens: até 999 sem sufixo, "412 k" até 999.999 e "1,84 M" a partir de 1.000.000
void fmt_tokens(char* buf, size_t size, long tokens);

// Ritmo em tokens por minuto: "850/min", "3,2 k/min", "45 k/min", "1,2 M/min"
void fmt_token_rate(char* buf, size_t size, long per_min);

// Tempo até o reinício: "1h48", "48 min"
void fmt_duration(char* buf, size_t size, long seconds);
