#include "clap/entry.h"
#define PLUGINS_COUNT 1

// ye, namespaces are good sometimes
extern const clap_plugin_entry_t clap_entry;

const clap_plugin_entry_t *plugins[PLUGINS_COUNT] = {&clap_entry};