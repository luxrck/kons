#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include <wchar.h>
#include <stdint.h>


#include <ft2build.h>
#include FT_FREETYPE_H


#define argb(a, r, g, b) ((a<<24) | (r<<16) | (g<<8) | b)
#define rgb(r, g, b) argb(0, r, g, b)


typedef uint32_t rune;


void fatal(const char *errstr, ...);


struct vt;
struct output;


struct options {
  //
  // public members
  //
  char font_family[512];         // Monospace Font
  uint8_t font_size;
  char background[512];
  uint32_t fg, bg;        // color index if @ < 8 else rgb(?,?,?) color
  int8_t tab_width;
  int16_t scroll_back;   // max: 1000 line


  //
  // private/runtime members
  //

  int8_t font_width, font_height, border;

  // font renderer
  FT_Library library;
  FT_Face face;

  struct vt *vt;
  struct output *output;
};


extern struct options options;
void options_default();
void options_init(char *path);


struct text {
  uint32_t bg, fg, attrs;
  rune code;
};


// we treat cursor as a special unicode character.
struct cursor {
  uint32_t *bitmap;
  int32_t x, y, mode;
  int32_t enabled, w, h;
  int8_t dirty;

  void (*init)(struct cursor *c, int32_t w, int32_t h);
  void (*xor)(struct cursor *cc, uint32_t *buffer, int32_t y, int32_t x, int32_t buffer_width);
  void (*destroy)(struct cursor *c);
};
void cursor_init(struct cursor *c, int32_t w, int32_t h);
void cursor_xor(struct cursor *c, uint32_t *buffer, int32_t y, int32_t x, int32_t buffer_width);
void cursor_destroy(struct cursor *c);


#define OUTPUT_BACKEND_DRM 0
struct output {
  void *backend;
  struct cursor *c;
  int32_t w, h, cols, rows;

  void (*init)(struct output *o, struct cursor *c);
  void (*clear)(struct output *o, int32_t fr, int32_t fc, int32_t er, int32_t ec);
  void (*scroll)(struct output *o, int offset);
  void (*updateCursor)(struct output *o, int32_t y, int32_t x);
  void (*drawText)(struct output *o, struct text *t);
  void (*drawBitmap)(struct output *o, int32_t *bitmap, int32_t x, int32_t y,
                     int32_t w, int32_t h);
  void (*destroy)(struct output *o);
} drm_output, *outputs[1];


#define ATTR_BOLD       1
#define ATTR_UNDERLINE  2
#define ATTR_REVERSE    4
struct vt {
  uint8_t state;
  uint8_t intermediate_chars[3];
  int8_t num_intermediate_chars;
  uint8_t collect_ignored;

  int32_t params[16];
  int8_t num_params;

  uint32_t attrs;

  int32_t cols, rows;
  uint32_t fg, bg;

  int32_t saved_line_characters_num;

  int stdin;
  int amaster;

  struct cursor c;
  struct cursor saved_c;
  struct text *textbuffer;
  struct output *output;

  // int (*parseInput)(struct vt *vt, int fp, struct text *t);
  // int (*parseChar)(struct vt *vt, struct text *t);
  void (*init)(struct vt *vt, int backend, int amaster);
  void (*run)(struct vt *vt);
  void (*destroy)(struct vt *vt);
};


void vt_init(struct vt *vt, int backend, int amaster);
void vt_destroy(struct vt *vt);


extern wchar_t CHARSETS[256][256];
extern uint8_t keymap[256];
extern uint32_t colortb[16];

#endif /* !__TERMINAL_H__ */
