/*
 * Minimal Windows stub for tty functions.
 * Original tty.c requires POSIX headers (termios.h, sys/ioctl.h, etc.).
 * This stub provides no-op implementations for Windows.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "tty.h"

SIXELSTATUS
sixel_tty_wait_stdin(int usec)
{
    (void)usec;
    return SIXEL_FALSE;
}

SIXELSTATUS
sixel_tty_scroll(
    sixel_write_function f_write,
    int outfd,
    int height,
    int is_animation)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int nwrite;

    (void)height;
    (void)is_animation;

    nwrite = f_write("\033[H", 3, &outfd);
    if (nwrite < 0) {
        status = SIXEL_LIBC_ERROR;
        sixel_helper_set_additional_message(
            "sixel_tty_scroll: f_write() failed.");
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}
