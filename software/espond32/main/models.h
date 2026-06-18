#ifndef MODELS_H
#define MODELS_H

#include <stdbool.h>
#include <inttypes.h>
#include "config.h"

typedef struct PUMP {
  bool on;
  uint8_t id;
} PUMP;

typedef struct VALVE {
  bool on;
  uint8_t id;
} VALVE;

typedef struct ESPOND32 {
  bool connected;
  bool enabled;

  PUMP pumps[PUMP_AMOUNT];
} ESPOND32;

#endif // !MODELS_H
