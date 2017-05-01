#ifndef __FONT_H__
#define __FONT_H__

#include <glib.h>

#include <vt.h>


struct glyph {
  int32_t w, h;
  uint8_t *data;
};

struct cache {
  GHashTable *dict;
  GList *lru;
  int32_t capacity;
  int32_t size;
};


void font_init();
void font_destroy();

void cache_init(struct cache *cache, int32_t capacity);
struct glyph* cache_get(struct cache *cache, uint32_t key);
void cache_set(struct cache *cache, uint32_t key, void *data);

int u_getc(int fp, char *buf);
rune u_rune(char *s, int size);
int u_render(struct text *t, uint32_t *buffer, int32_t buffer_width);

#endif /* !__FONT_H__ */
