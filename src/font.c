#include <fontconfig/fontconfig.h>

#include <unistd.h>
#include <font.h>

static struct cache cache;


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

  cache_init(&cache, 8192);
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


struct glyph* u_glyph(struct text *t) {
  struct glyph *glyph = cache_get(&cache, t->code);
  // if (glyph)
  // printf("find: %x %c %d %d\r\n", t->code, t->code, glyph->w, glyph->h);
  if (!glyph) {
    rune c = FT_Get_Char_Index(options.face, t->code);
    FT_Load_Glyph(options.face, c, FT_LOAD_TARGET_NORMAL);
    FT_Render_Glyph(options.face->glyph, FT_RENDER_MODE_NORMAL);

    FT_GlyphSlot slot = options.face->glyph;
    FT_Bitmap *bitmap = &(slot->bitmap);

    int32_t w = (options.font_height - bitmap->width <= 2) ? options.font_height : options.font_width;
    int32_t h = options.font_height;

    glyph = malloc(sizeof(struct glyph));
    glyph->w = w;
    glyph->h = h;
    glyph->data = calloc(1, w * h);

    int32_t ox = 0,                                              // origin.x
            oy = options.font_height - options.font_height / 5,  // origin.y
            sx = slot->bitmap_left,
            sy = oy - slot->bitmap_top;
            sx = sx < 0 ? -sx : 0,
            sy = sy < 0 ? -sy : 0;
    int32_t ex = bitmap->width - sx - w,
            ey = oy + bitmap->rows - (slot->bitmap_top - sy);
            ex = ex < 0 ? bitmap->width : sx + w,
            ey = ey > h ? bitmap->rows + h - ey : bitmap->rows;   // !!!!!!
    int32_t bx = sx == 0 ? slot->bitmap_left : 0,
            by = sy == 0 ? oy - slot->bitmap_top : 0;
    // printf("render: %x %d %d - %d %d - %d %d - %d %d - %d %d - %d %d\r\n",
          // t->code, ox, oy, sx, sy, ex, ey, bx, by, w, h, slot->bitmap_left, slot->bitmap_top);
    for (int r = sy; r < ey; r++) {
      for (int c = sx; c < ex; c++) {
        uint8_t p = bitmap->buffer[r * bitmap->width + c];
        if (!p) continue;
        glyph->data[(by + r - sy) * w + bx + c - sx] = p;
      }
    }
    cache_set(&cache, t->code, glyph);
  }
  return glyph;
}


int u_render(struct text *t, uint32_t *buffer, int32_t buffer_width) {
  struct glyph *glyph = u_glyph(t);
  struct output *o = options.output;

  if (buffer_width < 0) buffer_width = o->w;

  int32_t w = glyph->w;
  int32_t h = glyph->h;

  // update cursor before rendering.
  int32_t x = o->c->x, y = o->c->y;
  if (w == h && o->c->x == o->cols - 1) {
    x++; o->updateCursor(o, y, x, OUTPUT_CURSOR_NOREDRAW);
  } else {
    o->c->dirty = 0;
  }

  uint_fast32_t tfg = t->fg, tbg = t->bg;
  if (t->attrs & ATTR_BOLD) tfg += 8;
  tfg = colortb[tfg], tbg = colortb[tbg];
  if (t->attrs & ATTR_REVERSE) {
    tfg += tbg, tbg = tfg - tbg, tfg = tfg - tbg;
  }

  int32_t cx = o->c->x * options.font_width, cy = o->c->y * options.font_height;
  uint8_t *fgc = (uint8_t*)&tfg, *bgc = (uint8_t*)&tbg;
  uint_fast32_t r, g, b;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      uint32_t index = (cy + y) * buffer_width + cx + x,
               gindex= y * w + x;
      uint_fast32_t p= glyph->data[gindex];

      if (p == 0) {
        buffer[index] = tbg;
      } else if (p == 255) {
        buffer[index] = tfg;
      } else {
        // if (t->attrs & ATTR_REVERSE) p = ~p;
        uint8_t *buffer_u8 = (uint8_t*)(buffer + index);

        // gitub.com:kmscon/src/uterm_drm2d_render.c
        /* Division by 255 (t /= 255) is done with:
				 *   t += 0x80
				 *   t = (t + (t >> 8)) >> 8
				 * This speeds up the computation by ~20% as the
				 * division is not needed. */
        b = fgc[0] * p + bgc[0] * (255 - p);
        b+= 0x80;
        b = (b + (b >> 8)) >> 8;

        g = fgc[1] * p + bgc[1] * (255 - p);
        g+= 0x80;
        g = (g + (g >> 8)) >> 8;

        r = fgc[2] * p + bgc[2] * (255 - p);
        r+= 0x80;
        r = (r + (r >> 8)) >> 8;

        buffer_u8[0] = b;
        buffer_u8[1] = g;
        buffer_u8[2] = r;
        // buffer[index] = (r<< 16) | (g << 8) | b;

        // uint32_t fg = tfg;
        // uint8_t *colors = (uint8_t*)&fg;
        // colors[0] = (uint16_t)(colors[0] + p) >> 1;
        // colors[1] = (uint16_t)(colors[1] + p) >> 1;
        // colors[2] = (uint16_t)(colors[2] + p) >> 1;
        // buffer[index] = fg;

        // buffer_u8[0] = (uint16_t)(fgc[0] + p) >> 1;
        // buffer_u8[1] = (uint16_t)(fgc[1] + p) >> 1;
        // buffer_u8[2] = (uint16_t)(fgc[2] + p) >> 1;
      }
    }
  }

  if (t->attrs & ATTR_UNDERLINE) {
    for (int c = 0; c < w; c++)
      buffer[(cy + h - 2) * buffer_width + c + cx] = tfg;
  }

  x = o->c->x + (w / options.font_width);
  // Had to set this flag because of the performence issue...
  o->updateCursor(o, o->c->y, x, OUTPUT_CURSOR_NOREDRAW);

  return w / options.font_width;
}
