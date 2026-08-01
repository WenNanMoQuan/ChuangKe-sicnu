#pragma once
#include <Arduino.h>

/**
 * @file    base64_util.h
 * @brief   内置 Base64 编码（无需外部 Base64 库，保证一键编译通过）
 * @note    仅用于把 OV2640 的 JPEG 帧编码为可放入 JSON 的字符串。
 */
static const char B64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static String base64_encode(const uint8_t *data, size_t len) {
    String out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t oct = ((uint32_t)data[i]) << 16;
        if (i + 1 < len) oct |= ((uint32_t)data[i + 1]) << 8;
        if (i + 2 < len) oct |= (uint32_t)data[i + 2];
        uint8_t n = (len - i >= 3) ? 3 : (uint8_t)(len - i);
        out += B64_CHARS[(oct >> 18) & 0x3F];
        out += B64_CHARS[(oct >> 12) & 0x3F];
        if (n > 1) out += B64_CHARS[(oct >> 6) & 0x3F]; else out += '=';
        if (n > 2) out += B64_CHARS[oct & 0x3F];        else out += '=';
    }
    return out;
}
