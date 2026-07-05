#include "date_and_time.h"

struct tm get_local_time(void)
{
  time_t now;
  struct tm timeinfo;

  time(&now);
  localtime_r(&now, &timeinfo);

  return timeinfo;
}
