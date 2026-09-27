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
