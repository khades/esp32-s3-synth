#include "audio-task.h"
#include "audio.h"
#include "driver/i2s_common.h"
#include "esp_log.h"

const char *TAG = "AudioTask";

void audioTask(void *pvParameters) {
  size_t written = 0;

  for (;;) {
    const struct soundI2SContext *soundCTX = pvParameters;

    if (soundCTX->streamBuffer == NULL) {
      ESP_LOGE(TAG, "Buffer creation error");
      continue;
    }

    size_t processableSize =
        xStreamBufferBytesAvailable(*soundCTX->streamBuffer);
    // floor rounding
    int frames_to_read = processableSize / AUDIO_CHANNELS / sizeof(int32_t);

    // reading samples for stereo on two
    if (frames_to_read > SAMPLES_PER_TICK) {
      frames_to_read = SAMPLES_PER_TICK;
    }

    uint32_t *bufferToUse[frames_to_read * 2];
    if (sizeof(bufferToUse) == 0) {
      ESP_LOGW(TAG, "Reading buffer starved");

      vTaskDelay(1);
      continue;
    }
    ESP_LOGD(TAG, "Reading %d bytes", sizeof(bufferToUse));

    const size_t sizeOfRead = xStreamBufferReceive(
        *soundCTX->streamBuffer, bufferToUse, sizeof(bufferToUse), 0);

    if (sizeOfRead != sizeof(bufferToUse)) {
      ESP_LOGW(TAG, "Expected %d, got %d bytes instead", sizeof(bufferToUse),
               sizeOfRead);
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_channel_write(
        *soundCTX->tx_handle, bufferToUse, sizeOfRead,
        // in ideal world timeout should be exactly buffer multiplier, aka 1
        &written, portMAX_DELAY));

    if (sizeOfRead != written) {
      ESP_LOGW(TAG, "Expected %d, wrote %d bytes instead", sizeof(bufferToUse),
               sizeOfRead);
    }
    vTaskDelay(1);
  }

  vTaskDelete(NULL);
}
