#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <pty.h>

#include <vt.h>
#include <font.h>


#define _GNU_SOURCE


static struct vt vt = { 0 };


void onexit(int signo) {
  vt_destroy(&vt);
}


int main(int argc, char *argv[]) {
  int amaster = 0; pid_t child = 0;

  if ((child = forkpty(&amaster, NULL, NULL, NULL))) {
    struct termios tios;
    tcgetattr(amaster, &tios);
    tios.c_lflag &= ~(ECHO | ECHONL);
    tcsetattr(amaster, TCSAFLUSH, &tios);

    struct sigaction act = { 0 };
    act.sa_handler = onexit;

    sigaction(SIGINT,  &act, 0);
    sigaction(SIGTERM, &act, 0);
    sigaction(SIGQUIT, &act, 0);

    options_init(NULL);
    font_init();

    vt_init(&vt, OUTPUT_BACKEND_DRM);

    struct pollfd pfds[2] = { { .fd = STDIN_FILENO, .events = POLLIN }, { .fd = amaster, .events = POLLIN } };
    struct text t = { 0 };
    int r = 0;
    while (1) {
      char input = 0, output = 0; int e = 0;

      r = poll(pfds, 2, -1);

      if (r < 0 && errno != EINTR) break;
      if (pfds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) break;

      if (pfds[1].revents & POLLIN) {
        // read(pfds[1].fd, &output, 1);
        // printf("%c", output);
        vt.parseInput(&vt, pfds[1].fd, &t);
      }

      if (pfds[0].revents & POLLIN) {
        if (!(r = read(pfds[0].fd, &input, 1))) break;
        write(pfds[1].fd, &input, 1);
        // struct text t = { .code = input, .fg = options.fg, .bg = options.bg };
        // vt.output->drawText(vt.output, &t, -1, -1);
      }
    }

    font_destroy();
    vt_destroy(&vt);
  } else {
    setsid();
    char *sh = getenv("SHELL");
    if (!sh) sh = "/bin/bash";
    char *args[] = { sh, "-i", NULL };
    execvp(args[0], args);
    exit(0);
  }

  // sleep(3);

  return 0;
}
