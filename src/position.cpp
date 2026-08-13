#include "position.hpp"
#include "genmoves.hpp"
#include "piece.hpp"
#include "square.hpp"
#include <cctype>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

namespace chess {
static char castle_rights_to_char(CastleRights &rights) {
    int index = std::countr_zero(static_cast<unsigned int>(rights));
    if (static_cast<std::size_t>(index) < sizeof(unsigned int)) {
        rights &= rights - 1;
    }
    switch (index) {
    case 0:  return 'Q';
    case 1:  return 'K';
    case 2:  return 'q';
    case 3:  return 'k';
    default: return '-';
    }
}
} // namespace chess

void chess::output_pos_as_fen(const Position &pos, std::ostream &out) {
    // irank = inverted rank
    for (rank_t irank = 0; irank < 8; irank++) {
        rank_t rank = 7 - irank;
        file_t empty_count = 0;
        for (file_t file = 0; file < 8; file++) {
            sqr_t sqr = rank * 8 + file;
            if (pos.board[sqr].type == PieceType::none) {
                empty_count += 1;
                continue;
            }
            if (empty_count != 0) {
                out << empty_count;
                empty_count = 0;
            }
            char as_char = static_cast<char>(pos.board[sqr].type);
            if (pos.board[sqr].color == Color::white) {
                as_char = std::toupper(as_char);
            }
            out << as_char;
        }
        if (empty_count != 0) {
            out << empty_count;
            empty_count = 0;
        }
        if (irank < 7) {
            out << '/';
        }
    }

    out << ' ';
    if (pos.state.active_color == Color::white) {
        out << 'w';
    } else {
        out << 'b';
    }

    out << ' ';
    CastleRights copy = pos.state.castle_rights;
    do {
        out << castle_rights_to_char(copy);
    } while (static_cast<unsigned int>(copy) != 0);

    out << ' ';
    if (pos.state.en_passant_sqr == -1) {
        out << '-';
    } else {
        out << sqr_format(pos.state.en_passant_sqr);
    }

    out << ' ' << pos.state.halfmove_clock;
    out << ' ' << pos.state.on_move << '\n';
}

namespace chess {
static bool is_char_in_PieceType(char c) {
    switch (static_cast<chess::PieceType>(c)) {
    case PieceType::none:
    case PieceType::pawn:
    case PieceType::knight:
    case PieceType::bishop:
    case PieceType::rook:
    case PieceType::queen:
    case PieceType::king:   return true;
    default:                return false;
    }
}

static bool set_castle_rights(Position &pos, const std::string &sub_str) {
    for (std::size_t i = 0; i < sub_str.length(); i++) {
        char c = sub_str[i];
        sqr_t sqr = -1;
        Color color;
        CastleRights rights = CastleRights::none;
        switch (c) {
        case 'Q':
            rights = CastleRights::white_queen;
            sqr = a1;
            color = Color::white;
            break;
        case 'K':
            rights = CastleRights::white_king;
            sqr = h1;
            color = Color::white;
            break;
        case 'q':
            rights = CastleRights::black_queen;
            sqr = a8;
            color = Color::black;
            break;
        case 'k':
            rights = CastleRights::black_king;
            sqr = h8;
            color = Color::black;
            break;
        case '-': break;
        default:  return true;
        }
        if (sqr == -1) {
            continue;
        }
        if (pos.board[sqr].type != PieceType::rook || pos.board[sqr].color != color) {
            return true;
        }

        sqr_t king_sqr = 4;
        if (color == Color::black) {
            king_sqr = 60;
        }
        if (pos.board[king_sqr].type != PieceType::king || pos.board[king_sqr].color != color) {
            return true;
        }
        pos.state.castle_rights |= rights;
    }
    return false;
}
} // namespace chess

std::optional<chess::Position> chess::fen_parser(std::string_view sv) {
    if (sv.empty()) {
        return {};
    }

    Position pos;
    rank_t rank = 7;
    file_t file = 0;
    std::size_t i;
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
        sqr_t sqr = rank * 8 + file;
        Color color = isupper ? Color::white : Color::black;
        pos.board[sqr] = Piece{as_piece_type, color};
        if (as_piece_type == PieceType::king) {
            const auto &king_bitboards = pos.bb.get_kings();
            if ((isupper && king_bitboards[Color::white] != 0ULL) ||
                (!isupper && king_bitboards[Color::black] != 0ULL)) {
                // king position has already been assigned meaning there are two kings
                return {};
            }
        }

        pos.bb.set_or_bitboard(as_piece_type, color, 1ULL << sqr);

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
    for (std::size_t i = 0; i < not_board.length(); i++) {
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
        std::size_t next = i + 1;
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
            char c;
            try {
                c = sub_str[0];
            } catch (const std::exception &e) {
                return {};
            }
            pos.state.active_color = c == 'w' ? Color::white : Color::black;
            break;
        case on_t::castling_rights: {
            if (set_castle_rights(pos, sub_str)) {
                return {};
            }
            break;
        }
        case on_t::en_passant: pos.state.en_passant_sqr = parse_sqr(sub_str); break;
        case on_t::halfmove_clock:
            try {
                pos.state.halfmove_clock =
                    std::stoi(sub_str); // comment here cuz clang-format is dumb
            } catch (const std::exception &e) {
                return {};
            }
            if (pos.state.halfmove_clock > 50) {
                return {};
            }
            break;
        case on_t::move_count:
            try {
                pos.state.on_move = std::stoi(sub_str); // comment here cuz clang-format is dumb
            } catch (const std::exception &e) {
                return {};
            }
            // with a forced 50 move rule 5949 is the longest theoretical game
            if (pos.state.on_move > 5949) {
                return {};
            }
            break;
            break;
        }
        slice_start = -1;
    }

    return pos;
}

void chess::ouput_pos(const Position &pos, std::ostream &out) {
    // irank = inverted rank
    for (rank_t irank = 0; irank < 8; irank++) {
        rank_t rank = 7 - irank;
        out << rank + 1 << "  ";
        for (file_t file = 0; file < 8; file++) {
            sqr_t sqr = rank * 8 + file;
            char as_char = static_cast<char>(pos.board[sqr].type);
            if (pos.board[sqr].color == Color::white) {
                as_char = std::toupper(as_char);
            }
            out << as_char << " ";
        }
        out << '\n';
    }
    out << '\n';
    out << "   A B C D E F G H\n";

    output_pos_as_fen(pos, out);
}

namespace chess {
static void strip_castle_rights(const Position &pos, State &state, sqr_t sqr) {
    PieceType type = pos.board[sqr].type;
    Color color = pos.board[sqr].color;

    if (sqr == a1 && type == PieceType::rook && color == Color::white) {
        state.castle_rights &= ~CastleRights::white_queen;
        return;
    }
    if (sqr == h1 && type == PieceType::rook && color == Color::white) {
        state.castle_rights &= ~CastleRights::white_king;
        return;
    }
    if (sqr == e1 && type == PieceType::king && color == Color::white) {
        state.castle_rights &= ~(CastleRights::white_queen | CastleRights::white_king);
        return;
    }

    if (sqr == a8 && type == PieceType::rook && color == Color::black) {
        state.castle_rights &= ~CastleRights::black_queen;
        return;
    }
    if (sqr == h8 && type == PieceType::rook && color == Color::black) {
        state.castle_rights &= ~CastleRights::black_king;
        return;
    }
    if (sqr == e8 && type == PieceType::king && color == Color::black) {
        state.castle_rights &= ~(CastleRights::black_queen | CastleRights::black_king);
        return;
    }
}
} // namespace chess

void chess::do_move(Position &pos, const Move &move) {
    State state;
    state = pos.state;
    state.en_passant_sqr = -1;
    state.captured_piece = pos.board[move.to];
    state.active_color = !pos.state.active_color;
    state.ply++;
    if (state.ply % 2 == 0) {
        state.on_move++;
    }
    strip_castle_rights(pos, state, move.from);

    pos.history.push_back(pos.state);

    Piece &from_piece = pos.board[move.from];

    rank_t from_file = move.from % 8;
    rank_t to_file = move.to % 8;
    rank_t rank_dist = abs(move.from / 8 - move.to / 8);
    file_t file_dist = abs(from_file - to_file);

    const bool is_from_white = from_piece.color == Color::white;
    if (from_piece.type == PieceType::pawn) {
        if (rank_dist == 2) {
            state.en_passant_sqr = move.to;
        } else if (pos.board[move.to].type == PieceType::none && file_dist == 1) {
            rank_t rank_sign = is_from_white ? 1 : -1;
            sqr_t capture_sqr = move.to + (-8 * rank_sign);
            pos.bb.set_and_bitboard(PieceType::pawn, !from_piece.color, ~(1ULL << capture_sqr));
            pos.board[capture_sqr].type = PieceType::none;
        }
    } else if (from_piece.type == PieceType::king && file_dist == 2) {
        const bool queen_side = to_file < from_file;
        sqr_t from_rook_sqr = is_from_white ? (queen_side ? a1 : h8) : (queen_side ? a8 : h8);
        sqr_t to_rook_sqr = move.to + (queen_side ? 1 : -1);

        pos.bb.set_or_bitboard(PieceType::rook, from_piece.color, 1ULL << to_rook_sqr);
        pos.board[to_rook_sqr].type = PieceType::rook;
        pos.board[to_rook_sqr].color = from_piece.color;
        pos.bb.set_and_bitboard(PieceType::rook, from_piece.color, ~(1ULL << from_rook_sqr));
        pos.board[from_rook_sqr].type = PieceType::none;
    }

    pos.bb.set_or_bitboard(from_piece.type, from_piece.color, 1ULL << move.to);
    pos.board[move.to] = pos.board[move.from];
    pos.bb.set_and_bitboard(from_piece.type, from_piece.color, ~(1ULL << move.from));
    pos.board[move.from].type = PieceType::none;
    if (state.captured_piece.type != PieceType::none) {
        pos.bb.set_and_bitboard(
            state.captured_piece.type, state.captured_piece.color, ~(1ULL << move.to)
        );
    }

    pos.state = state;
    pos.state.checks_bb = attacks_to_king(pos, pos.board[move.to].color);
}

void chess::undo_move(Position &pos, const Move &move) {
    Piece &to_piece = pos.board[move.to];

    Piece captured = pos.state.captured_piece;

    rank_t from_file = move.from % 8;
    rank_t to_file = move.to % 8;
    file_t file_dist = abs(from_file - to_file);

    const bool is_to_white = to_piece.color == Color::white;
    if (captured.type != PieceType::none) {
        pos.bb.set_or_bitboard(captured.type, captured.color, 1ULL << move.to);
    } else if (to_piece.type == PieceType::pawn && file_dist == 1) {
        rank_t rank_sign = is_to_white ? 1 : -1;
        sqr_t capture_sqr = move.to + (-8 * rank_sign);
        pos.bb.set_or_bitboard(PieceType::pawn, !to_piece.color, 1ULL << capture_sqr);
        pos.board[capture_sqr].type = PieceType::pawn;
        pos.board[capture_sqr].color = !to_piece.color;
    }

    if (to_piece.type == PieceType::king && file_dist == 2) {
        const bool queen_side = to_file < from_file;
        sqr_t from_rook_sqr = is_to_white ? (queen_side ? a1 : h8) : (queen_side ? a8 : h8);
        sqr_t to_rook_sqr = move.to + (queen_side ? 1 : -1);

        pos.bb.set_or_bitboard(PieceType::rook, to_piece.color, 1ULL << from_rook_sqr);
        pos.board[from_rook_sqr].type = PieceType::rook;
        pos.board[from_rook_sqr].color = to_piece.color;
        pos.bb.set_and_bitboard(PieceType::rook, to_piece.color, ~(1ULL << to_rook_sqr));
        pos.board[to_rook_sqr].type = PieceType::none;
    }

    pos.bb.set_and_bitboard(to_piece.type, to_piece.color, ~(1ULL << move.to));
    pos.bb.set_or_bitboard(to_piece.type, to_piece.color, 1ULL << move.from);

    pos.board[move.from] = pos.board[move.to];
    pos.board[move.to] = captured;
    if (captured.type != PieceType::none) {
        pos.bb.set_or_bitboard(captured.type, captured.color, 1ULL << move.to);
    }

    pos.state = pos.history.back();
    pos.history.pop_back();
}

namespace chess {
static uint64_t perft_out_helper(Position &pos, uint32_t depth, uint32_t max, std::ostream &out) {
    if (depth >= max) {
        return 0;
    }
    std::vector<Move> moves;
    gen_legal_moves(pos, moves);

    std::string indent = "";
    for (uint32_t i = 0; i < depth; i++) {
        indent += "  ";
    }

    out << indent << "{\n";
    std::stringstream ss;
    std::ostringstream oss;
    ouput_pos(pos, ss);
    std::string str;
    while (std::getline(ss, str)) {
        oss << indent << str << '\n';
    }
    out << oss.str() << '\n';

    out << indent << "Moves: " << moves.size() << '\n';
    for (const Move &move : moves) {
        out << indent << "  " << sqr_format(move.from) << sqr_format(move.to) << " " << '\n';
    }
    if (depth >= max - 1) {
        out << indent << "}\n";
        return moves.size();
    }

    uint64_t move_count = 0;
    for (const Move &move : moves) {
        out << indent << "done move: " << sqr_format(move.from) << sqr_format(move.to) << '\n';
        do_move(pos, move);
        move_count += perft_out_helper(pos, depth + 1, max, out);
        undo_move(pos, move);
    }
    out << indent << "total move count: " << move_count << '\n';
    out << indent << "}\n";
    return move_count;
}
} // namespace chess

uint64_t chess::perft(Position &state, uint32_t depth) {
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

uint64_t chess::perft_out(Position &state, uint32_t depth, std::ostream &out) {
    return perft_out_helper(state, 0, depth, out);
}
