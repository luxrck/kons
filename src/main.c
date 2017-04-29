#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#include <pty.h>

#include <vt.h>
#include <font.h>


#define _GNU_SOURCE


static struct vt vt = { 0 };


void fatal(const char *errstr, ...) {
	va_list ap;
	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}


void onexit(int signo) {
  vt_destroy(&vt);
}


int main(int argc, char *argv[]) {
  int amaster = 0; pid_t child = 0;

  if ((child = forkpty(&amaster, NULL, NULL, NULL))) {
    struct sigaction act = { 0 };
    act.sa_handler = onexit;

    sigaction(SIGINT,  &act, 0);
    sigaction(SIGTERM, &act, 0);
    sigaction(SIGQUIT, &act, 0);

    options_init(NULL);
    font_init();

    vt_init(&vt, OUTPUT_BACKEND_DRM, amaster);

    vt.run(&vt);
		// vt.output->updateCursor(vt.output, 2, 2);
		// sleep(5);

    font_destroy();
    vt_destroy(&vt);
  } else {
    char *sh = getenv("SHELL");
    if (!sh) sh = "/bin/bash";
    char *args[] = { sh, "-i", NULL };
    execvp(args[0], args);
    exit(0);
  }

  return 0;
}
