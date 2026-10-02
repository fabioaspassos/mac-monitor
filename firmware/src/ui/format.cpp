#include "format.h"

#include <math.h>
#include <stdio.h>

// arredonda a divisão inteira de valores não negativos
static long divRound(long value, long divisor) {
  return (value + divisor / 2) / divisor;
}

int fmt_pct(float value) {
  long pct = lroundf(value);
  if (pct < 0) return 0;
  if (pct > 100) return 100;
  return (int)pct;
}

void fmt_swap(char* buf, size_t size, float swap) {
  long tenths = lroundf(swap * 10);
  if (tenths < 0) tenths = 0;
  snprintf(buf, size, "%ld,%ld%%", tenths / 10, tenths % 10);
}

void fmt_rate(char* buf, size_t size, long bps) {
  if (bps < 0) bps = 0;
  if (bps < 1000) {
    snprintf(buf, size, "%ld B/s", bps);
    return;
  }
  // a unidade é escolhida depois de arredondar: 999.600 B/s vira "1,0 MB/s"
  long kb = divRound(bps, 1000);
  if (kb < 1000) {
    snprintf(buf, size, "%ld KB/s", kb);
    return;
  }
  long mbTenths = divRound(bps, 100000);
  if (mbTenths < 100) {
    snprintf(buf, size, "%ld,%ld MB/s", mbTenths / 10, mbTenths % 10);
    return;
  }
  snprintf(buf, size, "%ld MB/s", divRound(bps, 1000000));
}

void fmt_axis_kb(char* buf, size_t size, int32_t kb) {
  if (kb < 1000) {
    snprintf(buf, size, "%ldK", (long)kb);
    return;
  }
  long tenths = divRound(kb, 100);
  if (tenths % 10 == 0) {
    snprintf(buf, size, "%ldM", tenths / 10);
  } else {
    snprintf(buf, size, "%ld,%ldM", tenths / 10, tenths % 10);
  }
}

void fmt_tokens(char* buf, size_t size, long tokens) {
  if (tokens < 0) tokens = 0;
  if (tokens < 1000) {
    snprintf(buf, size, "%ld", tokens);
    return;
  }
  long k = divRound(tokens, 1000);
  if (k < 1000) {
    snprintf(buf, size, "%ld k", k);
    return;
  }
  long mbHundredths = divRound(tokens, 10000);
  if (mbHundredths < 1000) {
    snprintf(buf, size, "%ld,%02ld M", mbHundredths / 100, mbHundredths % 100);
    return;
  }
  long mbTenths = divRound(tokens, 100000);
  if (mbTenths < 1000) {
    snprintf(buf, size, "%ld,%ld M", mbTenths / 10, mbTenths % 10);
    return;
  }
  snprintf(buf, size, "%ld M", divRound(tokens, 1000000));
}

void fmt_token_rate(char* buf, size_t size, long perMin) {
  if (perMin < 0) perMin = 0;
  if (perMin < 1000) {
    snprintf(buf, size, "%ld/min", perMin);
    return;
  }
  long kTenths = divRound(perMin, 100);
  if (kTenths < 100) {
    snprintf(buf, size, "%ld,%ld k/min", kTenths / 10, kTenths % 10);
    return;
  }
  long k = divRound(perMin, 1000);
  if (k < 1000) {
    snprintf(buf, size, "%ld k/min", k);
    return;
  }
  long mTenths = divRound(perMin, 100000);
  snprintf(buf, size, "%ld,%ld M/min", mTenths / 10, mTenths % 10);
}

void fmt_duration(char* buf, size_t size, long seconds) {
  if (seconds < 0) seconds = 0;
  long minutes = seconds / 60;
  if (minutes < 60) {
    snprintf(buf, size, "%ld min", minutes);
    return;
  }
  snprintf(buf, size, "%ldh%02ld", minutes / 60, minutes % 60);
}
