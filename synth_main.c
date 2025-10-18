#include "audio.h"
#include "clap/events.h"
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "esp_log.h"
#include "midi-events.h"
#include "plugin-host.h"
#include "sound.h"
#include "usbmidi.h"
#include "xtensa/config/specreg.h"

#define SAFE_FLOAT_TO_INT32(x)                                                 \
  ((x > 1.0f)    ? 2147483647                                                  \
   : (x < -1.0f) ? -2147483648                                                 \
                 : (int32_t)(x * 2147483647.0f))

static const char *TAG = "MAIN";

struct UsbMidi midi;
i2s_chan_handle_t tx_handle;

struct event_list_container midiEvents = {0, NULL, NULL};

float left[SAMPLES_PER_TICK];
float right[SAMPLES_PER_TICK];

float *output[AUDIO_CHANNELS] = {left, right};

// Double buffering
// lame, should be ring buffer??
uint32_t adaptedOutputFirst[SAMPLES_PER_TICK * 2];

uint32_t adaptedOutputSecond[SAMPLES_PER_TICK * 2];

size_t outputBufferSize = sizeof(adaptedOutputFirst);

uint32_t *adaptedOutputs[2] = {adaptedOutputFirst, adaptedOutputSecond};

bool useMain = false;

struct soundContext {
  bool *useMain;
  // i hate it there
  uint32_t **outputs;
};

const struct soundContext soundContextValue = {&useMain, adaptedOutputs};

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
size_t written = 0;

// i need to cut out that functions out, especially when i have pvParameters
// context
void audioTask(void *pvParameters) {
  // do we need to use ring buffer with callback?
  for (;;) {
    const struct soundContext *soundCTX = pvParameters;

    uint32_t *bufferToUse =
        soundCTX->useMain ? soundCTX->outputs[0] : soundCTX->outputs[1];

    // it sure blocks data in way it feels there's no dma
    // and it crackles cause of
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_channel_write(
        tx_handle, bufferToUse, outputBufferSize,
        // in ideal world timeout should be exactly buffer multiplier, aka 1
        &written, BUFFER_MULTIPLIER));
  }

  vTaskDelete(NULL);
}

void midiTask() {
  for (;;) {

    UsbMidi_update(&midi);

    plugins_process(&midiEvents, output);

    event_list_clear(&midiEvents);

    uint32_t *bufferToUse = useMain ? adaptedOutputs[1] : adaptedOutputs[0];

    for (int i = 0; i < SAMPLES_PER_TICK; i++) {
      // -1 ... 1 to proper int values
      bufferToUse[2 * i] = SAFE_FLOAT_TO_INT32(left[i]);
      bufferToUse[2 * i + 1] = SAFE_FLOAT_TO_INT32(right[i]);
    }

    useMain = useMain ? false : true;
  }

  vTaskDelete(NULL);
}

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

  xTaskCreatePinnedToCore(audioTask, "audio", 102400,
                          (void *)&soundContextValue, configMAX_PRIORITIES - 1,
                          NULL, 0);

  xTaskCreatePinnedToCore(midiTask, "midi + synth", 102400, NULL,
                          configMAX_PRIORITIES - 1, NULL, 1);
}
