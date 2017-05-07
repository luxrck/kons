#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
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
#define DRM_BUFFERS 2

#define FRONTBUFFER(d) (d->current_buf)
#define ALTERBUFFER(d) (&(d->buf[d->current_buf_index ^ 1]))

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
  drmEventContext ev;
  drmModeCrtc *saved_crtc;

  int fd;
  uint32_t connector_id;
  uint32_t encoder_id;
  uint32_t crtc_id;

  int8_t current_buf_index;
  int8_t pflipping;

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
      .flags = 0,
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
void __drm_pflipped(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data) {
  struct __drm *d = (struct __drm*)data;
  d->pflipping = 0;
}
struct __drm* __drm_init() {
  struct __drm *d = calloc(1, sizeof(struct __drm));  // Aaaaaaa

  d->fd = open(DRM_CARD0, O_RDWR);

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

  d->current_buf_index = 0;
  d->pflipping = 0;
  d->saved_crtc = NULL;

  drmModeFreeResources(res);

  __drm_init_buffer(d);
  __drm_init_crtc(d);

  // la la la
  // drmDropMaster(d->fd);

  d->ev.version = 2;
  d->ev.page_flip_handler = __drm_pflipped;

  return d;
}


void __drm_destroy(struct __drm *d) {
  if (!d) return;
  // drmSetMaster(d->fd);

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
  o->cols = o->w / options.font_width;
  o->rows = o->h / options.font_height;

  // int r = drmModeSetCursor(d->fd, d->crtc_id, 0, 40, 40);
  // printf("set cursor: %d\r\n", r);
  // o->updateCursor(o);
}


void drm_output_clear_line(struct output *o, int32_t r, int32_t sc, int32_t ec) {
  struct __drm *d = (struct __drm*)o->backend;
  struct vt *vt = options.vt;
  struct text *texts = vt->texts;

  if (sc < 0) sc = 0;
  if (ec < 0 || ec >= o->cols) ec = o->cols - 1;

  memset(texts + r * vt->cols + sc, 0, (ec - sc + 1) * sizeof(struct text));

  int32_t es = (ec - sc + 1) * options.font_width;
  for (int i = 0; i < options.font_height; i++)
    memset(d->current_buf->data + o->w * (r * options.font_height + i) + sc * options.font_width, 0, es * 4);
}
void drm_output_clear(struct output *o, int32_t sr, int32_t sc, int32_t er, int32_t ec) {
  if (sr < 0) sr = 0;
  if (sc < 0) sc = 0;
  if (er < 0 || er >= o->cols) er = o->rows - 1;
  if (ec < 0 || ec >= o->cols) ec = o->cols - 1;

  if (sr > er) return;
  if (sr == er && sc > ec) return;

  struct cursor *c = o->c;
  struct __drm *d = (struct __drm*)o->backend;

  if (sr == er) {
    drm_output_clear_line(o, sr, sc, ec);
  } else {
    drm_output_clear_line(o, sr, sc, -1);
    for (int r = sr + 1; r < er; r++)
      drm_output_clear_line(o, r, 0, -1);
    drm_output_clear_line(o, er, 0, ec);
  }

  // drop last cursor.
  if (c->dirty) c->dirty = 0;
}

void print_texts() {
  printf("-------------\r\n");
  struct vt *vt = options.vt;
  struct text *texts = vt->texts;
  for (int i=0; i<vt->rows; i++) {
    for (int j=0; j<vt->cols; j++)
      printf("%c ", texts[i*vt->cols +j].code);
    printf("\r\n");
  }
}
void drm_output_redraw(struct output *o) {
  struct __drm *d = (struct __drm*)o->backend;

  if (d->pflipping) return;
  d->current_buf = ALTERBUFFER(d);
  memset(d->current_buf->data, 0, d->current_buf->size);

  struct vt *vt = options.vt;
  struct text *texts = vt->texts;
  for (int r = 0; r < vt->rows; r++) {
    for (int c = 0; c < vt->cols; c++) {
      struct text *t = texts + r * vt->cols + c;
      if (!t->code) {
        c++; continue;
      } else {
        o->drawText(o, t, r, c);
      }
    }
  }

  int ret = drmModePageFlip(d->fd, d->crtc_id, d->current_buf->id, DRM_MODE_PAGE_FLIP_EVENT, d);
  if (!ret) {
		d->current_buf_index ^= 1;
		d->pflipping = 1;
    // struct __buffer *buf = ALTERBUFFER(d);
    // memset(buf, 0, buf->size);
	} else {
    printf("pageflip: %s\r\n", strerror(errno));
    d->current_buf = ALTERBUFFER(d); return;
  }

  // poll page flip finish event
  struct pollfd fds[1] = { { .events = POLLIN, .fd = d->fd } };
  poll(fds, 1, 2000);
  if (fds[0].revents & POLLIN) {
    ret = drmHandleEvent(d->fd, &(d->ev));
    // d->pflipping = 0;
  }
}


void drm_output_scroll(struct output *o, int32_t offset) {
  if (offset == 0) return;
  struct vt *vt = options.vt;
  struct text *texts = vt->texts;
  struct __drm *d = (struct __drm*)o->backend;

  vt->scroll(vt, offset);
  drm_output_redraw(o);
  // return;
  // uint32_t *d1 = d->current_buf->data, *d2 = d1 + o->w * options.font_height;
  // if (offset > 0) {
  //   memmove(d1, d2, (o->h - options.font_height) * o->w * 4);
  //   memset(d1 + o->w * (o->rows - 1) * options.font_height, 0, options.font_height * o->w * 4);
  // } else {
  //   memmove(d2, d1, (o->h - options.font_height) * o->w * 4);
  //   memset(d1, 0, options.font_height * o->w * 4);
  // }
}


void drm_output_update_cursor(struct output *o, int32_t y, int32_t x, int32_t redraw) {
  struct __drm *d = (struct __drm*)o->backend;
  struct cursor *c = o->c;

  if (o->c->y == y && o->c->x == x) return;

  // clear old cursor.
  if (o->c->dirty) {
    o->c->toggle(o->c, d->current_buf->data, -1, -1, -1);
  }

  o->c->y = y; o->c->x = x;
  if (c->x >= o->cols) c->x = 0, c->y++;

  if (c->y >= o->rows) {
    drm_output_scroll(o, 1);
    c->y = o->rows - 1;
  } else if (c->y < 0) {
    drm_output_scroll(o, -1);
    c->y = 0;
  }

  if (redraw && !o->c->dirty) {
    o->c->toggle(o->c, d->current_buf->data, -1, -1, -1);
  }
}


void drm_output_draw_text(struct output *o, struct text *t, int32_t y, int32_t x) {
  struct __drm *d = (struct __drm*)o->backend;
  int32_t cx = x * options.font_width, cy = y * options.font_height;
  u_render(t, d->current_buf->data, -1, cy, cx);
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
