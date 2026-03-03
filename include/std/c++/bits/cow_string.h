/**
 * @file cow_string.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief COW String
 * @version alpha-1.0.0
 * @date 2026-03-03
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

// 虽然叫做COW String
// 但是我们并不会真的向其中写入数据
// 所以这实际上是个CNW String (Copy Never Write)

#include <cstddef>
#include <fwd/stringfwd.h>

namespace std {
    class __cow_string {
    private:
        struct {
            const char *data;
            size_t data_len;
        };
    public:
        __cow_string(const char *str, size_t len) : data(str), data_len(len) {}
        __cow_string(const string &str);
        const char *c_str() const {
            return data;
        }
        size_t size() const {
            return data_len;
        }
    };
}