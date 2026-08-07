
//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2019 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

/** @file NumberValidator.h @brief A templated numeric validator for input overlays. */
#pragma once

#include <cursespp/InputOverlay.h>
#include <musikcore/utfutil.h>
#include <musikcore/i18n/Locale.h>

namespace cursespp {

    /** @brief An InputOverlay::IValidator that accepts only numeric input within a range.
     *
     *  @details NumberValidator parses the entered text with std::stod. If the
     *  configured bounds are "bounded" (i.e. not the full numeric_limits<T>
     *  extremes), the parsed value must fall within [minimum, maximum]. A
     *  Formatter callback converts the boundary values into localized strings
     *  used when building the error message.
     *
     *  @tparam T the numeric type being validated (e.g. int, double).
     */
    template <typename T>
    struct NumberValidator : public InputOverlay::IValidator {
        /** @brief Converts a numeric boundary value into a display string.
         *  @param T the value to format.
         *  @return the formatted string.
         */
        using Formatter = std::function<std::string(T)>;

        /** @brief Creates a validator with an optional range.
         *  @param minimum the inclusive lower bound.
         *  @param maximum the inclusive upper bound.
         *  @param formatter callback used to format boundaries in error messages.
         */
        NumberValidator(T minimum, T maximum, Formatter formatter)
            : minimum(minimum), maximum(maximum), formatter(formatter) {
        }

        /** @brief Checks whether the input is a number within the configured range.
         *  @param input the raw input string.
         *  @return true if the input parses and satisfies the bounds.
         */
        bool IsValid(const std::string& input) const override {
            try {
                double result = std::stod(input);
                if (bounded(minimum, maximum) && (result < minimum || result > maximum)) {
                    return false;
                }
            }
            catch (std::invalid_argument) {
                return false;
            }
            return true;
        }

        /** @brief Returns the localized error message for invalid input.
         *  @return the error string, including bounds when the range is bounded.
         */
        const std::string ErrorMessage() const override {
            if (bounded(minimum, maximum)) {
                std::string result = _TSTR("validator_dialog_number_parse_bounded_error");
                u8replace(result, "{{minimum}}", formatter(minimum));
                u8replace(result, "{{maximum}}", formatter(maximum));
                return result;
            }
            return _TSTR("validator_dialog_number_parse_error");
        }

        /** @brief Returns whether the bounds are meaningful (not the extremes of T).
         *  @param minimum the lower bound.
         *  @param maximum the upper bound.
         *  @return true if the range is bounded.
         */
        static bool bounded(T minimum, T maximum) {
            return
                minimum != std::numeric_limits<T>::min() &&
                maximum != std::numeric_limits<T>::max();
        }

        Formatter formatter;   /**< Formats boundary values for error messages. */
        T minimum, maximum;    /**< The inclusive validation range. */
    };

}
