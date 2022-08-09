#ifndef INC_CONFIG_H
#define INC_CONFIG_H
#include "shortcuts.h"
#include <ogc/system.h>

struct configuration {
  int debug_enabled;
  int verbose_enabled;
  struct boot_action default_action;
  struct boot_action shortcut_actions[NUM_SHORTCUTS];
};

struct configuration *load_parse_config();
const char *stringify_config(struct configuration *config);

#endif
