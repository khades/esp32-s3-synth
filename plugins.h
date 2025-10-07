#include "clap/entry.h"
#include "sine-ping.h"
#define PLUGINS_COUNT 1

// Oh i wish it would work
const clap_plugin_entry_t *plugins[PLUGINS_COUNT] = {&sine_ping_plugin};