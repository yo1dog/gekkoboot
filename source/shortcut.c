#include "shortcut.h"

struct shortcut shortcuts[] = {
  {"A",     PAD_BUTTON_A,     "A",     "A_CLI"    },
  {"B",     PAD_BUTTON_B,     "B",     "B_CLI"    },
  {"X",     PAD_BUTTON_X,     "X",     "X_CLI"    },
  {"Y",     PAD_BUTTON_Y,     "Y",     "Y_CLI"    },
  {"Z",     PAD_TRIGGER_Z,    "Z",     "Z_CLI"    },
  {"START", PAD_BUTTON_START, "START", "START_CLI"},
  // NOTE: Shouldn't use L, R or Joysticks as analog inputs are calibrated on boot.
  // Should also avoid D-Pad as it is used for special functions.
};
