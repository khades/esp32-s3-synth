#include "clap/events.h"
#include "esp_log.h"
#include "usb-midi.h"

static const char *TAG = "USB Midi Task";

void usbMidiTask(void *pvParameters) {

  struct UsbMidi *UsbMidi = pvParameters;

  int32_t counter = 0;

  int32_t ticks = 0;

  for (;;) {
    TickType_t tickStart = xTaskGetTickCount();

    // Wildly unoptimized
    UsbMidi_update(UsbMidi);
    TickType_t tickEnd = xTaskGetTickCount();

    if (counter == 1000) {
      ESP_LOGW(TAG, "Ran 1000 loops in %d ticks", ticks);
      counter = 0;
      ticks = 0;
    }
    counter = counter + 1;

    ticks = ticks + (tickEnd - tickStart);
    vTaskDelay(1);
  }
}
