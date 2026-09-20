#include "telemetry.h"

#include "app_config.h"

#include <string.h>

void telemetry_capture(telemetry_snapshot_t *snapshot) {
  if (snapshot == NULL) {
    return;
  }

  memset(snapshot, 0, sizeof(*snapshot));

  snapshot->uptime_ms =
      (uint32_t)chTimeI2MS(chVTGetSystemTimeX());

  const sysinterval_t max_age =
      TIME_MS2I(APP_SENSOR_MAX_AGE_MS);

  snapshot->gps_fresh =
      gps_get_latest(&snapshot->gps, max_age);

  snapshot->ppg_fresh =
      ppg_get_latest(&snapshot->ppg, max_age);

  snapshot->mq2_fresh =
      mq2_get_latest(&snapshot->mq2, max_age);
}
