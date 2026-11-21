#ifndef MUSIKCORE_ONLINE_H
#define MUSIKCORE_ONLINE_H

#include <musikcore/context.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (MC_CALL *mc_online_text_fn)(mc_result status, mc_string_view text, void* user_data);
typedef void (MC_CALL *mc_lastfm_link_fn)(
    mc_result status, mc_string_view token, mc_string_view url, void* user_data);

/* Accepted requests complete through context dispatch or shutdown. Copy output
   views during the callback to retain them. Inputs are copied at submission.
   Keep user_data valid until completion and use mc_request_cancel to cancel. */
MC_API mc_result MC_CALL mc_lyrics_find_async(mc_context* context, mc_string_view artist, mc_string_view title,
    mc_online_text_fn complete, void* user_data, mc_request_id* out_request);

/* Request an authorization URL. Retain the token to finish linking after the
   listener authorizes the application in a browser. */
MC_API mc_result MC_CALL mc_lastfm_request_link_async(mc_context* context,
    mc_lastfm_link_fn complete, void* user_data, mc_request_id* out_request);

/* Register the authorized account and persist its credentials on success.
   Returns the account name to the completion. */
MC_API mc_result MC_CALL mc_lastfm_complete_link_async(mc_context* context, mc_string_view token,
    mc_online_text_fn complete, void* user_data, mc_request_id* out_request);

/* Read the linked account name inline. If unlinked, return MC_ERROR_NOT_FOUND
   and skip the callback. The text expires as soon as the callback returns. */
MC_API mc_result MC_CALL mc_lastfm_read_account(mc_online_text_fn read, void* user_data);

/* Clear the persisted credentials. An unlinked account may be cleared again. */
MC_API mc_result MC_CALL mc_lastfm_unlink(void);

#ifdef __cplusplus
}
#endif

#endif
