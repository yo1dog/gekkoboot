#ifndef INC_SHORTCUT_H
#define INC_SHORTCUT_H
#include <ogc/system.h>

#define NUM_SHORTCUTS 6

struct shortcut {
  const char * const name;
  const u16 pad_buttons;
  const char * const config_name;
  const char * const config_cli_name;
};

struct shortcut *shortcuts;

#endif