#pragma once

#include "position.hpp"
#include "tranpostable.hpp"

#include <array>

namespace chess {
class Engine {
  public:
    using PhasePST = std::array<int16_t, 64>;
    struct PieceSquareTables {
        PhasePST middlegame{};
        PhasePST endgame{};
    };

    static constexpr std::array<int16_t, 64>
    convert_formatted_board(const std::array<int16_t, 64> &board) {
        std::array<int16_t, 64> arr{};

        for (std::size_t i = 0; i < 64; i++) {
            const file_t file = i % 8;
            const rank_t rank = 7 - (i / 8);
            arr[file + rank * 8] = board[i];
        }

        return arr;
    }

    static constexpr PieceArray<PieceSquareTables> make_default_pst() {
        PieceArray<PieceSquareTables> result{};

        result[PieceType::pawn].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/    0,    0,    0,    0,    0,    0,    0,    0,
            /*7*/  100,  100,  100,  100,  100,  100,  100,  100,
            /*6*/   67,   94,   99,   95,   85,  100,   97,   30,
            /*5*/  -10,  -70,   20,   45,   30,   20,  -65,  -45,
            /*4*/  -80,  -85,  -80,   -5,  -55,  -25,  -75,  -35,
            /*3*/  -95,  -55,  -55,  -45,   10,    0,   -5,  -30,
            /*2*/ -100,  -85,  -30,  -96,   10,   35,   10, -100,
            /*1*/    0,    0,    0,    0,    0,    0,    0,    0
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::pawn].endgame = convert_formatted_board({
            // clang-format off
            /*8*/    0,    0,    0,    0,    0,    0,    0,    0,
            /*7*/  100,  100,  100,  100,  100,  100,  100,  100,
            /*6*/  100,  100,  100,  100,   75,  100,  100,  100,
            /*5*/  100,  100,  100,   25,   90,  100,  100,  100,
            /*4*/   95,  100,   95,   65,   80,   75,   65,   45,
            /*3*/   80,   55,   75,  100,   85,   80,   40,   30,
            /*2*/  100,   90,   70,   98,   90,   60,   70,   55,
            /*1*/    0,    0,    0,    0,    0,    0,    0,    0
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::knight].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/ -100, -100,  -95,  100,   99, -100,  100,   85,
            /*7*/ -100,  -99,  100,  100,   50,   93,  -45,  -94,
            /*6*/ -100,  -97,    0,  100,  100,  100,  100,  100,
            /*5*/  -96,   23,  -12,  100,    0,   71,  100,   44,
            /*4*/  -94, -100,  -45,   85,   88,  100,  100,  100,
            /*3*/ -100,  -99,  -40,  -50,   97,   -5,  -22,    9,
            /*2*/ -100,  -76, -100,  -72,  -83,  -98,  -34,  -82,
            /*1*/ -100, -100, -100,  -85,  -97,   94,  -38, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::knight].endgame = convert_formatted_board({
            // clang-format off
            /*8*/   87,  -43,  100,   89,  100,  -39,  100, -100,
            /*7*/  -81,  100,   96,   20,   83,  100,   10,  -98,
            /*6*/    4,   96,   45,   60,  100,   48,   34,   91,
            /*5*/   54,   95,  100,   44,   97,  100,  -70,   99,
            /*4*/  -65,  -27,   70,   -2,   36,   -1,  -24,   28,
            /*3*/  -99,  -15,   75,   67,    5,    0,   11, -100,
            /*2*/ -100,  -16, -100,  -65,  -18,   33,  -41,   10,
            /*1*/ -100,  -50, -100,  -18,  -95, -100, -100,   83
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::bishop].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/  -98, -100, -100, -100,  -98,  -74, -100,  -99,
            /*7*/   99,  -47,   99, -100,  -70,   57,   61,   56,
            /*6*/  100,  -46,   90,   97,   97,   99,   60,   -1,
            /*5*/   19,  -86,    5,  100,  100,   96,  -11,  -80,
            /*4*/   41,  100,  100,  -40,   98,   28,  100,  -45,
            /*3*/ -100,   81,  -65,   20,  -55,   35,  -35,  -40,
            /*2*/  -16,  -60,   33,  -70,  -30,  -22,   95,  -96,
            /*1*/   33,  100,  -85, -100,  -99,   17,   99,   43
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::bishop].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  100,  100,   97,   26,   33,  -99,  -69,  100,
            /*7*/    6,   90,  -10,  -54,   -9,  100,   47,   84,
            /*6*/   19,   36,  -20,  -56,  -55,   -3,    7,   96,
            /*5*/   35,  100,   -2,  -15,   10,  -56,   64,   69,
            /*4*/  -53,   25,   15,   90,   -7,    5,   81,  -96,
            /*3*/  -82,   36,  100,   40,   85,  -35,   -9,   25,
            /*2*/  -60,   25,   37,   25,   35,  -50,  -30,   75,
            /*1*/  -61, -100,  -10, -100,  -21,  -65, -100, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::rook].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/  -26,  -55,    5,   99,   20,   43,   22, -100,
            /*7*/ -100,  -98,  -98,   39,   25,   99,  100,   56,
            /*6*/   43,   65,  -60,  -49,  100,  100,  -94,   97,
            /*5*/   20,   49, -100,  -29,  -36,   23,  -99,   97,
            /*4*/  -99,  -53,  -99,  -90,  -22, -100,  100,   45,
            /*3*/ -100, -100,  -68, -100,  -45,   51,  100, -100,
            /*2*/  -81,  -53,  -10,  -45,  -48,    0,   98,  -97,
            /*1*/ -100, -100,  -90,  -60,  -60,   45,  -60,    0
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::rook].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  100,  100,   18,  -98,   21,  100,  100,  100,
            /*7*/  100,  100,  100,  100,  100,  100,   57,  -30,
            /*6*/   96,   44,  100,   45,   72,   93,  100,  -20,
            /*5*/   25,   99,   79,  100,   50,    8,  100,  100,
            /*4*/   61,   17,   99,   55,    3,   97,  -46,   12,
            /*3*/   39,   24,  -20,   -2,   -2,  -62,  -40,   18,
            /*2*/ -100, -100,  -77,  -20,   33,  -67, -100,  -68,
            /*1*/   55,   45,   20,  -10,   30,  -60,   40,  -60
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::queen].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/    4,  -25,   92,    0,  100,  100,  100,    1,
            /*7*/  -60, -100,   77,   36,  100,  100,  100,  100,
            /*6*/  -49,  -97,  -34,   67,   62,  100,  100,  100,
            /*5*/  -38,  -47,  -69,   99,   -4,  100,  -17,  100,
            /*4*/ -100,   85,   20,    7,   53,   44,  -20,  -90,
            /*3*/  -34,  -97,  -99,  -55,  -45,  100,  -20, -100,
            /*2*/ -100,    2,  -94,   -5,   40,  -49,  -15,    8,
            /*1*/ -100, -100,  -60,  -99, -100,  -99,  -92,  100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::queen].endgame = convert_formatted_board({
            // clang-format off
            /*8*/   10,  100,    6,   50,  100,  100,  100,  100,
            /*7*/   85,   43,   53,  100,  100,  100,  100,  100,
            /*6*/   99,   84,  100,  100,  100,  100,  100,  100,
            /*5*/ -100,  100,   99,   62,  100,   98,  100,  -55,
            /*4*/ -100,  -99,   -6,   99,  100,   18,  100,   99,
            /*3*/  -50,  100,  100,   95,  100,   -5,  -32,  100,
            /*2*/ -100,  -65,    2,   65, -100, -100,  -79, -100,
            /*1*/   94, -100, -100,  -58, -100, -100,  100,  -40
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::king].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/  -99,  -93,   99,  100,  -94,  -56,  100,   99,
            /*7*/   99,    3,  100,   77,  100,  -91,  100,  100,
            /*6*/   98,   81, -100,  -24, -100,  100,  100,   67,
            /*5*/ -100,  -22, -100,  -92, -100, -100,  100, -100,
            /*4*/ -100,   -5,  100,  100,   41, -100,   50, -100,
            /*3*/  -40, -100,  -73,   57,   34, -100,  -18,   -8,
            /*2*/  100,    0,  100,  -40,  -15,  -80,   65,    5,
            /*1*/ -100,  100,  -20, -100,  -80,  -95,    0,  -10
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::king].endgame = convert_formatted_board({
            // clang-format off
            /*8*/ -100,   59,  -22,   94,   32, -100,  100,  -53,
            /*7*/    2,  100,  100,  100,  100,  100,  100,   99,
            /*6*/  100,  100,   20,  100,  -20,   73,  100,  -23,
            /*5*/  -28, -100,   31,  100,   47,   95,   61,  -38,
            /*4*/  -65,   -2,   34,   42,   48,   -8,   18, -100,
            /*3*/  -98,   16,  -67,   12,  -33,  -13,  -31,  -95,
            /*2*/  -72,   -8,  -75,   15,  -20,    0,  -25,  -90,
            /*1*/ -100,  -85,  -60,  -30,  -30, -100,  -35, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        return result;
    }

    struct EvalSettings {
        int32_t max_phase = 24;

        static constexpr int32_t king_val = 20'000;
        int32_t queen_val = 1200;
        int32_t rook_val = 623;
        int32_t bishop_val = 421;
        int32_t knight_val = 438;
        static constexpr int32_t pawn_val = 100;

        int32_t blocked_penalty = 32;
        int32_t doubled_penalty = 3;
        int32_t isolated_penalty = 27;
        int32_t backward_penalty = 1;

        int32_t passed_bonus = 65;

        int32_t king_shield_2_bonus = 30;
        int32_t king_shield_3_bonus = 12;

        int32_t mobility_0_knight_penalty = 7;
        int32_t mg_knight_mobility_multiplier = 4;
        int32_t eg_knight_mobility_multiplier = 4;
        int32_t knight_king_attackers_weight = 23;

        int32_t bishop_pair_bonus = 106;
        int32_t returning_bishop_bonus = 0;
        int32_t mobility_0_bishop_penalty = 7;
        int32_t mg_bishop_mobility_multiplier = 3;
        int32_t eg_bishop_mobility_multiplier = 3;
        int32_t bishop_king_attackers_weight = 31;

        int32_t mobility_0_rook_penalty = 0;
        int32_t mg_rook_mobility_multiplier = 2;
        int32_t eg_rook_mobility_multiplier = 4;
        int32_t mg_rook_7th_bonus = 34;
        int32_t eg_rook_7th_bonus = 23;
        int32_t rook_full_open_file_bonus = 48;
        int32_t rook_half_open_file_bonus = 19;
        int32_t rook_king_attackers_weight = 9;

        int32_t early_developed_queen_penalty = 25;
        int32_t mobility_0_queen_penalty = 0;
        int32_t mg_queen_mobility_multiplier = 1;
        int32_t eg_queen_mobility_multiplier = 2;
        int32_t queen_king_attackers_weight = 50;

        // indexed based off of pawn count
        std::array<int32_t, 9> rook_adj = {-50, 10, -3, 0, 7, -3, 11, 31, 35};
        // indexed based off of pawn count
        std::array<int32_t, 9> knight_adj = {-50, -23, 14, 3, -31, 4, 25, 43, 50};

        PieceArray<PieceSquareTables> pst = make_default_pst();
    };
    static constexpr EvalSettings get_default_eval_settings() { return EvalSettings{}; }

    struct SearchSettings {
        int32_t inf = 1'500'000;
        int32_t mate_score = 1'000'000;
        static constexpr int32_t max_ply = 256;
        int32_t mate_threshold = mate_score - max_ply;
        int32_t aspiration_window = 25;

        int32_t max_qply = 16;
        uint64_t time_check_interval = 1024;

        static constexpr int32_t max_killer_ply = 256;
        static constexpr int32_t history_size = 64 * 64;
        int32_t history_max = 16384;
    };
    static constexpr SearchSettings get_default_search_settings() { return SearchSettings{}; }

  private:
    using Clock = std::chrono::steady_clock;

    struct MoveHeuristics {
        std::array<std::array<Move, 2>, SearchSettings::max_killer_ply> killers{};
        ColorArray<std::array<int32_t, SearchSettings::history_size>> history{};
        std::array<Move, SearchSettings::history_size> counter_history{};
    };

    struct SearchContext {
        std::optional<Clock::time_point> deadline;
        bool stopped = false;

        Move prev_move = Move::none();

        std::array<std::array<chess::Move, SearchSettings::max_ply>, SearchSettings::max_ply> pv{};
        std::array<std::size_t, SearchSettings::max_ply> pv_length{};

        std::array<Move, SearchSettings::max_ply> prev_pv{};
        std::size_t prev_pv_length = 0;

        MoveHeuristics heuristics;

        uint64_t nodes = 0;
    };

    class MovePicker {
      public:
        MovePicker(
            const Position &pos, const MoveStack<> &legal_moves, const Move &tt_move,
            const Move &pv_move, const Move &prev_move, int ply, Color side,
            const MoveHeuristics &heuristics, const EvalSettings &eval, const SearchSettings &search
        );

        bool next(Move &move);

      private:
        const Position &m_pos;
        Color m_side;
        const MoveHeuristics &m_heuristics;
        const EvalSettings &eval;
        const SearchSettings &search;

        enum class Stage {
            tt,
            pv,
            good,
            queen_promo,
            counter,
            killers,
            quiets,
            under_promo,
            bad,
            done
        };
        Stage stage;

        Move m_tt_move = Move::none();
        bool m_has_tt_move = false;

        Move m_pv_move = Move::none();
        bool m_has_pv_move = false;
        bool m_pv_move_pending = false;

        Move m_counter_move = Move::none();
        bool m_has_counter_move = false;
        bool m_counter_move_pending = false;

        MoveStack<Move, 64> m_captures;
        std::size_t m_captures_idx = 0;
        MoveStack<Move, 8> m_queen_promos;
        std::size_t m_queen_promo_idx = 0;
        MoveStack<Move, 64> m_killers;
        std::size_t m_killer_idx = 0;
        MoveStack<Move, 128> m_quiets;
        std::size_t m_quiet_idx = 0;
        MoveStack<Move, 8> m_under_promos;
        std::size_t m_under_promo_idx = 0;
        MoveStack<Move, 32> m_bad_captures;
        std::size_t m_bad_idx = 0;

        bool contains(const MoveStack<> &moves, const Move &target);

        int capture_order_score(const Move &move) const;

        int tactical_score(const Move &move) const;
    };

  public:
    StateStack st_stack;
    Position pos;
    MoveStack<> moves;
    TransposTable<64> tt;

  private:
    static int32_t move_piece_value(PieceType type, const EvalSettings &eval);
    static int32_t
    static_exchange_eval(const Position &pos, const Move &move, const EvalSettings &eval);
    bool should_stop(SearchContext &ctx, const SearchSettings &search);
    Move get_tt_move(SearchContext &ctx);
    int32_t capture_score(const Move &move);
    int32_t move_score(const Move &move, const Move &pv_move, const Move &tt_move);
    void order_moves(MoveStack<> &moves, const Move &pv_move, const Move &tt_move);
    void update_pv(SearchContext &ctx, uint16_t ply, const Move &move);
    int32_t quiescence(
        int32_t alpha, int32_t beta, SearchContext &ctx, int ply, int qply,
        const EvalSettings &eval, const SearchSettings &search
    );
    int32_t score_to_tt(int32_t score, uint16_t ply, const SearchSettings &search);
    int32_t score_from_tt(int32_t score, uint16_t ply, const SearchSettings &search);
    int32_t negamax(
        uint16_t depth, uint16_t ply, int32_t alpha, int32_t beta, SearchContext &ctx,
        bool prev_move_was_null, const EvalSettings &eval, const SearchSettings &search
    );
    int32_t root_search(
        MoveStack<> &root_moves, uint16_t depth, SearchContext &ctx, int32_t alpha, int32_t beta,
        Move &best_move, const EvalSettings &eval, const SearchSettings &search
    );

  public:
    Engine();
    Engine(std::string_view fen, Position::FenParseErr &out_err);

    void operator=(const Engine &) = delete;

    Position::FenParseErr parse_fen(std::string_view fen);

    virtual Position &position();
    const Position &position() const;

    const MoveStack<> &gen_legal_moves();

    void do_move(const Move &move);
    void undo_move(const Move &move);

    int32_t evaluate(const EvalSettings &eval) const;
    int32_t evaluate() const;

    struct SearchResult {
        chess::Move best_move = Move::none();
        int32_t score = 0;
        uint16_t depth = 0;
        uint64_t nodes = 0;
    };

    SearchResult
    search(std::chrono::milliseconds time, const EvalSettings &eval, const SearchSettings &search);
    SearchResult search(std::chrono::milliseconds time);

    SearchResult search(uint16_t depth, const EvalSettings &eval, const SearchSettings &search);
    SearchResult search(uint16_t depth);
};
} // namespace chess
