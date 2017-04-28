#ifndef __FONT_H__
#define __FONT_H__


#include <vt.h>


void font_init();
void font_destroy();

int u_getc(int fp, char *buf);
rune u_rune(char *s, int size);
int u_render(struct text *t, uint32_t *buffer, int32_t by, int32_t bx, int32_t buffer_width);

#endif /* !__FONT_H__ */
