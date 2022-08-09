#ifndef INC_TYPES_H
#define INC_TYPES_H

typedef enum {
  BOOT_TYPE_NONE = 0,
  BOOT_TYPE_DOL,
  BOOT_TYPE_ONBOARD
  // Changes to this enum should also be made to boot_type_names 
} BOOT_TYPE;

const char *get_boot_type_name(BOOT_TYPE type);

struct boot_action {
  BOOT_TYPE type;
  const char *dol_path;
  const char *dol_cli_options;
};

const char *get_fresult_message(FRESULT result);

#endif
