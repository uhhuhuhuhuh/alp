#include "position.hpp"
#include "benchmarking.hpp"
#include "bitboard.hpp"
#include "genmoves.hpp"
#include "move.hpp"
#include "piece.hpp"
#include "square.hpp"
#include "zobrist.hpp"
#include <bit>
#include <cctype>
#include <cstdint>
#include <iomanip>
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

void chess::Position::output_as_fen(std::ostream &out) {
    // irank = inverted rank
    for (rank_t irank = 0; irank < 8; irank++) {
        rank_t rank = 7 - irank;
        file_t empty_count = 0;
        for (file_t file = 0; file < 8; file++) {
            sqr_t sqr = rank * 8 + file;
            if (board[sqr].type == PieceType::none) {
                empty_count += 1;
                continue;
            }
            if (empty_count != 0) {
                out << empty_count;
                empty_count = 0;
            }
            out << board[sqr].format();
        }
        if (empty_count != 0) {
            out << empty_count;
            empty_count = 0;
        }
        if (irank < 7) {
            out << '/';
        }
    }

    out << ' ' << (st->active_color == Color::white ? 'w' : 'b') << ' ';

    uint8_t castle_rights = st->castle_rights;
    do {
        out << castle_rights_to_char(castle_rights);
    } while (static_cast<unsigned int>(castle_rights) != 0);

    out << ' ';
    if (st->ep_sqr == -1) {
        out << '-';
    } else {
        out << sqr_format(st->ep_sqr + (st->active_color == Color::white ? 8 : -8));
    }

    out << ' ' << st->halfmove_clock;
    out << ' ' << game_ply / 2 << '\n';
}
void chess::Position::output(std::ostream &out) {
    // irank = inverted rank
    for (rank_t irank = 0; irank < 8; irank++) {
        rank_t rank = 7 - irank;
        out << rank + 1 << "  ";
        for (file_t file = 0; file < 8; file++) {
            sqr_t sqr = rank * 8 + file;
            out << board[sqr].format() << " ";
        }
        out << '\n';
    }
    out << '\n';
    out << "   A B C D E F G H\n";

    output_as_fen(out);
}

namespace chess {
static Position::FenParseErr set_castle_rights(Position &pos, const std::string &sub_str) {
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
        default:  return Position::FenParseErr::bad_castling;
        }
        if (file == -1) {
            continue;
        }

        sqr_t sqr = file + (rank * 8);
        if (pos.board[sqr].type != PieceType::rook || pos.board[sqr].color != color) {
            return Position::FenParseErr::no_castle_rook;
        }

        sqr_t king_sqr = color == Color::white ? e1 : e8;
        if (pos.board[king_sqr].type != PieceType::king || pos.board[king_sqr].color != color) {
            return Position::FenParseErr::no_castle_king;
        }
        pos.st->castle_rights |= rights;
    }
    return Position::FenParseErr::ok;
}

enum class On_Non_Board {
    active_color,
    castling_rights,
    en_passant,
    halfmove_clock,
    move_count,
};

static Position::FenParseErr
handle_non_board(On_Non_Board on, Position &pos, const std::string &str) {
    switch (on) {
    case On_Non_Board::active_color:
        char c;
        try {
            c = str[0];
        } catch (const std::exception &e) {
            return Position::FenParseErr::bad_active_color;
        }
        pos.st->active_color = c == 'w' ? Color::white : Color::black;
        break;
    case On_Non_Board::castling_rights: {
        auto err = set_castle_rights(pos, str);
        if (err != Position::FenParseErr::ok) {
            return err;
        }
        break;
    }
    case On_Non_Board::en_passant:
        pos.st->ep_sqr = parse_sqr(str);
        if (pos.st->ep_sqr != -1) {
            pos.st->ep_sqr -= (pos.st->active_color == Color::white ? 8 : -8);
        } else if (str != "-") {
            return Position::FenParseErr::bad_ep_sqr;
        }
        break;
    case On_Non_Board::halfmove_clock:
        try {
            pos.st->halfmove_clock = std::stoi(str); // comment here cuz clang-format
        } catch (const std::exception &e) {
            return Position::FenParseErr::bad_halfmove;
        }
        if (pos.st->halfmove_clock > 100) {
            return Position::FenParseErr::past_50_move;
        }
        break;
    case On_Non_Board::move_count:
        try {
            pos.game_ply = std::stoi(str) * 2; // comment here cuz clang-format is dumb
            if (pos.st->active_color == Color::black) {
                pos.game_ply++;
            }
        } catch (const std::exception &e) {
            return Position::FenParseErr::bad_curr_move;
        }
        break;
    default: return Position::FenParseErr::unknown_property;
    }
    return Position::FenParseErr::ok;
}
} // namespace chess

chess::Position::FenParseErr chess::Position::parse_fen(std::string_view sv, State &st) {
    if (sv.empty()) {
        return FenParseErr::empty;
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
            return FenParseErr::bad_rank_file_count;
        }
        if (c == '/') {
            rank--;
            if (file < 8) {
                return FenParseErr::bad_rank_file_count;
            }
            file = 0;
            continue;
        }

        if (std::isdigit(c)) {
            int asint = c - '0';
            if (asint == 0 || asint > 8 - file) {
                return FenParseErr::bad_rank_file_count;
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
            return FenParseErr::bad_piece;
        }
        if (type == PieceType::king && pos.bb.get(PieceType::king)[color] != 0ULL) {
            // king position has already been assigned meaning there are two kings
            return FenParseErr::bad_king_count;
        }

        pos.bb.place(type, color, sqr);

        file++;
    }

    if (file != 8 || rank != 0) {
        return FenParseErr::bad_rank_file_count;
    }
    auto &kings = pos.bb.get(PieceType::king);
    if (std::popcount(kings[Color::white]) != 1 || std::popcount(kings[Color::black]) != 1) {
        return FenParseErr::bad_king_count;
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
        if (err != FenParseErr::ok) {
            return err;
        }
        slice_start = -1;
    }

    if (on != On_Non_Board::move_count) {
        return FenParseErr::too_few_properties;
    }

    pos.st->hash = zobrist::get_hash(pos);
    *this = pos;
    return FenParseErr::ok;
}

chess::Position::Position(std::string_view sv, State &st) { parse_fen(sv, st); }

std::string chess::Position::format_fen_err(FenParseErr err) {
    std::unordered_map<FenParseErr, std::string_view> map = {
        {FenParseErr::ok,                  "ok"                          },
        {FenParseErr::empty,               "string empty"                },
        {FenParseErr::bad_rank_file_count, "bad rank or file count"      },
        {FenParseErr::bad_piece,           "bad piece"                   },
        {FenParseErr::bad_king_count,      "bad king count"              },
        {FenParseErr::bad_active_color,    "bad active color"            },
        {FenParseErr::bad_castling,        "unkown castling right"       },
        {FenParseErr::no_castle_rook,      "no castle rook"              },
        {FenParseErr::no_castle_king,      "no castle king"              },
        {FenParseErr::bad_ep_sqr,          "bad en passant square"       },
        {FenParseErr::bad_halfmove,        "invlaid halfmove count"      },
        {FenParseErr::past_50_move,        "past 50 move rule draw"      },
        {FenParseErr::bad_curr_move,       "invalid current move"        },
        {FenParseErr::unknown_property,    "unkown non board property"   },
        {FenParseErr::too_few_properties,  "too few non board properties"},
    };
    if (map.count(err) == 0) {
        return "";
    }
    std::string err_str = static_cast<std::string>(map[err]);
    err_str += ", Error code: ";
    err_str += std::to_string(static_cast<int>(err));
    return err_str;
}

void chess::Position::set_game_end_state(const MoveStack &moves) {
    if (moves.is_empty()) {
        if (st->checkers != 0) {
            st->game_end_state = State::GameEnd::checkmate;
        } else {
            st->game_end_state = State::GameEnd::stalemate;
        }
        return;
    }

    if (st->halfmove_clock >= 100) {
        st->game_end_state = State::GameEnd::move50;
        return;
    }

    // DEAD POSITION

    bb::U64 occ = bb.occupancy();
    if (std::popcount(occ) < 4) {
        if (bb.get_all(PieceType::king) == occ) {
            st->game_end_state = State::GameEnd::dead_pos;
            return;
        }

        const auto has_only_king_and_minor = [&](Color c) -> bool {
            const bb::U64 minor = bb.get(PieceType::knight)[c] | bb.get(PieceType::bishop)[c];
            const Color enemy = !c;

            return (std::popcount(minor) == 1 &&
                    (bb.get(PieceType::king)[c] | minor) == bb.every_type()[c]) &&
                   (bb.get(PieceType::king)[enemy] == bb.every_type()[enemy]);
        };

        if (has_only_king_and_minor(Color::white) || has_only_king_and_minor(Color::black)) {
            st->game_end_state = State::GameEnd::dead_pos;
            return;
        }

        const auto has_only_king_and_bishop = [&](Color c, bb::U64 bishop) -> bool {
            return std::popcount(bishop) == 1 &&
                   (bb.get(PieceType::king)[c] | bishop) == bb.every_type()[c];
        };

        const bb::U64 w_bishop = bb.get(PieceType::bishop)[Color::white];
        const bb::U64 b_bishop = bb.get(PieceType::bishop)[Color::black];
        if (has_only_king_and_bishop(Color::white, w_bishop) &&
            has_only_king_and_bishop(Color::black, b_bishop) &&
            bb::is_sqr_white(std::countr_zero(w_bishop)) ==
                bb::is_sqr_white(std::countr_zero(b_bishop))) {
            st->game_end_state = State::GameEnd::dead_pos;
            return;
        }
    }

    // THREE FOLD

    if (st->halfmove_clock < 4 || st->prev == nullptr || st->prev->prev == nullptr) {
        st->game_end_state = State::GameEnd::proceed;
        return;
    }

    uint8_t count = 1, dist = 2;
    for (State *curr = st->prev->prev;
         curr->prev != nullptr && curr->prev->prev != nullptr && dist <= st->halfmove_clock;
         curr = curr->prev->prev, dist += 2) {

        if (curr->hash == st->hash && ++count == 3) {
            st->game_end_state = State::GameEnd::threefold;
            return;
        }
    }

    // PROCEED
    st->game_end_state = State::GameEnd::proceed;
}

namespace chess {
inline static void __strip_castle_rights_helper(const Position &pos, State &state, sqr_t sqr) {
    uint8_t rights[64]{};
    rights[a1] = CastleRights::white_queen;
    rights[h1] = CastleRights::white_king;
    rights[e1] = CastleRights::white_queen | CastleRights::white_king;
    rights[a8] = CastleRights::black_queen;
    rights[h8] = CastleRights::black_king;
    rights[e8] = CastleRights::black_queen | CastleRights::black_king;

    state.castle_rights &= ~rights[sqr];
}
inline static void strip_castle_rights(const Position &pos, State &state, sqr_t from, sqr_t to) {
    __strip_castle_rights_helper(pos, state, from);
    __strip_castle_rights_helper(pos, state, to);
}

inline static void set_halfmove_clock(const Position &pos, State &state, const Move &move) {
    if (pos.board[move.from].type == PieceType::pawn) {
        state.halfmove_clock = 0;
        return;
    }
    if (move.flag == MoveFlag::capture || move.flag == MoveFlag::ep) {
        state.halfmove_clock = 0;
        return;
    }
    state.halfmove_clock++;
}

static inline void move_piece(Position &pos, State &st, sqr_t from, sqr_t to, uint8_t flag) {
    PieceType type = pos.board[from].type;
    Color color = pos.board[from].color;
    PieceType captured_type = pos.board[to].type;
    Color captured_color = pos.board[to].color;

    pos.board[to].type = type;
    pos.board[to].color = color;
    pos.board[from].type = PieceType::none;

    st.hash ^= zobrist::piece(type, color, from);
    if (flag == MoveFlag::capture) {
        pos.bb.capture(type, color, from, captured_type, captured_color, to);
        st.hash ^= zobrist::piece(captured_type, captured_color, to);
    } else {
        pos.bb.move(type, color, from, to);
    }
    st.hash ^= zobrist::piece(type, color, to);
}
} // namespace chess

void chess::Position::do_move(const Move &move, State &new_st) {
    new_st = *st;
    new_st.prev = st;
    new_st.ep_sqr = -1;
    new_st.captured_piece = board[move.to];
    new_st.active_color = !st->active_color;
    game_ply++;
    strip_castle_rights(*this, new_st, move.from, move.to);
    set_halfmove_clock(*this, new_st, move);

    Piece &from_piece = board[move.from];

    switch (move.flag) {
    case MoveFlag::ep: {
        bb.remove(PieceType::pawn, !from_piece.color, st->ep_sqr);
        new_st.hash ^= zobrist::piece(PieceType::pawn, !from_piece.color, st->ep_sqr);
        board[st->ep_sqr].type = PieceType::none;
        break;
    }
    case MoveFlag::ep_victim: new_st.ep_sqr = move.to; break;
    case MoveFlag::castle:    {
        const bool queen_side = move.to < move.from;
        rank_t from_rank = move.from / 8;
        sqr_t from_rook_sqr = (queen_side ? 0 : 7) + (from_rank * 8);
        sqr_t to_rook_sqr = move.to + (queen_side ? 1 : -1);

        move_piece(*this, new_st, from_rook_sqr, to_rook_sqr, MoveFlag::quiet);
        break;
    }
    }

    move_piece(*this, new_st, move.from, move.to, move.flag);
    if (move.promotion_type != PieceType::none) {
        Color color = board[move.to].color;
        bb.remove(board[move.to].type, color, move.to);
        bb.place(move.promotion_type, color, move.to);
        new_st.hash ^= zobrist::piece(board[move.to].type, color, move.to) ^
                       zobrist::piece(move.promotion_type, color, move.to);
        board[move.to].type = move.promotion_type;
    }

    if (new_st.castle_rights != st->castle_rights) {
        new_st.hash ^= zobrist::castle_rights(st->castle_rights) ^
                       zobrist::castle_rights(new_st.castle_rights);
    }

    new_st.hash ^= zobrist::ep(st->ep_sqr);
    if (new_st.ep_sqr != st->ep_sqr) {
        new_st.hash ^= zobrist::ep(new_st.ep_sqr);
    }

    new_st.hash ^= zobrist::black_active;

    assert(std::popcount(king_bb) == 1);

    st = &new_st;

    Color enemy = !board[move.to].color;
    bb::U64 king_bb = bb.get(PieceType::king)[enemy];
    st->checkers = attacks_to_sqr(*this, std::countr_zero(king_bb), enemy);
}

void chess::Position::undo_move(const Move &move) {
    assert(st->prev != nullptr);
    Piece &to_piece = board[move.to];

    Piece captured = st->captured_piece;

    switch (move.flag) {
    case MoveFlag::ep: {
        sqr_t capture_sqr = st->prev->ep_sqr;
        bb.place(PieceType::pawn, !to_piece.color, capture_sqr);
        board[capture_sqr].type = PieceType::pawn, board[capture_sqr].color = !to_piece.color;
        break;
    }
    case MoveFlag::castle: {
        const bool queen_side = move.to < move.from;
        rank_t from_rank = move.from / 8;
        sqr_t from_rook_sqr = (queen_side ? 0 : 7) + (from_rank * 8);
        sqr_t to_rook_sqr = move.to + (queen_side ? 1 : -1);
        move_piece(*this, *st, to_rook_sqr, from_rook_sqr, MoveFlag::quiet);
        break;
    }
    }

    move_piece(*this, *st, move.to, move.from, MoveFlag::quiet);
    if (move.promotion_type != PieceType::none) {
        Color color = board[move.from].color;
        bb.remove(board[move.from].type, color, move.from);
        bb.place(PieceType::pawn, color, move.from);
        board[move.from].type = PieceType::pawn;
    }
    board[move.to] = captured;
    if (captured.type != PieceType::none) {
        bb.place(captured.type, captured.color, move.to);
    }

    st = st->prev;
    game_ply--;
}

namespace chess {
template <bool root> uint64_t perft_recursive(Position &pos, uint8_t depth, std::ostream &out) {
    uint64_t count = 0, nodes = 0;
    const bool leaf = depth == 2;

    auto moves = gen_moves<GenType::legal>(pos);
    for (const Move &move : moves) {
        if (root && depth <= 1) {
            count = 1;
            nodes++;
        } else {
            State st;
            pos.do_move(move, st);
            count = leaf ? gen_moves<GenType::legal>(pos).size()
                         : perft_recursive<false>(pos, depth - 1, out);
            nodes += count;
            pos.undo_move(move);
        }
        if (root) {
            out << move.format() << ": " << count << '\n';
        }
    }

    return nodes;
}
} // namespace chess

uint64_t chess::perft(Position &state, uint8_t depth, std::ostream &out) {
    Timer timer;
    timer.start();
    auto nodes = perft_recursive<true>(state, depth, out);
    timer.stop();
    auto duration = timer.duration() / 1'000'000;
    auto nps = nodes / duration;

    out << "Total Time seconds: " << duration << '\n';
    out << "Nodes per Second: " << std::fixed << std::setprecision(0) << nps << '\n';
    return nodes;
}