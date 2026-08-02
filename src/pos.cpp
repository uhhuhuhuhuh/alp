#include "pos.hpp"

chess::pos_t chess::parse_pos(std::string_view sv) {
    if (sv.length() != 2) {
        return -1;
    }
    pos_t pos = 0;
    if (sv[0] >= 'a' && sv[0] <= 'h') {
        pos += sv[0] - 'a';
    } else {
        return -1;
    }
    if (sv[1] >= '1' && sv[1] <= '8') {
        pos += 8 * (sv[1] - '0');
    } else {
        return -1;
    }
    return pos;
}
