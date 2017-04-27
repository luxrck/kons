#ifndef __FONT_H__
#define __FONT_H__


#include <vt.h>


void font_init();
void font_destroy();

rune u_getc(int fp);
rune u_rune(char *s);
int u_render(struct text *t, uint32_t *bitmap, uint32_t *w, uint32_t *h);

#endif /* !__FONT_H__ */
