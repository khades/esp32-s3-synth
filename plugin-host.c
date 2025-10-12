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

#define PATH "Plugin host"
#include "midi-events.h"

static const char *TAG = "PLUGIN_HOST";
static const clap_plugin_t *pluginRegistry[PLUGINS_COUNT] = {};
struct clap_host host = {.clap_version = CLAP_VERSION};

// TODO params
clap_input_events_t events = {
    NULL,
    event_list_size,
    event_get,
};

struct event_list_container outBuffer = {0, NULL, NULL};

clap_output_events_t outEvents = {
    NULL,
    try_push,
};

void plugins_init() {
  ESP_LOGI(TAG, "INIT");
  for (int i = 0; i < PLUGINS_COUNT; i++) {
    const clap_plugin_entry_t *plugin = plugins[i];
    ESP_LOGI(TAG, "INIT PLUGIN %d", i);
    if (plugin == NULL) {
      ESP_LOGE(TAG, "No plugin. How?");
    }
    ESP_LOGD(TAG, "Plugin pointer is at address: %p", plugin);

    ESP_LOGD(TAG, "Plugin init function pointer is at address: %p",
             plugin->init);
    ESP_LOGD(TAG, "Plugin init function pointer is at address: %p",
             plugin->get_factory);

    if (plugin->init == NULL) {
      ESP_LOGE(TAG, "No init plugin. How?");
    }
    // Fails here now
    plugin->init(PATH);
    ESP_LOGI(TAG, "HAD INIT");

    clap_plugin_factory_t *factory =
        (clap_plugin_factory_t *)plugin->get_factory(CLAP_PLUGIN_FACTORY_ID);

    if (factory == NULL) {
      ESP_LOGE(TAG, "Factory Fail");
    }
    // LOADING ONLY FIRST PLUGIN
    // if not - TODO
    const clap_plugin_descriptor_t *descriptor =
        factory->get_plugin_descriptor(factory, 0);
    if (descriptor == NULL) {
      ESP_LOGE(TAG, "Descriptor Fail");
    }
    const clap_plugin_t *pluginInstance =
        factory->create_plugin(factory, &host, descriptor->id);
    ESP_LOGD(TAG, "Plugin instance pointer is at address: %p", pluginInstance);
    ESP_LOGD(TAG, "Plugin instance init function pointer is at address: %p",
             pluginInstance->init);

    pluginRegistry[i] = pluginInstance;

    pluginInstance->init(pluginInstance);
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
void plugins_process(struct event_list_container *event_list, float **output) {
  events.ctx = event_list;
  outBuffer.count = 0;
  outBuffer.first = NULL;
  outBuffer.last = NULL;
  outEvents.ctx = &outBuffer;

  float left[MONO_FRAMES_TO_RENDER];
  float right[MONO_FRAMES_TO_RENDER];

  float *soundBuffer[AUDIO_CHANNELS] = {left, right};
  memcpy(soundBuffer[0], output[0], MONO_FRAMES_TO_RENDER);
  memcpy(soundBuffer[1], output[1], MONO_FRAMES_TO_RENDER);

  const clap_audio_buffer_t inputBuffer[1] = {{.data32 = soundBuffer,
                                               .data64 = NULL,
                                               .channel_count = AUDIO_CHANNELS,
                                               .latency = 0,
                                               .constant_mask = 0}};

  clap_audio_buffer_t outputBuffer[1] = {{.data32 = output,
                                          .data64 = NULL,
                                          .channel_count = AUDIO_CHANNELS,
                                          .latency = 0,
                                          .constant_mask = 0}};

  // ESP_LOGD(TAG, "Plugin host output pointer in clap is at address: %p",
  //          outputBuffer[0].data32);
  // ESP_LOGD(TAG, "Plugin host output buffer pointer is at address: %p",
  //          outputBuffer[0]);
  // ESP_LOGD(TAG, "Plugin host output buffer pointer is at address: %p",
  //          outputBuffer);

  const clap_process_t process = {
      .steady_time = -1,
      .frames_count = MONO_FRAMES_TO_RENDER,
      .transport = NULL,
      .audio_inputs = inputBuffer,
      .audio_outputs = outputBuffer,
      .audio_inputs_count = 1,
      .audio_outputs_count = 1,
      .in_events = &events,
      // TODO
      // if i do some arp
      .out_events = &outEvents,
  };

  for (int i = 0; i < PLUGINS_COUNT; i++) {
    // as i see it now, all events should be on frame 1
    // is it made for offline rendering?
    // Rendering doesnt work for now, something is off with output
    // Upper poiners are unobviously broken for me
    pluginRegistry[i]->process(pluginRegistry[i], &process);
    // copying output to input so we proceed  processing
    memcpy(soundBuffer[0], output[0], MONO_FRAMES_TO_RENDER);
    memcpy(soundBuffer[1], output[1], MONO_FRAMES_TO_RENDER);
    event_list_clear(event_list);

    event_list->count = outBuffer.count;
    event_list->first = outBuffer.first;
    event_list->last = outBuffer.last;

    outBuffer.count = 0;
    outBuffer.first = NULL;
    outBuffer.last = NULL;
  }
}