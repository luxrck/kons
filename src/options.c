#include <string.h>

#include <vt.h>


struct options options = { 0 };


void options_default() {
  strcpy(options.font, "WenQuanYi Micro Hei Mono 16");
  options.font_size = 16;
  options.fg = rgb(255, 255, 255);
  options.bg = rgb(0, 0, 0);
  options.tab_width = 4;
  options.scroll_back = 1000;
}


void options_init(char *path) {
  options_default();

  options.font_width = options.font_size / 2;
  options.font_height = options.font_size;
}
