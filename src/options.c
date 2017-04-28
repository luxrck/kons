#include <string.h>

#include <vt.h>


struct options options = { 0 };

struct output *outputs[] = { &drm_output };

uint8_t keymap[256] = {
  [0x0d] = '\n',
  [0x7f] = '\b',
};

// xterm palette
// uint32_t colortb[16] = {
//   // [0] = { // normal
//     rgb(0 ,0, 0),
//     rgb(205, 0, 0),
//     rgb(0, 205, 0),
//     rgb(205, 205, 0),
//     rgb(0, 0, 238),
//     rgb(205, 0, 205),
//     rgb(0, 205, 205),
//     rgb(229, 229, 229),
//   // },
//   // [1] = { // bright
//     rgb(127, 127, 127),
//     rgb(255, 0, 0),
//     rgb(0, 255, 0),
//     rgb(255, 255, 0),
//     rgb(92, 92, 255),
//     rgb(255, 0, 255),
//     rgb(0, 255, 255),
//     rgb(255, 255, 255),
//   // }
// };

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
  strcpy(options.font, "WenQuanYi Micro Hei Mono 16");
  options.font_size = 16;
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
