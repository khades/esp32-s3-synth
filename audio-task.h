#ifndef __AUDIO_TASK
#define __AUDIO_TASK
#include "driver/i2s_types.h"
#include "freertos/idf_additions.h"

void audioTask(void *pvParameters);

struct audioTaskContextStruct {
  StreamBufferHandle_t *audioStreamBuffer;
  i2s_chan_handle_t *tx_handle;
};

#endif