#include <stdint.h>

typedef struct
{
  uint8_t fps;
  unsigned long lastTriggerTime;
} SkipConfig;

bool checkSkip(SkipConfig *skipConfig);
bool checkSkipWithTime(SkipConfig *skipConfig, unsigned long currTime);