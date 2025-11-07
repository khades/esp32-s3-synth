#include "synth-task.h"
#include "audio.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "plugin-host.h"

static const char *TAG = "SynthTask";

// Saturation cast
#define SAFE_FLOAT_TO_INT32(x)                                                 \
  ((x > 1.0f)    ? 2147483647                                                  \
   : (x < -1.0f) ? -2147483648                                                 \
                 : (int32_t)(x * 2147483647.0f))

void synthTask(void *pvParameters) {
  const struct synthTaskContextStruct *taskContext = pvParameters;

  // streamBuffer output structure
  uint32_t bufferToUse[FRAME_NUMBERS * BUFFER_MULTIPLIER * AUDIO_CHANNELS];

  // synth outputs
  float left[FRAME_NUMBERS * BUFFER_MULTIPLIER];
  float right[FRAME_NUMBERS * BUFFER_MULTIPLIER];

  float *output[AUDIO_CHANNELS] = {left, right};

  // performance counters
  int32_t counter = 0;
  int32_t ticks = 0;

  for (;;) {
    if (taskContext->audioStreamBuffer == NULL) {
      ESP_LOGE(TAG, "Buffer creation error");
      continue;
    }

    size_t free_size =
        xStreamBufferSpacesAvailable(*taskContext->audioStreamBuffer);

    if (free_size == 0) {
      continue;
    }

    int frames_to_render = free_size / AUDIO_CHANNELS / sizeof(int32_t);

    if (frames_to_render > SAMPLES_PER_TICK + 16) {
      frames_to_render = SAMPLES_PER_TICK + 16;
    }

    int bytes_to_transfer = frames_to_render * 2 * sizeof(int32_t);

    TickType_t tickStart = xTaskGetTickCount();

    // Renders silence for now
    plugins_process(taskContext->midi_events, output, frames_to_render);

    for (int i = 0; i < frames_to_render; i++) {
      // -1 ... 1 to proper int values
      bufferToUse[2 * i] = SAFE_FLOAT_TO_INT32(left[i]);
      bufferToUse[2 * i + 1] = SAFE_FLOAT_TO_INT32(right[i]);
    }

    ESP_LOGD(TAG, "Sending in one write");

    TickType_t tickEnd = xTaskGetTickCount();

    ESP_LOGD(TAG, "Preparing to send %d bytes", bytes_to_transfer);

    size_t bytesWrote =
        xStreamBufferSend(*taskContext->audioStreamBuffer, (void *)bufferToUse,
                          bytes_to_transfer, 2);

    if (bytesWrote != bytes_to_transfer) {
      ESP_LOGW(TAG, "Wrote %d bytes instead %d", bytesWrote,
               sizeof(bufferToUse));
    }

    if (counter == 1000) {
      ESP_LOGW(TAG, "Ran 1000 loops in %d ticks", ticks);
      counter = 0;
      ticks = 0;
    }
    counter = counter + 1;

    ticks = ticks + (tickEnd - tickStart);
    vTaskDelay(1);

    ESP_LOGD(TAG, "Sending done!");
  }

  vTaskDelete(NULL);
}
