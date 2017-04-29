#include <fontconfig/fontconfig.h>

#include <unistd.h>
#include <font.h>


void font_init() {
  FcConfig* config = FcInitLoadConfigAndFonts();

  FcPattern *pattern = FcNameParse((const FcChar8 *)(options.font_family));
  FcConfigSubstitute(config, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result = FcResultNoMatch; FcPattern* founded = FcFontMatch(config, pattern, &result);

  FcChar8 *foundedfile = NULL;
  if (FcPatternGetString(founded, FC_FILE, 0, &foundedfile) != FcResultMatch)
    fatal("Font '%s' search failed.\n", options.font_family);

  int faceindex = 0;
  FcPatternGetInteger(founded, "index", 0, &faceindex);

  FT_Init_FreeType(&options.library);
  FT_New_Face(options.library,
              (char*)foundedfile,
              faceindex,
              &options.face);
  FT_Set_Pixel_Sizes(options.face, 0, options.font_height);
  FT_Select_Charmap(options.face, FT_ENCODING_UNICODE);

  FcPatternDestroy(pattern);
  FcPatternDestroy(founded);
}


void font_destroy() {
  FT_Done_Face(options.face);
  FT_Done_FreeType(options.library);
}


rune u_rune(char *s, int size) {
  if (size <= 1) return s[0];

  uint8_t t = 0, mask[3] = { 192, 224, 240 };
  rune u = (uint8_t)s[0] & ~mask[size - 2];

  for (uint8_t i = 0; i < size - 1; i++) {
    t = (uint8_t)s[1 + i];
    u = (u << 6) + (t & 0x7f);
  }
  // printf("u_rune: %x %x %x %x - %x\n", s[0], s[1], s[2], s[3], u);
  return u;
}


int u_getc(int fp, char *buf) {
  uint8_t c = 0, mask[3] = { 192, 224, 240 };

  if (read(fp, buf, 1) == EOF) return EOF;

  if ((buf[0] & mask[0]) == mask[0]) c++;
  if ((buf[0] & mask[1]) == mask[1]) c++;
  if ((buf[0] & mask[2]) == mask[2]) c++;

  // printf("u_getc: %x %x %x\r\n", buf[0], buf[1], c);
  if (c == 0) return c + 1;

  // for (uint8_t i = 0; i < c; i++) {
  read(fp, buf + 1, c);
  // }
  // printf("u_getc-2: %x %x - %d\r\n", buf[0], buf[1], c);
  return c + 1;
}


int u_render(struct text *t, uint32_t *buffer, int32_t buffer_width) {
  rune c = FT_Get_Char_Index(options.face, t->code);
  FT_Load_Glyph(options.face, c, FT_LOAD_TARGET_NORMAL);
  FT_Render_Glyph(options.face->glyph, FT_RENDER_MODE_NORMAL);

  FT_GlyphSlot slot = options.face->glyph;
  FT_Bitmap *bitmap = &(slot->bitmap);

  struct output *o = options.output;

  if (buffer_width < 0) buffer_width = o->w;

  int32_t w = (options.font_height - bitmap->width <= 2) ? options.font_height : options.font_width;
  int32_t h = options.font_height;
  uint32_t tfg = t->fg, tbg = t->bg;

  // update cursor before rendering.
  int32_t x = o->c->x, y = o->c->y;
  if (w == h && o->c->x == o->cols - 1) {
    x++; o->updateCursor(o, y, x, OUTPUT_CURSOR_NOREDRAW);
  } else {
    o->c->dirty = 0;
  }

  int32_t cx = o->c->x * options.font_width, cy = o->c->y * options.font_height;

  if (t->attrs & ATTR_BOLD) tfg += 8;
  tfg = colortb[tfg], tbg = colortb[tbg];
  if (t->attrs & ATTR_REVERSE) {
    tfg += tbg, tbg = tfg - tbg, tfg = tfg - tbg;
  }

  int32_t ox = 0,                                              // origin.x
          oy = options.font_height - options.font_height / 5;  // origin.y
          //  y = (y < slot->bitmap_top) ? slot->bitmap_top : y;
  int32_t sx= slot->bitmap_left, sy = oy - slot->bitmap_top,
          ex= sx + bitmap->width, ey = sy + bitmap->rows;

  // printf("---:%c %d %d %d %d - %d %d - %d - %d %d\n",t->code, sx, ex, sy, ey, bitmap->width, bitmap->rows, slot->bitmap_top, x, y);

  for (int r = 0; r < h; r++)
    for (int c = 0; c < w; c++) {
      // ATTENTION: In fact, we did not specify `FT_LOAD_COLOR`, so bitmap is accually a
      // 256-level gray bitmap. But, when we draw it on screen, it shows blue color. why?
      // Because:
      //     1. the memory layout of bitmap cell is: [256-level gray data]
      //     2. the buffer is 32-bit size pixel use format argb, which layout on memory is: [b][g][r][a]
      // So we just cast 8-bit bitmap cell data to 32-bit buffer cell data, which makes it blue!!!
      // buffer[r * bitmap->width + c] = (bitmap->buffer[r * bitmap->width + c]);
      uint32_t fg = tfg, bg = tbg, index = (cy + r) * buffer_width + c + cx;

      buffer[index] = bg;

      if (!(r < sy || r >= ey || c < sx || c >= ex)) {
        uint8_t g = bitmap->buffer[(r - sy) * bitmap->width + c - sx];
        if (!g) continue;

        if (t->attrs & ATTR_REVERSE) g = ~g;

        uint8_t *colors = (uint8_t*)&fg;
        colors[0] = (uint16_t)(colors[0] + g) / 2;
        colors[1] = (uint16_t)(colors[1] + g) / 2;
        colors[2] = (uint16_t)(colors[2] + g) / 2;

        buffer[index] = fg;
      }
    }

  if (t->attrs & ATTR_UNDERLINE) {
    for (int c = 0; c < w; c++)
      buffer[(cy + h - 2) * buffer_width + c + cx] = tfg;
  }

  x = o->c->x + (w / options.font_width);
  // no need to update cursor now, because the rendered text is already overwrite cursor.
  o->updateCursor(o, o->c->y, x, OUTPUT_CURSOR_REDRAW);

  return w / options.font_width;
}
