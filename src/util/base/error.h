#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace omvr::util {

// An expected failure: a code (a Windows error, an HRESULT, an XrResult, as
// the context says) captured at the failing call, and what was being done.
struct Error {
    std::int32_t     code = 0;
    std::string_view context;
};

template <class T>
using Result = std::expected<T, Error>;

} // namespace omvr::util
