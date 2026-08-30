#include "position.hpp"
#include "bitboard.hpp"
#include "genmoves.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "square.hpp"
#include <bit>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>

namespace chess {
static inline char castle_rights_to_char(uint8_t &rights) {
    int index = std::countr_zero(rights);
    if (static_cast<std::size_t>(index) < sizeof(uint8_t)) {
        rights &= rights - 1;
    }
    switch (1u << index) {
    case CastleRights::white_queen: return 'Q';
    case CastleRights::white_king:  return 'K';
    case CastleRights::black_queen: return 'q';
    case CastleRights::black_king:  return 'k';
    default:                        return '-';
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
            out << pos.board[sqr].format();
        }
        if (empty_count != 0) {
            out << empty_count;
            empty_count = 0;
        }
        if (irank < 7) {
            out << '/';
        }
    }

    out << ' ' << (pos.st->active_color == Color::white ? 'w' : 'b') << ' ';

    uint8_t castle_rights = pos.st->castle_rights;
    do {
        out << castle_rights_to_char(castle_rights);
    } while (static_cast<unsigned int>(castle_rights) != 0);

    out << ' ';
    if (pos.st->ep_sqr == -1) {
        out << '-';
    } else {
        out << sqr_format(pos.st->ep_sqr + (pos.st->active_color == Color::white ? 8 : -8));
    }

    out << ' ' << pos.st->halfmove_clock;
    out << ' ' << pos.game_ply / 2 << '\n';
}

namespace chess {
static Position::fen_parse_err set_castle_rights(Position &pos, const std::string &sub_str) {
    for (std::size_t i = 0; i < sub_str.length(); i++) {
        char c = sub_str[i];
        rank_t rank = -1;
        file_t file = -1;
        Color color = std::isupper(c) ? Color::white : Color::black;
        rank = color == Color::white ? 0 : 7;
        CastleRights rights = CastleRights::none;
        switch (std::tolower(c)) {
        case 'q':
            rights = color == Color::white ? CastleRights::white_queen : CastleRights::black_queen;
            file = File::A;
            break;
        case 'k':
            rights = color == Color::white ? CastleRights::white_king : CastleRights::black_king;
            file = File::H;
            break;
        case '-': break;
        default:  return Position::fen_parse_err::bad_castling;
        }
        if (file == -1) {
            continue;
        }

        sqr_t sqr = file + (rank * 8);
        if (pos.board[sqr].type != PieceType::rook || pos.board[sqr].color != color) {
            return Position::fen_parse_err::no_castle_rook;
        }

        sqr_t king_sqr = color == Color::white ? e1 : e8;
        if (pos.board[king_sqr].type != PieceType::king || pos.board[king_sqr].color != color) {
            return Position::fen_parse_err::no_castle_king;
        }
        pos.st->castle_rights |= rights;
    }
    return Position::fen_parse_err::ok;
}

enum class On_Non_Board {
    active_color,
    castling_rights,
    en_passant,
    halfmove_clock,
    move_count,
};

static Position::fen_parse_err
handle_non_board(On_Non_Board on, Position &pos, const std::string &str) {
    switch (on) {
    case On_Non_Board::active_color:
        char c;
        try {
            c = str[0];
        } catch (const std::exception &e) {
            return Position::fen_parse_err::bad_active_color;
        }
        pos.st->active_color = c == 'w' ? Color::white : Color::black;
        break;
    case On_Non_Board::castling_rights: {
        auto err = set_castle_rights(pos, str);
        if (err != Position::fen_parse_err::ok) {
            return err;
        }
        break;
    }
    case On_Non_Board::en_passant:
        pos.st->ep_sqr = parse_sqr(str);
        if (pos.st->ep_sqr != -1) {
            pos.st->ep_sqr -= (pos.st->active_color == Color::white ? 8 : -8);
        } else if (str != "-") {
            return Position::fen_parse_err::bad_ep_sqr;
        }
        break;
    case On_Non_Board::halfmove_clock:
        try {
            pos.st->halfmove_clock = std::stoi(str); // comment here cuz clang-format
        } catch (const std::exception &e) {
            return Position::fen_parse_err::bad_halfmove;
        }
        if (pos.st->halfmove_clock > 100) {
            return Position::fen_parse_err::past_50_move;
        }
        break;
    case On_Non_Board::move_count:
        try {
            pos.game_ply = std::stoi(str) * 2; // comment here cuz clang-format is dumb
            if (pos.st->active_color == Color::black) {
                pos.game_ply++;
            }
        } catch (const std::exception &e) {
            return Position::fen_parse_err::bad_curr_move;
        }
        break;
    default: return Position::fen_parse_err::unknown_property;
    }
    return Position::fen_parse_err::ok;
}
} // namespace chess

chess::Position::fen_parse_err chess::Position::parse_fen(std::string_view sv, State &st) {
    if (sv.empty()) {
        return fen_parse_err::empty;
    }

    Position pos;
    pos.st = &st;

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
            return fen_parse_err::bad_rank_file_count;
        }
        if (c == '/') {
            rank--;
            if (file < 8) {
                return fen_parse_err::bad_rank_file_count;
            }
            file = 0;
            continue;
        }

        if (std::isdigit(c)) {
            int asint = c - '0';
            if (asint == 0 || asint > 8 - file) {
                return fen_parse_err::bad_rank_file_count;
            }
            // The board is already initiliazed with all pieces set to none
            file += asint;
            continue;
        }

        sqr_t sqr = rank * 8 + file;
        pos.board[sqr] = Piece{c};
        PieceType type = pos.board[sqr].type;
        Color color = pos.board[sqr].color;
        if (type == PieceType::none) {
            return fen_parse_err::bad_piece;
        }
        if (type == PieceType::king && pos.bb.get(PieceType::king)[color] != 0ULL) {
            // king position has already been assigned meaning there are two kings
            return fen_parse_err::bad_king_count;
        }

        pos.bb.place(type, color, sqr);

        file++;
    }

    if (file != 8 || rank != 0) {
        return fen_parse_err::bad_rank_file_count;
    }
    auto &kings = pos.bb.get(PieceType::king);
    if (std::popcount(kings[Color::white]) != 1 || std::popcount(kings[Color::black]) != 1) {
        return fen_parse_err::bad_king_count;
    }

    std::string not_board = static_cast<std::string>(sv);
    On_Non_Board on = On_Non_Board::active_color;
    not_board = not_board.substr(i);
    bool is_last_space = false;
    int64_t slice_start = -1;
    for (std::size_t i = 0; i < not_board.length(); i++) {
        char c = not_board[i];
        // spaces act as delimiters in FEN notation
        if (std::isspace(c)) {
            if (i > 0 && !is_last_space) {
                on = static_cast<On_Non_Board>(static_cast<int>(on) + 1);
            }
            is_last_space = true;
            continue;
        }

        // slice construction
        is_last_space = false;
        if (slice_start == -1) {
            slice_start = i;
        }

        std::string sub_str = not_board.substr(slice_start, i - slice_start + 1);
        auto err = handle_non_board(on, pos, sub_str);
        if (err != fen_parse_err::ok) {
            return err;
        }
        slice_start = -1;
    }

    if (on != On_Non_Board::move_count) {
        return fen_parse_err::too_few_properties;
    }

    *this = pos;
    return fen_parse_err::ok;
}

chess::Position::Position(std::string_view sv, State &st) { parse_fen(sv, st); }

std::string_view chess::Position::format_fen_err(fen_parse_err err) {
    std::unordered_map<fen_parse_err, std::string_view> map = {
        {fen_parse_err::ok,                  "ok"                          },
        {fen_parse_err::empty,               "string empty"                },
        {fen_parse_err::bad_rank_file_count, "bad rank or file count"      },
        {fen_parse_err::bad_piece,           "bad piece"                   },
        {fen_parse_err::bad_king_count,      "bad king count"              },
        {fen_parse_err::bad_active_color,    "bad active color"            },
        {fen_parse_err::bad_castling,        "unkown castling right"       },
        {fen_parse_err::no_castle_rook,      "no castle rook"              },
        {fen_parse_err::no_castle_king,      "no castle king"              },
        {fen_parse_err::bad_ep_sqr,          "bad en passant square"       },
        {fen_parse_err::bad_halfmove,        "invlaid halfmove count"      },
        {fen_parse_err::past_50_move,        "past 50 move rule draw"      },
        {fen_parse_err::bad_curr_move,       "invalid current move"        },
        {fen_parse_err::unknown_property,    "unkown non board property"   },
        {fen_parse_err::too_few_properties,  "too few non board properties"},
    };
    if (map.count(err) != 0) {
        return map[err];
    }
    return "";
}

void chess::output_pos(const Position &pos, std::ostream &out) {
    // irank = inverted rank
    for (rank_t irank = 0; irank < 8; irank++) {
        rank_t rank = 7 - irank;
        out << rank + 1 << "  ";
        for (file_t file = 0; file < 8; file++) {
            sqr_t sqr = rank * 8 + file;
            out << pos.board[sqr].format() << " ";
        }
        out << '\n';
    }
    out << '\n';
    out << "   A B C D E F G H\n";

    output_pos_as_fen(pos, out);
}

namespace chess {
inline static void __strip_castle_rights_helper(const Position &pos, State &state, sqr_t sqr) {
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
static void strip_castle_rights(const Position &pos, State &state, sqr_t from, sqr_t to) {
    __strip_castle_rights_helper(pos, state, from);
    __strip_castle_rights_helper(pos, state, to);
}

static inline void move_piece(Position &pos, sqr_t from, sqr_t to, uint8_t flag) {
    PieceType type = pos.board[from].type;
    Color color = pos.board[from].color;
    switch (flag) {
    case MoveFlag::capture: {
        PieceType captured_type = pos.board[to].type;
        Color captured_color = pos.board[to].color;
        pos.bb.capture(type, color, from, captured_type, captured_color, to);
        break;
    }
    default: pos.bb.move(type, color, from, to); break;
    }

    pos.board[to].type = type;
    pos.board[to].color = color;
    pos.board[from].type = PieceType::none;
}
} // namespace chess

void chess::do_move(Position &pos, const Move &move, State &st) {
    st = *pos.st;
    st.prev = pos.st;
    st.ep_sqr = -1;
    st.captured_piece = pos.board[move.to];
    st.active_color = !pos.st->active_color;
    pos.game_ply++;
    strip_castle_rights(pos, st, move.from, move.to);

    Piece &from_piece = pos.board[move.from];

    switch (move.flag) {
    case MoveFlag::ep: {
        rank_t rank_sign = from_piece.color == Color::white ? 1 : -1;
        sqr_t capture_sqr = move.to + (-8 * rank_sign);
        pos.bb.remove(PieceType::pawn, !from_piece.color, capture_sqr);
        pos.board[capture_sqr].type = PieceType::none;
        break;
    }
    case MoveFlag::ep_victim: st.ep_sqr = move.to; break;
    case MoveFlag::castle:    {
        const bool queen_side = move.to < move.from;
        rank_t from_rank = move.from / 8;
        sqr_t from_rook_sqr = (queen_side ? 0 : 7) + (from_rank * 8);
        sqr_t to_rook_sqr = move.to + (queen_side ? 1 : -1);

        move_piece(pos, from_rook_sqr, to_rook_sqr, MoveFlag::quiet);
        break;
    }
    }

    move_piece(pos, move.from, move.to, move.flag);
    if (move.promotion_type != PieceType::none) {
        Color color = pos.board[move.to].color;
        pos.bb.remove(pos.board[move.to].type, color, move.to);
        pos.bb.place(move.promotion_type, color, move.to);
        pos.board[move.to].type = move.promotion_type;
    }

    pos.st = &st;
    Color enemy = !pos.board[move.to].color;

    bb::U64 king_bb = pos.bb.get(PieceType::king)[enemy];
    assert(std::popcount(king_bb) == 1);
    pos.st->checks_bb = attacks_to_sqr(pos, std::countr_zero(king_bb), enemy);
}

void chess::undo_move(Position &pos, const Move &move) {
    Piece &to_piece = pos.board[move.to];

    Piece captured = pos.st->captured_piece;

    switch (move.flag) {
    case MoveFlag::ep: {
        rank_t rank_sign = to_piece.color == Color::white ? 1 : -1;
        sqr_t capture_sqr = move.to + (-8 * rank_sign);
        pos.bb.place(PieceType::pawn, !to_piece.color, capture_sqr);
        pos.board[capture_sqr].type = PieceType::pawn,
        pos.board[capture_sqr].color = !to_piece.color;
        break;
    }
    case MoveFlag::castle: {
        const bool queen_side = move.to < move.from;
        rank_t from_rank = move.from / 8;
        sqr_t from_rook_sqr = (queen_side ? 0 : 7) + (from_rank * 8);
        sqr_t to_rook_sqr = move.to + (queen_side ? 1 : -1);
        move_piece(pos, to_rook_sqr, from_rook_sqr, MoveFlag::quiet);
        break;
    }
    }

    move_piece(pos, move.to, move.from, MoveFlag::quiet);
    if (move.promotion_type != PieceType::none) {
        Color color = pos.board[move.from].color;
        pos.bb.remove(pos.board[move.from].type, color, move.from);
        pos.bb.place(PieceType::pawn, color, move.from);
        pos.board[move.from].type = PieceType::pawn;
    }
    pos.board[move.to] = captured;
    if (captured.type != PieceType::none) {
        pos.bb.place(captured.type, captured.color, move.to);
    }

    pos.st = pos.st->prev;
    pos.game_ply--;
}

namespace chess {
static uint64_t
perft_out_recursive(Position &pos, uint32_t depth, uint32_t max, std::ostream &out) {
    if (depth >= max) {
        return 0;
    }
    auto moves = gen_moves<GenType::legal>(pos);

    std::string indent = "";
    for (uint32_t i = 0; i < depth; i++) {
        indent += "  ";
    }

    out << indent << "{\n";
    std::stringstream ss;
    output_pos(pos, ss);
    std::string str;
    while (std::getline(ss, str)) {
        out << indent << str << '\n';
    }

    out << indent << "Moves: " << moves.size() << '\n';
    for (const Move &move : moves) {
        out << indent << "  " << move.format() << " " << '\n';
    }
    if (depth >= max - 1) {
        out << indent << "}\n";
        return moves.size();
    }

    uint64_t move_count = 0;
    for (const Move &move : moves) {
        out << indent << "done move: " << move.format() << '\n';
        State st;
        do_move(pos, move, st);
        move_count += perft_out_recursive(pos, depth + 1, max, out);
        undo_move(pos, move);
    }
    out << indent << "total move count: " << move_count << '\n';
    out << indent << "}\n";
    return move_count;
}

template <bool root> uint64_t perft_recursive(Position &pos, uint32_t depth, std::ostream &out) {
    uint64_t count = 0, nodes = 0;
    const bool leaf = depth == 2;

    for (const Move &move : gen_moves<GenType::legal>(pos)) {
        if (root && depth <= 1) {
            count = 1;
            nodes++;
        } else {
            State st;
            do_move(pos, move, st);
            count = leaf ? gen_moves<GenType::legal>(pos).size()
                         : perft_recursive<false>(pos, depth - 1, out);
            nodes += count;
            undo_move(pos, move);
        }
        if (root) {
            out << move.format() << ": " << count << '\n';
        }
    }

    return nodes;
}
} // namespace chess

uint64_t chess::perft(Position &state, uint32_t depth, std::ostream &out) {
    return perft_recursive<true>(state, depth, out);
}

uint64_t chess::perft_out(Position &state, uint32_t depth, std::ostream &out) {
    return perft_out_recursive(state, 0, depth, out);
}