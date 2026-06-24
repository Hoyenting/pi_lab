#ifndef HTTP_API_H
#define HTTP_API_H

#include "sensor_sht35.h"
#include "alert.h"

/*
 * Minimal Redfish-inspired HTTP API (port 8080, single-threaded accept loop).
 * Runs in a detached background thread; shares sensor state via atomic snapshot.
 *
 * Endpoints:
 *   GET /redfish/v1/Chassis/1/Thermal
 *   GET /redfish/v1/Systems/1/LogServices/SEL/Entries
 *   GET /redfish/v1/              (service root)
 */

typedef struct {
    float         temp_c;
    float         humidity_pct;
    float         pico_temp_c;
    int           pico_valid;
    alert_level_t level;
} http_sensor_snapshot_t;

/* Start the HTTP listener thread on the given port (default 8080). */
int  http_api_start(int port);

/* Update the shared snapshot (called from main loop after each read). */
void http_api_update(const http_sensor_snapshot_t *snap);

/* Signal the listener thread to stop and join. */
void http_api_stop(void);

#endif /* HTTP_API_H */
