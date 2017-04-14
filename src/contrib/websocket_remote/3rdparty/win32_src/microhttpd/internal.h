/*
  This file is part of libmicrohttpd
  Copyright (C) 2007-2017 Daniel Pittman and Christian Grothoff

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
 * @file microhttpd/internal.h
 * @brief  internal shared structures
 * @author Daniel Pittman
 * @author Christian Grothoff
 */

#ifndef INTERNAL_H
#define INTERNAL_H

#include "mhd_options.h"
#include "platform.h"
#include "microhttpd.h"
#ifdef HTTPS_SUPPORT
#include <gnutls/gnutls.h>
#if GNUTLS_VERSION_MAJOR >= 3
#include <gnutls/abstract.h>
#endif
#endif /* HTTPS_SUPPORT */

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif


#ifdef MHD_PANIC
/* Override any defined MHD_PANIC macro with proper one */
#undef MHD_PANIC
#endif /* MHD_PANIC */

#ifdef HAVE_MESSAGES
/**
 * Trigger 'panic' action based on fatal errors.
 *
 * @param msg error message (const char *)
 */
#define MHD_PANIC(msg) do { mhd_panic (mhd_panic_cls, __FILE__, __LINE__, msg); BUILTIN_NOT_REACHED; } while (0)
#else
/**
 * Trigger 'panic' action based on fatal errors.
 *
 * @param msg error message (const char *)
 */
#define MHD_PANIC(msg) do { mhd_panic (mhd_panic_cls, __FILE__, __LINE__, NULL); BUILTIN_NOT_REACHED; } while (0)
#endif

#include "mhd_threads.h"
#include "mhd_locks.h"
#include "mhd_sockets.h"
#include "mhd_itc_types.h"


/**
 * Close FD and abort execution if error is detected.
 * @param fd the FD to close
 */
#define MHD_fd_close_chk_(fd) do {             \
    if (0 == close ((fd)) && (EBADF == errno)) \
      MHD_PANIC(_("Failed to close FD.\n"));   \
  } while(0)

/**
 * Should we perform additional sanity checks at runtime (on our internal
 * invariants)?  This may lead to aborts, but can be useful for debugging.
 */
#define EXTRA_CHECKS MHD_NO

#define MHD_MAX(a,b) (((a)<(b)) ? (b) : (a))
#define MHD_MIN(a,b) (((a)<(b)) ? (a) : (b))


/**
 * Minimum size by which MHD tries to increment read/write buffers.
 * We usually begin with half the available pool space for the
 * IO-buffer, but if absolutely needed we additively grow by the
 * number of bytes given here (up to -- theoretically -- the full pool
 * space).
 */
#define MHD_BUF_INC_SIZE 1024


/**
 * Handler for fatal errors.
 */
extern MHD_PanicCallback mhd_panic;

/**
 * Closure argument for "mhd_panic".
 */
extern void *mhd_panic_cls;

/* If we have Clang or gcc >= 4.5, use __buildin_unreachable() */
#if defined(__clang__) || (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define BUILTIN_NOT_REACHED __builtin_unreachable()
#elif defined(_MSC_FULL_VER)
#define BUILTIN_NOT_REACHED __assume(0)
#else
#define BUILTIN_NOT_REACHED
#endif

/**
 * Determine length of static string / macro strings at compile time.
 */
#define MHD_STATICSTR_LEN_(macro) (sizeof(macro)/sizeof(char) - 1)

/**
 * State of the socket with respect to epoll (bitmask).
 */
enum MHD_EpollState
{

  /**
   * The socket is not involved with a defined state in epoll() right
   * now.
   */
  MHD_EPOLL_STATE_UNREADY = 0,

  /**
   * epoll() told us that data was ready for reading, and we did
   * not consume all of it yet.
   */
  MHD_EPOLL_STATE_READ_READY = 1,

  /**
   * epoll() told us that space was available for writing, and we did
   * not consume all of it yet.
   */
  MHD_EPOLL_STATE_WRITE_READY = 2,

  /**
   * Is this connection currently in the 'eready' EDLL?
   */
  MHD_EPOLL_STATE_IN_EREADY_EDLL = 4,

  /**
   * Is this connection currently in the epoll() set?
   */
  MHD_EPOLL_STATE_IN_EPOLL_SET = 8,

  /**
   * Is this connection currently suspended?
   */
  MHD_EPOLL_STATE_SUSPENDED = 16,

  /**
   * Is this connection in some error state?
   */
  MHD_EPOLL_STATE_ERROR = 128
};


/**
 * What is this connection waiting for?
 */
enum MHD_ConnectionEventLoopInfo
{
  /**
   * We are waiting to be able to read.
   */
  MHD_EVENT_LOOP_INFO_READ = 0,

  /**
   * We are waiting to be able to write.
   */
  MHD_EVENT_LOOP_INFO_WRITE = 1,

  /**
   * We are waiting for the application to provide data.
   */
  MHD_EVENT_LOOP_INFO_BLOCK = 2,

  /**
   * We are finished and are awaiting cleanup.
   */
  MHD_EVENT_LOOP_INFO_CLEANUP = 3
};


/**
 * Additional test value for enum MHD_FLAG to check only for MHD_ALLOW_SUSPEND_RESUME and
 * NOT for MHD_USE_ITC.
 */
#define MHD_TEST_ALLOW_SUSPEND_RESUME 8192

/**
 * Maximum length of a nonce in digest authentication.  32(MD5 Hex) +
 * 8(Timestamp Hex) + 1(NULL); hence 41 should suffice, but Opera
 * (already) takes more (see Mantis #1633), so we've increased the
 * value to support something longer...
 */
#define MAX_NONCE_LENGTH 129


/**
 * A structure representing the internal holder of the
 * nonce-nc map.
 */
struct MHD_NonceNc
{

  /**
   * Nonce counter, a value that increases for each subsequent
   * request for the same nonce.
   */
  uint64_t nc;

  /**
   * Bitmask over the nc-64 previous nonce values.  Used to
   * allow out-of-order nonces.
   */
  uint64_t nmask;

  /**
   * Nonce value:
   */
  char nonce[MAX_NONCE_LENGTH];

};

#ifdef HAVE_MESSAGES
/**
 * fprintf()-like helper function for logging debug
 * messages.
 */
void
MHD_DLOG (const struct MHD_Daemon *daemon,
	  const char *format,
          ...);
#endif


/**
 * Header or cookie in HTTP request or response.
 */
struct MHD_HTTP_Header
{
  /**
   * Headers are kept in a linked list.
   */
  struct MHD_HTTP_Header *next;

  /**
   * The name of the header (key), without the colon.
   */
  char *header;

  /**
   * The value of the header.
   */
  char *value;

  /**
   * Type of the header (where in the HTTP protocol is this header
   * from).
   */
  enum MHD_ValueKind kind;

};


/**
 * Representation of a response.
 */
struct MHD_Response
{

  /**
   * Headers to send for the response.  Initially
   * the linked list is created in inverse order;
   * the order should be inverted before sending!
   */
  struct MHD_HTTP_Header *first_header;

  /**
   * Buffer pointing to data that we are supposed
   * to send as a response.
   */
  char *data;

  /**
   * Closure to give to the content reader @e crc
   * and content reader free callback @e crfc.
   */
  void *crc_cls;

  /**
   * How do we get more data?  NULL if we are
   * given all of the data up front.
   */
  MHD_ContentReaderCallback crc;

  /**
   * NULL if data must not be freed, otherwise
   * either user-specified callback or "&free".
   */
  MHD_ContentReaderFreeCallback crfc;

#ifdef UPGRADE_SUPPORT
  /**
   * Application function to call once we are done sending the headers
   * of the response; NULL unless this is a response created with
   * #MHD_create_response_for_upgrade().
   */
  MHD_UpgradeHandler upgrade_handler;

  /**
   * Closure for @e uh.
   */
  void *upgrade_handler_cls;
#endif /* UPGRADE_SUPPORT */

  /**
   * Mutex to synchronize access to @e data, @e size and
   * @e reference_count.
   */
  MHD_mutex_ mutex;

  /**
   * Set to #MHD_SIZE_UNKNOWN if size is not known.
   */
  uint64_t total_size;

  /**
   * At what offset in the stream is the
   * beginning of @e data located?
   */
  uint64_t data_start;

  /**
   * Offset to start reading from when using @e fd.
   */
  uint64_t fd_off;

  /**
   * Number of bytes ready in @e data (buffer may be larger
   * than what is filled with payload).
   */
  size_t data_size;

  /**
   * Size of the data buffer @e data.
   */
  size_t data_buffer_size;

  /**
   * Reference count for this response.  Free once the counter hits
   * zero.
   */
  unsigned int reference_count;

  /**
   * File-descriptor if this response is FD-backed.
   */
  int fd;

  /**
   * Flags set for the MHD response.
   */
  enum MHD_ResponseFlags flags;

};


/**
 * States in a state machine for a connection.
 *
 * The main transitions are any-state to #MHD_CONNECTION_CLOSED, any
 * state to state+1, #MHD_CONNECTION_FOOTERS_SENT to
 * #MHD_CONNECTION_INIT.  #MHD_CONNECTION_CLOSED is the terminal state
 * and #MHD_CONNECTION_INIT the initial state.
 *
 * Note that transitions for *reading* happen only after the input has
 * been processed; transitions for *writing* happen after the
 * respective data has been put into the write buffer (the write does
 * not have to be completed yet).  A transition to
 * #MHD_CONNECTION_CLOSED or #MHD_CONNECTION_INIT requires the write
 * to be complete.
 */
enum MHD_CONNECTION_STATE
{
  /**
   * Connection just started (no headers received).
   * Waiting for the line with the request type, URL and version.
   */
  MHD_CONNECTION_INIT = 0,

  /**
   * 1: We got the URL (and request type and version).  Wait for a header line.
   */
  MHD_CONNECTION_URL_RECEIVED = MHD_CONNECTION_INIT + 1,

  /**
   * 2: We got part of a multi-line request header.  Wait for the rest.
   */
  MHD_CONNECTION_HEADER_PART_RECEIVED = MHD_CONNECTION_URL_RECEIVED + 1,

  /**
   * 3: We got the request headers.  Process them.
   */
  MHD_CONNECTION_HEADERS_RECEIVED = MHD_CONNECTION_HEADER_PART_RECEIVED + 1,

  /**
   * 4: We have processed the request headers.  Send 100 continue.
   */
  MHD_CONNECTION_HEADERS_PROCESSED = MHD_CONNECTION_HEADERS_RECEIVED + 1,

  /**
   * 5: We have processed the headers and need to send 100 CONTINUE.
   */
  MHD_CONNECTION_CONTINUE_SENDING = MHD_CONNECTION_HEADERS_PROCESSED + 1,

  /**
   * 6: We have sent 100 CONTINUE (or do not need to).  Read the message body.
   */
  MHD_CONNECTION_CONTINUE_SENT = MHD_CONNECTION_CONTINUE_SENDING + 1,

  /**
   * 7: We got the request body.  Wait for a line of the footer.
   */
  MHD_CONNECTION_BODY_RECEIVED = MHD_CONNECTION_CONTINUE_SENT + 1,

  /**
   * 8: We got part of a line of the footer.  Wait for the
   * rest.
   */
  MHD_CONNECTION_FOOTER_PART_RECEIVED = MHD_CONNECTION_BODY_RECEIVED + 1,

  /**
   * 9: We received the entire footer.  Wait for a response to be queued
   * and prepare the response headers.
   */
  MHD_CONNECTION_FOOTERS_RECEIVED = MHD_CONNECTION_FOOTER_PART_RECEIVED + 1,

  /**
   * 10: We have prepared the response headers in the writ buffer.
   * Send the response headers.
   */
  MHD_CONNECTION_HEADERS_SENDING = MHD_CONNECTION_FOOTERS_RECEIVED + 1,

  /**
   * 11: We have sent the response headers.  Get ready to send the body.
   */
  MHD_CONNECTION_HEADERS_SENT = MHD_CONNECTION_HEADERS_SENDING + 1,

  /**
   * 12: We are ready to send a part of a non-chunked body.  Send it.
   */
  MHD_CONNECTION_NORMAL_BODY_READY = MHD_CONNECTION_HEADERS_SENT + 1,

  /**
   * 13: We are waiting for the client to provide more
   * data of a non-chunked body.
   */
  MHD_CONNECTION_NORMAL_BODY_UNREADY = MHD_CONNECTION_NORMAL_BODY_READY + 1,

  /**
   * 14: We are ready to send a chunk.
   */
  MHD_CONNECTION_CHUNKED_BODY_READY = MHD_CONNECTION_NORMAL_BODY_UNREADY + 1,

  /**
   * 15: We are waiting for the client to provide a chunk of the body.
   */
  MHD_CONNECTION_CHUNKED_BODY_UNREADY = MHD_CONNECTION_CHUNKED_BODY_READY + 1,

  /**
   * 16: We have sent the response body. Prepare the footers.
   */
  MHD_CONNECTION_BODY_SENT = MHD_CONNECTION_CHUNKED_BODY_UNREADY + 1,

  /**
   * 17: We have prepared the response footer.  Send it.
   */
  MHD_CONNECTION_FOOTERS_SENDING = MHD_CONNECTION_BODY_SENT + 1,

  /**
   * 18: We have sent the response footer.  Shutdown or restart.
   */
  MHD_CONNECTION_FOOTERS_SENT = MHD_CONNECTION_FOOTERS_SENDING + 1,

  /**
   * 19: This connection is to be closed.
   */
  MHD_CONNECTION_CLOSED = MHD_CONNECTION_FOOTERS_SENT + 1,

  /**
   * 20: This connection is finished (only to be freed)
   */
  MHD_CONNECTION_IN_CLEANUP = MHD_CONNECTION_CLOSED + 1,

  /*
   *  SSL/TLS connection states
   */

  /**
   * The initial connection state for all secure connectoins
   * Handshake messages will be processed in this state & while
   * in the #MHD_TLS_HELLO_REQUEST state
   */
  MHD_TLS_CONNECTION_INIT = MHD_CONNECTION_IN_CLEANUP + 1,

#ifdef UPGRADE_SUPPORT
  /**
   * Connection was "upgraded" and socket is now under the
   * control of the application.
   */
  MHD_CONNECTION_UPGRADE = MHD_TLS_CONNECTION_INIT + 1,
#endif /* UPGRADE_SUPPORT */

};

/**
 * Should all state transitions be printed to stderr?
 */
#define DEBUG_STATES MHD_NO


#ifdef HAVE_MESSAGES
#if DEBUG_STATES
const char *
MHD_state_to_string (enum MHD_CONNECTION_STATE state);
#endif
#endif

/**
 * Function to receive plaintext data.
 *
 * @param conn the connection struct
 * @param write_to where to write received data
 * @param max_bytes maximum number of bytes to receive
 * @return number of bytes written to @a write_to
 */
typedef ssize_t
(*ReceiveCallback) (struct MHD_Connection *conn,
                    void *write_to,
                    size_t max_bytes);


/**
 * Function to transmit plaintext data.
 *
 * @param conn the connection struct
 * @param read_from where to read data to transmit
 * @param max_bytes maximum number of bytes to transmit
 * @return number of bytes transmitted
 */
typedef ssize_t
(*TransmitCallback) (struct MHD_Connection *conn,
                     const void *write_to,
                     size_t max_bytes);


/**
 * State kept for each HTTP request.
 */
struct MHD_Connection
{

#ifdef EPOLL_SUPPORT
  /**
   * Next pointer for the EDLL listing connections that are epoll-ready.
   */
  struct MHD_Connection *nextE;

  /**
   * Previous pointer for the EDLL listing connections that are epoll-ready.
   */
  struct MHD_Connection *prevE;
#endif

  /**
   * Next pointer for the DLL describing our IO state.
   */
  struct MHD_Connection *next;

  /**
   * Previous pointer for the DLL describing our IO state.
   */
  struct MHD_Connection *prev;

  /**
   * Next pointer for the XDLL organizing connections by timeout.
   * This DLL can be either the
   * 'manual_timeout_head/manual_timeout_tail' or the
   * 'normal_timeout_head/normal_timeout_tail', depending on whether a
   * custom timeout is set for the connection.
   */
  struct MHD_Connection *nextX;

  /**
   * Previous pointer for the XDLL organizing connections by timeout.
   */
  struct MHD_Connection *prevX;

  /**
   * Reference to the MHD_Daemon struct.
   */
  struct MHD_Daemon *daemon;

  /**
   * Linked list of parsed headers.
   */
  struct MHD_HTTP_Header *headers_received;

  /**
   * Tail of linked list of parsed headers.
   */
  struct MHD_HTTP_Header *headers_received_tail;

  /**
   * Response to transmit (initially NULL).
   */
  struct MHD_Response *response;

  /**
   * The memory pool is created whenever we first read from the TCP
   * stream and destroyed at the end of each request (and re-created
   * for the next request).  In the meantime, this pointer is NULL.
   * The pool is used for all connection-related data except for the
   * response (which maybe shared between connections) and the IP
   * address (which persists across individual requests).
   */
  struct MemoryPool *pool;

  /**
   * We allow the main application to associate some pointer with the
   * HTTP request, which is passed to each #MHD_AccessHandlerCallback
   * and some other API calls.  Here is where we store it.  (MHD does
   * not know or care what it is).
   */
  void *client_context;

  /**
   * We allow the main application to associate some pointer with the
   * TCP connection (which may span multiple HTTP requests).  Here is
   * where we store it.  (MHD does not know or care what it is).
   * The location is given to the #MHD_NotifyConnectionCallback and
   * also accessible via #MHD_CONNECTION_INFO_SOCKET_CONTEXT.
   */
  void *socket_context;

  /**
   * Request method.  Should be GET/POST/etc.  Allocated in pool.
   */
  char *method;

  /**
   * Requested URL (everything after "GET" only).  Allocated
   * in pool.
   */
  const char *url;

  /**
   * HTTP version string (i.e. http/1.1).  Allocated
   * in pool.
   */
  char *version;

  /**
   * Buffer for reading requests.  Allocated in pool.  Actually one
   * byte larger than @e read_buffer_size (if non-NULL) to allow for
   * 0-termination.
   */
  char *read_buffer;

  /**
   * Buffer for writing response (headers only).  Allocated
   * in pool.
   */
  char *write_buffer;

  /**
   * Last incomplete header line during parsing of headers.
   * Allocated in pool.  Only valid if state is
   * either #MHD_CONNECTION_HEADER_PART_RECEIVED or
   * #MHD_CONNECTION_FOOTER_PART_RECEIVED.
   */
  char *last;

  /**
   * Position after the colon on the last incomplete header
   * line during parsing of headers.
   * Allocated in pool.  Only valid if state is
   * either #MHD_CONNECTION_HEADER_PART_RECEIVED or
   * #MHD_CONNECTION_FOOTER_PART_RECEIVED.
   */
  char *colon;

  /**
   * Foreign address (of length @e addr_len).  MALLOCED (not
   * in pool!).
   */
  struct sockaddr *addr;

  /**
   * Thread handle for this connection (if we are using
   * one thread per connection).
   */
  MHD_thread_handle_ pid;

  /**
   * Size of @e read_buffer (in bytes).  This value indicates
   * how many bytes we're willing to read into the buffer;
   * the real buffer is one byte longer to allow for
   * adding zero-termination (when needed).
   */
  size_t read_buffer_size;

  /**
   * Position where we currently append data in
   * @e read_buffer (last valid position).
   */
  size_t read_buffer_offset;

  /**
   * Size of @e write_buffer (in bytes).
   */
  size_t write_buffer_size;

  /**
   * Offset where we are with sending from @e write_buffer.
   */
  size_t write_buffer_send_offset;

  /**
   * Last valid location in write_buffer (where do we
   * append and up to where is it safe to send?)
   */
  size_t write_buffer_append_offset;

  /**
   * How many more bytes of the body do we expect
   * to read? #MHD_SIZE_UNKNOWN for unknown.
   */
  uint64_t remaining_upload_size;

  /**
   * Current write position in the actual response
   * (excluding headers, content only; should be 0
   * while sending headers).
   */
  uint64_t response_write_position;

#if LINUX
  enum MHD_resp_sender_
  {
    MHD_resp_sender_std = 0,
    MHD_resp_sender_sendfile
  } resp_sender;
#endif /* LINUX */

  /**
   * Position in the 100 CONTINUE message that
   * we need to send when receiving http 1.1 requests.
   */
  size_t continue_message_write_offset;

  /**
   * Length of the foreign address.
   */
  socklen_t addr_len;

  /**
   * Last time this connection had any activity
   * (reading or writing).
   */
  time_t last_activity;

  /**
   * After how many seconds of inactivity should
   * this connection time out?  Zero for no timeout.
   */
  time_t connection_timeout;

  /**
   * Special member to be returned by #MHD_get_connection_info()
   */
  unsigned int connection_timeout_dummy;

  /**
   * Did we ever call the "default_handler" on this connection?  (this
   * flag will determine if we call the #MHD_OPTION_NOTIFY_COMPLETED
   * handler when the connection closes down).
   */
  bool client_aware;

  /**
   * Socket for this connection.  Set to #MHD_INVALID_SOCKET if
   * this connection has died (daemon should clean
   * up in that case).
   */
  MHD_socket socket_fd;

  /**
   * true if #socket_fd is non-blocking, false otherwise.
   */
  bool sk_nonblck;

  /**
   * Has this socket been closed for reading (i.e.  other side closed
   * the connection)?  If so, we must completely close the connection
   * once we are done sending our response (and stop trying to read
   * from this socket).
   */
  bool read_closed;

  /**
   * Set to `true` if the thread has been joined.
   */
  bool thread_joined;

  /**
   * Are we currently inside the "idle" handler (to avoid recursively
   * invoking it).
   */
  bool in_idle;

  /**
   * Are we currently inside the "idle" handler (to avoid recursively
   * invoking it).
   */
  bool in_cleanup;

#ifdef EPOLL_SUPPORT
  /**
   * What is the state of this socket in relation to epoll?
   */
  enum MHD_EpollState epoll_state;
#endif

  /**
   * State in the FSM for this connection.
   */
  enum MHD_CONNECTION_STATE state;

  /**
   * What is this connection waiting for?
   */
  enum MHD_ConnectionEventLoopInfo event_loop_info;

  /**
   * HTTP response code.  Only valid if response object
   * is already set.
   */
  unsigned int responseCode;

  /**
   * Are we receiving with chunked encoding?  This will be set to
   * #MHD_YES after we parse the headers and are processing the body
   * with chunks.  After we are done with the body and we are
   * processing the footers; once the footers are also done, this will
   * be set to #MHD_NO again (before the final call to the handler).
   */
  bool have_chunked_upload;

  /**
   * If we are receiving with chunked encoding, where are we right
   * now?  Set to 0 if we are waiting to receive the chunk size;
   * otherwise, this is the size of the current chunk.  A value of
   * zero is also used when we're at the end of the chunks.
   */
  uint64_t current_chunk_size;

  /**
   * If we are receiving with chunked encoding, where are we currently
   * with respect to the current chunk (at what offset / position)?
   */
  uint64_t current_chunk_offset;

  /**
   * Handler used for processing read connection operations
   * @sa #MHD_connection_handle_read, #MHD_tls_connection_handle_read
   */
  int (*read_handler) (struct MHD_Connection *connection);

  /**
   * Handler used for processing write connection operations
   * @sa #MHD_connection_handle_write, #MHD_tls_connection_handle_write
   */
  int (*write_handler) (struct MHD_Connection *connection);

  /**
   * Handler used for processing idle connection operations
   * @sa #MHD_connection_handle_idle, #MHD_tls_connection_handle_idle
   */
  int (*idle_handler) (struct MHD_Connection *connection);

  /**
   * Function used for reading HTTP request stream.
   */
  ReceiveCallback recv_cls;

  /**
   * Function used for writing HTTP response stream.
   */
  TransmitCallback send_cls;

#ifdef UPGRADE_SUPPORT
  /**
   * If this connection was upgraded, this points to
   * the upgrade response details such that the
   * #thread_main_connection_upgrade()-logic can perform the
   * bi-directional forwarding.
   */
  struct MHD_UpgradeResponseHandle *urh;
#endif /* UPGRADE_SUPPORT */

#ifdef HTTPS_SUPPORT

  /**
   * State required for HTTPS/SSL/TLS support.
   */
  gnutls_session_t tls_session;

  /**
   * Memory location to return for protocol session info.
   */
  int protocol;

  /**
   * Memory location to return for protocol session info.
   */
  int cipher;

  /**
   * Could it be that we are ready to read due to TLS buffers
   * even though the socket is not?
   */
  bool tls_read_ready;

  /**
   * TLS layer was shut down?
   */
  bool tls_closed;
#endif /* HTTPS_SUPPORT */

  /**
   * Is the connection suspended?
   */
  bool suspended;

  /**
   * Is the connection wanting to resume?
   */
  bool resuming;
};


#ifdef UPGRADE_SUPPORT
/**
 * Buffer we use for upgrade response handling in the unlikely
 * case where the memory pool was so small it had no buffer
 * capacity left.  Note that we don't expect to _ever_ use this
 * buffer, so it's mostly wasted memory (except that it allows
 * us to handle a tricky error condition nicely). So no need to
 * make this one big.  Applications that want to perform well
 * should just pick an adequate size for the memory pools.
 */
#define RESERVE_EBUF_SIZE 8

/**
 * Context we pass to epoll() for each of the two sockets
 * of a `struct MHD_UpgradeResponseHandle`.  We need to do
 * this so we can distinguish the two sockets when epoll()
 * gives us event notifications.
 */
struct UpgradeEpollHandle
{
  /**
   * Reference to the overall response handle this struct is
   * included within.
   */
  struct MHD_UpgradeResponseHandle *urh;

  /**
   * The socket this event is kind-of about.  Note that this is NOT
   * necessarily the socket we are polling on, as for when we read
   * from TLS, we epoll() on the connection's socket
   * (`urh->connection->socket_fd`), while this then the application's
   * socket (where the application will read from).  Nevertheless, for
   * the application to read, we need to first read from TLS, hence
   * the two are related.
   *
   * Similarly, for writing to TLS, this epoll() will be on the
   * connection's `socket_fd`, and this will merely be the FD which
   * the applicatio would write to.  Hence this struct must always be
   * interpreted based on which field in `struct
   * MHD_UpgradeResponseHandle` it is (`app` or `mhd`).
   */
  MHD_socket socket;

  /**
   * IO-state of the @e socket (or the connection's `socket_fd`).
   */
  enum MHD_EpollState celi;

};


/**
 * Handle given to the application to manage special
 * actions relating to MHD responses that "upgrade"
 * the HTTP protocol (i.e. to WebSockets).
 */
struct MHD_UpgradeResponseHandle
{
  /**
   * The connection for which this is an upgrade handle.  Note that
   * because a response may be shared over many connections, this may
   * not be the only upgrade handle for the response of this connection.
   */
  struct MHD_Connection *connection;

#ifdef HTTPS_SUPPORT
  /**
   * Kept in a DLL per daemon.
   */
  struct MHD_UpgradeResponseHandle *next;

  /**
   * Kept in a DLL per daemon.
   */
  struct MHD_UpgradeResponseHandle *prev;

#ifdef EPOLL_SUPPORT
  /**
   * Next pointer for the EDLL listing urhs that are epoll-ready.
   */
  struct MHD_UpgradeResponseHandle *nextE;

  /**
   * Previous pointer for the EDLL listing urhs that are epoll-ready.
   */
  struct MHD_UpgradeResponseHandle *prevE;

  /**
   * Specifies whether urh already in EDLL list of ready connections.
   */
  bool in_eready_list;
#endif

  /**
   * The buffer for receiving data from TLS to
   * be passed to the application.  Contains @e in_buffer_size
   * bytes (unless @e in_buffer_size is zero). Do not free!
   */
  char *in_buffer;

  /**
   * The buffer for receiving data from the application to
   * be passed to TLS.  Contains @e out_buffer_size
   * bytes (unless @e out_buffer_size is zero). Do not free!
   */
  char *out_buffer;

  /**
   * Size of the @e in_buffer.
   * Set to 0 if the TLS connection went down for reading or socketpair
   * went down for writing.
   */
  size_t in_buffer_size;

  /**
   * Size of the @e out_buffer.
   * Set to 0 if the TLS connection went down for writing or socketpair
   * went down for reading.
   */
  size_t out_buffer_size;

  /**
   * Number of bytes actually in use in the @e in_buffer.  Can be larger
   * than @e in_buffer_size if and only if @a in_buffer_size is zero and
   * we still have bytes that can be forwarded.
   * Reset to zero if all data was forwarded to socketpair or
   * if socketpair went down for writing.
   */
  size_t in_buffer_used;

  /**
   * Number of bytes actually in use in the @e out_buffer. Can be larger
   * than @e out_buffer_size if and only if @a out_buffer_size is zero and
   * we still have bytes that can be forwarded.
   * Reset to zero if all data was forwarded to TLS connection or
   * if TLS connection went down for writing.
   */
  size_t out_buffer_used;

  /**
   * The socket we gave to the application (r/w).
   */
  struct UpgradeEpollHandle app;

  /**
   * If @a app_sock was a socketpair, our end of it, otherwise
   * #MHD_INVALID_SOCKET; (r/w).
   */
  struct UpgradeEpollHandle mhd;

  /**
   * Emergency IO buffer we use in case the memory pool has literally
   * nothing left.
   */
  char e_buf[RESERVE_EBUF_SIZE];

#endif /* HTTPS_SUPPORT */

  /**
   * Set to true after the application finished with the socket
   * by #MHD_UPGRADE_ACTION_CLOSE.
   *
   * When BOTH @e was_closed (changed by command from application)
   * AND @e clean_ready (changed internally by MHD) are set to
   * #MHD_YES, function #MHD_resume_connection() will move this
   * connection to cleanup list.
   * @remark This flag could be changed from any thread.
   */
  volatile bool was_closed;

  /**
   * Set to true if connection is ready for cleanup.
   *
   * In TLS mode functions #MHD_connection_finish_forward_() must
   * be called before setting this flag to true.
   *
   * In thread-per-connection mode, true in this flag means
   * that connection's thread exited or about to exit and will
   * not use MHD_Connection::urh data anymore.
   *
   * In any mode true in this flag also means that
   * MHD_Connection::urh data will not be used for socketpair
   * forwarding and forwarding itself is finished.
   *
   * When BOTH @e was_closed (changed by command from application)
   * AND @e clean_ready (changed internally by MHD) are set to
   * true, function #MHD_resume_connection() will move this
   * connection to cleanup list.
   * @remark This flag could be changed from thread that process
   * connection's recv(), send() and response.
   */
  bool clean_ready;
};
#endif /* UPGRADE_SUPPORT */


/**
 * Signature of function called to log URI accesses.
 *
 * @param cls closure
 * @param uri uri being accessed
 * @param con connection handle
 * @return new closure
 */
typedef void *
(*LogCallback)(void *cls,
               const char *uri,
               struct MHD_Connection *con);

/**
 * Signature of function called to unescape URIs.  See also
 * #MHD_http_unescape().
 *
 * @param cls closure
 * @param conn connection handle
 * @param uri 0-terminated string to unescape (should be updated)
 * @return length of the resulting string
 */
typedef size_t
(*UnescapeCallback)(void *cls,
                    struct MHD_Connection *conn,
                    char *uri);


/**
 * State kept for each MHD daemon.  All connections are kept in two
 * doubly-linked lists.  The first one reflects the state of the
 * connection in terms of what operations we are waiting for (read,
 * write, locally blocked, cleanup) whereas the second is about its
 * timeout state (default or custom).
 */
struct MHD_Daemon
{

  /**
   * Callback function for all requests.
   */
  MHD_AccessHandlerCallback default_handler;

  /**
   * Closure argument to default_handler.
   */
  void *default_handler_cls;

  /**
   * Head of doubly-linked list of our current, active connections.
   */
  struct MHD_Connection *connections_head;

  /**
   * Tail of doubly-linked list of our current, active connections.
   */
  struct MHD_Connection *connections_tail;

  /**
   * Head of doubly-linked list of our current but suspended connections.
   */
  struct MHD_Connection *suspended_connections_head;

  /**
   * Tail of doubly-linked list of our current but suspended connections.
   */
  struct MHD_Connection *suspended_connections_tail;

  /**
   * Head of doubly-linked list of connections to clean up.
   */
  struct MHD_Connection *cleanup_head;

  /**
   * Tail of doubly-linked list of connections to clean up.
   */
  struct MHD_Connection *cleanup_tail;

#ifdef EPOLL_SUPPORT
  /**
   * Head of EDLL of connections ready for processing (in epoll mode).
   */
  struct MHD_Connection *eready_head;

  /**
   * Tail of EDLL of connections ready for processing (in epoll mode)
   */
  struct MHD_Connection *eready_tail;

#ifdef UPGRADE_SUPPORT
  /**
   * Head of EDLL of upgraded connections ready for processing (in epoll mode).
   */
  struct MHD_UpgradeResponseHandle *eready_urh_head;

  /**
   * Tail of EDLL of upgraded connections ready for processing (in epoll mode)
   */
  struct MHD_UpgradeResponseHandle *eready_urh_tail;
#endif /* UPGRADE_SUPPORT */
#endif /* EPOLL_SUPPORT */

  /**
   * Head of the XDLL of ALL connections with a default ('normal')
   * timeout, sorted by timeout (earliest at the tail, most recently
   * used connection at the head).  MHD can just look at the tail of
   * this list to determine the timeout for all of its elements;
   * whenever there is an event of a connection, the connection is
   * moved back to the tail of the list.
   *
   * All connections by default start in this list; if a custom
   * timeout that does not match @e connection_timeout is set, they
   * are moved to the @e manual_timeout_head-XDLL.
   * Not used in MHD_USE_THREAD_PER_CONNECTION mode as each thread
   * needs only one connection-specific timeout.
   */
  struct MHD_Connection *normal_timeout_head;

  /**
   * Tail of the XDLL of ALL connections with a default timeout,
   * sorted by timeout (earliest timeout at the tail).
   * Not used in MHD_USE_THREAD_PER_CONNECTION mode.
   */
  struct MHD_Connection *normal_timeout_tail;

  /**
   * Head of the XDLL of ALL connections with a non-default/custom
   * timeout, unsorted.  MHD will do a O(n) scan over this list to
   * determine the current timeout.
   * Not used in MHD_USE_THREAD_PER_CONNECTION mode.
   */
  struct MHD_Connection *manual_timeout_head;

  /**
   * Tail of the XDLL of ALL connections with a non-default/custom
   * timeout, unsorted.
   * Not used in MHD_USE_THREAD_PER_CONNECTION mode.
   */
  struct MHD_Connection *manual_timeout_tail;

  /**
   * Function to call to check if we should accept or reject an
   * incoming request.  May be NULL.
   */
  MHD_AcceptPolicyCallback apc;

  /**
   * Closure argument to apc.
   */
  void *apc_cls;

  /**
   * Function to call when we are done processing
   * a particular request.  May be NULL.
   */
  MHD_RequestCompletedCallback notify_completed;

  /**
   * Closure argument to @e notify_completed.
   */
  void *notify_completed_cls;

  /**
   * Function to call when we are starting/stopping
   * a connection.  May be NULL.
   */
  MHD_NotifyConnectionCallback notify_connection;

  /**
   * Closure argument to @e notify_connection.
   */
  void *notify_connection_cls;

  /**
   * Function to call with the full URI at the
   * beginning of request processing.  May be NULL.
   * <p>
   * Returns the initial pointer to internal state
   * kept by the client for the request.
   */
  LogCallback uri_log_callback;

  /**
   * Closure argument to @e uri_log_callback.
   */
  void *uri_log_callback_cls;

  /**
   * Function to call when we unescape escape sequences.
   */
  UnescapeCallback unescape_callback;

  /**
   * Closure for @e unescape_callback.
   */
  void *unescape_callback_cls;

#ifdef HAVE_MESSAGES
  /**
   * Function for logging error messages (if we
   * support error reporting).
   */
  void (*custom_error_log) (void *cls, const char *fmt, va_list va);

  /**
   * Closure argument to @e custom_error_log.
   */
  void *custom_error_log_cls;
#endif

  /**
   * Pointer to master daemon (NULL if this is the master)
   */
  struct MHD_Daemon *master;

  /**
   * Worker daemons (one per thread)
   */
  struct MHD_Daemon *worker_pool;

  /**
   * Table storing number of connections per IP
   */
  void *per_ip_connection_count;

  /**
   * Size of the per-connection memory pools.
   */
  size_t pool_size;

  /**
   * Increment for growth of the per-connection memory pools.
   */
  size_t pool_increment;

  /**
   * Size of threads created by MHD.
   */
  size_t thread_stack_size;

  /**
   * Number of worker daemons
   */
  unsigned int worker_pool_size;

  /**
   * The select thread handle (if we have internal select)
   */
  MHD_thread_handle_ pid;

  /**
   * Mutex for per-IP connection counts.
   */
  MHD_mutex_ per_ip_connection_mutex;

  /**
   * Mutex for (modifying) access to the "cleanup", "normal_timeout" and
   * "manual_timeout" DLLs.
   */
  MHD_mutex_ cleanup_connection_mutex;

  /**
   * Listen socket.
   */
  MHD_socket listen_fd;

  /**
   * Whether to allow/disallow/ignore reuse of listening address.
   * The semantics is the following:
   * 0: ignore (user did not ask for neither allow/disallow, use SO_REUSEADDR
   *    except W32)
   * >0: allow (use SO_REUSEPORT on most platforms, SO_REUSEADDR on Windows)
   * <0: disallow (mostly no action, SO_EXCLUSIVEADDRUSE on Windows or SO_EXCLBIND
   *     on Solaris)
   */
  int listening_address_reuse;

#ifdef EPOLL_SUPPORT
  /**
   * File descriptor associated with our epoll loop.
   */
  int epoll_fd;

  /**
   * true if the listen socket is in the 'epoll' set,
   * false if not.
   */
  bool listen_socket_in_epoll;

#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  /**
   * File descriptor associated with the #run_epoll_for_upgrade() loop.
   * Only available if #MHD_USE_HTTPS_EPOLL_UPGRADE is set.
   */
  int epoll_upgrade_fd;

  /**
   * true if @e epoll_upgrade_fd is in the 'epoll' set,
   * false if not.
   */
  bool upgrade_fd_in_epoll;
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */

#endif

  /**
   * Inter-thread communication channel.
   */
  struct MHD_itc_ itc;

  /**
   * Are we shutting down?
   */
  volatile bool shutdown;

  /**
   * Has this deamon been quiesced via #MHD_quiesce_daemon()?
   * If so, we should no longer use the @e listen_fd (including
   * removing it from the @e epoll_fd when possible).
   */
  volatile bool was_quiesced;

  /**
   * Did we hit some system or process-wide resource limit while
   * trying to accept() the last time? If so, we don't accept new
   * connections until we close an existing one.  This effectively
   * temporarily lowers the "connection_limit" to the current
   * number of connections.
   */
  bool at_limit;

  /*
   * Do we need to process resuming connections?
   */
  bool resuming;

  /**
   * 'True' if some data is already waiting to be processed.
   * If set to 'true' - zero timeout for select()/poll*()
   * is used.
   * Should be reset each time before processing connections
   * and raised by any connection which require additional
   * immediately processing (application does not provide
   * data for response, data waiting in TLS buffers etc.)
   */
  bool data_already_pending;

  /**
   * Number of active parallel connections.
   */
  unsigned int connections;

  /**
   * Limit on the number of parallel connections.
   */
  unsigned int connection_limit;

  /**
   * After how many seconds of inactivity should
   * connections time out?  Zero for no timeout.
   */
  time_t connection_timeout;

  /**
   * Maximum number of connections per IP, or 0 for
   * unlimited.
   */
  unsigned int per_ip_connection_limit;

  /**
   * Daemon's flags (bitfield).
   */
  enum MHD_FLAG options;

  /**
   * Listen port.
   */
  uint16_t port;

#ifdef HTTPS_SUPPORT
#ifdef UPGRADE_SUPPORT
  /**
   * Head of DLL of upgrade response handles we are processing.
   * Used for upgraded TLS connections when thread-per-connection
   * is not used.
   */
  struct MHD_UpgradeResponseHandle *urh_head;

  /**
   * Tail of DLL of upgrade response handles we are processing.
   * Used for upgraded TLS connections when thread-per-connection
   * is not used.
   */
  struct MHD_UpgradeResponseHandle *urh_tail;
#endif /* UPGRADE_SUPPORT */

  /**
   * Desired cipher algorithms.
   */
  gnutls_priority_t priority_cache;

  /**
   * What kind of credentials are we offering
   * for SSL/TLS?
   */
  gnutls_credentials_type_t cred_type;

  /**
   * Server x509 credentials
   */
  gnutls_certificate_credentials_t x509_cred;

  /**
   * Diffie-Hellman parameters
   */
  gnutls_dh_params_t dh_params;

#if GNUTLS_VERSION_MAJOR >= 3
  /**
   * Function that can be used to obtain the certificate.  Needed
   * for SNI support.  See #MHD_OPTION_HTTPS_CERT_CALLBACK.
   */
  gnutls_certificate_retrieve_function2 *cert_callback;
#endif

  /**
   * Pointer to our SSL/TLS key (in ASCII) in memory.
   */
  const char *https_mem_key;

  /**
   * Pointer to our SSL/TLS certificate (in ASCII) in memory.
   */
  const char *https_mem_cert;

  /**
   * Pointer to 0-terminated HTTPS passphrase in memory.
   */
  const char *https_key_password;

  /**
   * Pointer to our SSL/TLS certificate authority (in ASCII) in memory.
   */
  const char *https_mem_trust;

  /**
   * Our Diffie-Hellman parameters in memory.
   */
  gnutls_dh_params_t https_mem_dhparams;

  /**
   * true if we have initialized @e https_mem_dhparams.
   */
  bool have_dhparams;

#endif /* HTTPS_SUPPORT */

#ifdef DAUTH_SUPPORT

  /**
   * Character array of random values.
   */
  const char *digest_auth_random;

  /**
   * An array that contains the map nonce-nc.
   */
  struct MHD_NonceNc *nnc;

  /**
   * A rw-lock for synchronizing access to @e nnc.
   */
  MHD_mutex_ nnc_lock;

  /**
   * Size of `digest_auth_random.
   */
  size_t digest_auth_rand_size;

  /**
   * Size of the nonce-nc array.
   */
  unsigned int nonce_nc_size;

#endif

#ifdef TCP_FASTOPEN
  /**
   * The queue size for incoming SYN + DATA packets.
   */
  unsigned int fastopen_queue_size;
#endif

  /**
   * The size of queue for listen socket.
   */
  unsigned int listen_backlog_size;
};


#if EXTRA_CHECKS
#define EXTRA_CHECK(a) do { if (!(a)) abort(); } while (0)
#else
#define EXTRA_CHECK(a)
#endif


/**
 * Insert an element at the head of a DLL. Assumes that head, tail and
 * element are structs with prev and next fields.
 *
 * @param head pointer to the head of the DLL
 * @param tail pointer to the tail of the DLL
 * @param element element to insert
 */
#define DLL_insert(head,tail,element) do { \
  EXTRA_CHECK (NULL == (element)->next); \
  EXTRA_CHECK (NULL == (element)->prev); \
  (element)->next = (head); \
  (element)->prev = NULL; \
  if ((tail) == NULL) \
    (tail) = element; \
  else \
    (head)->prev = element; \
  (head) = (element); } while (0)


/**
 * Remove an element from a DLL. Assumes
 * that head, tail and element are structs
 * with prev and next fields.
 *
 * @param head pointer to the head of the DLL
 * @param tail pointer to the tail of the DLL
 * @param element element to remove
 */
#define DLL_remove(head,tail,element) do { \
  EXTRA_CHECK ( (NULL != (element)->next) || ((element) == (tail)));  \
  EXTRA_CHECK ( (NULL != (element)->prev) || ((element) == (head)));  \
  if ((element)->prev == NULL) \
    (head) = (element)->next;  \
  else \
    (element)->prev->next = (element)->next; \
  if ((element)->next == NULL) \
    (tail) = (element)->prev;  \
  else \
    (element)->next->prev = (element)->prev; \
  (element)->next = NULL; \
  (element)->prev = NULL; } while (0)



/**
 * Insert an element at the head of a XDLL. Assumes that head, tail and
 * element are structs with prevX and nextX fields.
 *
 * @param head pointer to the head of the XDLL
 * @param tail pointer to the tail of the XDLL
 * @param element element to insert
 */
#define XDLL_insert(head,tail,element) do { \
  EXTRA_CHECK (NULL == (element)->nextX); \
  EXTRA_CHECK (NULL == (element)->prevX); \
  (element)->nextX = (head); \
  (element)->prevX = NULL; \
  if (NULL == (tail)) \
    (tail) = element; \
  else \
    (head)->prevX = element; \
  (head) = (element); } while (0)


/**
 * Remove an element from a XDLL. Assumes
 * that head, tail and element are structs
 * with prevX and nextX fields.
 *
 * @param head pointer to the head of the XDLL
 * @param tail pointer to the tail of the XDLL
 * @param element element to remove
 */
#define XDLL_remove(head,tail,element) do { \
  EXTRA_CHECK ( (NULL != (element)->nextX) || ((element) == (tail)));  \
  EXTRA_CHECK ( (NULL != (element)->prevX) || ((element) == (head)));  \
  if (NULL == (element)->prevX) \
    (head) = (element)->nextX;  \
  else \
    (element)->prevX->nextX = (element)->nextX; \
  if (NULL == (element)->nextX) \
    (tail) = (element)->prevX;  \
  else \
    (element)->nextX->prevX = (element)->prevX; \
  (element)->nextX = NULL; \
  (element)->prevX = NULL; } while (0)


/**
 * Insert an element at the head of a EDLL. Assumes that head, tail and
 * element are structs with prevE and nextE fields.
 *
 * @param head pointer to the head of the EDLL
 * @param tail pointer to the tail of the EDLL
 * @param element element to insert
 */
#define EDLL_insert(head,tail,element) do { \
  (element)->nextE = (head); \
  (element)->prevE = NULL; \
  if ((tail) == NULL) \
    (tail) = element; \
  else \
    (head)->prevE = element; \
  (head) = (element); } while (0)


/**
 * Remove an element from a EDLL. Assumes
 * that head, tail and element are structs
 * with prevE and nextE fields.
 *
 * @param head pointer to the head of the EDLL
 * @param tail pointer to the tail of the EDLL
 * @param element element to remove
 */
#define EDLL_remove(head,tail,element) do { \
  if ((element)->prevE == NULL) \
    (head) = (element)->nextE;  \
  else \
    (element)->prevE->nextE = (element)->nextE; \
  if ((element)->nextE == NULL) \
    (tail) = (element)->prevE;  \
  else \
    (element)->nextE->prevE = (element)->prevE; \
  (element)->nextE = NULL; \
  (element)->prevE = NULL; } while (0)


/**
 * Convert all occurrences of '+' to ' '.
 *
 * @param arg string that is modified (in place), must be 0-terminated
 */
void
MHD_unescape_plus (char *arg);


/**
 * Callback invoked when iterating over @a key / @a value
 * argument pairs during parsing.
 *
 * @param connection context of the iteration
 * @param key 0-terminated key string, never NULL
 * @param value 0-terminated value string, may be NULL
 * @param kind origin of the key-value pair
 * @return #MHD_YES on success (continue to iterate)
 *         #MHD_NO to signal failure (and abort iteration)
 */
typedef int
(*MHD_ArgumentIterator_)(struct MHD_Connection *connection,
			 const char *key,
			 const char *value,
			 enum MHD_ValueKind kind);


/**
 * Parse and unescape the arguments given by the client
 * as part of the HTTP request URI.
 *
 * @param kind header kind to pass to @a cb
 * @param connection connection to add headers to
 * @param[in|out] args argument URI string (after "?" in URI),
 *        clobbered in the process!
 * @param cb function to call on each key-value pair found
 * @param[out] num_headers set to the number of headers found
 * @return #MHD_NO on failure (@a cb returned #MHD_NO),
 *         #MHD_YES for success (parsing succeeded, @a cb always
 *                               returned #MHD_YES)
 */
int
MHD_parse_arguments_ (struct MHD_Connection *connection,
		      enum MHD_ValueKind kind,
		      char *args,
		      MHD_ArgumentIterator_ cb,
		      unsigned int *num_headers);


#endif
