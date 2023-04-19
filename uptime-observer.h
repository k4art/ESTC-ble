#ifndef ESTC_UPTIME_OBSERVER_H__

#include <stdint.h>

typedef intmax_t uptime_t;
typedef void (* uptime_cb_t)(void * context, const uptime_t * seconds);

void uptime_observer_init(void);

void uptime_observer_subscribe(uptime_cb_t callback, void * context);

#endif

