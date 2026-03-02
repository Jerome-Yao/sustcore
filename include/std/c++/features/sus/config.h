/**
 * @file config.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief configuraions
 * @version alpha-1.0.0
 * @date 2026-03-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <features/default/config.h>

namespace std::__config {
    using ::__defconf::integral_traps;
    using ::__defconf::float_has_denorm_loss;
    using ::__defconf::float_traps;
    using ::__defconf::float_tinyness_before;
    using ::__defconf::double_has_denorm_loss;
    using ::__defconf::double_traps;
    using ::__defconf::double_tinyness_before;
    using ::__defconf::long_double_has_denorm_loss;
    using ::__defconf::long_double_traps;
    using ::__defconf::long_double_tinyness_before;
}