/*
  This file is part of libmicrohttpd
  Copyright (C) 2014-2016 Karlson2k (Evgeny Grin)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

/**
 * @file microhttpd/mhd_sockets.c
 * @brief  Implementation for sockets functions
 * @author Karlson2k (Evgeny Grin)
 */

#include "mhd_sockets.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <fcntl.h>

#ifdef MHD_WINSOCK_SOCKETS

/**
 * Return pointer to string description of specified WinSock error
 * @param err the WinSock error code.
 * @return pointer to string description of specified WinSock error.
 */
const char* MHD_W32_strerror_winsock_(int err)
{
  switch (err)
    {
  case 0:
    return "No error";
  case WSA_INVALID_HANDLE:
    return "Specified event object handle is invalid";
  case WSA_NOT_ENOUGH_MEMORY:
    return "Insufficient memory available";
  case WSA_INVALID_PARAMETER:
    return "One or more parameters are invalid";
  case WSA_OPERATION_ABORTED:
    return "Overlapped operation aborted";
  case WSA_IO_INCOMPLETE:
    return "Overlapped I/O event object not in signaled state";
  case WSA_IO_PENDING:
    return "Overlapped operations will complete later";
  case WSAEINTR:
    return "Interrupted function call";
  case WSAEBADF:
    return "File handle is not valid";
  case WSAEACCES:
    return "Permission denied";
  case WSAEFAULT:
    return "Bad address";
  case WSAEINVAL:
    return "Invalid argument";
  case WSAEMFILE:
    return "Too many open files";
  case WSAEWOULDBLOCK:
    return "Resource temporarily unavailable";
  case WSAEINPROGRESS:
    return "Operation now in progress";
  case WSAEALREADY:
    return "Operation already in progress";
  case WSAENOTSOCK:
    return "Socket operation on nonsocket";
  case WSAEDESTADDRREQ:
    return "Destination address required";
  case WSAEMSGSIZE:
    return "Message too long";
  case WSAEPROTOTYPE:
    return "Protocol wrong type for socket";
  case WSAENOPROTOOPT:
    return "Bad protocol option";
  case WSAEPROTONOSUPPORT:
    return "Protocol not supported";
  case WSAESOCKTNOSUPPORT:
    return "Socket type not supported";
  case WSAEOPNOTSUPP:
    return "Operation not supported";
  case WSAEPFNOSUPPORT:
    return "Protocol family not supported";
  case WSAEAFNOSUPPORT:
    return "Address family not supported by protocol family";
  case WSAEADDRINUSE:
    return "Address already in use";
  case WSAEADDRNOTAVAIL:
    return "Cannot assign requested address";
  case WSAENETDOWN:
    return "Network is down";
  case WSAENETUNREACH:
    return "Network is unreachable";
  case WSAENETRESET:
    return "Network dropped connection on reset";
  case WSAECONNABORTED:
    return "Software caused connection abort";
  case WSAECONNRESET:
    return "Connection reset by peer";
  case WSAENOBUFS:
    return "No buffer space available";
  case WSAEISCONN:
    return "Socket is already connected";
  case WSAENOTCONN:
    return "Socket is not connected";
  case WSAESHUTDOWN:
    return "Cannot send after socket shutdown";
  case WSAETOOMANYREFS:
    return "Too many references";
  case WSAETIMEDOUT:
    return "Connection timed out";
  case WSAECONNREFUSED:
    return "Connection refused";
  case WSAELOOP:
    return "Cannot translate name";
  case WSAENAMETOOLONG:
    return "Name too long";
  case WSAEHOSTDOWN:
    return "Host is down";
  case WSAEHOSTUNREACH:
    return "No route to host";
  case WSAENOTEMPTY:
    return "Directory not empty";
  case WSAEPROCLIM:
    return "Too many processes";
  case WSAEUSERS:
    return "User quota exceeded";
  case WSAEDQUOT:
    return "Disk quota exceeded";
  case WSAESTALE:
    return "Stale file handle reference";
  case WSAEREMOTE:
    return "Item is remote";
  case WSASYSNOTREADY:
    return "Network subsystem is unavailable";
  case WSAVERNOTSUPPORTED:
    return "Winsock.dll version out of range";
  case WSANOTINITIALISED:
    return "Successful WSAStartup not yet performed";
  case WSAEDISCON:
    return "Graceful shutdown in progress";
  case WSAENOMORE:
    return "No more results";
  case WSAECANCELLED:
    return "Call has been canceled";
  case WSAEINVALIDPROCTABLE:
    return "Procedure call table is invalid";
  case WSAEINVALIDPROVIDER:
    return "Service provider is invalid";
  case WSAEPROVIDERFAILEDINIT:
    return "Service provider failed to initialize";
  case WSASYSCALLFAILURE:
    return "System call failure";
  case WSASERVICE_NOT_FOUND:
    return "Service not found";
  case WSATYPE_NOT_FOUND:
    return "Class type not found";
  case WSA_E_NO_MORE:
    return "No more results";
  case WSA_E_CANCELLED:
    return "Call was canceled";
  case WSAEREFUSED:
    return "Database query was refused";
  case WSAHOST_NOT_FOUND:
    return "Host not found";
  case WSATRY_AGAIN:
    return "Nonauthoritative host not found";
  case WSANO_RECOVERY:
    return "This is a nonrecoverable error";
  case WSANO_DATA:
    return "Valid name, no data record of requested type";
  case WSA_QOS_RECEIVERS:
    return "QoS receivers";
  case WSA_QOS_SENDERS:
    return "QoS senders";
  case WSA_QOS_NO_SENDERS:
    return "No QoS senders";
  case WSA_QOS_NO_RECEIVERS:
    return "QoS no receivers";
  case WSA_QOS_REQUEST_CONFIRMED:
    return "QoS request confirmed";
  case WSA_QOS_ADMISSION_FAILURE:
    return "QoS admission error";
  case WSA_QOS_POLICY_FAILURE:
    return "QoS policy failure";
  case WSA_QOS_BAD_STYLE:
    return "QoS bad style";
  case WSA_QOS_BAD_OBJECT:
    return "QoS bad object";
  case WSA_QOS_TRAFFIC_CTRL_ERROR:
    return "QoS traffic control error";
  case WSA_QOS_GENERIC_ERROR:
    return "QoS generic error";
  case WSA_QOS_ESERVICETYPE:
    return "QoS service type error";
  case WSA_QOS_EFLOWSPEC:
    return "QoS flowspec error";
  case WSA_QOS_EPROVSPECBUF:
    return "Invalid QoS provider buffer";
  case WSA_QOS_EFILTERSTYLE:
    return "Invalid QoS filter style";
  case WSA_QOS_EFILTERTYPE:
    return "Invalid QoS filter type";
  case WSA_QOS_EFILTERCOUNT:
    return "Incorrect QoS filter count";
  case WSA_QOS_EOBJLENGTH:
    return "Invalid QoS object length";
  case WSA_QOS_EFLOWCOUNT:
    return "Incorrect QoS flow count";
  case WSA_QOS_EUNKOWNPSOBJ:
    return "Unrecognized QoS object";
  case WSA_QOS_EPOLICYOBJ:
    return "Invalid QoS policy object";
  case WSA_QOS_EFLOWDESC:
    return "Invalid QoS flow descriptor";
  case WSA_QOS_EPSFLOWSPEC:
    return "Invalid QoS provider-specific flowspec";
  case WSA_QOS_EPSFILTERSPEC:
    return "Invalid QoS provider-specific filterspec";
  case WSA_QOS_ESDMODEOBJ:
    return "Invalid QoS shape discard mode object";
  case WSA_QOS_ESHAPERATEOBJ:
    return "Invalid QoS shaping rate object";
  case WSA_QOS_RESERVED_PETYPE:
    return "Reserved policy QoS element type";
    }
  return "Unknown winsock error";
}


/**
 * Create pair of mutually connected TCP/IP sockets on loopback address
 * @param sockets_pair array to receive resulted sockets
 * @param non_blk if set to non-zero value, sockets created in non-blocking mode
 *                otherwise sockets will be in blocking mode
 * @return non-zero if succeeded, zero otherwise
 */
int
MHD_W32_socket_pair_(SOCKET sockets_pair[2], int non_blk)
{
  int i;

  if (! sockets_pair)
    {
      WSASetLastError (WSAEFAULT);
      return 0;
    }

#define PAIRMAXTRYIES 800
  for (i = 0; i < PAIRMAXTRYIES; i++)
    {
      struct sockaddr_in listen_addr;
      SOCKET listen_s;
      static const int c_addinlen = sizeof(struct sockaddr_in); /* help compiler to optimize */
      int addr_len = c_addinlen;
      unsigned long on_val = 1;
      unsigned long off_val = 0;

      listen_s = socket (AF_INET,
                         SOCK_STREAM,
                         IPPROTO_TCP);
      if (INVALID_SOCKET == listen_s)
        break; /* can't create even single socket */

      listen_addr.sin_family = AF_INET;
      listen_addr.sin_port = 0; /* same as htons(0) */
      listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      if ( (0 == bind (listen_s,
                       (struct sockaddr*) &listen_addr,
                       c_addinlen) &&
            (0 == listen (listen_s,
                          1) ) &&
            (0 == getsockname (listen_s,
                               (struct sockaddr*) &listen_addr,
                               &addr_len))) )
        {
          SOCKET client_s = socket(AF_INET,
                                   SOCK_STREAM,
                                   IPPROTO_TCP);
          struct sockaddr_in accepted_from_addr;
          struct sockaddr_in client_addr;
          SOCKET server_s;

          if (INVALID_SOCKET == client_s)
            {
              /* try again */
              closesocket (listen_s);
              continue;
            }

          if ( (0 != ioctlsocket (client_s,
                                  FIONBIO,
                                  &on_val)) ||
               ( (0 != connect (client_s,
                                (struct sockaddr*) &listen_addr,
                                c_addinlen)) &&
                 (WSAGetLastError() != WSAEWOULDBLOCK)) )
            {
              /* try again */
              closesocket (listen_s);
              closesocket (client_s);
              continue;
            }

          addr_len = c_addinlen;
          server_s = accept (listen_s,
                             (struct sockaddr*) &accepted_from_addr,
                             &addr_len);
          if (INVALID_SOCKET == server_s)
            {
              /* try again */
              closesocket (listen_s);
              closesocket (client_s);
              continue;
            }

          addr_len = c_addinlen;
          if ( (0 == getsockname (client_s,
                                  (struct sockaddr*) &client_addr,
                                  &addr_len)) &&
               (accepted_from_addr.sin_family == client_addr.sin_family) &&
               (accepted_from_addr.sin_port == client_addr.sin_port) &&
               (accepted_from_addr.sin_addr.s_addr == client_addr.sin_addr.s_addr) &&
               ( (0 != non_blk) ?
                    (0 == ioctlsocket(server_s,
                                      FIONBIO,
                                      &on_val)) :
                    (0 == ioctlsocket(client_s,
                                      FIONBIO,
                                      &off_val)) ) )
            {
              closesocket (listen_s);
              sockets_pair[0] = server_s;
              sockets_pair[1] = client_s;
              return !0;
            }
          closesocket (server_s);
          closesocket (client_s);
        }
      closesocket(listen_s);
    }

  sockets_pair[0] = INVALID_SOCKET;
  sockets_pair[1] = INVALID_SOCKET;
  WSASetLastError(WSAECONNREFUSED);

  return 0;
}

#endif /* MHD_WINSOCK_SOCKETS */


/**
 * Add @a fd to the @a set.  If @a fd is
 * greater than @a max_fd, set @a max_fd to @a fd.
 *
 * @param fd file descriptor to add to the @a set
 * @param set set to modify
 * @param max_fd maximum value to potentially update
 * @param fd_setsize value of FD_SETSIZE
 * @return non-zero if succeeded, zero otherwise
 */
int
MHD_add_to_fd_set_ (MHD_socket fd,
                    fd_set *set,
                    MHD_socket *max_fd,
                    unsigned int fd_setsize)
{
  if ( (NULL == set) ||
       (MHD_INVALID_SOCKET == fd) )
    return 0;
  if (! MHD_SCKT_FD_FITS_FDSET_SETSIZE_ (fd,
                                         set,
                                         fd_setsize))
    return 0;
  MHD_SCKT_ADD_FD_TO_FDSET_SETSIZE_(fd,
                                    set,
                                    fd_setsize);
  if ( (NULL != max_fd) &&
       ( (fd > *max_fd) ||
         (MHD_INVALID_SOCKET == *max_fd) ) )
    *max_fd = fd;
  return ! 0;
}


/**
 * Change socket options to be non-blocking.
 *
 * @param sock socket to manipulate
 * @return non-zero if succeeded, zero otherwise
 */
int
MHD_socket_nonblocking_ (MHD_socket sock)
{
#if defined(MHD_POSIX_SOCKETS)
  int flags;

  flags = fcntl (sock,
                 F_GETFL);
  if (-1 == flags)
    return 0;

  if ( ((flags | O_NONBLOCK) != flags) &&
       (0 != fcntl (sock,
                    F_SETFL,
                    flags | O_NONBLOCK)) )
    return 0;
#elif defined(MHD_WINSOCK_SOCKETS)
  unsigned long flags = 1;

  if (0 != ioctlsocket (sock,
                        FIONBIO,
                        &flags))
    return 0;
#endif /* MHD_WINSOCK_SOCKETS */
  return !0;
}


/**
 * Change socket options to be non-inheritable.
 *
 * @param sock socket to manipulate
 * @return non-zero if succeeded, zero otherwise
 * @warning Does not set socket error on W32.
 */
int
MHD_socket_noninheritable_ (MHD_socket sock)
{
#if defined(MHD_POSIX_SOCKETS)
  int flags;

  flags = fcntl (sock,
                 F_GETFD);
  if (-1 == flags)
    return 0;

  if ( ((flags | FD_CLOEXEC) != flags) &&
       (0 != fcntl (sock,
                    F_SETFD,
                    flags | FD_CLOEXEC)) )
    return 0;
#elif defined(MHD_WINSOCK_SOCKETS)
  if (! SetHandleInformation ((HANDLE)sock,
                              HANDLE_FLAG_INHERIT,
                              0))
    return 0;
#endif /* MHD_WINSOCK_SOCKETS */
  return !0;
}


/**
 * Create a listen socket, with noninheritable flag if possible.
 *
 * @param use_ipv6 if set to non-zero IPv6 is used
 * @return created socket or MHD_INVALID_SOCKET in case of errors
 */
MHD_socket
MHD_socket_create_listen_ (int use_ipv6)
{
  int domain;
  MHD_socket fd;
  int cloexec_set;

#ifdef HAVE_INET6
  domain = (use_ipv6) ? PF_INET6 : PF_INET;
#else  /* ! HAVE_INET6 */
  if (use_ipv6)
    return MHD_INVALID_SOCKET;
  domain = PF_INET;
#endif /* ! HAVE_INET6 */

#if defined(MHD_POSIX_SOCKETS) && defined(SOCK_CLOEXEC)
  fd = socket (domain,
               SOCK_STREAM | SOCK_CLOEXEC,
               0);
  cloexec_set = !0;
#elif defined(MHD_WINSOCK_SOCKETS) && defined (WSA_FLAG_NO_HANDLE_INHERIT)
  fd = WSASocketW (domain,
                   SOCK_STREAM,
                   0,
                   NULL,
                   0,
                   WSA_FLAG_NO_HANDLE_INHERIT);
  cloexec_set = !0;
#else  /* !SOCK_CLOEXEC */
  fd = MHD_INVALID_SOCKET;
#endif /* !SOCK_CLOEXEC */
  if (MHD_INVALID_SOCKET == fd)
    {
      fd = socket (domain,
                   SOCK_STREAM,
                   0);
      cloexec_set = 0;
    }
  if (MHD_INVALID_SOCKET == fd)
    return MHD_INVALID_SOCKET;
#ifdef MHD_socket_nosignal_
  if(! MHD_socket_nosignal_(fd))
    {
      const int err = MHD_socket_get_error_ ();
      (void) MHD_socket_close_ (fd);
      MHD_socket_fset_error_ (err);
      return MHD_INVALID_SOCKET;
    }
#endif /* MHD_socket_nosignal_ */
  if (! cloexec_set)
    (void) MHD_socket_noninheritable_ (fd);

  return fd;
}
