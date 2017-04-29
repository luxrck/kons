#include <stdlib.h>
#include <string.h>

#include <vt.h>


void cursor_init(struct cursor *c, int32_t w, int32_t h) {
  c->w = w; c->h = h;
  c->x = c->y = c->mode = 0;
  c->enabled = 1;

  c->dirty = 0;

  c->bitmap = (uint32_t*)malloc(w * h * 4);
  memset(c->bitmap, 0xff, w * h * 4);

  c->init = cursor_init;
  c->toggle =  cursor_toggle;
  c->destroy = cursor_destroy;
}


void cursor_toggle(struct cursor *cc, uint32_t *buffer, int32_t y, int32_t x, int32_t buffer_width) {
  if (buffer_width < 0) buffer_width = options.output->w;
  if (y < 0) y = cc->y;
  if (x < 0) x = cc->x;
  for (int r = 0; r < cc->h; r++)
    for (int c = 0; c < cc->w; c++)
      buffer[(y * options.font_height + r) * buffer_width + x * options.font_width + c] ^= cc->bitmap[r * cc->w + c];
  cc->dirty = !cc->dirty;
}


void cursor_clear(struct cursor *c, uint32_t *buffer) {
  if (c->dirty) c->toggle(c, buffer, -1, -1 ,-1);
}


void cursor_set(struct cursor *c, uint32_t *buffer) {
  if (!c->dirty) c->toggle(c, buffer, -1, -1, -1);
}


void cursor_destroy(struct cursor *c) {
  free(c->bitmap);
  c->enabled = 0;
  c->w = c->h = 0;
  c->x = c->y = c->mode = 0;
  c->bitmap = NULL;
}
