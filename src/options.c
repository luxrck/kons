#include <string.h>

#include <vt.h>


struct options options = { 0 };

struct output *outputs[] = { &drm_output };

uint8_t keymap[256] = {
  [0x0d] = '\n',
  [0x7f] = '\b',
};

// One Dark
uint32_t colortb[16] = {
  0x000000,
  0xE06C75,
  0x98C379,
  0xD19A66,
  0x61AFEF,
  0xC678DD,
  0x56B6C2,
  0xABB2BF,
  0x5C6370,
  0xE06C75,
  0x98C379,
  0xD19A66,
  0x61AFEF,
  0xC678DD,
  0x56B6C2,
  0xFFFEFE,
};

void options_default() {
  strcpy(options.font_family, "Noto Sans Mono CJK SC");
  options.font_size = 12;
  options.fg = 7;
  options.bg = 0;
  options.tab_width = 4;
  options.scroll_back = 1000;

  // runtime attrs.
  options.border = 2;
}


void options_init(char *path) {
  options_default();

  options.font_width = options.font_size / 2;
  options.font_height = options.font_size;
}
