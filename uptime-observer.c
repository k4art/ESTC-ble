#include "app_timer.h"
#include "uptime-observer.h"

#define TIMER_PERIOD_MS        1000
#define MAX_SUBSCRIBERS_NUMBER 2

APP_TIMER_DEF(m_uptime_timeout_timer);

typedef struct uptime_observer_cb_s
{
  bool        is_up;
  uptime_t    seconds;
  size_t      cb_count;

  struct
  {
    uptime_cb_t   function;
    void        * argument;
  } callbacks[MAX_SUBSCRIBERS_NUMBER];
} uptime_observer_cb_t;

static uptime_observer_cb_t m_cb;

static void uptime_observer_increment(void)
{
  m_cb.seconds++;
}

static void on_timer_tick(void * context)
{
  (void) context;

  uptime_observer_increment();

  for (size_t i = 0; i < m_cb.cb_count; i++)
  {
    uptime_cb_t   cb      = m_cb.callbacks[i].function;
    void        * context = m_cb.callbacks[i].argument;

    cb(context, &m_cb.seconds);
  }
}

void uptime_observer_init(void)
{
  ret_code_t ret;

  ret = app_timer_create(&m_uptime_timeout_timer,
                         APP_TIMER_MODE_REPEATED,
                         on_timer_tick);

  app_timer_start(m_uptime_timeout_timer,
                  APP_TIMER_TICKS(TIMER_PERIOD_MS),
                  NULL);

  APP_ERROR_CHECK(ret);
}

void uptime_observer_subscribe(uptime_cb_t callback, void * context)
{
  m_cb.callbacks[m_cb.cb_count].function = callback;
  m_cb.callbacks[m_cb.cb_count].argument = context;

  m_cb.cb_count++;
}

