#pragma once

#include <Unknwn.h>
#include <winrt/base.h>

#include "util/Log.h"

namespace overlaydesk {

using winrt::com_ptr;

namespace detail {

// Logs before throwing. A bare winrt::check_hresult loses the call site, and the graphics
// bring-up path is exactly where a post-mortem log matters most.
inline void CheckHResult(HRESULT hr, const char* expression, const char* file, int line) {
    if (FAILED(hr)) {
        LogError("HRESULT 0x{:08X} from `{}` at {}:{}", static_cast<uint32_t>(hr), expression, file,
                 line);
        winrt::throw_hresult(hr);
    }
}

}  // namespace detail

// Wrap every COM/D3D call whose failure should abort the operation in progress.
#define OS_CHECK_HR(expr) ::overlaydesk::detail::CheckHResult((expr), #expr, __FILE__, __LINE__)

// Formats whatever exception is in flight at a catch(...) boundary into a loggable
// string. RNF-009: a capture or device failure must be reported, not swallowed, and must
// not take the process down with it.
inline std::string DescribeCurrentException() noexcept {
    try {
        throw;
    } catch (const winrt::hresult_error& e) {
        try {
            return std::format("hresult 0x{:08X}: {}", static_cast<uint32_t>(e.code()),
                               winrt::to_string(e.message()));
        } catch (...) {
            return "hresult_error";
        }
    } catch (const std::exception& e) {
        try {
            return e.what();
        } catch (...) {
            return "std::exception";
        }
    } catch (...) {
        return "unknown exception";
    }
}

}  // namespace overlaydesk
