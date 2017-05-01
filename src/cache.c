#include <font.h>


void cache_init(struct cache *cache, int32_t capacity) {
  cache->dict = g_hash_table_new(g_int_hash, g_int_equal);
  cache->lru = NULL;
  cache->capacity = capacity;
  cache->size = 0;
}


struct glyph* cache_get(struct cache *cache, uint32_t key) {
  return g_hash_table_lookup(cache->dict, &key);
}


void cache_set(struct cache *cache, uint32_t key, void *data) {
  g_hash_table_replace(cache->dict, &key, data);
}
