#pragma once

#include <stdint.h>

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

// Linha "t":"cc" do agent: uso do Claude Code (docs/claude/claude-code-dados-agente.md,
// seção 5). Chega a cada 10 s, separada da linha de métricas.
struct ClaudeUsage {
  enum Source : uint8_t { SRC_NONE, SRC_SL, SRC_EST, SRC_API };

  bool ok = false;             // ok    0 = Claude Code não encontrado (o resto pode faltar)
  Source src = SRC_NONE;       // src   "api" endpoint de uso, "sl" status line, "est" estimado, "none" sem bloco
  bool hasBlock = false;       // b5    não nulo
  int blockPct = -1;           // b5.pct  (%), -1 quando nulo (modo estimado)
  long blockRst = 0;           // b5.rst  segundos até o reinício, contados na chegada
  bool hasWeek = false;        // w7    não nulo
  int weekPct = 0;             // w7.pct  (%)
  int weekDay = -1;            // w7.wd   dia do reinício, 0=domingo
  int weekHour = -1;           // w7.wh   hora local do reinício
  long tokBlock = 0;           // tb    tokens no bloco atual
  long tokDay = 0;             // td    tokens hoje
  long rate = 0;               // rate  tokens/min (média de 5 min)
  int sessions = 0;            // ses   sessões ativas
  int hour = 0;                // hr    hora local atual (0..23)
  long hours[24] = {};         // h     milhares de tokens por hora de hoje
};
