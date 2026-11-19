#ifndef MUSIKCORE_FFI_CONSUMER_REQUEST_H
#define MUSIKCORE_FFI_CONSUMER_REQUEST_H

#include <musikcore/musikcore.h>
#include <functional>

namespace musik { namespace core { namespace ffi {

    using RequestCompletion = void (MC_CALL *)(mc_result, void *);

    /* Work owns its inputs because cancellation can dispatch before the worker returns. */
    mc_result submit_request(mc_context *context, std::function<mc_result()> work,
        RequestCompletion completion, void *user_data, mc_request_id *out_request);

} } }

#endif
