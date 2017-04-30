// http://vt100.net/emu/dec_ansi_parser
#include <poll.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>

#include <vt.h>
#include <font.h>


#define STATE(sa) (sa >> 4)
#define ACTION(sa) (sa & 15)
#define STATEACTION(s, a) ((s << 4) | a)
#define SETDEFAULT(a, b) (a) = (a) ? (a) : (b)


static int TAB_WIDTH = 4;


typedef enum {
  VTPST_ANYWHERE = 0,
  VTPST_GROUND,

  VTPST_ESC,
  VTPST_ESC_INTERMEDIATE,

  VTPST_CSI_ENTRY,
  VTPST_CSI_PARAM,
  VTPST_CSI_INTERMEDIATE,
  VTPST_CSI_IGNORE,

  VTPST_DCS_ENTRY,
  VTPST_DCS_PARAM,
  VTPST_DCS_INTERMEDIATE,
  VTPST_DCS_PASSTHROUGH,
  VTPST_DCS_IGNORE,

  VTPST_OSC_STRING,
  VTPST_SOS_STRING,
} vtpst_t;


static char *vtpst_tb[] = {
  "VTPST_ANYWHERE",
  "VTPST_GROUND",
  "VTPST_ESC",
  "VTPST_ESC_INTERMEDIATE",
  "VTPST_CSI_ENTRY",
  "VTPST_CSI_PARAM",
  "VTPST_CSI_INTERMEDIATE",
  "VTPST_CSI_IGNORE",
  "VTPST_DCS_ENTRY",
  "VTPST_DCS_PARAM",
  "VTPST_DCS_INTERMEDIATE",
  "VTPST_DCS_PASSTHROUGH",
  "VTPST_DCS_IGNORE",
  "VTPST_OSC_STRING",
  "VTPST_SOS_STRING",
};


typedef enum {
  VTPAC_CLEAR = 1,
  VTPAC_COLLECT,
  VTPAC_CSI_DISPATCH,
  VTPAC_ESC_DISPATCH,
  VTPAC_EXECUTE,
  VTPAC_HOOK,
  VTPAC_IGNORE,
  VTPAC_OSC_END,
  VTPAC_OSC_PUT,
  VTPAC_OSC_START,
  VTPAC_PARAM,
  VTPAC_PRINT,
  VTPAC_PUT,
  VTPAC_UNHOOK,
} vtpac_t;


static char *vtpac_tb[] = {
  "VTPAC_INVALID",
  "VTPAC_CLEAR",
  "VTPAC_COLLECT",
  "VTPAC_CSI_DISPATCH",
  "VTPAC_ESC_DISPATCH",
  "VTPAC_EXECUTE",
  "VTPAC_HOOK",
  "VTPAC_IGNORE",
  "VTPAC_OSC_END",
  "VTPAC_OSC_PUT",
  "VTPAC_OSC_START",
  "VTPAC_PARAM",
  "VTPAC_PRINT",
  "VTPAC_PUT",
  "VTPAC_UNHOOK",
};


typedef int (*vtpac_handler_t) (struct vt *vt, struct text *t, vtpac_t action);

/*
 * vt parser action handlers
 */
static int vtpac_handle_print(struct vt *vt, struct text *t, vtpac_t action) {
  t->fg = vt->fg; t->bg = vt->bg; t->attrs = vt->attrs;
  vt->output->drawText(vt->output, t);
  return 1;
}

static int vtpac_handle_execute(struct vt *vt, struct text *t, vtpac_t action) {
  rune c = t->code;
  int32_t x = vt->c.x, y = vt->c.y;
  switch (c) {
    case '\a':  // 0x07 BEL
      return 1;
    case '\b':  // 0x08 BS
      if (vt->c.x) x--;
      break;
    case '\t':  // 0x09
      x = ((x + 1) / TAB_WIDTH + 1) * TAB_WIDTH;
      if (x >= vt->cols) x = vt->cols - 1;
      break;
    // case '\r':
    case '\n':  // 0x0a
      // printf("vtpac_handle_execute: %x\n", c);
      x = 0;
      y++;
      break;
    case '\v':  // 0x0b
    case '\f':  // 0x0c
      y++;
      break;
    case '\r':  // 0x0d
      x = 0;
      break;
    case ' ':   // 0x20
      x++;
      break;
    default:
      return 0;
  }
  vt->output->updateCursor(vt->output, y, x, OUTPUT_CURSOR_REDRAW);
  return 1;
}

static int vtpac_handle_esc_dispatch(struct vt *vt, struct text *t, vtpac_t action) {
  rune c = t->code;
  int32_t x = vt->c.x, y = vt->c.y;

  switch (vt->intermediate_chars[0]) {
  case '(':
    if (!vt->collect_ignored) {
      t->code = CHARSETS[vt->intermediate_chars[0]][c];
      if (!t->code) t->code = c;
    }
    break;
  case '#':
    switch (c) {
    case '8': {
      struct text tt = { .code = 'E', .fg = options.fg, .bg = options.bg, .attrs = 0 };
      int32_t cx = vt->c.x, cy = vt->c.y;
      vt->c.x = vt->c.y = 0;
      for (int y = 0; y < vt->output->rows; y++)
        for (int x = 0; x < vt->output->cols; x++)
          vt->output->drawText(vt->output, &tt);
      vt->c.x = cx; vt->c.y = cy;
      break;
      }
    }
    break;
  }

  switch (c) {
  case 'c':
    vt->output->clear(vt->output, 0, 0, -1, -1);
    x = 0; y = 0;
    break;
  case 'D':
    y++; break;
  case 'E':
    x = 0; y++;
    break;
  case 'M':
    y--; break;
  default: return 0;
  }
  vt->output->updateCursor(vt->output, y, x, OUTPUT_CURSOR_REDRAW);
  return 0;
}

static void sgr_handler(struct vt *vt, int param) {
  switch (param) {
  case 0:
    vt->attrs &= ~(ATTR_BOLD | ATTR_UNDERLINE | ATTR_REVERSE);
    vt->fg = options.fg;
    vt->bg = options.bg;
    // printf("vt reset......\r\n");
    break;
  case 1: vt->attrs |= ATTR_BOLD; break;
  case 4: vt->attrs |= ATTR_UNDERLINE; break;
  case 7: vt->attrs |= ATTR_REVERSE; break;
  case 22: vt->attrs &= ~ATTR_BOLD; break;
  case 24: vt->attrs &= ~ATTR_UNDERLINE; break;
  case 27: vt->attrs &= ~ATTR_REVERSE; break;
  case 38:
    vt->attrs |= ATTR_UNDERLINE;
    vt->fg = options.fg;
    break;
  case 39:
    vt->attrs &= ~ATTR_UNDERLINE;
    vt->fg = options.fg;
    // printf("+++++: %d\n", param, options.fg);
    break;
  case 49:
    vt->bg = options.bg;
    break;
  default:
    if (param >= 30 && param <= 37)
      vt->fg = param - 30; //colortb[vt->attrs & ATTR_BOLD][param - 30];
    else if (param >= 40 && param <= 47)
      vt->bg = param - 40; //colortb[vt->attrs & ATTR_BOLD][param - 40];
    break;
  }
  // printf("vt: %x %x %x %d\n", vt->fg, vt->bg, vt->attrs, param);
}

static int vtpac_handle_csi_dispatch(struct vt *vt, struct text *t, vtpac_t action) {
  rune c = t->code;
  int32_t x = vt->c.x, y = vt->c.y;
  switch (c) {
    case 'A': { // CUU
      SETDEFAULT(vt->params[0], 1);
      y -= vt->params[0];
      break;
    }
    case 'B': { // CUD
      SETDEFAULT(vt->params[0], 1);
      y += vt->params[0];
      break;
    }
    case 'C': { // CUF
      SETDEFAULT(vt->params[0], 1);
      x += vt->params[0];
      break;
    }
    case 'D': { // CUB
      SETDEFAULT(vt->params[0], 1);
      x -= vt->params[0];
      break;
    }
    case 'E': { // CNL
      SETDEFAULT(vt->params[0], 1);
      y += vt->params[0];
      x = 0;
      break;
    }
    case 'F': { // CPL
      SETDEFAULT(vt->params[0], 1);
      y -= vt->params[0];
      x = 0;
      break;
    }
    case 'G': { // CHA
      SETDEFAULT(vt->params[0], 0);
      x = vt->params[0];
      break;
    }
    case 'H': { // CUP
      SETDEFAULT(vt->params[0], 1);
      SETDEFAULT(vt->params[1], 1);
      y = vt->params[0] - 1;
      x = vt->params[1] - 1;
      break;
    }
    case 'J': { // ED
      // printf("clear screen: %d\r\n", vt->params[0]);
      switch (vt->params[0]) {
        case 0: // clear region: from cursor to end of screen.
          vt->output->clear(vt->output, vt->c.y, vt->c.x, vt->c.y, -1);
          // 操蛋的`man console_codes`的中文翻译！！！！！！
          if (vt->c.y < vt->output->rows - 1)
            vt->output->clear(vt->output, vt->c.y + 1, 0, -1, -1);
          break;
        case 1:
          vt->output->clear(vt->output, 0, 0, vt->c.y, vt->c.x); break;
        case 2:
        case 3: // ??? in tty, `clear` generate 3 ?
          vt->output->clear(vt->output, 0, 0, -1, -1); break;
      }
      return 1;
    }
    case 'K': { // EL
      switch (vt->params[0]) {
        case 0:
          vt->output->clear(vt->output, vt->c.y, vt->c.x, vt->c.y, -1); break;
        case 1:
          vt->output->clear(vt->output, vt->c.y, 0, vt->c.y, vt->c.x); break;
        case 2:
          vt->output->clear(vt->output, vt->c.y, 0, vt->c.y, -1); break;
      }
      return 1;
    }
    case 'd': { // VPA
      SETDEFAULT(vt->params[0], 1);
      y = vt->params[0] - 1;
      break;
    }
    case 's':
      vt->saved_c = vt->c; break;
    case 'u':
      x = vt->saved_c.x; y = vt->saved_c.y;
      break;
    case '`':  {  // HPA
      SETDEFAULT(vt->params[0], 1);
      x = vt->params[0] - 1;
      break;
    }
    case 'f': { // HVP
      SETDEFAULT(vt->params[0], 1);
      SETDEFAULT(vt->params[1], 1);
      y = vt->params[0] - 1;
      x = vt->params[1] - 1;
      break;
    }
    case 'l':   // NOTE: Accually 'l'(RM) is not the same as 'h'(SM).
    case 'h': { // SM
      switch (vt->params[0]) {
      case 1049:
      case 1047:
        vt->output->clear(vt->output, 0, 0, -1, -1);
        if (vt->params[0] == 1047) break;
      case 1048:
        vt->saved_c = vt->c; break;
      }
      return 1;
    }
    case 'm': {  // SGR
      // "\e[1mfuck\e[myou!"
      SETDEFAULT(vt->num_params, 1);
      // printf("csi:sgr:params: %d %d %d %d\r\n", vt->params[0], vt->params[1], vt->params[2], vt->params[3]);
      for (int i = 0; i < vt->num_params; i++)
        sgr_handler(vt, vt->params[i]);
      return 1;
      // printf("SGR: fg: %x, bg: %x, params: %d %d %d %d\n", vt->fg, vt->bg, vt->params[0], vt->params[1], vt->params[2], vt->params[3]);
    }
    case 'n': { // DSR
      switch (vt->params[0]) {
      case 5:
        write(vt->amaster, "\e[0n", 4); break;
      case 6: {
        char buffer[32] = { 0 };
        sprintf(buffer, "\e[%d;%dR", vt->c.y, vt->c.x);
        write(vt->amaster, buffer, strlen(buffer));
        }
      }
      return 1;
    }
  }
  vt->output->updateCursor(vt->output, y, x, OUTPUT_CURSOR_REDRAW);
  return 1;
}


static vtpac_handler_t vtpac_handlers[16] = {
  [VTPAC_PRINT] = vtpac_handle_print,
  [VTPAC_EXECUTE] = vtpac_handle_execute,
  [VTPAC_CSI_DISPATCH] = vtpac_handle_csi_dispatch,
  [VTPAC_ESC_DISPATCH] = vtpac_handle_esc_dispatch,
};

static uint8_t action_dispatch(struct vt *vt, struct text *t, vtpac_t action) {
  rune c = t->code;
  switch(action) {
    case VTPAC_PRINT:
    case VTPAC_EXECUTE:
    case VTPAC_HOOK:
    case VTPAC_PUT:
    case VTPAC_OSC_START:
    case VTPAC_OSC_PUT:
    case VTPAC_OSC_END:
    case VTPAC_UNHOOK:
    case VTPAC_CSI_DISPATCH:
    case VTPAC_ESC_DISPATCH:
      if (vtpac_handlers[action]) {
        return vtpac_handlers[action](vt, t, action);
      }
      break;

    case VTPAC_IGNORE:
        /* do nothing */
        break;

    case VTPAC_COLLECT:
      if(vt->num_intermediate_chars + 1 >= 3)
          vt->collect_ignored = 1;
      else
          vt->intermediate_chars[vt->num_intermediate_chars++] = t->code;
      break;

    case VTPAC_PARAM: {
      /* process the param character */
      if(c == ';') {
          vt->num_params += 1;
          vt->params[vt->num_params-1] = 0;
      } else {
          /* the character is a digit */
          int current_param;

          if(vt->num_params == 0) {
              vt->num_params = 1;
              vt->params[0]  = 0;
          }

          current_param = vt->num_params - 1;
          vt->params[current_param] *= 10;
          vt->params[current_param] += (c - '0');
      }

      break;
    }

    case VTPAC_CLEAR:
      // printf("vtpac-clear:-----\r\n");
      memset(vt->params, 0, 16 * 4);
      vt->intermediate_chars[0] = '\0';
      vt->num_params            = 0;
      vt->collect_ignored       = 0;
      break;
  }
  return 0;
}
static uint8_t state_dispatch(struct vt *vt, struct text *t, vtpst_t state) {
  uint8_t ns = state, na = 0;
  rune c = t->code;
  if (c >= 0xa0 && c <= 0xff) c -= 0x80;
  switch (state) {
  case VTPST_ANYWHERE:
    if (c == 0x18 || c == 0x1a || (c >= 0x80 && c <= 0x8f) || (c >= 0x91 && c <= 0x97) || c == 0x99 || c == 0x9a) {
      ns = VTPST_GROUND;
      na = VTPAC_EXECUTE;
    } else if (c == 0x9c) {
      ns = VTPST_GROUND;
    } else if (c == 0x98 || c == 0x9e || c == 0x9f) {
      ns = VTPST_SOS_STRING;
    } else if (c == 0x1b) {
      ns = VTPST_ESC;
    } else if (c == 0x90) {
      ns = VTPST_DCS_ENTRY;
    } else if (c == 0x9d) {
      ns = VTPST_OSC_STRING;
    } else if (c == 0x9b) {
      ns = VTPST_CSI_ENTRY;
    }
    break;

  case VTPST_GROUND:
    if ((c <= 0x17) || c == 0x19 || (c >= 0x1c && c <= 0x1f)) {
      na = VTPAC_EXECUTE;
    } else {
      na = VTPAC_PRINT;
    }
    break;

  case VTPST_SOS_STRING:
label_sos_string:
    if (c == 0x9c) {
      ns = VTPST_GROUND;
    } else if (c != 0x18 && c != 0x1a && c != 0x1b) {
      na = VTPAC_IGNORE;
    }
    break;

  case VTPST_ESC:
    switch (c) {
    case 0x58:
    case 0x5e:
    case 0x5f:
      ns = VTPST_SOS_STRING; break;
    case 0x50:
      ns = VTPST_DCS_ENTRY; break;
    case 0x5d:
      ns = VTPST_OSC_STRING; break;
    case 0x5b:
      ns = VTPST_CSI_ENTRY; break;
    default:
      if (c >= 0x20 && c <= 0x2f) {
        ns = VTPST_ESC_INTERMEDIATE;
        na = VTPAC_COLLECT;
      } else {
        ns = VTPST_GROUND;
        na = VTPAC_ESC_DISPATCH;
      }
      break;
    }
    break;
  case VTPST_ESC_INTERMEDIATE:
    if ((c <= 0x17) || c == 0x19 || (c >= 0x1c && c <=0x1f)) {
      na = VTPAC_EXECUTE;
    } else if (c >= 0x20 && c <= 0x2f) {
      na = VTPAC_COLLECT;
    } else if ((c >= 0x30 && c <= 0x7e)) {
      ns = VTPST_GROUND;
      na = VTPAC_ESC_DISPATCH;
    } else if (c == 0x7f) {
      na = VTPAC_IGNORE;
    }
    break;

  case VTPST_DCS_ENTRY:
    if ((c <= 0x17) || c == 0x19 || (c >= 0x1c && c <=0x1f) || c == 0x7f) {
      na = VTPAC_IGNORE;
    } else if (c >= 0x20 && c <= 0x2f) {
      ns = VTPST_DCS_INTERMEDIATE;
      na = VTPAC_COLLECT;
    } else if (c == 0x3a) {
      ns = VTPST_DCS_IGNORE;
    } else if ((c >= 0x30 && c <= 0x39) || c == 0x3b) {
      ns = VTPST_DCS_PARAM;
      na = VTPAC_PARAM;
    } else if (c >= 0x3c && c <= 0x3f) {
      ns = VTPST_DCS_PARAM;
      na = VTPAC_COLLECT;
    } else if (c >= 0x40) {
      ns = VTPST_DCS_PASSTHROUGH;
    }
    break;
  case VTPST_DCS_INTERMEDIATE:
    if ((c <= 0x17) || c == 0x19 || (c >= 0x1c && c <=0x1f)) {
      na = VTPAC_IGNORE;
    } else if (c >= 0x20 && c <= 0x2f) {
      na = VTPAC_COLLECT;
    } else if (c >= 0x30 && c <= 0x3f) {
      ns = VTPST_DCS_IGNORE;
    } else if (c == 0x7f) {
      na = VTPAC_IGNORE;
    } else if (c >= 0x40) {
      ns = VTPST_DCS_PASSTHROUGH;
    }
    break;
  case VTPST_DCS_IGNORE:
    goto label_sos_string;
  case VTPST_DCS_PARAM:
    if ((c <= 0x17) || c == 0x19 || (c >= 0x1c && c <=0x1f)) {
      na = VTPAC_IGNORE;
    } else if (c >= 0x20 && c <= 0x2f) {
      ns = VTPST_DCS_INTERMEDIATE;
      na = VTPAC_COLLECT;
    } else if ((c >= 0x30 && c <= 0x39) || c == 0x3b) {
      na = VTPAC_PARAM;
    } else if (c == 0x3a || (c >= 0x3c && c <= 0x3f)) {
      ns = VTPST_DCS_IGNORE;
    } else if (c == 0x7f) {
      na = VTPAC_IGNORE;
    } else if (c >= 0x40) {
      ns = VTPST_DCS_PASSTHROUGH;
    }
    break;
  case VTPST_DCS_PASSTHROUGH:
    if (c == 0x7f) {
      na = VTPAC_IGNORE;
    } else if (c == 0x9c) {
      ns = VTPST_GROUND;
    } else if (c != 0x18 && c != 0x1a && c != 0x1b) {
      na = VTPAC_PUT;
    }
    break;

  case VTPST_CSI_ENTRY:
    if ((c <= 0x17) || c == 0x19 || (c >= 0x1c && c <=0x1f)) {
      na = VTPAC_EXECUTE;
    } else if (c == 0x7f) {
      na = VTPAC_IGNORE;
    } else if (c >= 0x20 && c <= 0x2f) {
      ns = VTPST_CSI_INTERMEDIATE;
      na = VTPAC_COLLECT;
    } else if (c == 0x3a) {
      ns = VTPST_CSI_IGNORE;
    } else if ((c >= 0x30 && c <= 0x39) || c == 0x3b) {
      ns = VTPST_CSI_PARAM;
      na = VTPAC_PARAM;
    } else if (c >= 0x3c && c <= 0x3f) {
      ns = VTPST_CSI_PARAM;
      na = VTPAC_COLLECT;
    } else if (c >= 0x40) {
      ns = VTPST_GROUND;
      na = VTPAC_CSI_DISPATCH;
    }
    break;
  case VTPST_CSI_INTERMEDIATE:
    if ((c <= 0x17) || c == 0x19 || (c >= 0x1c && c <= 0x1f)) {
      na = VTPAC_EXECUTE;
    } else if (c >= 0x20 && c <= 0x2f) {
      na = VTPAC_COLLECT;
    } else if (c >= 0x30 && c <= 0x3f) {
      ns = VTPST_CSI_IGNORE;
    } else if (c == 0x7f) {
      na = VTPAC_IGNORE;
    } else if (c >= 0x40) {
      ns = VTPST_GROUND;
      na = VTPAC_CSI_DISPATCH;
    }
    break;
  case VTPST_CSI_IGNORE:
    if ((c <= 0x17) || c == 0x19 || (c >= 0x1c && c <= 0x1f)) {
      na = VTPAC_EXECUTE;
    } else if ((c >= 0x20 && c <= 0x3f) || c == 0x7f) {
      na = VTPAC_IGNORE;
    } else if (c >= 0x40) {
      ns = VTPST_GROUND;
    }
    break;
  case VTPST_CSI_PARAM:
    if ((c <= 0x17) || c == 0x19 || (c >= 0x1c && c <= 0x1f)) {
      na = VTPAC_EXECUTE;
    } else if (c >= 0x20 && c <= 0x2f) {
      ns = VTPST_CSI_INTERMEDIATE;
      na = VTPAC_COLLECT;
    } else if ((c >= 0x30 && c <= 0x39) || c == 0x3b) {
      na = VTPAC_PARAM;
    } else if (c == 0x3a || (c >= 0x3c && c <= 0x3f)) {
      ns = VTPST_CSI_IGNORE;
    } else if (c == 0x7f) {
      na = VTPAC_IGNORE;
    } else if (c >= 0x40) {
      ns = VTPST_GROUND;
      na = VTPAC_CSI_DISPATCH;
    }
    break;

  default: break;
  }
  return STATEACTION(ns, na);
}
static int vt_parse_c(struct vt *vt, struct text *t) {
  int result = 0; uint8_t sa = 0, state = 0, action = 0;

  sa = state_dispatch(vt, t, VTPST_ANYWHERE);
  if (!sa) sa = state_dispatch(vt, t, vt->state);
  state = STATE(sa); action = ACTION(sa);

  // printf("vtp:%c:%x %s %s\n", t->code, t->code, vtpac_tb[action], vtpst_tb[state]);
  // printf("%c", t->code);

  if (!state)
    return action_dispatch(vt, t, action);

  // exec exit action
  if (vt->state & VTPST_DCS_PASSTHROUGH) {
    action_dispatch(vt, t, VTPAC_UNHOOK);
  } else if (vt->state & VTPST_OSC_STRING) {
    action_dispatch(vt, t, VTPAC_OSC_END);
  }

  // exec action
  result = action_dispatch(vt, t, action);

  // exec entry action
  switch (state) {
    case VTPST_CSI_ENTRY:
    case VTPST_DCS_ENTRY:
    case VTPST_ESC:
      action_dispatch(vt, t, VTPAC_CLEAR); break;
    case VTPST_DCS_PASSTHROUGH:
      action_dispatch(vt, t, VTPAC_HOOK); break;
    case VTPST_OSC_STRING:
      action_dispatch(vt, t, VTPAC_OSC_START); break;
  }

  vt->state = state;
  return result;
}
int vt_parse_fp(struct vt *vt, int inputfp, struct text *t) {
  int r = 0; char c[5] = { 0 };
  // t->fg = options.fg; t->bg = options.bg;
  if ((r = u_getc(inputfp, c))) {
    t->code = u_rune(c, r);
    vt_parse_c(vt, t); return 1;
  }
  return 0;
}


void vt_destroy(struct vt *vt) {
  vt->output->destroy(vt->output);
  vt->c.destroy(&vt->c);
}


void vt_run(struct vt *vt) {
  struct termios tios;

  tcgetattr(vt->stdin, &tios);
  tios.c_lflag &= ~(ICANON | ISIG);
  tios.c_iflag |= IUTF8;
  tios.c_cc[VEOF] = 0;
  if (tcsetattr(vt->stdin, TCSAFLUSH, &tios) < 0) fatal("terminal set raw mode failed.\n");

  tcgetattr(vt->amaster, &tios);
  tios.c_iflag |= IUTF8 | IXON;
  if (tcsetattr(vt->amaster, TCSAFLUSH, &tios) < 0) fatal("terminal set raw mode failed.\n");

  struct pollfd pfds[2] = { { .fd = vt->stdin, .events = POLLIN }, { .fd = vt->amaster, .events = POLLIN } };
  struct text t = { 0 };
  int32_t r = 0;

  while (1) {
    char input = 0, output = 0; int e = 0;

    r = poll(pfds, 2, -1);

    if (r < 0 && errno != EINTR) break;
    if (pfds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) break;

    if (pfds[1].revents & POLLIN) {
      // read(pfds[1].fd, &output, 1);
      // printf("%c", output);
      vt_parse_fp(vt, pfds[1].fd, &t);
    }

    if (pfds[0].revents & POLLIN) {
      read(pfds[0].fd, &input, 1);
      write(pfds[1].fd, &input, 1);
    }
  }
}


void vt_init(struct vt *vt, int backend, int amaster) {
  // vt->parseInput = vt_parse_fp;
  // vt->parseChar = vt_parse_c;
  vt->run = vt_run;
  vt->destroy = vt_destroy;

  vt->output = outputs[backend];
  vt->output->init(vt->output, &vt->c);

  cursor_init(&vt->c, options.font_width, options.font_height);

  options.vt = vt;
  options.output = vt->output;

  vt->state = VTPST_GROUND;
  vt->num_intermediate_chars = 0;
  vt->collect_ignored = 0;
  memset(vt->params, 0, 16 * 4);
  vt->num_params = 0;
  vt->attrs = 0;
  vt->fg = options.fg;
  vt->bg = options.bg;

  vt->stdin = STDIN_FILENO;
  vt->amaster = amaster;

  struct winsize w = { vt->output->rows, vt->output->cols, 0, 0 };
  ioctl(amaster, TIOCSWINSZ, &w);
}
