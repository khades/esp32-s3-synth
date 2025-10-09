#include "audio.h"
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "esp_log.h"
#include "plugin-host.h"
#include "sound.h"
#include "usbmidi.h"
static const char *TAG = "MAIN";

struct UsbMidi midi;
i2s_chan_handle_t tx_handle;

void onMidiMessage(const uint8_t data[4]) {
  uint8_t channel = data[1] & 0x0F;

  switch (data[0]) {
  case MIDI_CIN_NOTE_ON: {
    uint8_t pitch = data[2];
    uint8_t velocity = data[3];
    ESP_LOGI(TAG, "NoteOn: ch:%d / pitch:%d / vel:%d\n", channel, pitch,
             velocity);
    break;
  }

  case MIDI_CIN_NOTE_OFF: {
    uint8_t pitch = data[2];
    ESP_LOGI(TAG, "NoteOff: ch:%d / key:%d\n", channel, pitch);
    break;
  }

  case MIDI_CIN_CONTROL_CHANGE: {
    uint8_t control = data[2];
    uint8_t value = data[3];
    ESP_LOGI(TAG, "Control Chnage: ch:%d / cc:%d / v:%d\n", channel, control,
             value);
    break;
  }

  case MIDI_CIN_PROGRAM_CHANGE: {
    uint8_t program = data[2];
    ESP_LOGI(TAG, "Program Change: program:%d\n", program);
    break;
  }
  default:
    break;
  }
}

void onMidiDeviceConnected() { ESP_LOGI(TAG, "onMidiDeviceConnected\n"); }

void onMidiDeviceDisconnected() { ESP_LOGI(TAG, "onMidiDeviceDisconnected\n"); }

void app_main(void) {
  // USBMidi section
  UsbMidiInit(&midi);

  UsbMidi_onMidiMessage(&midi, onMidiMessage);
  UsbMidi_onDeviceConnected(&midi, onMidiDeviceConnected);
  UsbMidi_onDeviceDisconnected(&midi, onMidiDeviceDisconnected);
  usb_midi_begin(&midi);

  // I2S audio section
  i2s_new_channel(&chan_cfg, &tx_handle, NULL);

  i2s_channel_init_std_mode(tx_handle, &std_cfg);
  ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

  // pluginHostInit

  plugins_init();
  plugins_activate(SAMPLE_RATE);

  float left[FRAME_SIZE];
  float right[FRAME_SIZE];

  float *output[AUDIO_CHANNELS] = {left, right};
  float adaptedOutput[FRAME_SIZE_FOR_ALL_CHANNELS];

  size_t written = 0;
  while (1) {
    // UsbMidi_update(&midi);

    plugins_process(NULL, output);

    for (int i = 0; i < FRAME_SIZE; i++) {
      adaptedOutput[i * 2] = left[i];
      adaptedOutput[i * 2 + 1] = right[i];
    }

    i2s_channel_write(tx_handle, adaptedOutput,
                      FRAME_SIZE_FOR_ALL_CHANNELS * sizeof(float), &written,
                      20);
    // ESP_LOGI(TAG, "Written %d bytes", written);
    // writes 512
    vTaskDelay(1);
  }
}