#ifndef SEL_H
#define SEL_H

#include <stdint.h>

/*
 * System Event Log — IPMI-inspired event record.
 * Entries are appended on alert state transitions (not every loop).
 */

typedef enum {
    SEL_INFO     = 0,
    SEL_WARNING  = 1,
    SEL_CRITICAL = 2,
} sel_severity_t;

typedef struct {
    uint32_t       record_id;
    /* time_t stored as seconds since epoch; formatted on write */
    long           timestamp;
    sel_severity_t severity;
    char           sensor_name[16];
    float          value;
    char           description[64];
} sel_entry_t;

int         sel_init(const char *path);
void        sel_add(sel_severity_t severity, const char *sensor,
                    float value, const char *description);
void        sel_close(void);
const char *sel_severity_str(sel_severity_t s);

#endif /* SEL_H */
