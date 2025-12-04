/**
 * @file uaccess.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief User space memory access接口实现
 * @version alpha-1.0.0
 * @date 2025-12-04
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <syscall/uaccess.h>
#include <string.h>

void ua_memcpy(void *dst, const void *src, size_t size)
{
    ua_start_access();
    memcpy(dst, src, size);
    ua_end_access();
}

void ua_strcpy(char *dst, const char *src)
{
    ua_start_access();
    strcpy(dst, src);
    ua_end_access();
}

size_t ua_strlen(const char *str)
{
    ua_start_access();
    size_t len = strlen(str);
    ua_end_access();
    return len;
}