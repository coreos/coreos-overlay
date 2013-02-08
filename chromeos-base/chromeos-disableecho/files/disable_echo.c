/*
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  int ret, fd;
  struct termios t;

  fd = open("/dev/tty1", O_RDWR);
  if (fd < 0)
    return fd;

  ret = tcgetattr(fd, &t);
  if (ret)
    goto end;

  /* Disable ECHO and other trouble makers on this console. */
  t.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
  ret = tcsetattr(fd, TCSANOW, &t);
  if (ret)
    goto end;

  pause(); /* Never gonna give you up, never gonna let you down. */

end:
  close(fd);

  return ret;
}
