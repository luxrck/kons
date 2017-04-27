#include <fontconfig/fontconfig.h>

#include <unistd.h>
#include <font.h>


void font_init() {
  FcConfig* config = FcInitLoadConfigAndFonts();

  FcPattern *pattern = FcNameParse((const FcChar8 *)(options.font));
  FcConfigSubstitute(config, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result = FcResultNoMatch; FcPattern* founded = FcFontMatch(config, pattern, &result);

  FcChar8 *foundedfile = NULL;
  if (FcPatternGetString(founded, FC_FILE, 0, &foundedfile) != FcResultMatch) exit(-1);

  FT_Init_FreeType(&options.library);
  FT_New_Face(options.library,
              (char*)foundedfile,
              1,
              &options.face);
  FT_Set_Pixel_Sizes(options.face, options.font_height, options.font_height);
  // FT_Set_Char_Size(options.face, options.font_size * 64, 0, 72, 72);

  FcPatternDestroy(pattern);
  FcPatternDestroy(founded);
}


void font_destroy() {
  FT_Done_Face(options.face);
  FT_Done_FreeType(options.library);
}


rune u_rune(char *s) {
  rune u = 0;
  uint8_t t = 0, c = 0, mask[3] = { 192, 224, 240 };

  t = (uint8_t)s[0];

  u = t;

  if ((t & mask[0]) == mask[0]) c++, u = t & (~mask[0]);
  if ((t & mask[1]) == mask[1]) c++, u = t & (~mask[1]);
  if ((t & mask[2]) == mask[2]) c++, u = t & (~mask[2]);

  if (c == 0) return u;

  for (uint8_t i = 0; i < c; i++) {
    t = (uint8_t)s[1+i];
    u = (u << 6) + (t & 0x7f);
  }
  return u;
}


rune u_getc(int fp) {
  rune u = 0;
  uint8_t t = 0, c = 0, mask[3] = { 192, 224, 240 };

  if (read(fp, &t, 1) == EOF) return EOF;

  u = t;

  if ((t & mask[0]) == mask[0]) c++, u = t & (~mask[0]);
  if ((t & mask[1]) == mask[1]) c++, u = t & (~mask[1]);
  if ((t & mask[2]) == mask[2]) c++, u = t & (~mask[2]);

  if (c == 0) return u;

  for (uint8_t i = 0; i < c; i++) {
    read(fp, &t, 1);
    u = (u << 6) + (t & 0x7f);
  }
  return u;
}


int u_render(struct text *t, uint32_t *buffer, uint32_t *bw, uint32_t *bh) {
  // int e = 0;

  FT_Select_Charmap(options.face, FT_ENCODING_UNICODE);
  rune c = FT_Get_Char_Index(options.face, t->code);
  FT_Load_Glyph(options.face, c, FT_LOAD_TARGET_NORMAL);
  FT_Render_Glyph(options.face->glyph, FT_RENDER_MODE_NORMAL);

  FT_GlyphSlot slot = options.face->glyph;
  FT_Bitmap *bitmap = &(slot->bitmap);

  uint32_t w = t->code > 0x7f ? options.font_height : options.font_width;
  uint32_t h = options.font_height;

  uint32_t tfg = t->fg, tbg = t->bg;
  if (t->attrs & ATTR_BOLD)
    tfg |= 0xff000000, tbg &= 0xff000000;

  int32_t x = 0,
          y = options.font_height - options.font_height / 5 - 1;  // origin
          //  y = (y < slot->bitmap_top) ? slot->bitmap_top : y;
  int32_t sx= (slot->bitmap_left < 0 ? -slot->bitmap_left : 0), sy = y - slot->bitmap_top, // (y < slot->bitmap_top ? 0 : y - slot->bitmap_top),
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
      uint32_t fg = tfg, bg = tbg;

      if (r < sy || r >= ey) {
        buffer[r * w + c] = bg;
      } else if (c < sx || c >= ex) {
        buffer[r * w + c] = bg;
      } else {
        uint8_t g = bitmap->buffer[(r - sy) * bitmap->width + c - sx];

        if (!g) {
          buffer[r * w + c] = bg; continue;
        }

        float raito = g / 255.0;
        uint8_t *colors = (uint8_t*)&fg;

        colors[0] = (uint8_t)(colors[0] * raito);
        colors[1] = (uint8_t)(colors[1] * raito);
        colors[2] = (uint8_t)(colors[2] * raito);

        buffer[r * w + c] = fg;
      }
    }

  if (t->attrs & ATTR_UNDERLINE)
    for (int c = 0; c < w; c++)
      buffer[(h - 1) * w + c] = tfg;

  if (bw) *bw = w; // bitmap->width;
  if (bh) *bh = h; // bitmap->rows;

  return 0;
}
