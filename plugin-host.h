#include "clap/plugin.h"
#include "midi-events.h"

typedef struct {
  const clap_plugin_descriptor_t *descriptor;
  const clap_plugin_t *plugin;
} plugin_entry_t;

void plugins_init();
void plugins_activate(double sample_rate);
void plugins_process(struct event_list_container *in_events, float **output);