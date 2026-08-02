#include "gamestate.hpp"
#include "board.hpp"
#include <cctype>
#include <stdexcept>
#include <string>

std::string chess::active_color_to_str(ActiveColor ac) {
    if (ac == ActiveColor::white) {
        return "white";
    }
    return "black";
}

static bool is_char_in_PieceType(char c) {
    switch (static_cast<chess::PieceType>(c)) {
    case chess::PieceType::none:
    case chess::PieceType::pawn:
    case chess::PieceType::knight:
    case chess::PieceType::bishop:
    case chess::PieceType::rook:
    case chess::PieceType::queen:
    case chess::PieceType::king:
        return true;
    default:
        return false;
    }
}

std::optional<chess::GameState> chess::fen_parser(std::string_view sv) {
    if (sv.empty()) {
        return {};
    }

    GameState state;
    rank_t rank = 7;
    file_t file = 0;
    size_t i;
    for (i = 0; i < sv.length(); i++) {
        char c = sv[i];
        if (c == ' ') {
            break;
        }
        if (file > 8) {
            return {};
        }
        if (c == '/') {
            rank--;
            if (file < 8) {
                return {};
            }
            file = 0;
            continue;
        }

        if (std::isdigit(c)) {
            int asint = c - '0';
            if (asint == 0 || asint > 8 - file) {
                return {};
            }
            // The board is already initiliazed with all pieces set to none
            file += asint;
            continue;
        }

        char as_lower = std::tolower(c);
        if (!is_char_in_PieceType(as_lower)) {
            return {};
        }
        bool isupper = std::isupper(c);
        PieceType as_piece_type = static_cast<PieceType>(as_lower);
        if (as_piece_type == PieceType::none) {
            return {};
        }
        pos_t pos = rank * 8 + file;
        state.board[pos] = Piece{as_piece_type, isupper ? PieceColor::white : PieceColor::black};

        file++;
    }

    enum class on_t {
        active_color,
        castling_rights,
        en_passant,
        halfmove_clock,
        move_count,
    };

    std::string not_board = static_cast<std::string>(sv);
    on_t on = on_t::active_color;
    not_board = not_board.substr(i);
    bool is_last_space = false;
    int slice_start = -1;
    for (size_t i = 0; i < not_board.length(); i++) {
        char c = not_board[i];
        if (std::isspace(c)) {
            if (i > 0 and !is_last_space) {
                on = static_cast<on_t>(static_cast<int>(on) + 1);
            }
            is_last_space = true;
            continue;
        } else {
            is_last_space = false;
            if (slice_start == -1) {
                slice_start = i;
            }
            size_t next = i + 1;
            char char_next = ' ';
            bool slice_end = false;
            try {
                char_next = not_board.at(next);
            } catch (const std::out_of_range &e) {
                slice_end = true;
            }
            if (std::isspace(char_next)) {
                slice_end = true;
            }
            if (!slice_end) {
                continue;
            }
        }

        std::string sub_str = not_board.substr(slice_start, i - slice_start + 1);

        switch (on) {
        case on_t::active_color:
            state.active_color = sub_str[0] == 'w' ? ActiveColor::white : ActiveColor::black;
            break;
        case on_t::castling_rights: {
            for (size_t i = 0; i < sub_str.length(); i++) {
                pos_t pos = 0;
                switch (c) {
                case 'Q':
                    pos = 0;
                    break;
                case 'K':
                    pos = 7;
                    break;
                case 'q':
                    pos = 56;
                    break;
                case 'k':
                    pos = 63;
                    break;
                default:
                    return {};
                }
                state.board[pos].can_castle = true;
            }
            break;
        }
        case on_t::en_passant:
            state.en_passant_pos = parse_pos(sub_str);
            break;
        case on_t::halfmove_clock:
            state.halfmove_clock = std::stoi(sub_str);
            break;
        case on_t::move_count:
            state.move_count = std::stoi(sub_str);
            break;
        }
        slice_start = -1;
    }

    return state;
}

void chess::ouput_game_state(const GameState &state, std::ostream &out) {
    constexpr std::string_view border = "+-------------------------------+";
    for (rank_t inverted_rank = 0; inverted_rank < 8; inverted_rank++) {
        rank_t rank = 7 - inverted_rank;
        out << border << '\n';
        out << "| ";
        for (file_t file = 0; file < 8; file++) {
            pos_t pos = rank * 8 + file;
            char as_char = static_cast<char>(state.board[pos].type);
            if (state.board[pos].color == PieceColor::white) {
                as_char = std::toupper(as_char);
            }
            out << as_char << " | ";
        }
        out << '\n';
    }
    out << border << '\n';

    out << '\n';
    out << "Active Color: " << active_color_to_str(state.active_color)
        << " En Passant pos: " << static_cast<int>(state.en_passant_pos)
        << " Halfmove clock: " << state.halfmove_clock
        << " Move count: " << state.move_count << '\n';
}
