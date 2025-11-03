#include "audio-task.h"
#include "audio.h"
#include "synth-task.h"
#include "usb-midi-task.h"

#include "clap/events.h"
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "esp_log.h"

#include "freertos/idf_additions.h"

#include "midi-events.h"
#include "plugin-host.h"
#include "portmacro.h"
#include "sound.h"
#include "usb-midi.h"
#include <stdint.h>

static const char *TAG = "MAIN";

struct UsbMidi midi;
i2s_chan_handle_t tx_handle;

struct event_list_container midiEvents = {0, NULL, NULL};

StreamBufferHandle_t audioStreamBuffer;
StreamBufferHandle_t midiStreamBuffer;

const struct audioTaskContextStruct audioTaskContext = {&audioStreamBuffer,
                                                        &tx_handle};

const struct synthTaskContextStruct synthTaskContext = {&audioStreamBuffer,
                                                        &midiEvents};

void onMidiMessage(const uint8_t data[4]) {
  uint8_t channel = data[1] & 0x0F;

  switch (data[0]) {
  case MIDI_CIN_NOTE_ON: {
    uint8_t pitch = data[2];
    uint8_t velocity = data[3];

    clap_event_note_t event = {};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type =
        velocity == 0 ? CLAP_EVENT_NOTE_OFF : CLAP_EVENT_NOTE_ON;
    event.header.flags = 0;
    event.key = pitch;
    event.note_id = pitch;
    event.channel = channel;
    event.port_index = 0;
    event.velocity = velocity;

    event_add(&midiEvents, &event);
    ESP_LOGI(TAG, "NoteOn: ch:%d / pitch:%d / vel:%d\n", channel, pitch,
             velocity);
    break;
  }

  case MIDI_CIN_NOTE_OFF: {
    uint8_t pitch = data[2];

    clap_event_note_t event = {};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_NOTE_OFF;
    event.header.flags = 0;
    event.key = pitch;
    event.note_id = pitch;
    event.channel = channel;
    event.port_index = 0;
    event_add(&midiEvents, &event);

    ESP_LOGI(TAG, "NoteOff: ch:%d / key:%d", channel, pitch);
    break;
  }

  case MIDI_CIN_CONTROL_CHANGE: {
    uint8_t control = data[2];
    uint8_t value = data[3];
    // We need a mapper here
    // there's diference between CLAP_EVENT_PARAM_VALUE and CLAP_EVENT_PARAM_MOD
    ESP_LOGI(TAG, "Control Change: ch:%d / cc:%d / v:%d", channel, control,
             value);
    break;
  }

  case MIDI_CIN_PROGRAM_CHANGE: {
    uint8_t program = data[2];
    // https://www.mixagesoftware.com/en/midikit/help/HTML/midi_events.html
    ESP_LOGI(TAG, "Program Change: program:%d", program);
    break;
  }

  case MIDI_CIN_PITCH_BEND: {
    // https://www.mixagesoftware.com/en/midikit/help/HTML/midi_events.html
    ESP_LOGI(TAG, "Pitchbend: params:%d %d ", data[2], data[3]);
    break;
  }

  default:
    // ESP_LOGI(TAG, "We have no idea what came %d", data[0]);
    break;
  }
}

void onMidiDeviceConnected() { ESP_LOGI(TAG, "onMidiDeviceConnected\n"); }

void onMidiDeviceDisconnected() { ESP_LOGI(TAG, "onMidiDeviceDisconnected\n"); }

void app_main(void) {
  // USBMidi section
  UsbMidiInit(&midi);

  // Double the size cause stereo
  // and extra double the size cause bigger buffer for backpressure
  // rendering extra buffer to be sure we have one buffer full of data
  // and rendering missing part afterwards

  audioStreamBuffer =
      xStreamBufferCreate(FRAME_NUMBERS * AUDIO_CHANNELS * sizeof(int32_t),
                          FRAME_NUMBERS * AUDIO_CHANNELS * sizeof(int32_t));

  UsbMidi_onMidiMessage(&midi, onMidiMessage);
  UsbMidi_onDeviceConnected(&midi, onMidiDeviceConnected);
  UsbMidi_onDeviceDisconnected(&midi, onMidiDeviceDisconnected);
  usb_midi_begin(&midi);

  // I2S audio section
  i2s_new_channel(&chan_cfg, &tx_handle, NULL);

  i2s_channel_init_std_mode(tx_handle, &std_cfg);
  ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

  // pluginHostInit
  if (audioStreamBuffer == NULL) {
    ESP_LOGE(TAG, "Buffer creation error");
  }

  plugins_init();
  plugins_activate(SAMPLE_RATE);

  xTaskCreate(audioTask, "audio", 102400, (void *)&audioTaskContext,
              configMAX_PRIORITIES - 1, NULL);

  xTaskCreate(synthTask, "synth", 102400, (void *)&synthTaskContext,
              configMAX_PRIORITIES - 1, NULL);

  xTaskCreate(usbMidiTask, "usbmidi", 102400, (void *)&midi,
              configMAX_PRIORITIES - 4, NULL);
}
