#ifndef MUSIKCORE_DETAIL_ASYNC_HPP
#define MUSIKCORE_DETAIL_ASYNC_HPP

#include <musikcore/context.hpp>
#include <memory>
#include <utility>

namespace musik { namespace core { namespace api { namespace detail {

        /* Shared submission adapter. Keep callback state owned locally until
           C admission succeeds, then release it to the terminal callback. */
        template<typename Completion, typename Start>
        mc_request_id submitRequest(Completion completion, Start start) {
            if (!completion) {
                throw Error(MC_ERROR_INVALID_ARGUMENT);
            }

            /* An accepted request owns the callback state until its terminal callback. */
            auto state = std::make_unique<Completion>(std::move(completion));
            mc_request_id request = 0;
            const auto result = start(state.get(), &request);
            if (result != MC_OK) {
                throw Error(result);
            }
            state.release();
            return request;
        }

} } } }

#endif
