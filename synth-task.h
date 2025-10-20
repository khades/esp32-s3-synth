#ifndef __SYNTH_TASK
#define __SYNTH_TASK
#include "freertos/idf_additions.h"

void synthTask(void *pvParameters);

struct synthTaskContextStruct {
  StreamBufferHandle_t *audioStreamBuffer;
  struct event_list_container *midi_events;
};

#endif