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
 * @brief  Header for platform-independent sockets abstraction
 * @author Karlson2k (Evgeny Grin)
 *
 * Provides basic abstraction for sockets.
 * Any functions can be implemented as macro on some platforms
 * unless explicitly marked otherwise.
 * Any function argument can be skipped in macro, so avoid
 * variable modification in function parameters.
 */

#ifndef MHD_SOCKETS_H
#define MHD_SOCKETS_H 1
#include "mhd_options.h"

#include <errno.h>

#if !defined(MHD_POSIX_SOCKETS) && !defined(MHD_WINSOCK_SOCKETS)
#  if !defined(_WIN32) || defined(__CYGWIN__)
#    define MHD_POSIX_SOCKETS 1
#  else  /* defined(_WIN32) && !defined(__CYGWIN__) */
#    define MHD_WINSOCK_SOCKETS 1
#  endif /* defined(_WIN32) && !defined(__CYGWIN__) */
#endif /* !MHD_POSIX_SOCKETS && !MHD_WINSOCK_SOCKETS */

/*
 * MHD require headers that define socket type, socket basic functions
 * (socket(), accept(), listen(), bind(), send(), recv(), select()), socket
 * parameters like SOCK_CLOEXEC, SOCK_NONBLOCK, additional socket functions
 * (poll(), epoll(), accept4()), struct timeval and other types, required
 * for socket function.
 */
#if defined(MHD_POSIX_SOCKETS)
#  ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h> /* required on old platforms */
#  endif
#  ifdef HAVE_SYS_SOCKET_H
#    include <sys/socket.h>
#  endif
#  if defined(__VXWORKS__) || defined(__vxworks) || defined(OS_VXWORKS)
#    ifdef HAVE_SOCKLIB_H
#      include <sockLib.h>
#    endif /* HAVE_SOCKLIB_H */
#    ifdef HAVE_INETLIB_H
#      include <inetLib.h>
#    endif /* HAVE_INETLIB_H */
#    include <strings.h>  /* required for FD_SET (bzero() function) */
#  endif /* __VXWORKS__ || __vxworks || OS_VXWORKS */
#  ifdef HAVE_NETINET_IN_H
#    include <netinet/in.h>
#  endif /* HAVE_NETINET_IN_H */
#  ifdef HAVE_ARPA_INET_H
#    include <arpa/inet.h>
#  endif
#  ifdef HAVE_NET_IF_H
#    include <net/if.h>
#  endif
#  ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#  endif
#  ifdef HAVE_TIME_H
#    include <time.h>
#  endif
#  ifdef HAVE_NETDB_H
#    include <netdb.h>
#  endif
#  ifdef HAVE_SYS_SELECT_H
#    include <sys/select.h>
#  endif
#  ifdef EPOLL_SUPPORT
#    include <sys/epoll.h>
#  endif
#  ifdef HAVE_NETINET_TCP_H
     /* for TCP_FASTOPEN and TCP_CORK */
#    include <netinet/tcp.h>
#  endif
#  ifdef HAVE_STRING_H
#    include <string.h> /* for strerror() */
#  endif
#elif defined(MHD_WINSOCK_SOCKETS)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN 1
#  endif /* !WIN32_LEAN_AND_MEAN */
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif /* MHD_WINSOCK_SOCKETS */

#if defined(HAVE_POLL_H) && defined(HAVE_POLL)
#  include <poll.h>
#endif

#include <stddef.h>
#if defined(_MSC_FULL_VER) && !defined (_SSIZE_T_DEFINED)
#  include <stdint.h>
#  define _SSIZE_T_DEFINED
   typedef intptr_t ssize_t;
#endif /* !_SSIZE_T_DEFINED */

#include "mhd_limits.h"

#ifdef _MHD_FD_SETSIZE_IS_DEFAULT
#  define _MHD_SYS_DEFAULT_FD_SETSIZE FD_SETSIZE
#else  /* ! _MHD_FD_SETSIZE_IS_DEFAULT */
#  include "sysfdsetsize.h"
#  define _MHD_SYS_DEFAULT_FD_SETSIZE get_system_fdsetsize_value()
#endif /* ! _MHD_FD_SETSIZE_IS_DEFAULT */

#ifndef MHD_PANIC
#  include <stdio.h>
#  include <stdlib.h>
/* Simple implementation of MHD_PANIC, to be used outside lib */
#  define MHD_PANIC(msg) do { fprintf (stderr,           \
     "Abnormal termination at %d line in file %s: %s\n", \
     (int)__LINE__, __FILE__, msg); abort();} while(0)
#endif /* ! MHD_PANIC */

#ifndef MHD_SOCKET_DEFINED
/**
 * MHD_socket is type for socket FDs
 */
#  if defined(MHD_POSIX_SOCKETS)
     typedef int MHD_socket;
#    define MHD_INVALID_SOCKET (-1)
#  elif defined(MHD_WINSOCK_SOCKETS)
     typedef SOCKET MHD_socket;
#    define MHD_INVALID_SOCKET (INVALID_SOCKET)
#  endif /* MHD_WINSOCK_SOCKETS */

#  define MHD_SOCKET_DEFINED 1
#endif /* ! MHD_SOCKET_DEFINED */

#ifdef SOCK_CLOEXEC
#  define MAYBE_SOCK_CLOEXEC SOCK_CLOEXEC
#else  /* ! SOCK_CLOEXEC */
#  define MAYBE_SOCK_CLOEXEC 0
#endif /* ! SOCK_CLOEXEC */

#ifdef HAVE_SOCK_NONBLOCK
#  define MAYBE_SOCK_NONBLOCK SOCK_NONBLOCK
#else  /* ! HAVE_SOCK_NONBLOCK */
#  define MAYBE_SOCK_NONBLOCK 0
#endif /* ! HAVE_SOCK_NONBLOCK */

#ifdef MSG_NOSIGNAL
#  define MAYBE_MSG_NOSIGNAL MSG_NOSIGNAL
#else  /* ! MSG_NOSIGNAL */
#  define MAYBE_MSG_NOSIGNAL 0
#endif /* ! MSG_NOSIGNAL */

#if !defined(SHUT_WR) && defined(SD_SEND)
#  define SHUT_WR SD_SEND
#endif
#if !defined(SHUT_RD) && defined(SD_RECEIVE)
#  define SHUT_RD SD_RECEIVE
#endif
#if !defined(SHUT_RDWR) && defined(SD_BOTH)
#  define SHUT_RDWR SD_BOTH
#endif

#if HAVE_ACCEPT4+0 != 0 && (defined(HAVE_SOCK_NONBLOCK) || defined(SOCK_CLOEXEC))
#  define USE_ACCEPT4 1
#endif

#if defined(HAVE_EPOLL_CREATE1) && defined(EPOLL_CLOEXEC)
#  define USE_EPOLL_CREATE1 1
#endif /* HAVE_EPOLL_CREATE1 && EPOLL_CLOEXEC */

#ifdef TCP_FASTOPEN
/**
 * Default TCP fastopen queue size.
 */
#define MHD_TCP_FASTOPEN_QUEUE_SIZE_DEFAULT 10
#endif


/**
 * MHD_SCKT_OPT_BOOL_ is type for bool parameters for setsockopt()/getsockopt()
 */
#ifdef MHD_POSIX_SOCKETS
  typedef int MHD_SCKT_OPT_BOOL_;
#else /* MHD_WINSOCK_SOCKETS */
  typedef BOOL MHD_SCKT_OPT_BOOL_;
#endif /* MHD_WINSOCK_SOCKETS */

/**
 * MHD_SCKT_SEND_SIZE_ is type used to specify size for send and recv
 * functions
 */
#if !defined(MHD_WINSOCK_SOCKETS)
  typedef size_t MHD_SCKT_SEND_SIZE_;
#else
  typedef int MHD_SCKT_SEND_SIZE_;
#endif

/**
 * MHD_SCKT_SEND_MAX_SIZE_ is maximum send()/recv() size value.
 */
#if !defined(MHD_WINSOCK_SOCKETS)
#  define MHD_SCKT_SEND_MAX_SIZE_ SSIZE_MAX
#else
#  define MHD_SCKT_SEND_MAX_SIZE_ INT_MAX
#endif

/**
 * MHD_socket_close_(fd) close any FDs (non-W32) / close only socket
 * FDs (W32).  Note that on HP-UNIX, this function may leak the FD if
 * errno is set to EINTR.  Do not use HP-UNIX.
 *
 * @param fd descriptor to close
 * @return boolean true on success (error codes like EINTR and EIO are
 *         counted as success, only EBADF counts as an error!),
 *         boolean false otherwise.
 */
#if !defined(MHD_WINSOCK_SOCKETS)
#  define MHD_socket_close_(fd) ((0 == close((fd))) || (EBADF != errno))
#else
#  define MHD_socket_close_(fd) (0 == closesocket((fd)))
#endif

/**
 * MHD_socket_close_chk_(fd) close socket and abort execution
 * if error is detected.
 * @param fd socket to close
 */
#define MHD_socket_close_chk_(fd) do {        \
    if (!MHD_socket_close_(fd))               \
      MHD_PANIC(_("Close socket failed.\n")); \
  } while(0)


/**
 * MHD_send_ is wrapper for system's send()
 * @param s the socket to use
 * @param b the buffer with data to send
 * @param l the length of data in @a b
 * @return ssize_t type value
 */
#define MHD_send_(s,b,l) \
  ((ssize_t)send((s),(const void*)(b),((MHD_SCKT_SEND_SIZE_)l), MAYBE_MSG_NOSIGNAL))


/**
 * MHD_recv_ is wrapper for system's recv()
 * @param s the socket to use
 * @param b the buffer for data to receive
 * @param l the length of @a b
 * @return ssize_t type value
 */
#define MHD_recv_(s,b,l) \
  ((ssize_t)recv((s),(void*)(b),((MHD_SCKT_SEND_SIZE_)l), 0))


/**
 * Check whether FD can be added to fd_set with specified FD_SETSIZE.
 * @param fd   the fd to check
 * @param pset the pointer to fd_set to check or NULL to check
 *             whether FD can be used with fd_sets.
 * @param setsize the value of FD_SETSIZE.
 * @return boolean true if FD can be added to fd_set,
 *         boolean false otherwise.
 */
#if defined(MHD_POSIX_SOCKETS)
#  define MHD_SCKT_FD_FITS_FDSET_SETSIZE_(fd,pset,setsize) ((fd) < ((MHD_socket)setsize))
#elif defined(MHD_WINSOCK_SOCKETS)
#  define MHD_SCKT_FD_FITS_FDSET_SETSIZE_(fd,pset,setsize) ( ((void*)(pset)==(void*)0) || \
                                                             (((fd_set*)(pset))->fd_count < ((unsigned)setsize)) || \
                                                             (FD_ISSET((fd),(pset))) )
#endif

/**
 * Check whether FD can be added to fd_set with current FD_SETSIZE.
 * @param fd   the fd to check
 * @param pset the pointer to fd_set to check or NULL to check
 *             whether FD can be used with fd_sets.
 * @return boolean true if FD can be added to fd_set,
 *         boolean false otherwise.
 */
#define MHD_SCKT_FD_FITS_FDSET_(fd,pset) MHD_SCKT_FD_FITS_FDSET_SETSIZE_((fd),(pset),FD_SETSIZE)

/**
 * Add FD to fd_set with specified FD_SETSIZE.
 * @param fd   the fd to add
 * @param pset the valid pointer to fd_set.
 * @param setsize the value of FD_SETSIZE.
 * @note  To work on W32 with value of FD_SETSIZE different from currently defined value,
 *        system definition of FD_SET() is not used.
 */
#if defined(MHD_POSIX_SOCKETS)
#  define MHD_SCKT_ADD_FD_TO_FDSET_SETSIZE_(fd,pset,setsize) FD_SET((fd),(pset))
#elif defined(MHD_WINSOCK_SOCKETS)
#  define MHD_SCKT_ADD_FD_TO_FDSET_SETSIZE_(fd,pset,setsize)                     \
        do {                                                                     \
           u_int _i_ = 0;                                                        \
           fd_set* const _s_ = (fd_set*)(pset);                                  \
           while((_i_ < _s_->fd_count) && ((fd) != _s_->fd_array[_i_])) {++_i_;} \
           if ((_i_ == _s_->fd_count)) {_s_->fd_array[_s_->fd_count++] = (fd);}  \
        } while(0)
#endif

 /* MHD_SYS_select_ is wrapper macro for system select() function */
#if !defined(MHD_WINSOCK_SOCKETS)
#  define MHD_SYS_select_(n,r,w,e,t) select((n),(r),(w),(e),(t))
#else
#  define MHD_SYS_select_(n,r,w,e,t) \
( ( (((void*)(r) == (void*)0) || ((fd_set*)(r))->fd_count == 0) &&  \
    (((void*)(w) == (void*)0) || ((fd_set*)(w))->fd_count == 0) &&  \
    (((void*)(e) == (void*)0) || ((fd_set*)(e))->fd_count == 0) ) ? \
  ( ((void*)(t) == (void*)0) ? 0 :                                  \
    (Sleep(((struct timeval*)(t))->tv_sec * 1000 +                  \
           ((struct timeval*)(t))->tv_usec / 1000), 0) ) :          \
  (select((int)0,(r),(w),(e),(t))) )
#endif

#if defined(HAVE_POLL)
/* MHD_sys_poll_ is wrapper macro for system poll() function */
#  if !defined(MHD_WINSOCK_SOCKETS)
#    define MHD_sys_poll_ poll
#  else  /* MHD_WINSOCK_SOCKETS */
#    define MHD_sys_poll_ WSAPoll
#  endif /* MHD_WINSOCK_SOCKETS */

#  ifdef POLLPRI
#    define MHD_POLLPRI_OR_ZERO POLLPRI
#  else  /* ! POLLPRI */
#    define MHD_POLLPRI_OR_ZERO 0
#  endif /* ! POLLPRI */
#  ifdef POLLRDBAND
#    define MHD_POLLRDBAND_OR_ZERO POLLRDBAND
#  else  /* ! POLLRDBAND */
#    define MHD_POLLRDBAND_OR_ZERO 0
#  endif /* ! POLLRDBAND */
#  ifdef POLLNVAL
#    define MHD_POLLNVAL_OR_ZERO POLLNVAL
#  else  /* ! POLLNVAL */
#    define MHD_POLLNVAL_OR_ZERO 0
#  endif /* ! POLLNVAL */

/* MHD_POLL_EVENTS_ERR_DISC is 'events' mask for errors and disconnect.
 * Note: Out-of-band data is treated as error. */
#  if defined(_WIN32)
#    define MHD_POLL_EVENTS_ERR_DISC POLLRDBAND
#  elif defined(__linux__)
#    define MHD_POLL_EVENTS_ERR_DISC POLLPRI
#  else /* ! __linux__ */
#    define MHD_POLL_EVENTS_ERR_DISC (MHD_POLLPRI_OR_ZERO | MHD_POLLRDBAND_OR_ZERO)
#  endif /* ! __linux__ */
/* MHD_POLL_REVENTS_ERR_DISC is 'revents' mask for errors and disconnect.
 * Note: Out-of-band data is treated as error. */
#  define MHD_POLL_REVENTS_ERR_DISC \
     (MHD_POLLPRI_OR_ZERO | MHD_POLLRDBAND_OR_ZERO | MHD_POLLNVAL_OR_ZERO | POLLERR | POLLHUP)
/* MHD_POLL_REVENTS_ERRROR is 'revents' mask for errors.
 * Note: Out-of-band data is treated as error. */
#  define MHD_POLL_REVENTS_ERRROR \
     (MHD_POLLPRI_OR_ZERO | MHD_POLLRDBAND_OR_ZERO | MHD_POLLNVAL_OR_ZERO | POLLERR)
#endif /* HAVE_POLL */

#define MHD_SCKT_MISSING_ERR_CODE_ 31450

#if defined(MHD_POSIX_SOCKETS)
#  if defined(EAGAIN)
#    define MHD_SCKT_EAGAIN_      EAGAIN
#  elif defined(EWOULDBLOCK)
#    define MHD_SCKT_EAGAIN_      EWOULDBLOCK
#  else  /* !EAGAIN && !EWOULDBLOCK */
#    define MHD_SCKT_EAGAIN_      MHD_SCKT_MISSING_ERR_CODE_
#  endif /* !EAGAIN && !EWOULDBLOCK */
#  if defined(EWOULDBLOCK)
#    define MHD_SCKT_EWOULDBLOCK_ EWOULDBLOCK
#  elif defined(EAGAIN)
#    define MHD_SCKT_EWOULDBLOCK_ EAGAIN
#  else  /* !EWOULDBLOCK && !EAGAIN */
#    define MHD_SCKT_EWOULDBLOCK_ MHD_SCKT_MISSING_ERR_CODE_
#  endif /* !EWOULDBLOCK && !EAGAIN */
#  ifdef EINTR
#    define MHD_SCKT_EINTR_       EINTR
#  else  /* ! EINTR */
#    define MHD_SCKT_EINTR_       MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! EINTR */
#  ifdef ECONNRESET
#    define MHD_SCKT_ECONNRESET_  ECONNRESET
#  else  /* ! ECONNRESET */
#    define MHD_SCKT_ECONNRESET_  MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ECONNRESET */
#  ifdef ECONNABORTED
#    define MHD_SCKT_ECONNABORTED_  ECONNABORTED
#  else  /* ! ECONNABORTED */
#    define MHD_SCKT_ECONNABORTED_  MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ECONNABORTED */
#  ifdef ENOTCONN
#    define MHD_SCKT_ENOTCONN_    ENOTCONN
#  else  /* ! ENOTCONN */
#    define MHD_SCKT_ENOTCONN_    MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ENOTCONN */
#  ifdef EMFILE
#    define MHD_SCKT_EMFILE_      EMFILE
#  else  /* ! EMFILE */
#    define MHD_SCKT_EMFILE_      MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! EMFILE */
#  ifdef ENFILE
#    define MHD_SCKT_ENFILE_      ENFILE
#  else  /* ! ENFILE */
#    define MHD_SCKT_ENFILE_      MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ENFILE */
#  ifdef ENOMEM
#    define MHD_SCKT_ENOMEM_      ENOMEM
#  else  /* ! ENOMEM */
#    define MHD_SCKT_ENOMEM_      MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ENOMEM */
#  ifdef ENOBUFS
#    define MHD_SCKT_ENOBUFS_     ENOBUFS
#  else  /* ! ENOBUFS */
#    define MHD_SCKT_ENOBUFS_     MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ENOBUFS */
#  ifdef EBADF
#    define MHD_SCKT_EBADF_       EBADF
#  else  /* ! EBADF */
#    define MHD_SCKT_EBADF_       MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! EBADF */
#  ifdef ENOTSOCK
#    define MHD_SCKT_ENOTSOCK_    ENOTSOCK
#  else  /* ! ENOTSOCK */
#    define MHD_SCKT_ENOTSOCK_    MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ENOTSOCK */
#  ifdef EINVAL
#    define MHD_SCKT_EINVAL_      EINVAL
#  else  /* ! EINVAL */
#    define MHD_SCKT_EINVAL_      MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! EINVAL */
#  ifdef EFAULT
#    define MHD_SCKT_EFAUL_       EFAULT
#  else  /* ! EFAULT */
#    define MHD_SCKT_EFAUL_       MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! EFAULT */
#  ifdef ENOSYS
#    define MHD_SCKT_ENOSYS_      ENOSYS
#  else  /* ! ENOSYS */
#    define MHD_SCKT_ENOSYS_      MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ENOSYS */
#  ifdef ENOTSUP
#    define MHD_SCKT_ENOTSUP_     ENOTSUP
#  else  /* ! ENOTSUP */
#    define MHD_SCKT_ENOTSUP_     MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ENOTSUP */
#  ifdef EOPNOTSUPP
#    define MHD_SCKT_EOPNOTSUPP_  EOPNOTSUPP
#  else  /* ! EOPNOTSUPP */
#    define MHD_SCKT_EOPNOTSUPP_  MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! EOPNOTSUPP */
#  ifdef EACCES
#    define MHD_SCKT_EACCESS_     EACCES
#  else  /* ! EACCES */
#    define MHD_SCKT_EACCESS_     MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! EACCES */
#  ifdef ENETDOWN
#    define MHD_SCKT_ENETDOWN_    ENETDOWN
#  else  /* ! ENETDOWN */
#    define MHD_SCKT_ENETDOWN_    MHD_SCKT_MISSING_ERR_CODE_
#  endif /* ! ENETDOWN */
#elif defined(MHD_WINSOCK_SOCKETS)
#  define MHD_SCKT_EAGAIN_        WSAEWOULDBLOCK
#  define MHD_SCKT_EWOULDBLOCK_   WSAEWOULDBLOCK
#  define MHD_SCKT_EINTR_         WSAEINTR
#  define MHD_SCKT_ECONNRESET_    WSAECONNRESET
#  define MHD_SCKT_ECONNABORTED_  WSAECONNABORTED
#  define MHD_SCKT_ENOTCONN_      WSAENOTCONN
#  define MHD_SCKT_EMFILE_        WSAEMFILE
#  define MHD_SCKT_ENFILE_        MHD_SCKT_MISSING_ERR_CODE_
#  define MHD_SCKT_ENOMEM_        MHD_SCKT_MISSING_ERR_CODE_
#  define MHD_SCKT_ENOBUFS_       WSAENOBUFS
#  define MHD_SCKT_EBADF_         WSAEBADF
#  define MHD_SCKT_ENOTSOCK_      WSAENOTSOCK
#  define MHD_SCKT_EINVAL_        WSAEINVAL
#  define MHD_SCKT_EFAUL_         WSAEFAULT
#  define MHD_SCKT_ENOSYS_        MHD_SCKT_MISSING_ERR_CODE_
#  define MHD_SCKT_ENOTSUP_       MHD_SCKT_MISSING_ERR_CODE_
#  define MHD_SCKT_EOPNOTSUPP_    WSAEOPNOTSUPP
#  define MHD_SCKT_EACCESS_       WSAEACCES
#  define MHD_SCKT_ENETDOWN_      WSAENETDOWN
#endif

/**
 * MHD_socket_error_ return system native error code for last socket error.
 * @return system error code for last socket error.
 */
#if defined(MHD_POSIX_SOCKETS)
#  define MHD_socket_get_error_() (errno)
#elif defined(MHD_WINSOCK_SOCKETS)
#  define MHD_socket_get_error_() WSAGetLastError()
#endif

#ifdef MHD_WINSOCK_SOCKETS
  /* POSIX-W32 sockets compatibility functions */

/**
 * Return pointer to string description of specified WinSock error
 * @param err the WinSock error code.
 * @return pointer to string description of specified WinSock error.
 */
  const char* MHD_W32_strerror_winsock_(int err);
#endif /* MHD_WINSOCK_SOCKETS */

/* MHD_socket_last_strerr_ is description string of specified socket error code */
#if defined(MHD_POSIX_SOCKETS)
#  define MHD_socket_strerr_(err) strerror((err))
#elif defined(MHD_WINSOCK_SOCKETS)
#  define MHD_socket_strerr_(err) MHD_W32_strerror_winsock_((err))
#endif

/* MHD_socket_last_strerr_ is description string of last errno (non-W32) /
 *                            description string of last socket error (W32) */
#define MHD_socket_last_strerr_() MHD_socket_strerr_(MHD_socket_get_error_())

/**
 * MHD_socket_fset_error_() set socket system native error code.
 */
#if defined(MHD_POSIX_SOCKETS)
#  define MHD_socket_fset_error_(err) (errno = (err))
#elif defined(MHD_WINSOCK_SOCKETS)
#  define MHD_socket_fset_error_(err) (WSASetLastError((err)))
#endif

/**
 * MHD_socket_try_set_error_() set socket system native error code if
 * specified code is defined on system.
 * @return non-zero if specified @a err code is defined on system
 *         and error was set;
 *         zero if specified @a err code is not defined on system
 *         and error was not set.
 */
#define MHD_socket_try_set_error_(err) ( (MHD_SCKT_MISSING_ERR_CODE_ != (err)) ? \
                                         (MHD_socket_fset_error_((err)), !0) : 0 )

/**
 * MHD_socket_set_error_() set socket system native error code to
 * specified code or replacement code if specified code is not
 * defined on system.
 */
#if defined(MHD_POSIX_SOCKETS)
#  if defined(ENOSYS)
#    define MHD_socket_set_error_(err) ( (MHD_SCKT_MISSING_ERR_CODE_ == (err)) ? \
                                         (errno = ENOSYS) : (errno = (err)) )
#  elif defined(EOPNOTSUPP)
#    define MHD_socket_set_error_(err) ( (MHD_SCKT_MISSING_ERR_CODE_ == (err)) ? \
                                         (errno = EOPNOTSUPP) : (errno = (err)) )
#  elif defined (EFAULT)
#    define MHD_socket_set_error_(err) ( (MHD_SCKT_MISSING_ERR_CODE_ == (err)) ? \
                                         (errno = EFAULT) : (errno = (err)) )
#  elif defined (EINVAL)
#    define MHD_socket_set_error_(err) ( (MHD_SCKT_MISSING_ERR_CODE_ == (err)) ? \
                                         (errno = EINVAL) : (errno = (err)) )
#  else  /* !EOPNOTSUPP && !EFAULT && !EINVAL */
#    warning No suitable replacement for missing socket error code is found. Edit this file and add replacement code which is defined on system.
#    define MHD_socket_set_error_(err) (errno = (err))
#  endif /* !EOPNOTSUPP && !EFAULT && !EINVAL*/
#elif defined(MHD_WINSOCK_SOCKETS)
#  define MHD_socket_set_error_(err) ( (MHD_SCKT_MISSING_ERR_CODE_ == (err)) ? \
                                       (WSASetLastError((WSAEOPNOTSUPP))) : \
                                       (WSASetLastError((err))) )
#endif

/**
 * Check whether given socket error is equal to specified system
 * native MHD_SCKT_E*_ code.
 * If platform don't have specific error code, result is
 * always boolean false.
 * @return boolean true if @a code is real error code and
 *         @a err equals to MHD_SCKT_E*_ @a code;
 *         boolean false otherwise
 */
#define MHD_SCKT_ERR_IS_(err,code) ( (MHD_SCKT_MISSING_ERR_CODE_ != (code)) && \
                                     ((code) == (err)) )

/**
 * Check whether last socket error is equal to specified system
 * native MHD_SCKT_E*_ code.
 * If platform don't have specific error code, result is
 * always boolean false.
 * @return boolean true if @a code is real error code and
 *         last socket error equals to MHD_SCKT_E*_ @a code;
 *         boolean false otherwise
 */
#define MHD_SCKT_LAST_ERR_IS_(code) MHD_SCKT_ERR_IS_(MHD_socket_get_error_() ,(code))

/* Specific error code checks */

/**
 * Check whether given socket error is equal to system's
 * socket error codes for EINTR.
 * @return boolean true if @a err is equal to sockets' EINTR code;
 *         boolean false otherwise.
 */
#define MHD_SCKT_ERR_IS_EINTR_(err) MHD_SCKT_ERR_IS_((err),MHD_SCKT_EINTR_)

/**
 * Check whether given socket error is equal to system's
 * socket error codes for EAGAIN or EWOULDBLOCK.
 * @return boolean true if @a err is equal to sockets' EAGAIN or EWOULDBLOCK codes;
 *         boolean false otherwise.
 */
#if MHD_SCKT_EAGAIN_ == MHD_SCKT_EWOULDBLOCK_
#  define MHD_SCKT_ERR_IS_EAGAIN_(err) MHD_SCKT_ERR_IS_((err),MHD_SCKT_EAGAIN_)
#else  /* MHD_SCKT_EAGAIN_ != MHD_SCKT_EWOULDBLOCK_ */
#  define MHD_SCKT_ERR_IS_EAGAIN_(err) ( MHD_SCKT_ERR_IS_((err),MHD_SCKT_EAGAIN_) || \
                                         MHD_SCKT_ERR_IS_((err),MHD_SCKT_EWOULDBLOCK_) )
#endif /* MHD_SCKT_EAGAIN_ != MHD_SCKT_EWOULDBLOCK_ */

/**
 * Check whether given socket error is any kind of "low resource" error.
 * @return boolean true if @a err is any kind of "low resource" error,
 *         boolean false otherwise.
 */
#define MHD_SCKT_ERR_IS_LOW_RESOURCES_(err) ( MHD_SCKT_ERR_IS_((err),MHD_SCKT_EMFILE_) || \
                                              MHD_SCKT_ERR_IS_((err),MHD_SCKT_ENFILE_) || \
                                              MHD_SCKT_ERR_IS_((err),MHD_SCKT_ENOMEM_) || \
                                              MHD_SCKT_ERR_IS_((err),MHD_SCKT_ENOBUFS_) )

/**
 * Check whether is given socket error is type of "incoming connection
 * was disconnected before 'accept()' is called".
 * @return boolean true is @a err match described socket error code,
 *         boolean false otherwise.
 */
#if defined(MHD_POSIX_SOCKETS)
#  define MHD_SCKT_ERR_IS_DISCNN_BEFORE_ACCEPT_(err) MHD_SCKT_ERR_IS_((err),MHD_SCKT_ECONNABORTED_)
#elif defined(MHD_WINSOCK_SOCKETS)
#  define MHD_SCKT_ERR_IS_DISCNN_BEFORE_ACCEPT_(err) MHD_SCKT_ERR_IS_((err),MHD_SCKT_ECONNRESET_)
#endif

/**
 * Check whether is given socket error is type of "connection was terminated
 * by remote side".
 * @return boolean true is @a err match described socket error code,
 *         boolean false otherwise.
 */
#define MHD_SCKT_ERR_IS_REMOTE_DISCNN_(err) ( MHD_SCKT_ERR_IS_((err),MHD_SCKT_ECONNRESET_) || \
                                              MHD_SCKT_ERR_IS_((err),MHD_SCKT_ECONNABORTED_))

/* Specific error code set */

/**
 * Set socket's error code to ENOMEM or equivalent if ENOMEM is not
 * available on platform.
 */
#if MHD_SCKT_MISSING_ERR_CODE_ != MHD_SCKT_ENOMEM_
#  define MHD_socket_set_error_to_ENOMEM() MHD_socket_set_error_(MHD_SCKT_ENOMEM_)
#elif MHD_SCKT_MISSING_ERR_CODE_ != MHD_SCKT_ENOBUFS_
#  define MHD_socket_set_error_to_ENOMEM() MHD_socket_set_error_(MHD_SCKT_ENOBUFS_)
#else
#  warning No suitable replacement for ENOMEM error codes is found. Edit this file and add replacement code which is defined on system.
#  define MHD_socket_set_error_to_ENOMEM() MHD_socket_set_error_(MHD_SCKT_ENOMEM_)
#endif

/* Socket functions */

#if defined(AF_LOCAL)
#  define MHD_SCKT_LOCAL AF_LOCAL
#elif defined(AF_UNIX)
#  define MHD_SCKT_LOCAL AF_UNIX
#endif /* AF_UNIX */

#if defined(MHD_POSIX_SOCKETS) && defined(MHD_SCKT_LOCAL)
#  define MHD_socket_pair_(fdarr) (!socketpair(MHD_SCKT_LOCAL, SOCK_STREAM, 0, (fdarr)))
#  if defined(HAVE_SOCK_NONBLOCK)
#    define MHD_socket_pair_nblk_(fdarr) (!socketpair(MHD_SCKT_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, 0, (fdarr)))
#  endif /* HAVE_SOCK_NONBLOCK*/
#elif defined(MHD_WINSOCK_SOCKETS)
   /**
    * Create pair of mutually connected TCP/IP sockets on loopback address
    * @param sockets_pair array to receive resulted sockets
    * @param non_blk if set to non-zero value, sockets created in non-blocking mode
    *                otherwise sockets will be in blocking mode
    * @return non-zero if succeeded, zero otherwise
    */
   int MHD_W32_socket_pair_(SOCKET sockets_pair[2], int non_blk);

#  define MHD_socket_pair_(fdarr) MHD_W32_socket_pair_((fdarr), 0)
#  define MHD_socket_pair_nblk_(fdarr) MHD_W32_socket_pair_((fdarr), 1)
#endif

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
                    unsigned int fd_setsize);


/**
 * Change socket options to be non-blocking.
 *
 * @param sock socket to manipulate
 * @return non-zero if succeeded, zero otherwise
 */
int
MHD_socket_nonblocking_ (MHD_socket sock);


/**
 * Change socket options to be non-inheritable.
 *
 * @param sock socket to manipulate
 * @return non-zero if succeeded, zero otherwise
 * @warning Does not set socket error on W32.
 */
int
MHD_socket_noninheritable_ (MHD_socket sock);


#if defined(SOL_SOCKET) && defined(SO_NOSIGPIPE)
   static const int _MHD_socket_int_one = 1;
/**
 * Change socket options to no signal on remote disconnect.
 *
 * @param sock socket to manipulate
 * @return non-zero if succeeded, zero otherwise
 */
#  define MHD_socket_nosignal_(sock) \
    (!setsockopt((sock),SOL_SOCKET,SO_NOSIGPIPE,&_MHD_socket_int_one,sizeof(_MHD_socket_int_one)))
#endif /* SOL_SOCKET && SO_NOSIGPIPE */

/**
 * Create a listen socket, with noninheritable flag if possible.
 *
 * @param use_ipv6 if set to non-zero IPv6 is used
 * @return created socket or MHD_INVALID_SOCKET in case of errors
 */
MHD_socket
MHD_socket_create_listen_ (int use_ipv6);

#endif /* ! MHD_SOCKETS_H */
