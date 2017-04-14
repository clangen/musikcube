/*
     This file is part of libmicrohttpd
     Copyright (C) 2016 Karlson2k (Evgeny Grin)

     libmicrohttpd is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
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
 * @file microhttpd/test_shutdown_select.c
 * @brief  Test whether shutdown socket triggers select()/poll()
 * @details On some platforms shutting down the socket in one thread
 *          trigger select() or poll() waiting for this socket in
 *          other thread. libmicrohttpd depend on this behavior on
 *          those platforms. This program check whether select()
 *          and poll() (if available) works as expected.
 * @author Karlson2k (Evgeny Grin)
 */

#include "MHD_config.h"
#include "platform.h"
#include "mhd_sockets.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_TIME_H
#include <time.h>
#endif /* HAVE_TIME_H */

#if defined(MHD_USE_POSIX_THREADS)
#include <pthread.h>
#endif /* MHD_USE_POSIX_THREADS */

#if defined(MHD_WINSOCK_SOCKETS)
#include <winsock2.h>
#include <windows.h>
#define sock_errno (WSAGetLastError())
#elif defined(MHD_POSIX_SOCKETS)
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */
#if defined(HAVE_POLL) && defined(HAVE_POLL_H)
#include <poll.h>
#endif /* HAVE_POLL && HAVE_POLL_H */
#define sock_errno (errno)
#endif /* MHD_POSIX_SOCKETS */

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif /* HAVE_STDBOOL_H */

#include "mhd_threads.h"

#ifndef SOMAXCONN
#define SOMAXCONN 511
#endif /* ! SOMAXCONN */

#if !defined(SHUT_RDWR) && defined(SD_BOTH)
#define SHUT_RDWR SD_BOTH
#endif

static bool check_err;


static bool
has_in_name(const char *prog_name, const char *marker)
{
  size_t name_pos;
  size_t pos;

  if (!prog_name || !marker)
    return 0;

  pos = 0;
  name_pos = 0;
  while (prog_name[pos])
    {
      if ('/' == prog_name[pos])
        name_pos = pos + 1;
#ifdef _WIN32
      else if ('\\' == prog_name[pos])
        name_pos = pos + 1;
#endif /* _WIN32 */
      pos++;
    }
  if (name_pos == pos)
    return !0;
  return strstr(prog_name + name_pos, marker) != NULL;
}

static MHD_socket
start_socket_listen(int domain)
{
/* Create sockets similarly to daemon.c */
  MHD_socket fd;
  int cloexec_set;
  struct sockaddr_in sock_addr;
  socklen_t addrlen;

#ifdef MHD_WINSOCK_SOCKETS
  unsigned long flags = 1;
#else  /* MHD_POSIX_SOCKETS */
  int flags;
#endif /* MHD_POSIX_SOCKETS */

#if defined(MHD_POSIX_SOCKETS) && defined(SOCK_CLOEXEC)
  fd = socket (domain, SOCK_STREAM | SOCK_CLOEXEC, 0);
  cloexec_set = 1;
#elif defined(MHD_WINSOCK_SOCKETS) && defined (WSA_FLAG_NO_HANDLE_INHERIT)
  fd = WSASocketW (domain, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);
  cloexec_set = 1;
#else  /* !SOCK_CLOEXEC */
  fd = socket (domain, SOCK_STREAM, 0);
  cloexec_set = 0;
#endif /* !SOCK_CLOEXEC */
  if ( (MHD_INVALID_SOCKET == fd) && (cloexec_set) )
    {
      fd = socket (domain, SOCK_STREAM, 0);
      cloexec_set = 0;
    }
  if (MHD_INVALID_SOCKET == fd)
    {
      fprintf (stderr, "Can't create socket: %u\n",
               (unsigned)sock_errno);
      return MHD_INVALID_SOCKET;
    }

  if (!cloexec_set)
    {
#ifdef MHD_WINSOCK_SOCKETS
    if (!SetHandleInformation ((HANDLE)fd, HANDLE_FLAG_INHERIT, 0))
      fprintf (stderr, "Failed to make socket non-inheritable: %u\n",
               (unsigned int)GetLastError ());
#else  /* MHD_POSIX_SOCKETS */
    flags = fcntl (fd, F_GETFD);
    if ( ( (-1 == flags) ||
           ( (flags != (flags | FD_CLOEXEC)) &&
             (0 != fcntl (fd, F_SETFD, flags | FD_CLOEXEC)) ) ) )
      fprintf (stderr, "Failed to make socket non-inheritable: %s\n",
               MHD_socket_last_strerr_ ());
#endif /* MHD_POSIX_SOCKETS */
    }

  memset (&sock_addr, 0, sizeof (struct sockaddr_in));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons (0);
#if HAVE_SOCKADDR_IN_SIN_LEN
  sock_addr.sin_len = sizeof (struct sockaddr_in);
#endif
  addrlen = sizeof (struct sockaddr_in);

  if (bind (fd, (const struct sockaddr*) &sock_addr, addrlen) < 0)
    {
      fprintf (stderr, "Failed to bind socket: %u\n",
               (unsigned)sock_errno);
      MHD_socket_close_chk_ (fd);
      return MHD_INVALID_SOCKET;
    }

#ifdef MHD_WINSOCK_SOCKETS
  if (0 != ioctlsocket (fd, FIONBIO, &flags))
    {
      fprintf (stderr, "Failed to make socket non-blocking: %u\n",
               (unsigned)sock_errno);
      MHD_socket_close_chk_ (fd);
      return MHD_INVALID_SOCKET;
    }
#else  /* MHD_POSIX_SOCKETS */
  flags = fcntl (fd, F_GETFL);
  if ( ( (-1 == flags) ||
         ( (flags != (flags | O_NONBLOCK)) &&
           (0 != fcntl (fd, F_SETFL, flags | O_NONBLOCK)) ) ) )
    {
      fprintf (stderr, "Failed to make socket non-blocking: %s\n",
              MHD_socket_last_strerr_ ());
      MHD_socket_close_chk_ (fd);
      return MHD_INVALID_SOCKET;
    }
#endif /* MHD_POSIX_SOCKETS */

  if (listen(fd, SOMAXCONN) < 0)
    {
      fprintf (stderr, "Failed to listen on socket: %u\n",
               (unsigned)sock_errno);
      MHD_socket_close_chk_ (fd);
      return MHD_INVALID_SOCKET;
    }

  return fd;
}


MHD_THRD_RTRN_TYPE_ MHD_THRD_CALL_SPEC_
select_thread(void* data)
{
  /* use select() like in daemon.c */
  MHD_socket listen_sock = *((MHD_socket*)data);
  fd_set rs, ws;
  struct timeval timeout;

  FD_ZERO(&rs);
  FD_ZERO(&ws);
  FD_SET(listen_sock, &rs);
  timeout.tv_usec = 0;
  timeout.tv_sec = 7;

  check_err = (0 > MHD_SYS_select_(listen_sock + 1, &rs, &ws, NULL, &timeout));

  return (MHD_THRD_RTRN_TYPE_)0;
}


#ifdef HAVE_POLL
MHD_THRD_RTRN_TYPE_ MHD_THRD_CALL_SPEC_
poll_thread(void* data)
{
  /* use poll() like in daemon.c */
  struct pollfd p[1];
  MHD_socket listen_sock = *((MHD_socket*)data);

  p[0].fd = listen_sock;
  p[0].events = POLLIN;
  p[0].revents = 0;

  check_err = (0 > MHD_sys_poll_ (p, 1, 7000));

  return (MHD_THRD_RTRN_TYPE_)0;
}
#endif /* HAVE_POLL */


static void
local_sleep(unsigned seconds)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
  Sleep(seconds * 1000);
#else
  unsigned seconds_left = seconds;
  do
    {
      seconds_left = sleep(seconds_left);
    } while (seconds_left > 0);
#endif
}


int
main (int argc, char *const *argv)
{
  int i;
  time_t start_t, end_t;
  int result = 0;
  MHD_THRD_RTRN_TYPE_ (MHD_THRD_CALL_SPEC_ *test_func)(void* data);
#ifdef MHD_WINSOCK_SOCKETS
  WORD ver_req;
  WSADATA wsa_data;
  int err;
#endif /* MHD_WINSOCK_SOCKETS */
  bool test_poll;

  test_poll = has_in_name(argv[0], "_poll");
  if (!test_poll)
    test_func = &select_thread;
  else
    {
#ifndef HAVE_POLL
      return 77;
#else  /* ! HAVE_POLL */
      test_func = &poll_thread;
#endif /* ! HAVE_POLL */
    }

#ifdef MHD_WINSOCK_SOCKETS
  ver_req = MAKEWORD(2, 2);

  err = WSAStartup(ver_req, &wsa_data);
  if (err != 0 || MAKEWORD(2, 2) != wsa_data.wVersion)
    {
      printf("WSAStartup() failed\n");
      WSACleanup();
      return 99;
    }
#endif /* MHD_WINSOCK_SOCKETS */

  /* try several times to ensure that accidental incoming connection
   * didn't interfere with test results
   */
  for (i = 0; i < 5 && result == 0; i++)
    {
      MHD_thread_handle_ sel_thrd;
      /* fprintf(stdout, "Creating, binding and listening socket...\n"); */
      MHD_socket listen_socket = start_socket_listen (AF_INET);
      if (MHD_INVALID_SOCKET == listen_socket)
        return 99;

      check_err = !0;
      /* fprintf (stdout, "Starting select() thread...\n"); */
#if defined(MHD_USE_POSIX_THREADS)
      if (0 != pthread_create (&sel_thrd, NULL, test_func, &listen_socket))
        {
          MHD_socket_close_chk_ (listen_socket);
          fprintf (stderr, "Can't start thread\n");
          return 99;
        }
#elif defined(MHD_USE_W32_THREADS)
      sel_thrd = (HANDLE)_beginthreadex (NULL, 0, test_func, &listen_socket, 0, NULL);
      if (0 == (sel_thrd))
        {
          MHD_socket_close_chk_ (listen_socket);
          fprintf (stderr, "Can't start select() thread\n");
          return 99;
        }
#else
#error No threading lib available
#endif
      /* fprintf (stdout, "Waiting...\n"); */
      local_sleep(1); /* make sure that select() is started */

      /* fprintf (stdout, "Shutting down socket...\n"); */
      start_t = time (NULL);
      shutdown (listen_socket, SHUT_RDWR);

      /* fprintf (stdout, "Waiting for thread to finish...\n"); */
      if (!MHD_join_thread_(sel_thrd))
        {
          MHD_socket_close_chk_(listen_socket);
          fprintf (stderr, "Can't join select() thread\n");
          return 99;
        }
      if (check_err)
        {
          MHD_socket_close_chk_(listen_socket);
          fprintf (stderr, "Error in waiting thread\n");
          return 99;
        }
      end_t = time (NULL);
      /* fprintf (stdout, "Thread finished.\n"); */
      MHD_socket_close_chk_(listen_socket);

      if (start_t == (time_t)-1 || end_t == (time_t)-1)
        {
          MHD_socket_close_chk_(listen_socket);
          fprintf (stderr, "Can't get current time\n");
          return 99;
        }
      if (end_t - start_t > 3)
        result++;
    }

#ifdef MHD_WINSOCK_SOCKETS
  WSACleanup();
#endif /* MHD_WINSOCK_SOCKETS */

  return result;
}
