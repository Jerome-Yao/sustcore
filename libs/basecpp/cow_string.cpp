/**
 * @file cow_string.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief cow_string的实现
 * @version alpha-1.0.0
 * @date 2026-03-03
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <bits/cow_string.h>
#include <string>

namespace std {
    __cow_string::__cow_string(const string &str)
        : data(str.data()), data_len(str.size())
    {
    }
}