#include "mc/mc_text.h"

/* 从 s 读 n 位十进制数字；任一位不是数字返回 false */
static bool read_digits(const char *s, size_t n, uint32_t *out)
{
    uint32_t v = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        v = v * 10u + (uint32_t)(s[i] - '0');
    }
    *out = v;
    return true;
}

bool mc_datetime_parse(const char *s, size_t len, mc_datetime_t *out)
{
    if (len != MC_DATETIME_TEXT_LEN || s[4] != '-' || s[7] != '-' || s[10] != ' ' || s[13] != ':' ||
        s[16] != ':') {
        return false;
    }
    uint32_t y, mo, d, h, mi, se;
    if (!read_digits(s, 4, &y) || !read_digits(s + 5, 2, &mo) || !read_digits(s + 8, 2, &d) ||
        !read_digits(s + 11, 2, &h) || !read_digits(s + 14, 2, &mi) || !read_digits(s + 17, 2, &se)) {
        return false;
    }
    /* 各字段已被位数限死在 uint8/int16 范围内，强转不丢值；合法性交给 mc_datetime_valid */
    mc_datetime_t dt = {{(int16_t)y, (uint8_t)mo, (uint8_t)d}, (uint8_t)h, (uint8_t)mi, (uint8_t)se};
    if (!mc_datetime_valid(&dt)) {
        return false;
    }
    *out = dt;
    return true;
}

static void write_digits(char *p, uint32_t v, size_t n)
{
    for (size_t i = n; i > 0; --i) {
        p[i - 1u] = (char)('0' + v % 10u);
        v /= 10u;
    }
}

void mc_datetime_format(const mc_datetime_t *dt, char out[MC_DATETIME_TEXT_LEN + 1u])
{
    if (dt->date.year >= 0 && dt->date.year <= 9999) {
        write_digits(out, (uint32_t)dt->date.year, 4);
    } else {
        out[0] = out[1] = out[2] = out[3] = '-';
    }
    out[4] = '-';
    write_digits(out + 5, dt->date.month, 2);
    out[7] = '-';
    write_digits(out + 8, dt->date.day, 2);
    out[10] = ' ';
    write_digits(out + 11, dt->hour, 2);
    out[13] = ':';
    write_digits(out + 14, dt->minute, 2);
    out[16] = ':';
    write_digits(out + 17, dt->second, 2);
    out[MC_DATETIME_TEXT_LEN] = '\0';
}
