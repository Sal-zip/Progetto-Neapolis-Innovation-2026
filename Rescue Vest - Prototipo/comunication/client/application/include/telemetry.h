#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "ch.h"

#include "thread_gps.h"
#include "thread_mq2.h"
#include "thread_ppg.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t uptime_ms;

  gps_data_t gps;
  bool gps_fresh;

  ppg_data_t ppg;
  bool ppg_fresh;

  mq2_data_t mq2;
  bool mq2_fresh;
} telemetry_snapshot_t;

/*
 * Copia in un unico oggetto gli ultimi dati pubblicati dai thread sensore.
 * I driver non vengono interrogati direttamente durante la serializzazione.
 */
void telemetry_capture(telemetry_snapshot_t *snapshot);

#endif /* TELEMETRY_H */
