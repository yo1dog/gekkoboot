#include "config.h"
#include "shortcut.h"
#include "types.h"
#include "fatfs/ff.h"
#include "inih/ini.h"
#include <gccore.h>
#include <string.h>
#include <malloc.h>

const char* config_path = "/iplboot.ini";

void default_config(struct configuration *config) {
  config->debug_enabled = false;
  config->verbose_enabled = false;
  config->default_action.type = BOOT_TYPE_DOL;
  config->default_action.dol_path = "/ipl.dol";
  config->default_action.dol_cli_options = NULL;
  
  for (int i = 0; i < NUM_SHORTCUTS; ++i) {
    config->shortcut_actions[i].type = BOOT_TYPE_NONE;
    config->shortcut_actions[i].dol_path = NULL;
    config->shortcut_actions[i].dol_cli_options = NULL;
  }
}

void parse_boot_action(const char* value, struct boot_action* action) {
  if (strcmp(value, "ONBOARD") == 0) {
    action->type = BOOT_TYPE_ONBOARD;
  }
  else {
    action->type = BOOT_TYPE_DOL;
    action->dol_path = value;
  }
}

int ini_parse_handler(void* _config, const char* section, const char* name, const char* value) {
  struct configuration *config = (struct configuration *)_config;
  
  if (strcmp(name, "DEBUG") == 0) {
    config->debug_enabled = strcmp(value, "TRUE");
    return 1;
  }
  if (strcmp(name, "VERBOSE") == 0) {
    config->verbose_enabled = strcmp(value, "TRUE");
    return 1;
  }
  if (strcmp(name, "DEFAULT") == 0) {
    parse_boot_action(value, &config->default_action);
    return 1;
  }
  if (strcmp(name, "DEFAULT_CLI") == 0) {
    config->default_action.dol_cli_options = value;
    return 1;
  }
  
  for (int i = 0; i < config->num_shortcuts; ++i) {
    if (strcmp(name, config->shortcuts[i].config_name) == 0) {
      parse_boot_action(value, &config->shortcuts[i].action);
      return 1;
    }
    if (strcmp(name, config->shortcuts[i].config_cli_name) == 0) {
      config->shortcuts[i].action.dol_cli_options = value;
      return 1;
    }
  }
  
  kprintf("Unknown configuration entry: %s\n", name);
  //return 0;
  return 1;
}

int read_parse_config(struct configuration *config) {
  int res = 0;
  
  default_config(config);
  
  kprintf("Reading %s\n", config_path);
  FIL file;
  FRESULT open_result = f_open(&file, config_path, FA_READ);
  if (open_result != FR_OK) {
    kprintf("Failed to open config file: %s\n", get_fresult_message(open_result));
    goto close;
  }
  
  size_t size = f_size(&file);
  char *config_str = (char *)malloc(size);
  
  if (!config_str) {
    kprintf("Couldn't allocate memory for config file: %iB\n", size);
    goto close;
  }
  
  UINT _;
  f_read(&file, config_str, size, &_);
  
  if (ini_parse_string(config_str, ini_parse_handler, config) != 0) {
    kprintf("Failed to parse config file\n");
    goto close;
  }
  
  res = 1;
  
close:
  f_close(&file);
  
  return res;
}

void print_config(struct configuration *config) {
  kprintf("debug_enabled: %i\n", config->debug_enabled);
  kprintf("verbose_enabled: %i\n", config->verbose_enabled);
  for (int i = 0; i < config->num_shortcuts; ++i) {
    kprintf("shortcuts[%s].action: %s\n", config->shortcuts[i].name, BOOT_TYPE_names[config->shortcuts[i].action.type]);
    kprintf("shortcuts[%s].dol_path: %s\n", config->shortcuts[i].name, config->shortcuts[i].action.dol_path);
    kprintf("shortcuts[%s].dol_cli_options: %s\n", config->shortcuts[i].name, config->shortcuts[i].action.dol_cli_options);
  }
  kprintf("defult.action: %s\n", BOOT_TYPE_names[config->default_action.type]);
  kprintf("defult.dol_path: %s\n", config->default_action.dol_path);
  kprintf("defult.dol_cli_options: %s\n", config->default_action.dol_cli_options);
}