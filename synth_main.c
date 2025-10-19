#include "audio-task-callback.h"
#include "audio-task.h"

#include "audio.h"

#include "clap/events.h"
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "esp_log.h"

#include "freertos/idf_additions.h"

#include "midi-events.h"
#include "plugin-host.h"
#include "sound.h"
#include "usb-midi.h"
#include <stdint.h>

#define SAFE_FLOAT_TO_INT32(x)                                                 \
  ((x > 1.0f)    ? 2147483647                                                  \
   : (x < -1.0f) ? -2147483648                                                 \
                 : (int32_t)(x * 2147483647.0f))

static const char *TAG = "MAIN";

struct UsbMidi midi;
i2s_chan_handle_t tx_handle;

struct event_list_container midiEvents = {0, NULL, NULL};

float left[SAMPLES_PER_TICK * 2];
float right[SAMPLES_PER_TICK * 2];

float *output[AUDIO_CHANNELS] = {left, right};

StreamBufferHandle_t streamBuffer;

const struct soundI2SContext soundI2SContextValue = {&streamBuffer, &tx_handle};

struct soundRenderingContext {
  float **output;
  struct event_list_container *midi_events;
  struct UsbMidi *midi;
  StreamBufferHandle_t *streamBuffer;
};

const struct soundRenderingContext soundRenderingContextValue = {
    output, &midiEvents, &midi, &streamBuffer};

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
    event.note_id = -1;
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
    event.note_id = -1;
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

void midiTask(void *pvParameters) {
  for (;;) {
    const struct soundRenderingContext *soundCTX = pvParameters;
    if (soundCTX->streamBuffer == NULL) {
      ESP_LOGE(TAG, "Buffer creation error");
      continue;
    }
    UsbMidi_update(soundCTX->midi);

    size_t free_size = xStreamBufferSpacesAvailable(*soundCTX->streamBuffer);

    if (free_size == 0) {
      continue;
    }

    int frames_to_render = free_size / AUDIO_CHANNELS / sizeof(int32_t);

    uint32_t bufferToUse[frames_to_render * 2];

    ESP_LOGD(TAG, "Writing %d bytes", sizeof(bufferToUse));
    ESP_LOGD(TAG, "Rendering");

    // Renders silence for now
    plugins_process(&midiEvents, output, frames_to_render);
    ESP_LOGD(TAG, "Clearing list");

    event_list_clear(soundCTX->midi_events);
    ESP_LOGD(TAG, "Converting");

    for (int i = 0; i < frames_to_render; i++) {
      // -1 ... 1 to proper int values
      bufferToUse[2 * i] = SAFE_FLOAT_TO_INT32(left[i]);
      bufferToUse[2 * i + 1] = SAFE_FLOAT_TO_INT32(right[i]);
    }

    ESP_LOGD(TAG, "Sending in one write");

    // Doing it in one write
    size_t bytesWrote = xStreamBufferSend(
        *soundCTX->streamBuffer, (void *)bufferToUse, sizeof(bufferToUse), 0);

    if (bytesWrote != sizeof(bufferToUse)) {
      ESP_LOGW(TAG, "Wrote %d bytes instead %d", bytesWrote,
               sizeof(bufferToUse));
    }

    vTaskDelay(1);

    ESP_LOGD(TAG, "Sending done!");
  }

  vTaskDelete(NULL);
}

void app_main(void) {
  // USBMidi section
  UsbMidiInit(&midi);

  // Double the size cause stereo
  // and extra double the size cause bigger buffer for backpressure
  // rendering extra buffer to be sure we have one buffer full of data
  // and rendering missing part afterwards
  // FAILS IF ONE OF 2 IS 4
  // shouldnt be 2 at all
  streamBuffer = xStreamBufferCreate(SAMPLES_PER_TICK * 2 * AUDIO_CHANNELS *
                                         sizeof(int32_t),
                                     AUDIO_CHANNELS * sizeof(int32_t));

  UsbMidi_onMidiMessage(&midi, onMidiMessage);
  UsbMidi_onDeviceConnected(&midi, onMidiDeviceConnected);
  UsbMidi_onDeviceDisconnected(&midi, onMidiDeviceDisconnected);
  usb_midi_begin(&midi);

  // I2S audio section
  i2s_new_channel(&chan_cfg, &tx_handle, NULL);
  // i2s_event_callbacks_t callbacks = {.on_sent = &audioTaskCallback};

  // i2s_channel_register_event_callback(tx_handle, &callbacks,
  //                                     (void *)&soundI2SContextValue);

  i2s_channel_init_std_mode(tx_handle, &std_cfg);
  ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

  // pluginHostInit
  if (streamBuffer == NULL) {
    ESP_LOGE(TAG, "Buffer creation error");
  }

  plugins_init();
  plugins_activate(SAMPLE_RATE);

  xTaskCreatePinnedToCore(audioTask, "audio", 102400,
                          (void *)&soundI2SContextValue,
                          configMAX_PRIORITIES - 1, NULL, 0);

  xTaskCreatePinnedToCore(midiTask, "midi + synth", 102400,
                          (void *)&soundRenderingContextValue,
                          configMAX_PRIORITIES - 1, NULL, 1);
}
