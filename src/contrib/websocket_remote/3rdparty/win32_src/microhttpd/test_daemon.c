/*
     This file is part of libmicrohttpd
     Copyright (C) 2007, 2017 Christian Grothoff

     libmicrohttpd is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libmicrohttpd is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libmicrohttpd; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
*/

/**
 * @file test_daemon.c
 * @brief  Testcase for libmicrohttpd starts and stops
 * @author Christian Grothoff
 */

#include "platform.h"
#include "microhttpd.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef WINDOWS
#include <unistd.h>
#endif


static int
testStartError ()
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_ERROR_LOG, 0, NULL, NULL, NULL, NULL);
  if (NULL != d)
  {
    fprintf (stderr,
             "Succeeded at binding to port 0?\n");
    return 1;
  }
  return 0;
}


static int
apc_nothing (void *cls,
             const struct sockaddr *addr,
             socklen_t addrlen)
{
  return MHD_NO;
}


static int
apc_all (void *cls,
         const struct sockaddr *addr,
         socklen_t addrlen)
{
  return MHD_YES;
}


static int
ahc_nothing (void *cls,
             struct MHD_Connection *connection,
             const char *url,
             const char *method,
             const char *version,
             const char *upload_data, size_t *upload_data_size,
             void **unused)
{
  return MHD_NO;
}


static int
testStartStop ()
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG,
                        1080,
                        &apc_nothing, NULL,
                        &ahc_nothing, NULL,
                        MHD_OPTION_END);
  if (NULL == d)
  {
    fprintf (stderr,
             "Failed to start daemon on port %u\n",
             1080);
    exit (77);
  }
  MHD_stop_daemon (d);
  return 0;
}


static int
testExternalRun ()
{
  struct MHD_Daemon *d;
  fd_set rs;
  MHD_socket maxfd;
  int i;

  d = MHD_start_daemon (MHD_USE_ERROR_LOG,
                        1081,
                        &apc_all, NULL,
                        &ahc_nothing, NULL,
                        MHD_OPTION_END);

  if (NULL == d)
  {
    fprintf (stderr,
             "Failed to start daemon on port %u\n",
             1081);
    exit (77);
  }
  i = 0;
  while (i < 15)
    {
      maxfd = 0;
      FD_ZERO (&rs);
      if (MHD_YES != MHD_get_fdset (d, &rs, &rs, &rs, &maxfd))
        {
          MHD_stop_daemon (d);
          fprintf (stderr,
                   "Failed in MHD_get_fdset()\n");
          return 256;
        }
      if (MHD_run (d) == MHD_NO)
        {
          MHD_stop_daemon (d);
          fprintf (stderr,
                   "Failed in MHD_run()\n");
          return 8;
        }
      i++;
    }
  MHD_stop_daemon (d);
  return 0;
}


static int
testThread ()
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_ERROR_LOG | MHD_USE_INTERNAL_POLLING_THREAD,
                        1082,
                        &apc_all, NULL,
                        &ahc_nothing, NULL,
                        MHD_OPTION_END);

  if (NULL == d)
  {
    fprintf (stderr,
             "Failed to start daemon on port %u\n",
             1082);
    exit (77);
  }
  if (MHD_run (d) != MHD_NO)
    {
      fprintf (stderr,
               "Failed in MHD_run()\n");
      return 32;
    }
  MHD_stop_daemon (d);
  return 0;
}


static int
testMultithread ()
{
  struct MHD_Daemon *d;

  d = MHD_start_daemon (MHD_USE_ERROR_LOG | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION,
                        1083,
                        &apc_all, NULL,
                        &ahc_nothing, NULL,
                        MHD_OPTION_END);

  if (NULL == d)
  {
    fprintf (stderr,
             "Failed to start daemon on port %u\n",
             1083);
    exit (77);
  }
  if (MHD_run (d) != MHD_NO)
    {
      fprintf (stderr,
               "Failed in MHD_run()\n");
      return 128;
    }
  MHD_stop_daemon (d);
  return 0;
}


int
main (int argc,
      char *const *argv)
{
  int errorCount = 0;

  errorCount += testStartError ();
  errorCount += testStartStop ();
  errorCount += testExternalRun ();
  errorCount += testThread ();
  errorCount += testMultithread ();
  if (0 != errorCount)
    fprintf (stderr,
             "Error (code: %u)\n",
             errorCount);
  return 0 != errorCount;       /* 0 == pass */
}
