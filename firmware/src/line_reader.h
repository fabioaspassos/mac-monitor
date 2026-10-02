#pragma once

#include <stddef.h>
#include <stdint.h>

#include "metrics.h"

// Tipo da linha que acabou de chegar
enum class LineType : uint8_t { None, Metrics, Claude };

// Junta bytes até o "\n" e converte a linha JSON (protocolo v1) em Metrics ou,
// quando "t" == "cc", em ClaudeUsage. Linhas com outro "t" são ignoradas.
// Cada fonte (USB, Bluetooth) tem o seu, para as linhas nunca se misturarem.
class LineReader {
 public:
  // Quando `c` fecha uma linha válida, devolve o tipo e preenche `m` ou `cu`.
  LineType feed(char c, Metrics& m, ClaudeUsage& cu);

 private:
  static constexpr size_t CAPACITY = 768;  // a linha "cc" tem ~250 bytes
  char buf_[CAPACITY];
  size_t len_ = 0;
  bool overflow_ = false;
};
