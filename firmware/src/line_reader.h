#pragma once

#include <stddef.h>

#include "metrics.h"

// Junta bytes até o "\n" e converte a linha JSON (protocolo v1) em Metrics.
// Cada fonte (USB, Bluetooth) tem o seu, para as linhas nunca se misturarem.
class LineReader {
 public:
  // Devolve true quando `c` fecha uma linha válida, já convertida em `m`.
  bool feed(char c, Metrics& m);

 private:
  static constexpr size_t CAPACITY = 768;
  char buf_[CAPACITY];
  size_t len_ = 0;
  bool overflow_ = false;
};
