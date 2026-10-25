#pragma once

#include "engine.hpp"
#include "position.hpp"
#include "tranpostable.hpp"

#include <array>

namespace chess {
class TunedPSTEngine : public IEngine {
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
            /*7*/   47,  -15,   28,   35,  100, -100,   26,   45,
            /*6*/   45,  -30,  -20,   20,   -5,   65,  -47,    0,
            /*5*/  -15,  -25,   10,    5,   15,   10,    0,   -5,
            /*4*/  -20,  -25,    0,    0,   10,    0,   -5,  -45,
            /*3*/  -40,  -20,    0,  -10,    5,  -15,   10,  -30,
            /*2*/  -40,  -20,  -35,  -27,  -15,  -15,    0,  -40,
            /*1*/    0,    0,    0,    0,    0,    0,    0,    0
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::pawn].endgame = convert_formatted_board({
            // clang-format off
            /*8*/    0,    0,    0,    0,    0,    0,    0,    0,
            /*7*/  100,  100,  100,  100,   64,  100,   96,  100,
            /*6*/   20,   50,   45,   10,   65,    5,   60,   30,
            /*5*/    0,    5,    5,  -20,  -20,   -5,   20,   10,
            /*4*/  -25,    5,  -15,  -20,  -30,  -25,  -35,  -15,
            /*3*/  -10,    0,  -20,  -15,   -5,    0,   -5,  -10,
            /*2*/  -20,   -5,   25,  -72,  -30,   -5,    0,  -30,
            /*1*/    0,    0,    0,    0,    0,    0,    0,    0
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::knight].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/  -93,  -95, -100, -100,  -38, -100,  -79, -100,
            /*7*/    0,   -2,   35,  -80,    5,   -3,  -59,  -10,
            /*6*/ -100,  -70,  -94,   50,   30,  -33,   22,  100,
            /*5*/  -15,  -17,  -10,    3,    5,   40,    0,   80,
            /*4*/  -40,  -35,    0,  -35,    8,  -12,   63,  -45,
            /*3*/  -30,   15,    5,   10,  -20,   10,   38,  -25,
            /*2*/  -60,  -36,   19,   -2,   -8,   20,  -62,  -15,
            /*1*/    2,  -32,  -61,  -50,  -64,  -28,  -38,   -8
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::knight].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  -60,  -74,   76,   74,   45,  -55,  100,    2,
            /*7*/  -85,   19,  -12,   55,  -17,  -28,  100,   27,
            /*6*/   81,  100,   79,   20,   30,   35,  100,  -27,
            /*5*/   -5,  100,   50,   71,    2,   10,   45,  -48,
            /*4*/   60,   90,   40,   75,   55,   97,  -73,   60,
            /*3*/   59,  -10,   45,   32,   70,   30,  -20,   52,
            /*2*/  -60,  -75,  -53,   -5,   22,   65,   90,  -75,
            /*1*/  -70,  -10,  -31,   -5,   84,   25,   30, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::bishop].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/  -20, -100,  -81, -100,  -55, -100, -100, -100,
            /*7*/  -36,  -93,   25,  -72, -100,   -2,  -89,   93,
            /*6*/  -27,    0,  -87,  -40,  -28, -100,   35,   55,
            /*5*/  -15,  -17,  -65,   10,  -15,  -53,    5,  -14,
            /*4*/  -13,  -18,   -5,  -25,  -20,   -2,  -40,   30,
            /*3*/   -5,    5,    5,    0,    0,    5,   20,  -25,
            /*2*/    5,   20,   10,  -25,  -15,   14,   15,   20,
            /*1*/   52,  -22,  -15,  -40,   30,  -25,  -86,   47
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::bishop].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  -32,   26,  100,   66,   -6,   45,   98,  -16,
            /*7*/  -10,   95,   73,  -35,   53,   15,  -30,    7,
            /*6*/   88,   55,   72,   95,   89,  100,   55,    5,
            /*5*/   25,   65,   65,   55,   70,   69,  -25,  -25,
            /*4*/  -11,   35,   30,   65,   20,   30,   26,  -90,
            /*3*/   45,   15,   75,   40,   20,   25,   27,   20,
            /*2*/   -5,  -45,    0,   30,   20,   40,   30,  -30,
            /*1*/  -95,    0,  -25,  -20,  -30,   20,  100, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::rook].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/ -100, -100, -100, -100, -100,  -80,  100, -100,
            /*7*/  -35,  -10,   14, -100,   90,   70,    3,  -15,
            /*6*/  -75,   10,  -45,  -60,   10,  -50,  -29,   56,
            /*5*/ -100,   -2,   -5,  -90,    5,   32,  -25,  -63,
            /*4*/    8,  -55,  -41,  -95,  -52,   35, -100,  -67,
            /*3*/  -87,  -99,  -48,  -38,  -95,  -15,  -23,  -13,
            /*2*/  -75,  -84,  -60,  -15,  -43,  -10,    8,  -45,
            /*1*/  -50,  -40,  -15,  -20,  -10,  -30,  -20,  -35
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::rook].endgame = convert_formatted_board({
            // clang-format off
            /*8*/   36,  -46,  -65,  -85,  -40,   39,  -37,  -10,
            /*7*/  -45,  -30,  -10,   32,  -90,  -50,    0,   -5,
            /*6*/  -37,    5,  -15,  -25,  -25,  -18,  -60,  -70,
            /*5*/  -50,  -30,  -25,  -80,  -60,  -45,  -60,  -28,
            /*4*/  -67,  -95,   10,   -5,  -47,  -80,  -42,  -30,
            /*3*/  -40,  -20,  -55,  -75,  -32,  -60,  -65,  -65,
            /*2*/  -35,  -45,  -12,  -60,  -27,  -77,  -90,  -10,
            /*1*/  -55,  -65,  -55,  -55,  -70,  -70,  -35, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::queen].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/  -46, -100,   -5,  -97, -100,  100,   40,  -52,
            /*7*/  -95,  -58,  -70, -100,   -5,   80,   45,   95,
            /*6*/  -26, -100,  -43,   21,   11,   75,   15,   43,
            /*5*/  -35,  -49,  -27,  -10,  -22,  -30,  -55,   20,
            /*4*/  -50,  -42,  -25,  -60,  -47,  -85,  -15,   15,
            /*3*/   10,  -27,  -57,  -50,  -75,  -25,  -30,  -30,
            /*2*/  -82,  -73,  -30,  -40,  -40,  -15,    0,   20,
            /*1*/   -3,  -29,  -75,  -42,  -57,  -76, -100,  -30
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::queen].endgame = convert_formatted_board({
            // clang-format off
            /*8*/ -100, -100, -100,  -63, -100,  -17, -100,  100,
            /*7*/ -100,  -65, -100,  -15, -100, -100, -100, -100,
            /*6*/ -100, -100, -100, -100, -100, -100, -100,  -67,
            /*5*/ -100, -100, -100,  -82,   -5,  -50, -100, -100,
            /*4*/  -75, -100,  -75,  -70,  -30,    3, -100, -100,
            /*3*/ -100, -100,  -37,  -90,  -25, -100,  -85,  -95,
            /*2*/  -60, -100, -100, -100, -100, -100, -100, -100,
            /*1*/  -97, -100, -100, -100, -100, -100, -100, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::king].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/  -99, -100,  -64, -100,  -98,  -94,  -98,  -89,
            /*7*/   99,   89,   97, -100,  -63,   91,   76, -100,
            /*6*/  -99,  -94, -100,  -74,   20,   28, -100,  -76,
            /*5*/   -5,  -98, -100, -100,  -18, -100, -100, -100,
            /*4*/  100, -100,  -97,   20,  -13, -100,    0, -100,
            /*3*/   -7,   13, -100,   25,   15,   15,   25,   -5,
            /*2*/  -70,  -40,    5,  -78,  -75,  -15,    0,   35,
            /*1*/ -100,   25,   15,  -95,    0,  -30,   30,   45
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::king].endgame = convert_formatted_board({
            // clang-format off
            /*8*/ -100,  -10,  100,   65, -100, -100,  -77,   10,
            /*7*/ -100,  -36,   21,  -11,  100,   27,   70,   74,
            /*6*/   18,   30,   40,  100,  -17,   75,  100,   18,
            /*5*/   25,    8,   50,   33,   46,   65,   20,   34,
            /*4*/   20,   15,   50,   60,   35,   27,    5,   20,
            /*3*/   10,  -15,   15,   20,   30,   15,  -50,  -15,
            /*2*/   93,   50,   20,   20,   30,  -25,  -10,  -45,
            /*1*/   70,  -50,  -35,   10,  -60,  -30,  -55,  -75
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        return result;
    }

    struct EvalSettings {
        static constexpr int32_t king_val = 20'000;
        int32_t queen_val = 700;
        int32_t rook_val = 350;
        int32_t bishop_val = 168;
        int32_t knight_val = 160;
        static constexpr int32_t pawn_val = 100;

        int32_t blocked_penalty = 11;
        int32_t doubled_penalty = 20;
        int32_t isolated_penalty = 4;
        int32_t backward_penalty = 0;
        int32_t passed_val = 0;

        // indexed based off of pawn count
        std::array<int32_t, 9> rook_adj = {19, 17, 32, 21, -9, -28, -38, -50, -50};
        // indexed based off of pawn count
        std::array<int32_t, 9> knight_adj = {-48, 13, 7, -6, 0, -7, -17, -22, -46};

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
        Clock::time_point deadline;
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
            const EvalSettings &eval_settings, const SearchSettings &search_settings
        );

        bool next(Move &move);

      private:
        const Position &m_pos;
        Color m_side;
        const MoveHeuristics &m_heuristics;
        const EvalSettings &eval_settings;
        const SearchSettings &search_settings;

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
    static int32_t move_piece_value(PieceType type, const EvalSettings &eval_settings);
    static int32_t
    static_exchange_eval(const Position &pos, const Move &move, const EvalSettings &eval_settings);
    bool should_stop(SearchContext &ctx, const SearchSettings &search_settings);
    Move get_tt_move(SearchContext &ctx);
    int32_t capture_score(const Move &move);
    int32_t move_score(const Move &move, const Move &pv_move, const Move &tt_move);
    void order_moves(MoveStack<> &moves, const Move &pv_move, const Move &tt_move);
    void update_pv(SearchContext &ctx, uint16_t ply, const Move &move);
    int32_t quiescence(
        int32_t alpha, int32_t beta, SearchContext &ctx, int ply, int qply,
        const EvalSettings &eval_settings, const SearchSettings &search_settings
    );
    int32_t score_to_tt(int32_t score, uint16_t ply, const SearchSettings &search_settings);
    int32_t score_from_tt(int32_t score, uint16_t ply, const SearchSettings &search_settings);
    int32_t negamax(
        uint16_t depth, uint16_t ply, int32_t alpha, int32_t beta, SearchContext &ctx,
        bool prev_move_was_null, const EvalSettings &eval_settings,
        const SearchSettings &search_settings
    );
    int32_t root_search(
        MoveStack<> &root_moves, uint16_t depth, SearchContext &ctx, int32_t alpha, int32_t beta,
        Move &best_move, const EvalSettings &eval_settings, const SearchSettings &search_settings
    );

  public:
    TunedPSTEngine();
    TunedPSTEngine(std::string_view fen, Position::FenParseErr &out_err);

    void operator=(const TunedPSTEngine &) = delete;

    Position::FenParseErr parse_fen(std::string_view fen) override;

    virtual Position &position() override;
    const Position &position() const override;

    const MoveStack<> &gen_legal_moves() override;

    void do_move(const Move &move) override;
    void undo_move(const Move &move) override;

    int32_t evaluate(const EvalSettings &eval_settings) const;
    int32_t evaluate() const override;

    SearchResult search(
        std::chrono::milliseconds time, const EvalSettings &eval_settings,
        const SearchSettings &search_settings
    );
    SearchResult search(std::chrono::milliseconds time) override;
};
} // namespace chess
