#include "audio-task.h"
#include "audio.h"
#include "driver/i2s_common.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"

static const char *TAG = "AudioTask";

void audioTask(void *pvParameters) {
  size_t written = 0;
  int32_t counter = 0;

  int32_t ticks = 0;
  for (;;) {
    const struct audioTaskContextStruct *taskContext = pvParameters;

    written = 0;
    TickType_t tickStart = xTaskGetTickCount();

    if (taskContext->audioStreamBuffer == NULL) {
      ESP_LOGE(TAG, "Buffer creation error");
      continue;
    }

    // dma size
    uint32_t *bufferToUse[FRAME_NUMBERS * BUFFERS];

    ESP_LOGD(TAG, "Reading %d bytes", sizeof(bufferToUse));

    const size_t sizeOfRead =
        xStreamBufferReceive(*taskContext->audioStreamBuffer, bufferToUse,
                             sizeof(bufferToUse), BUFFER_MULTIPLIER);

    if (sizeOfRead != sizeof(bufferToUse)) {
      ESP_LOGW(TAG, "Expected to get %d, got %d bytes instead",
               sizeof(bufferToUse), sizeOfRead);
    }

    if (sizeOfRead > 0) {
      ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_channel_write(
          *taskContext->tx_handle, bufferToUse, sizeOfRead,
          // in ideal world timeout should be exactly buffer multiplier, aka 1
          &written, 2));
    }

    if (sizeOfRead != written) {
      ESP_LOGW(TAG, "Expected to write %d, wrote %d bytes instead", sizeOfRead,
               written);
    }
    TickType_t tickEnd = xTaskGetTickCount();

    if (counter == 1000) {
      ESP_LOGW(TAG, "Ran 1000 loops in %d ticks", ticks);
      counter = 0;
      ticks = 0;
    }
    counter = counter + 1;

    ticks = ticks + (tickEnd - tickStart);

    // taskYIELD();
    // why do i need it?
    vTaskDelay(1);
  }

  vTaskDelete(NULL);
}
