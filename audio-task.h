#ifndef __AUDIO_TASK
#define __AUDIO_TASK
#include "driver/i2s_types.h"
#include "freertos/idf_additions.h"

struct soundI2SContext {
  StreamBufferHandle_t *streamBuffer;
  i2s_chan_handle_t *tx_handle;
};

void audioTask(void *pvParameters);
#endif