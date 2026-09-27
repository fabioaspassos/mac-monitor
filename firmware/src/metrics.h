#pragma once

constexpr int MAX_CORES = 20;

// Espelho do JSON enviado pelo agent (protocolo v1). Os nomes curtos dos
// campos do JSON estão comentados ao lado.
struct Metrics {
  float cpu = 0;               // cpu   média de todos os núcleos (%)
  float mem = 0;               // mem   (%)
  float swap = 0;              // swap  (%)
  float disk = 0;              // disk  ocupação do volume "/" (%)
  float gpu = 0;               // gpu   (%)
  int press = 0;               // press 1=normal, 2=alerta, 4=crítica
  int temp = 0;                // temp  NSProcessInfo.thermalState (0..3)
  long rx = 0, tx = 0;         // rx/tx rede em bytes/s
  long rd = 0, wr = 0;         // rd/wr disco em bytes/s
  float cores[MAX_CORES] = {}; // cores uso de cada núcleo (%)
  int coreCount = 0;           // tamanho do array cores (limitado a MAX_CORES)
};
