#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include <wchar.h>
#include <stdint.h>


#include <ft2build.h>
#include FT_FREETYPE_H


#define argb(a, r, g, b) ((a<<24) | (r<<16) | (g<<8) | b)
#define rgb(r, g, b) argb(0, r, g, b)


typedef uint32_t rune;


struct options {
  //
  // public members
  //
  char font[512];         // Monospace Font
  uint8_t font_size;
  char background[512];
  uint32_t fg, bg;
  uint8_t tab_width;
  uint16_t scroll_back;   // max: 1000 line


  //
  // private/runtime members
  //

  uint8_t font_width, font_height;

  // font renderer
  FT_Library library;
  FT_Face face;
};


extern struct options options;
void options_default();
void options_init(char *path);


struct text {
  uint32_t bg, fg, attrs;
  rune code;
};


struct cursor {
  int32_t x, y, mode;
};


#define OUTPUT_BACKEND_DRM 0
struct output {
  void *backend;
  struct cursor *c;
  uint32_t w, h, cols, rows;

  void (*init)(struct output *o, struct cursor *c);
  void (*clear)(struct output *o, int32_t fr, int32_t fc, int32_t er, int32_t ec);
  void (*scroll)(struct output *o, int offset);
  void (*updateCursor)(struct output *o);
  void (*drawText)(struct output *o, struct text *t, int32_t r, int32_t c);
  void (*drawBitmap)(struct output *o, uint32_t *bitmap, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h);
  void (*destroy)(struct output *o);
} drm_output, *outputs[1];


#define ATTR_BOLD       1
#define ATTR_UNDERLINE  2
#define ATTR_REVERSE    4
struct vt {
  uint8_t state;
  rune intermediate_chars[3];
  uint8_t intermediate_char_num;
  uint8_t collect_ignored;

  int32_t params[16];
  uint8_t param_num;

  uint32_t attrs;

  uint32_t cols, rows;
  uint32_t fg, bg;

  int stdin;

  struct cursor c;
  struct text *textbuffer;
  struct output *output;

  int (*parseInput)(struct vt *vt, int fp, struct text *t);
};


void vt_init(struct vt *vt, int backend);
void vt_destroy(struct vt *vt);

extern wchar_t CHARSETS[256][256];

#endif /* !__TERMINAL_H__ */
