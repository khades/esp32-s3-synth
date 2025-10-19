#include "audio-task.h"
#include "audio.h"
#include "driver/i2s_common.h"
#include "esp_log.h"

const char *TAG = "AudioTask";

bool audioTaskCallback(i2s_chan_handle_t handle, i2s_event_data_t *event,
                       void *user_ctx) {
  size_t written = 0;

  // ESP_LOGW(TAG, "Task Running");
  // return false;

  const struct soundI2SContext *soundCTX = user_ctx;

  if (soundCTX->streamBuffer == NULL) {
    ESP_LOGE(TAG, "Buffer creation error");
    return false;
  }

  size_t processableSize = xStreamBufferBytesAvailable(*soundCTX->streamBuffer);

  if (sizeof(processableSize) == 0) {
    // ESP_LOGW(TAG, "Reading buffer starved");

    vTaskDelay(5);
    return false;
  }
  // floor rounding
  size_t processableSizeClampedToDMASize =
      processableSize > event->size ? event->size : processableSize;

  int frames_to_read =
      processableSizeClampedToDMASize / AUDIO_CHANNELS / sizeof(int32_t);

  uint32_t *bufferToUse[frames_to_read * 2];

  // ESP_LOGD(TAG, "Reading %d bytes", sizeof(bufferToUse));

  const size_t sizeOfRead = xStreamBufferReceive(
      *soundCTX->streamBuffer, bufferToUse, sizeof(bufferToUse), 0);

  if (sizeOfRead != sizeof(bufferToUse)) {
    ESP_LOGW(TAG, "Expected %d, got %d bytes instead", sizeof(bufferToUse),
             sizeOfRead);
  }

  ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_channel_write(
      handle, bufferToUse, sizeOfRead,
      // in ideal world timeout should be exactly buffer multiplier, aka 1
      &written, portMAX_DELAY));

  if (sizeOfRead != written) {
    ESP_LOGW(TAG, "Expected %d, wrote %d bytes instead", sizeof(bufferToUse),
             sizeOfRead);
  }

  return false;
}
