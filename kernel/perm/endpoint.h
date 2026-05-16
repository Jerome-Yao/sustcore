/**
 * @file endpoint.h
 * @brief Endpoint Permissions
 */

#pragma once

#include <sus/types.h>
#include <sustcore/capability.h>

namespace perm::endpoint {
    constexpr b64 GRANT = 0x01'0000;
    constexpr b64 READ  = 0x02'0000;
    constexpr b64 WRITE = 0x04'0000;
}  // namespace perm::endpoint
