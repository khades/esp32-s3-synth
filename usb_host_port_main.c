#include "esp_log.h"
#include "usbmidi.h"

static const char *TAG = "MAIN";

struct UsbMidi midi;

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
  UsbMidiInit(&midi);

  UsbMidi_onMidiMessage(&midi, onMidiMessage);
  UsbMidi_onDeviceConnected(&midi, onMidiDeviceConnected);
  UsbMidi_onDeviceDisconnected(&midi, onMidiDeviceDisconnected);
  usb_midi_begin(&midi);
  while (1) {
    UsbMidi_update(&midi);
    /* Toggle the LED state */
    vTaskDelay(1);
  }
}