#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <vt.h>
#include <font.h>


#define DRM_CARD0 "/dev/dri/card0"

// buffer[0] & buffer[1] are for front & back drawing buffer.
// buffer[2] is for background image (if exist).
#define DRM_BUFFERS 3


struct __buffer {
  uint32_t *data;
  uint32_t w;
  uint32_t h;
  uint32_t pitch;
  uint32_t size;
  uint32_t handle;
  uint32_t id;
};


struct __drm {
  drmModeModeInfo mode;
  drmEventContext ev_ctx;
  drmModeCrtc *saved_crtc;

  int fd;
  uint32_t connector_id;
  uint32_t encoder_id;
  uint32_t crtc_id;

  struct __buffer buf[DRM_BUFFERS];
  struct __buffer *current_buf;
};


static void __drm_init_buffer(struct __drm *d) {
  d->current_buf = &d->buf[0];
  for (int i = 0; i < DRM_BUFFERS; i++) {
    struct __buffer *buf = &d->buf[i];
    buf->w = d->mode.hdisplay;
    buf->h = d->mode.vdisplay;

    struct drm_mode_create_dumb creq = {
      .width = buf->w,
      .height = buf->h,
      .bpp = 32,
    };

    drmIoctl(d->fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    buf->pitch = creq.pitch;
    buf->size = creq.size;
    buf->handle = creq.handle;
    drmModeAddFB(d->fd, buf->w, buf->h, 24, creq.bpp, buf->pitch, buf->handle, &buf->id);

    struct drm_mode_map_dumb mreq = {
      .handle = buf->handle,
    };

    drmIoctl(d->fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    buf->data = (uint32_t*)mmap(0, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED, d->fd, mreq.offset);

    memset(buf->data, 0, buf->size);
  }
}
void __drm_init_crtc(struct __drm *d) {
  d->saved_crtc = drmModeGetCrtc(d->fd, d->crtc_id);
  drmModeSetCrtc(d->fd, d->crtc_id, d->current_buf->id, 0, 0, &d->connector_id, 1, &d->mode);
}
struct __drm* __drm_init() {
  struct __drm *d = malloc(sizeof(struct __drm));

  d->fd = open(DRM_CARD0, O_RDWR | O_CLOEXEC);

  drmModeRes *res = drmModeGetResources(d->fd);

  for (int i = 0; i < res->count_connectors; i++) {
    drmModeConnector *conn = drmModeGetConnector(d->fd, res->connectors[i]);
    if (conn && conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
      d->connector_id = conn->connector_id;
      d->encoder_id = conn->encoder_id;
      d->mode = conn->modes[0];
      drmModeFreeConnector(conn);
      break;
    }
    drmModeFreeConnector(conn);
  }

  drmModeEncoder *encoder = drmModeGetEncoder(d->fd, d->encoder_id);
  d->crtc_id = encoder->crtc_id;
  drmModeFreeEncoder(encoder);

  d->saved_crtc = NULL;

  drmModeFreeResources(res);

  __drm_init_buffer(d);
  __drm_init_crtc(d);

  return d;
}


void __drm_destroy(struct __drm *d) {
  if (!d) return;
  struct drm_mode_destroy_dumb dreq = { 0 };
  for (int i = 0; i < DRM_BUFFERS; i++) {
    dreq.handle = d->buf[i].handle;
    drmModeRmFB(d->fd, d->buf[i].id);
    munmap(d->buf[i].data, d->buf[i].size);
    drmIoctl(d->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
  }
  if (d->saved_crtc) {
    drmModeSetCrtc(d->fd, d->saved_crtc->crtc_id,
                   d->saved_crtc->buffer_id,
                   d->saved_crtc->x, d->saved_crtc->y,
                   &d->connector_id, 1,
                   &d->saved_crtc->mode);
    drmModeFreeCrtc(d->saved_crtc);
    d->saved_crtc = NULL;
  }
  close(d->fd);
  free(d);
}


void drm_output_init(struct output *o, struct cursor *c) {
  struct __drm *d = __drm_init();
  o->backend = d;
  o->c = c;
  o->w = d->mode.hdisplay;
  o->h = d->mode.vdisplay;
  o->cols = o->w / options.font_size;
  o->rows = o->h / options.font_size;
}


void drm_output_clear_line(struct output *o, int32_t r, int32_t sc, int32_t ec) {
  struct __drm *d = (struct __drm*)o->backend;
  if (ec < 0) ec = o->cols - 1;
  int32_t es = (ec - sc + 1) * options.font_size;
  for (int i = 0; i < options.font_size; i++)
    memset(d->current_buf->data + o->w * (r * options.font_size + i) + sc * options.font_size, 0, es * 4);
}
void drm_output_clear(struct output *o, int32_t sr, int32_t sc, int32_t er, int32_t ec) {
  if (sr < 0 || sc < 0) return;
  if (er < 0) er = o->rows - 1;
  if (ec < 0) ec = o->cols - 1;
  if (sr > er) return;
  if (sr == er && sc > ec) return;

  if (sr == er) {
    drm_output_clear_line(o, sr, sc, ec);
  } else {
    drm_output_clear_line(o, sr, sc, -1);
    for (int r = sr + 1; r < er; r++)
      drm_output_clear_line(o, r, 0, -1);
    drm_output_clear_line(o, er, 0, ec);
  }
}


void drm_output_scroll(struct output *o, int32_t offset) {
  if (offset <= 0) return;
  struct __drm *d = (struct __drm*)o->backend;
  uint32_t *data = d->current_buf->data;
  memmove(data, data + o->w * options.font_size, (o->h - options.font_size) * o->w * 4);
  memset(data + o->w * (o->rows - 1) * options.font_size, 0, options.font_size * o->w * 4);
}


void drm_output_update_cursor(struct output *o) {
  struct cursor *c = o->c;
  if (c->x >= o->cols) c->x = 0, c->y++;
  if (c->y >= o->rows) drm_output_scroll(o, 1), c->y = o->rows - 1;
}


void drm_output_draw_text(struct output *o, struct text *t, int32_t row, int32_t col) {
  if (row < 0) row = o->c->y;
  if (col < 0) col = o->c->x;

  struct __drm *d = (struct __drm*)o->backend;

  uint32_t fw = options.font_width, fh = options.font_height;
  uint32_t sx = fw * col, sy = fh * row, w = 0, h = 0;
  uint32_t buffer[16384] = { 0 };

  u_render(t, buffer, &w, &h);
  // printf("draw: %c %d %d %d %d\n", t->code, w, h, o->c->x, o->c->y);

  for (int r = 0; r < h; r++)
    for (int c = 0; c < w; c++)
      d->current_buf->data[(sy + r) * o->w + sx + c] = buffer[r * w + c];

   o->c->x += (w > options.font_width ? 2 : 1);
   o->updateCursor(o);
}


void drm_output_destroy(struct output *o) {
  __drm_destroy(o->backend);
}


struct output drm_output = {
  .init = drm_output_init,
  .clear = drm_output_clear,
  .scroll = drm_output_scroll,
  .updateCursor = drm_output_update_cursor,
  .drawText = drm_output_draw_text,
  .drawBitmap = NULL,
  .destroy = drm_output_destroy,
};
