#include "gamestate.hpp"
#include "board.hpp"
#include "genmoves.hpp"
#include "pos.hpp"
#include <cctype>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

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
        // end of the board
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

    if (file != 8 || rank != 0) {
        return {};
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
        // spaces act as delimiters in FEN notation
        if (std::isspace(c)) {
            if (i > 0 && !is_last_space) {
                on = static_cast<on_t>(static_cast<int>(on) + 1);
            }
            is_last_space = true;
            continue;
        }

        // slice construction
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

        std::string sub_str = not_board.substr(slice_start, i - slice_start + 1);

        switch (on) {
        case on_t::active_color:
            state.active_color = sub_str[0] == 'w' ? PieceColor::white : PieceColor::black;
            break;
        case on_t::castling_rights: {
            for (size_t i = 0; i < sub_str.length(); i++) {
                char castle_rights = sub_str[i];
                pos_t pos = -1;
                switch (castle_rights) {
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
                case '-':
                    break;
                default:
                    return {};
                }
                if (pos >= 0 && pos < 64) {
                    state.board[pos].can_castle = true;
                }
            }
            break;
        }
        case on_t::en_passant:
            state.en_passant_pos = parse_pos(sub_str);
            break;
        case on_t::halfmove_clock:
            try {
                state.halfmove_clock = std::stoi(sub_str);
            } catch (const std::exception &e /* stoi can throw both out_of_range and invalid_arguement*/) {
                return {};
            }
            if (state.halfmove_clock > 50) {
                return {};
            }
            break;
        case on_t::move_count:
            try {
                state.on_move = std::stoi(sub_str);
            } catch (const std::exception &e /* stoi can throw both out_of_range and invalid_arguement*/) {
                return {};
            }
            // with a forced 50 move rule 5949 is the longest theoretical game
            if (state.on_move > 5949) {
                return {};
            }
            break;
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
        out << "   " << border << '\n';
        out << rank + 1 << "  | ";
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
    out << "   " << border << '\n';
    out << "     A   B   C   D   E   F   G   H\n";

    std::string white_castle_rights_str = "";
    const Piece *p = &state.board[0];
    if (p->type == PieceType::rook && p->color == PieceColor::white && p->can_castle) {
        white_castle_rights_str += "queen, ";
    }
    p = &state.board[7];
    if (p->type == PieceType::rook && p->color == PieceColor::white && p->can_castle) {
        white_castle_rights_str += "king, ";
    }
    if (white_castle_rights_str.empty()) {
        white_castle_rights_str = "none, ";
    }
    std::string black_castle_rights_str = "";
    p = &state.board[56];
    if (p->type == PieceType::rook && p->color == PieceColor::black && p->can_castle) {
        black_castle_rights_str += "queen, ";
    }
    p = &state.board[63];
    if (p->type == PieceType::rook && p->color == PieceColor::black && p->can_castle) {
        black_castle_rights_str += "king, ";
    }
    if (black_castle_rights_str.empty()) {
        black_castle_rights_str = "none, ";
    }

    out << '\n';
    out << "Active Color: " << piece_color_to_str(state.active_color)
        << ", Castle Rights: White: " << white_castle_rights_str << "Black: " << black_castle_rights_str
        << "En Passant pos: " << pos_format(state.en_passant_pos)
        << ", Halfmove clock: " << state.halfmove_clock
        << ", On Move " << state.on_move << '\n';
}

void chess::do_move(GameState &state, const Move &move) {
    state.board[move.to] = state.board[move.from];
    state.board[move.from].type = PieceType::none;
    state.board[move.from].color = PieceColor::none;
    state.active_color = state.active_color == PieceColor::white ? PieceColor::black : PieceColor::white;
}
void chess::undo_move(GameState &state, const Move &move) {
    state.board[move.from] = state.board[move.to];
    state.board[move.to] = move.to_piece;
    state.active_color = state.active_color == PieceColor::white ? PieceColor::black : PieceColor::white;
}

namespace chess {
static uint64_t perft_out_helper(GameState &state, uint32_t depth, uint32_t max, std::ostream &out) {
    if (depth >= max) {
        return 0;
    }
    std::vector<Move> moves;
    gen_legal_moves(state, moves);

    std::string indent = "";
    for (uint32_t i = 0; i < depth; i++) {
        indent += "  ";
    }

    out << indent << "{\n";
    std::stringstream ss;
    std::ostringstream oss;
    ouput_game_state(state, ss);
    std::string str;
    while (std::getline(ss, str)) {
        oss << indent << str << '\n';
    }
    out << oss.str() << '\n';

    out << indent << "Moves: " << moves.size() << '\n';
    for (const Move &move : moves) {
        out << indent << "  " << pos_format(move.from) << pos_format(move.to) << '\n';
    }
    if (depth >= max - 1) {
        out << indent << "}\n";
        return moves.size();
    }

    uint64_t move_count = 0;
    for (const Move &move : moves) {
        out << indent << "done move: " << pos_format(move.from) << pos_format(move.to) << '\n';
        do_move(state, move);
        move_count += perft_out_helper(state, depth + 1, max, out);
        undo_move(state, move);
    }
    out << indent << "total move count: " << move_count << '\n';
    out << indent << "}\n";
    return move_count;
}
} // namespace chess

uint64_t chess::perft(GameState &state, uint32_t depth) {
    if (depth == 0) {
        return 0;
    }
    std::vector<Move> moves;
    gen_legal_moves(state, moves);
    if (depth == 1) {
        return moves.size();
    }

    uint64_t move_count = 0;
    for (Move move : moves) {
        do_move(state, move);
        move_count += perft(state, depth - 1);
        undo_move(state, move);
    }
    return move_count;
}

uint64_t chess::perft_out(GameState &state, uint32_t depth, std::ostream &out) {
    return perft_out_helper(state, 0, depth, out);
}
