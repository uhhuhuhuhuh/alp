#pragma once

#include "engine.hpp"
#include "position.hpp"
#include "tranpostable.hpp"

#include <array>

namespace chess {
class BetterEvalEngine : public IEngine {
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
            /*7*/  100,  100,  100,  100,  100,   18,   98,  100,
            /*6*/   75,   26,   94,  100,  100,  100,   17,   25,
            /*5*/  -80,  -20,  -15,   10,   40,   70,   20,  -20,
            /*4*/  -80,  -65,  -35,   40,  -30,   10,  -40,  -35,
            /*3*/  -95,  -35,   15,    5,    0,  -45,   20,  -45,
            /*2*/  -95,  -50,   10,  -42,  -25,  -15,   70,  -60,
            /*1*/    0,    0,    0,    0,    0,    0,    0,    0
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::pawn].endgame = convert_formatted_board({
            // clang-format off
            /*8*/    0,    0,    0,    0,    0,    0,    0,    0,
            /*7*/  100,  100,  100,  100,  100,  100,  100,  100,
            /*6*/  100,  100,  100,  100,  100,  100,  100,  100,
            /*5*/  100,  100,  100,  100,   90,   45,  100,  100,
            /*4*/   70,  100,   80,   85,   95,   75,   65,   45,
            /*3*/   95,   70,   65,   45,   95,   90,  100,   95,
            /*2*/  100,   95,   70,   63,  100,   95,   30,   55,
            /*1*/    0,    0,    0,    0,    0,    0,    0,    0
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::knight].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/ -100,  100,  -11,  -59, -100,  -98, -100,  -76,
            /*7*/  -42,   81,  100,  100,  100,   -9,   25, -100,
            /*6*/  100,  -46,  100,  100,  100,   66,  100,  -48,
            /*5*/  -33,   58,   85,  100,  100,  100,   80,   58,
            /*4*/   -8,  -75,   25,   20,   23,   63,   96,  -35,
            /*3*/ -100,  -57,  -30,  -60,   22,   10,   13,  -98,
            /*2*/ -100, -100, -100,    3,   -8,  -81,  -98, -100,
            /*1*/ -100, -100, -100, -100,  -99,  -46, -100,   57
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::knight].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  -63,  -95,  -64,  100,   29,  -98, -100,  -49,
            /*7*/  -93,   -9,   28,    8,   58,  100,   70,  -98,
            /*6*/ -100,  100,  100,   65,  -26,   97,  -77, -100,
            /*5*/  -46,  -55,  100,   34,   27,  -10,   65,  -21,
            /*4*/ -100,  100,   80,   55,   70,  -20,  -80,  -80,
            /*3*/   21,   50,   60,   82,  -25,  -10,  -35,   24,
            /*2*/ -100,  -55, -100,  -80,  -48,   85,  -66,  -45,
            /*1*/ -100, -100,  -68,  -43,  -58,  -35, -100,  -70
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::bishop].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/ -100,   61, -100, -100, -100, -100, -100, -100,
            /*7*/  -66,   18,  -99, -100,  100,  -96, -100, -100,
            /*6*/ -100,   67,  -20,  -78,   -8,  100,  -20,    8,
            /*5*/    4,  100,  100,   70,   45,  -20,   10,  -98,
            /*4*/ -100,  -40,    5,   50,   40,  -22,   75,  100,
            /*3*/   40,   35,   60,   55,   30,   35,    0,  100,
            /*2*/  -63,  -15,    3,   20,  -20,   79,   45,  -66,
            /*1*/   39,   36,  -10,  -82,   -3,  -33, -100, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::bishop].endgame = convert_formatted_board({
            // clang-format off
            /*8*/   99,  100,    9,   16,   -2,  -99,  -39,  -90,
            /*7*/  100,   65,  100,   67,  -94,  100,   -8,   52,
            /*6*/   99,  100,  -30,   99,  100,   -8,   97,   -9,
            /*5*/   40,   41,   13,   90,  100,    9,   -8,    2,
            /*4*/  -88,   15,   60,  100,  -52,  100,  -69,  -41,
            /*3*/ -100,   35,   45,  100,   65,   65,   99,   48,
            /*2*/   60,   35,   32,   30,   95,   25,   55,  100,
            /*1*/  -63,  -33,  -45,   20,  100, -100,  -36,  -49
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::rook].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/   99, -100, -100,  -11, -100, -100,  -99,  100,
            /*7*/  -10,  -13,   89,   15,  100,   44,    3,  100,
            /*6*/   18,  -30,  100,  -44,   75,   95,    8,   57,
            /*5*/ -100,  -69,   40,  -90,  -53,   38,  100,   -8,
            /*4*/  -99,   22,  -90,   45,   48,  -10,   15,  -57,
            /*3*/  -40, -100,  -43,  -53,  -95,  -79,   80,   42,
            /*2*/  -61,  -48,  -65, -100,  -48,   10,  -37, -100,
            /*1*/ -100,  -40,  -60,  -45,  -35,  -40,   25, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::rook].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  100,  100,   -2,  -23,  -25,  -96,   49,   59,
            /*7*/   90,   55,   25,   43,   -2,  100,  100,  100,
            /*6*/   46,   79,   -2,   80,   25,  -62,  -10,  100,
            /*5*/  100,  100,   75,   55,  100,   58,  -12,  100,
            /*4*/   94,    1,   47, -100,  -97,    5,  -97,   -5,
            /*3*/  -10,  -31,    5,   15,   28,   38,  -75,  -80,
            /*2*/ -100,    0,  -27,   -5,  -77,  -72,  -15,   15,
            /*1*/   10,  -15,   80,    5,  -20,   15,  -20,   15
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::queen].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/   54,  100,    7,  100,  100, -100,   35,  100,
            /*7*/  -93,  -96,   88,   86,   94,  100,  100,  100,
            /*6*/  -92,   55,   -5,    9,   85,  100,  100,  100,
            /*5*/   27,   84,  -99,   -1,   43,   48,  100,   10,
            /*4*/ -100,    2,  -10,    7,   18,   -1,   10,   28,
            /*3*/   51,  -42,  -27,  -10,  -40,    0,    5,    5,
            /*2*/  -99,   27,  -20,  -40,    0,   31,   -5,  100,
            /*1*/  -81, -100, -100,  -37, -100,  -99,  -25,  100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::queen].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  -10,   16,   51,  100,  -92,   97,  -28, -100,
            /*7*/  100,   87,  -27,  -99,  100,   20,  100,  100,
            /*6*/   60,  -66,  100,  100,  100,    7,   21,  100,
            /*5*/  100,   20,   99,  100,   -5,   67,  -98,   90,
            /*4*/   12,  -47,   64,  100,  100,  -52,   15,   84,
            /*3*/   69,  -80,   53,   85,   35,   60,  100,  100,
            /*2*/  -83, -100,   25,   40,   45, -100,   -5, -100,
            /*1*/  100, -100,  -35,  -60,    5, -100, -100,  -51
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::king].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/ -100,  -94,  100,   99, -100,  -96,  -28,  100,
            /*7*/  100, -100, -100,   99, -100,  -42,   -9,  -99,
            /*6*/  100,  100,   99,  -61,  -97,  100,   66,  -60,
            /*5*/  100,  100,   92, -100, -100, -100,  -94, -100,
            /*4*/ -100,    2, -100, -100,  100, -100, -100, -100,
            /*3*/ -100,   99,  -55,   92,   28,  -75,    7, -100,
            /*2*/   20,  -75,   35,  -75,  -15,   20,   40,   80,
            /*1*/  -40,   90,   90,  -15,   10,  -45,  100,   40
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::king].endgame = convert_formatted_board({
            // clang-format off
            /*8*/ -100,   99, -100,  100,   40,  100,  100,  100,
            /*7*/  100,   10,   99,  100,    5,   94,  100,   19,
            /*6*/   82,  100,  100,  100,   68,  100,  100,    7,
            /*5*/   77,  100,    6,   98,   77,   60,  100,  -64,
            /*4*/ -100,   28,   -9,   28,   23,   42,   18,   -6,
            /*3*/  -73, -100,    8,  -38,    7,  -23,  -31,  -40,
            /*2*/  -62,   52,  -40,   40,  -20,    0,  -35, -100,
            /*1*/ -100, -100,  -85,  -80,  -95,  -45,  -85, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        return result;
    }

    struct EvalSettings {
        static constexpr int32_t king_val = 20'000;
        int32_t queen_val = 1200;
        int32_t rook_val = 618;
        int32_t bishop_val = 416;
        int32_t knight_val = 435;
        static constexpr int32_t pawn_val = 100;

        int32_t blocked_penalty = 25;
        int32_t doubled_penalty = 17;
        int32_t isolated_penalty = 22;
        int32_t backward_penalty = 0;

        int32_t passed_bonus = 61;

        int32_t king_shield_2_bonus = 20;
        int32_t king_shield_3_bonus = 12;

        int32_t bishop_pair_bonus = 72;
        int32_t returning_bishop_bonus = 86;
        int32_t mobility_0_bishop_penalty = 7;
        int32_t mg_bishop_mobility_multiplier = 3;
        int32_t eg_bishop_mobility_multiplier = 3;
        int32_t bishop_king_attackers_weight = 53;

        int32_t mobility_0_rook_penalty = 0;
        int32_t mg_rook_mobility_multiplier = 2;
        int32_t eg_rook_mobility_multiplier = 4;
        int32_t mg_rook_7th_bonus = 49;
        int32_t eg_rook_7th_bonus = 23;
        int32_t rook_full_open_file_bonus = 48;
        int32_t rook_half_open_file_bonus = 21;
        int32_t rook_king_attackers_weight = 16;

        int32_t early_developed_queen_penalty = 11;
        int32_t mobility_0_queen_penalty = 0;
        int32_t mg_queen_mobility_multiplier = 1;
        int32_t eg_queen_mobility_multiplier = 2;
        int32_t queen_king_attackers_weight = 45;

        // indexed based off of pawn count
        std::array<int32_t, 9> rook_adj = {10, 30, 27, 32, 2, 0, 1, 11, -3};
        // indexed based off of pawn count
        std::array<int32_t, 9> knight_adj = {-50, 19, 50, -2, 11, -11, -4, 23, 33};

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
    };
    static constexpr SearchSettings get_default_search_settings() { return SearchSettings{}; }

  private:
    using Clock = std::chrono::steady_clock;

    struct MoveHeuristics {
        std::array<std::array<Move, 2>, SearchSettings::max_killer_ply> killers{};
        ColorArray<std::array<int32_t, SearchSettings::history_size>> history{};
    };

    struct SearchContext {
        std::optional<Clock::time_point> deadline;
        bool stopped = false;

        std::array<std::array<chess::Move, SearchSettings::max_ply>, SearchSettings::max_ply> pv{};
        std::array<int, SearchSettings::max_ply> pv_length{};

        std::array<Move, SearchSettings::max_ply> prev_pv{};
        int prev_pv_length = 0;

        MoveHeuristics heuristics;

        uint64_t nodes = 0;
    };

    class MovePicker {
      public:
        MovePicker(
            const Position &pos, const MoveStack<> &legal_moves, const Move &tt_move,
            const Move &pv_move, int ply, Color side, const MoveHeuristics &heuristics,
            const EvalSettings &eval, const SearchSettings &search
        );

        bool next(Move &move);

      private:
        const Position &m_pos;
        Color m_side;
        const MoveHeuristics &m_heuristics;
        const EvalSettings &eval;
        const SearchSettings &search;

        Move m_tt_move{};
        bool m_has_tt_move = false;

        Move m_pv_move{};
        bool m_has_pv_move = false;
        bool m_pv_move_pending = false;

        MoveStack<64> m_good_captures;
        MoveStack<64> m_killers;
        MoveStack<128> m_quiets;
        MoveStack<64> m_bad_captures;

        std::size_t m_good_index = 0;
        std::size_t m_killer_index = 0;
        std::size_t m_quiet_index = 0;
        std::size_t m_bad_index = 0;

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
    BetterEvalEngine();
    BetterEvalEngine(std::string_view fen, Position::FenParseErr &out_err);

    void operator=(const BetterEvalEngine &) = delete;

    Position::FenParseErr parse_fen(std::string_view fen) override;

    virtual Position &position() override;
    const Position &position() const override;

    const MoveStack<> &gen_legal_moves() override;

    void do_move(const Move &move) override;
    void undo_move(const Move &move) override;

    int32_t evaluate(const EvalSettings &eval) const;
    int32_t evaluate() const override;

    SearchResult
    search(std::chrono::milliseconds time, const EvalSettings &eval, const SearchSettings &search);
    SearchResult search(std::chrono::milliseconds time) override;

    SearchResult search(uint16_t depth, const EvalSettings &eval, const SearchSettings &search);
    SearchResult search(uint16_t depth);
};
} // namespace chess
