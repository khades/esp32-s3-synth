#include "plugin-host.h"
#include "audio.h"
#include "clap/entry.h"
#include "clap/factory/plugin-factory.h"
#include "clap/host.h"
#include "clap/plugin.h"
#include "clap/version.h"

#include "esp_log.h"
#include "plugins.h"
#include "sine-ping.h"
#include <stdio.h>

#define PATH "aaaa"

static const char *TAG = "PLUGIN_HOST";
static const clap_plugin_t *pluginRegistry[PLUGINS_COUNT] = {};
struct clap_host host = {};

// TODO params

void plugins_init() {
  ESP_LOGI(TAG, "INIT");

  for (int i = 0; i < PLUGINS_COUNT; i++) {
    const clap_plugin_entry_t *plugin = plugins[i];
    ESP_LOGI(TAG, "INIT FIRST");

    // Fails here now
    plugin->init(PATH);
    ESP_LOGI(TAG, "HAD INIT");

    clap_plugin_factory_t *factory =
        (clap_plugin_factory_t *)plugin->get_factory(CLAP_PLUGIN_FACTORY_ID);

    if (factory == NULL) {
      ESP_LOGE(TAG, "Factory Fail");
    }
    pluginRegistry[i] = factory->create_plugin(factory, &host, TAG);
    pluginRegistry[i]->init(pluginRegistry[i]);
  }
}

void plugins_activate(double sample_rate
                      // , int min_frames_count,
                      //                     int max_frames_count
) {

  // WTF is max_frames_count???
  // in case of sine-ping it is not read
  for (int i = 0; i < PLUGINS_COUNT; i++) {
    pluginRegistry[i]->activate(pluginRegistry[i], sample_rate, 0, 0);
    pluginRegistry[i]->start_processing(pluginRegistry[i]);
    // maybe need it some times later, but not now
    // pluginRegistry[i]->get_extension()
  }
}

void plugins_process(clap_input_events_t *in_events, float **output) {
  float *soundBuffer[FRAME_SIZE_FOR_ALL_CHANNELS];

  const clap_audio_buffer_t in = {.data32 = soundBuffer,
                                  .data64 = NULL,
                                  .channel_count = 2,
                                  .latency = 0,
                                  .constant_mask = 0};

  clap_audio_buffer_t out = {.data32 = output,
                             .data64 = NULL,
                             .channel_count = 2,
                             .latency = 0,
                             .constant_mask = 0};

  const clap_process_t process = {
      .steady_time = -1,
      .frames_count = FRAME_SIZE,
      .transport = NULL,
      .audio_inputs = &in,
      .audio_outputs = &out,
      .audio_inputs_count = 2,
      .audio_outputs_count = 2,
      .in_events = in_events,
      .out_events = NULL,
  };

  for (int i = 0; i < PLUGINS_COUNT; i++) {
    // as i see it now, all events should be on frame 1
    // is it made for offline rendering?
    pluginRegistry[i]->process(pluginRegistry[i], &process);
    // copying output to input so we proceed  processing
    memcpy(soundBuffer, output, FRAME_SIZE_FOR_ALL_CHANNELS);
  }
}