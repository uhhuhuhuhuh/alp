#include "position.hpp"
#include "bitboard.hpp"
#include "genmoves.hpp"
#include "misc.hpp"
#include "move.hpp"
#include "piece_color.hpp"
#include "square.hpp"
#include "zobrist.hpp"

#include <format>
#include <string>
#include <unordered_map>

chess::Position::Position() {};
chess::Position::Position(std::string_view sv, State &st, FenParseErr &out_err) : st(&st) {
    out_err = parse_fen(sv, st);
}

chess::MoveStack chess::Position::gen_legal_moves() { return gen_moves<GenType::legal>(*this); }

std::string chess::format_game_end(GameEnd game_end) {
    std::unordered_map<GameEnd, std::string_view> map = {
        {GameEnd::proceed,   "proceed"      },
        {GameEnd::checkmate, "checkmate"    },
        {GameEnd::stalemate, "stalemate"    },
        {GameEnd::dead_pos,  "dead position"},
        {GameEnd::threefold, "threefold"    },
        {GameEnd::move50,    "50 move rule" },
    };
    auto it = map.find(game_end);
    return it == map.end() ? "" : std::string(it->second);
}

namespace chess {
static inline char castle_rights_to_char(uint8_t &rights) {
    switch (1u << poplsb(rights)) {
    case CastleRights::white_queen: return 'Q';
    case CastleRights::white_king:  return 'K';
    case CastleRights::black_queen: return 'q';
    case CastleRights::black_king:  return 'k';
    default:                        return '-';
    }
}
} // namespace chess

std::string chess::Position::format_as_fen() const {
    std::string formated;
    // irank = inverted rank
    for (rank_t irank = 0; irank < 8; irank++) {
        rank_t rank = 7 - irank;
        int empty_count = 0;
        for (file_t file = 0; file < 8; file++) {
            sqr_t sqr = rank * 8 + file;
            if (board[sqr].type == PieceType::none) {
                empty_count += 1;
                continue;
            }
            if (empty_count != 0) {
                formated += std::to_string(empty_count);
                empty_count = 0;
            }
            formated += board[sqr].format();
        }
        if (empty_count != 0) {
            formated += std::to_string(empty_count);
            empty_count = 0;
        }
        if (irank < 7) {
            formated += '/';
        }
    }

    formated += std::format(" {} ", st->active_color == Color::white ? 'w' : 'b');

    uint8_t castle_rights = st->castle_rights;
    if (castle_rights == CastleRights::none) {
        formated += '-';
    } else {
        do {
            formated += castle_rights_to_char(castle_rights);
        } while (static_cast<unsigned int>(castle_rights) != 0);
    }

    formated += std::format(
        " {} {} {}",
        st->ep_sqr == State::ep_none ? "-" : format_sqr(st->ep_sqr),
        st->halfmove_clock,
        game_ply / 2 + 1
    );
    return formated;
}
std::string chess::Position::format() const {
    std::string formated;
    // irank = inverted rank
    for (rank_t irank = 0; irank < 8; irank++) {
        rank_t rank = 7 - irank;
        formated += std::format("{} ", rank + 1);
        for (file_t file = 0; file < 8; file++) {
            sqr_t sqr = rank * 8 + file;
            formated += std::format("{} ", board[sqr].format());
        }
        formated += '\n';
    }

    return std::format(
        "{}\n"
        "   A B C D E F G H\n"
        "{}\n",
        formated,
        format_as_fen()
    );
    ;
}

namespace chess {
static Position::FenParseErr set_castle_rights(Position &pos, const std::string &sub_str) {
    pos.st->castle_rights = CastleRights::none;
    for (char c : sub_str) {
        const char as_lower = std::tolower(c);
        const bool white = std::isupper(c);
        const bool queen = as_lower == 'q';

        if (c == '-') {
            continue;
        }
        if (as_lower != 'k' && !queen) {
            return Position::FenParseErr::bad_castling;
        }

        const Color color = white ? Color::white : Color::black;
        const sqr_t king_sqr = color == Color::white ? e1 : e8;
        const sqr_t rook_sqr = (color == Color::white ? 0 : 56) + (queen ? File::A : File::H);

        if (pos.board[king_sqr].type != PieceType::king || pos.board[king_sqr].color != color) {
            return Position::FenParseErr::no_castle_king;
        }

        if (pos.board[rook_sqr].type != PieceType::rook || pos.board[rook_sqr].color != color) {
            return Position::FenParseErr::no_castle_rook;
        }

        pos.st->castle_rights |=
            queen ? (white ? CastleRights::white_queen : CastleRights::black_queen)
                  : (white ? CastleRights::white_king : CastleRights::black_king);
    }
    return Position::FenParseErr::ok;
}

enum class OnNonBoard {
    active_color,
    castling_rights,
    en_passant,
    halfmove_clock,
    move_count,
};

static Position::FenParseErr
handle_fen_non_board(OnNonBoard on, Position &pos, const std::string &str) {
    switch (on) {
    case OnNonBoard::active_color:
        if (str.size() != 1 || (str[0] != 'w' && str[0] != 'b')) {
            return Position::FenParseErr::bad_active_color;
        }
        pos.st->active_color = str[0] == 'w' ? Color::white : Color::black;
        break;
    case OnNonBoard::castling_rights: {
        auto err = set_castle_rights(pos, str);
        if (err != Position::FenParseErr::ok) {
            return err;
        }
        break;
    }
    case OnNonBoard::en_passant: {
        pos.st->ep_sqr = parse_sqr(str);
        if (pos.st->ep_sqr == -1) {
            pos.st->ep_sqr = State::ep_none;
            if (str != "-") {
                return Position::FenParseErr::bad_ep_sqr;
            }
            break;
        }

        bool is_white = pos.st->active_color == Color::white;
        if ((1ULL << pos.st->ep_sqr & bb::ranks[is_white ? 5 : 2]) == 0) {
            return Position::FenParseErr::bad_ep_rank;
        }
        if (pos.board[pos.st->ep_sqr].type != PieceType::none) {
            return Position::FenParseErr::ep_not_empty;
        }
        sqr_t epd_pawn_sqr = pos.st->ep_sqr + (is_white ? -8 : 8);
        if (pos.board[epd_pawn_sqr].type != PieceType::pawn ||
            pos.board[epd_pawn_sqr].color == pos.st->active_color) {
            return Position::FenParseErr::no_ep_pawn;
        }
        break;
    }
    case OnNonBoard::halfmove_clock:
        try {
            pos.st->halfmove_clock = std::stoi(str); // comment here cuz clang-format
        } catch (const std::exception &e) {
            return Position::FenParseErr::bad_halfmove;
        }
        if (pos.st->halfmove_clock > 100) {
            return Position::FenParseErr::past_50_move;
        }
        break;
    case OnNonBoard::move_count:
        try {
            pos.game_ply = (std::stoi(str) - 1) * 2; // comment here cuz clang-format
        } catch (const std::exception &e) {
            return Position::FenParseErr::bad_curr_move;
        }
        if (pos.st->active_color == Color::black) {
            pos.game_ply++;
        }
        if (pos.game_ply == 0xFFFF || pos.game_ply < pos.st->halfmove_clock) {
            return Position::FenParseErr::bad_curr_move;
        }
        break;
    default: return Position::FenParseErr::unknown_property;
    }
    return Position::FenParseErr::ok;
}

Position::FenParseErr handle_fen_board(std::string_view sv, chess::Position &pos, std::size_t &i) {
    rank_t rank = 7;
    file_t file = 0;
    for (i = 0; i < sv.length(); i++) {
        char c = sv[i];
        // end of the board
        if (c == ' ') {
            break;
        }
        if (file > 8) {
            return Position::FenParseErr::bad_rank_file_count;
        }
        if (c == '/') {
            rank--;
            if (file < 8) {
                return Position::FenParseErr::bad_rank_file_count;
            }
            file = 0;
            continue;
        }

        if (std::isdigit(c)) {
            int asint = c - '0';
            if (asint == 0 || asint > 8 - file) {
                return Position::FenParseErr::bad_rank_file_count;
            }
            // The board is already initiliazed with all pieces set to none
            file += asint;
            continue;
        }

        if (file >= 8) {
            return Position::FenParseErr::bad_rank_file_count;
        }
        sqr_t sqr = rank * 8 + file;
        pos.board[sqr] = Piece{c};
        PieceType type = pos.board[sqr].type;
        Color color = pos.board[sqr].color;
        if (type == PieceType::none) {
            return Position::FenParseErr::bad_piece;
        }
        if (type == PieceType::king && pos.bb.get(PieceType::king)[color] != 0ULL) {
            // king position has already been assigned meaning there are two kings
            return Position::FenParseErr::bad_king_count;
        }

        pos.bb.place(type, color, sqr);

        file++;
    }

    if (file != 8 || rank != 0) {
        return Position::FenParseErr::bad_rank_file_count;
    }
    auto &kings = pos.bb.get(PieceType::king);
    if (popcount(kings[Color::white]) != 1 || popcount(kings[Color::black]) != 1) {
        return Position::FenParseErr::bad_king_count;
    }
    return Position::FenParseErr::ok;
}
} // namespace chess

chess::Position::FenParseErr chess::Position::parse_fen(std::string_view sv, State &new_st) {
    if (sv.empty()) {
        return FenParseErr::empty;
    }

    Position pos;
    pos.st = &new_st;

    std::size_t board_end_idx;

    auto err = handle_fen_board(sv, pos, board_end_idx);
    if (err != FenParseErr::ok) {
        return err;
    }

    auto &kings = pos.bb.get(PieceType::king);
    if (popcount(kings[Color::white]) != 1 || popcount(kings[Color::black]) != 1) {
        return FenParseErr::bad_king_count;
    }

    pos.game_ply = 0xFFFF;

    std::string not_board = static_cast<std::string>(sv).substr(board_end_idx + 1);
    OnNonBoard on = OnNonBoard::active_color;
    bool is_last_space = false;
    int64_t slice_start = -1;
    for (std::size_t i = 0; i < not_board.length(); i++) {
        char c = not_board[i];
        // spaces act as delimiters in FEN notation
        if (std::isspace(c)) {
            if (!is_last_space) {
                on = static_cast<OnNonBoard>(static_cast<int>(on) + 1);
            }
            is_last_space = true;
            continue;
        }

        // slice construction
        is_last_space = false;
        if (slice_start == -1) {
            slice_start = i;
        }
        if (i != not_board.size() - 1 && !std::isspace(not_board[i + 1])) {
            continue;
        }

        auto err =
            handle_fen_non_board(on, pos, not_board.substr(slice_start, i - slice_start + 1));
        if (err != FenParseErr::ok) {
            return err;
        }
        slice_start = -1;
    }

    const bool has_set_game_ply = pos.game_ply != 0xFFFF;
    if (static_cast<int>(on) < static_cast<int>(OnNonBoard::move_count) || !has_set_game_ply) {
        return FenParseErr::too_few_properties;
    }

    pos.st->hash = zobrist::get_hash(pos);
    *this = pos;
    return FenParseErr::ok;
}

std::string chess::Position::format_fen_err(FenParseErr err) {
    std::unordered_map<FenParseErr, std::string_view> map = {
        {FenParseErr::ok,                  "ok"                             },
        {FenParseErr::empty,               "FEN empty"                      },
        {FenParseErr::bad_rank_file_count, "bad rank or file count"         },
        {FenParseErr::bad_piece,           "bad piece"                      },
        {FenParseErr::bad_king_count,      "too many or too little kings"   },
        {FenParseErr::bad_active_color,    "bad active color"               },
        {FenParseErr::bad_castling,        "unkown castling right"          },
        {FenParseErr::no_castle_rook,      "no rook to castle with"         },
        {FenParseErr::no_castle_king,      "no king to castle with"         },
        {FenParseErr::bad_ep_sqr,          "bad en passant square"          },
        {FenParseErr::bad_ep_rank,         "en passant square on wrong rank"},
        {FenParseErr::ep_not_empty,        "en passant square is not empty" },
        {FenParseErr::no_ep_pawn,          "no valid en passant pawn"       },
        {FenParseErr::bad_halfmove,        "invalid halfmove count"         },
        {FenParseErr::past_50_move,        "past 50 move rule draw"         },
        {FenParseErr::bad_curr_move,       "invalid current move"           },
        {FenParseErr::unknown_property,    "unkown non board property"      },
        {FenParseErr::too_few_properties,  "too few non board properties"   },
    };
    auto it = map.find(err);
    return it == map.end() ? ""
                           : std::format("{}, Error code: {}", it->second, static_cast<int>(err));
}

chess::GameEnd chess::Position::get_game_end(const MoveStack &moves) const {
    if (moves.is_empty()) {
        if (st->checkers != 0) {
            return GameEnd::checkmate;
        }
        return GameEnd::stalemate;
    }

    if (st->halfmove_clock >= 100) {
        return GameEnd::move50;
    }

    // DEAD POSITION
    bb::U64 occ = bb.occupancy();
    if (popcount(occ) < 4) {
        if (bb.get_all(PieceType::king) == occ) {
            return GameEnd::dead_pos;
        }

        const auto has_only_king_and_minor = [&](Color c) -> bool {
            const bb::U64 minor = bb.get(PieceType::knight)[c] | bb.get(PieceType::bishop)[c];
            const Color enemy = !c;

            return (popcount(minor) == 1 &&
                    (bb.get(PieceType::king)[c] | minor) == bb.every_type()[c]) &&
                   (bb.get(PieceType::king)[enemy] == bb.every_type()[enemy]);
        };

        if (has_only_king_and_minor(Color::white) || has_only_king_and_minor(Color::black)) {
            return GameEnd::dead_pos;
        }

        const auto has_only_king_and_bishop = [&](Color c, bb::U64 bishop) -> bool {
            return popcount(bishop) == 1 &&
                   (bb.get(PieceType::king)[c] | bishop) == bb.every_type()[c];
        };

        const bb::U64 w_bishop = bb.get(PieceType::bishop)[Color::white];
        const bb::U64 b_bishop = bb.get(PieceType::bishop)[Color::black];
        if (has_only_king_and_bishop(Color::white, w_bishop) &&
            has_only_king_and_bishop(Color::black, b_bishop) &&
            bb::is_sqr_white(lsb(w_bishop)) == bb::is_sqr_white(lsb(b_bishop))) {
            return GameEnd::dead_pos;
        }
    }

    // THREE FOLD
    if (st->halfmove_clock < 4) {
        return GameEnd::proceed;
    }

    uint8_t count = 1, distance = 2;
    for (State *curr = st->prev != nullptr ? st->prev->prev : nullptr;
         curr != nullptr && distance <= st->halfmove_clock;
         curr = st->prev != nullptr ? st->prev->prev : nullptr, distance += 2) {
        if (curr->hash == st->hash && ++count >= 3) {
            return GameEnd::threefold;
        }
    }
    return GameEnd::proceed;
}

namespace chess {
inline static constexpr std::array<uint8_t, 64> castle_rights = [] {
    std::array<uint8_t, 64> rights{};
    rights[a1] = CastleRights::white_queen;
    rights[h1] = CastleRights::white_king;
    rights[e1] = CastleRights::white_queen | CastleRights::white_king;
    rights[a8] = CastleRights::black_queen;
    rights[h8] = CastleRights::black_king;
    rights[e8] = CastleRights::black_queen | CastleRights::black_king;
    return rights;
}();
inline static void __strip_castle_rights_helper(const Position &pos, State &state, sqr_t sqr) {
    state.castle_rights &= ~castle_rights[sqr];
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
    new_st.prev = st;
    new_st.ep_sqr = State::ep_none;
    new_st.captured_piece = board[move.to];
    new_st.active_color = !st->active_color;
    new_st.castle_rights = st->castle_rights;
    strip_castle_rights(*this, new_st, move.from, move.to);
    set_halfmove_clock(*this, new_st, move);
    game_ply++;

    Color color = board[move.from].color;
    switch (move.flag) {
    case MoveFlag::ep: {
        Color enemy = !color;
        sqr_t capture_sqr = st->ep_sqr - (color == Color::white ? 8 : -8);
        bb.remove(PieceType::pawn, enemy, capture_sqr);
        new_st.hash ^= zobrist::piece(PieceType::pawn, enemy, capture_sqr);
        board[capture_sqr].type = PieceType::none;
        break;
    }
    case MoveFlag::ep_victim: new_st.ep_sqr = move.to - (color == Color::white ? 8 : -8); break;
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
        new_st.hash ^= zobrist::ep(st->ep_sqr);
    }

    new_st.hash ^= zobrist::side();

    st = &new_st;
}

void chess::Position::undo_move(const Move &move) {
    assert(st->prev != nullptr);

    Piece captured = st->captured_piece;

    switch (move.flag) {
    case MoveFlag::ep: {
        Color color = board[move.to].color;
        Color enemy = !color;

        sqr_t capture_sqr = st->prev->ep_sqr - (color == Color::white ? 8 : -8);
        bb.place(PieceType::pawn, enemy, capture_sqr);
        board[capture_sqr].type = PieceType::pawn, board[capture_sqr].color = enemy;
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

chess::bb::U64 chess::Position::attacks_to_sqr(sqr_t sqr, Color color) const {
    Color enemy = !color;

    bb::U64 occupancy = bb.occupancy();
    // Rook & Queen, Bishop & Queen
    bb::U64 RQ, BQ;
    RQ = BQ = bb.get(PieceType::queen)[enemy];
    RQ |= bb.get(PieceType::rook)[enemy];
    BQ |= bb.get(PieceType::bishop)[enemy];

    return (bb::pawn_attacks[color][sqr] & bb.get(PieceType::pawn)[enemy]) |
           (bb::knight_attacks[sqr] & bb.get(PieceType::knight)[enemy]) |
           (bb::get_bishop_attack(sqr, occupancy) & BQ) |
           (bb::get_rook_attack(sqr, occupancy) & RQ) |
           (bb::king_attacks[sqr] & bb.get(PieceType::king)[enemy]);
}

chess::PinBB chess::Position::get_pins(Color color) {
    PinBB pin_bbs;
    bb::U64 occ = bb.occupancy();
    bb::U64 color_occ = bb.every_type()[color];
    bb::U64 king_bb = bb.get(PieceType::king)[color];
    assert(popcount(king_bb) == 1);
    sqr_t king_sqr = lsb(king_bb);

    bb::U64 queen_bb = bb.get(PieceType::queen)[!color];

    bb::U64 pinner = bb::xray_bishop_attack(occ, color_occ, king_sqr) &
                     (bb.get(PieceType::bishop)[!color] | queen_bb);
    pin_bbs.pinners |= pinner;
    while (pinner != 0) {
        sqr_t sqr = poplsb(pinner);
        bb::U64 between = bb::slider_between[sqr][king_sqr];
        sqr_t pinned_sqr = lsb(between & color_occ);
        pin_bbs.pin_rays[pinned_sqr] = between | (1ULL << sqr);
        pin_bbs.pinned |= 1ULL << pinned_sqr;
    }

    pinner = bb::xray_rook_attack(occ, color_occ, king_sqr) &
             (bb.get(PieceType::rook)[!color] | queen_bb);
    pin_bbs.pinners |= pinner;
    while (pinner != 0) {
        sqr_t sqr = poplsb(pinner);
        bb::U64 between = bb::slider_between[sqr][king_sqr];
        sqr_t pinned_sqr = lsb(between & color_occ);
        pin_bbs.pin_rays[pinned_sqr] = between | (1ULL << sqr);
        pin_bbs.pinned |= 1ULL << pinned_sqr;
    }

    return pin_bbs;
}
