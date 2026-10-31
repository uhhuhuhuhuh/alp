#include "square.hpp"

#include <format>
#include <string>

chess::sqr_t chess::parse_sqr(std::string_view sv) {
    if (sv.length() != 2) { return -1; }
    sqr_t sqr = 0;
    if (sv[0] >= 'a' && sv[0] <= 'h') {
        sqr += sv[0] - 'a';
    } else {
        return -1;
    }
    if (sv[1] >= '1' && sv[1] <= '8') {
        sqr += 8 * (sv[1] - '1');
    } else {
        return -1;
    }
    return sqr;
}

std::string chess::format_sqr(sqr_t sqr) {
    return sqr < 0 || sqr > 63 ? std::to_string(sqr)
                               : std::format("{}{}", char(sqr % 8 + 'a'), char(sqr / 8 + '1'));
}
