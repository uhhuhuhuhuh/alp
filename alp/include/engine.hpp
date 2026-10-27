#pragma once

#include "position.hpp"
#include "tranpostable.hpp"

#include <array>

namespace chess {
class IEngine {
  public:
    struct SearchResult {
        chess::Move best_move = Move::none();
        int32_t score = 0;
        uint16_t depth = 0;
    };

    virtual ~IEngine() = default;

    virtual Position::FenParseErr parse_fen(std::string_view fen) = 0;

    virtual Position &position() = 0;
    virtual const Position &position() const = 0;

    virtual const MoveStack<> &gen_legal_moves() = 0;

    virtual void do_move(const Move &move) = 0;
    virtual void undo_move(const Move &move) = 0;

    virtual int32_t evaluate() const = 0;

    virtual SearchResult search(std::chrono::milliseconds time) = 0;
};

class Engine : public IEngine {
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
            /*7*/   99,  100,  100,  100,  100,  -23,   95,  100,
            /*6*/   75,   40,   83,  100,  100,  100,   18,   20,
            /*5*/  -85,  -20,  -15,   10,   40,   70,   20,  -30,
            /*4*/  -80,  -65,  -35,   40,  -30,   10,  -50,  -45,
            /*3*/  -95,  -25,   15,    5,    0,  -45,   20,  -45,
            /*2*/  -95,  -40,   10,  -52,  -25,  -15,   70,  -60,
            /*1*/    0,    0,    0,    0,    0,    0,    0,    0
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::pawn].endgame = convert_formatted_board({
            // clang-format off
            /*8*/    0,    0,    0,    0,    0,    0,    0,    0,
            /*7*/  100,  100,  100,  100,  100,  100,  100,  100,
            /*6*/  100,  100,  100,  100,  100,  100,  100,  100,
            /*5*/  100,  100,  100,  100,   90,   35,  100,  100,
            /*4*/   70,  100,   80,   85,   95,   75,   75,   45,
            /*3*/   95,   60,   65,   45,   95,   90,  100,   95,
            /*2*/  100,   95,   70,   73,  100,   95,   30,   55,
            /*1*/    0,    0,    0,    0,    0,    0,    0,    0
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::knight].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/ -100,  100,   43,  -56, -100, -100,  -99,  -46,
            /*7*/   -1,   95,  100,  100,  100,   20,   54, -100,
            /*6*/  100,  -43,  100,  100,  100,   77,  100,  -28,
            /*5*/  -12,   68,   95,  100,  100,  100,   80,   65,
            /*4*/    2,  -65,   25,   20,   23,   63,   96,  -30,
            /*3*/ -100,  -52,  -30,  -60,   12,   10,   13,  -93,
            /*2*/ -100, -100, -100,    3,  -18,  -75,  -96, -100,
            /*1*/ -100, -100, -100, -100,  -99,  -63, -100,   54
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::knight].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  -62,  -99,  -74,  100,   36, -100, -100,  -39,
            /*7*/  -93,   -9,   43,   16,   49,  100,   50,  -98,
            /*6*/  -97,  100,   99,   75,   -8,   87,  -65, -100,
            /*5*/  -65,  -60,  100,   49,   32,  -10,   65,  -23,
            /*4*/ -100,  100,   80,   55,   60,  -20,  -80,  -75,
            /*3*/    4,   45,   70,   87,  -25,  -10,  -35,   12,
            /*2*/ -100,  -65, -100,  -70,  -48,   85,  -66,  -35,
            /*1*/ -100, -100,  -80,  -63,  -71,  -35, -100,  -60
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::bishop].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/  -99,   69, -100, -100, -100, -100, -100, -100,
            /*7*/  -57,    7, -100, -100,  100,  -59, -100, -100,
            /*6*/  -99,  100,   15,  -25,   47,  100,   80,   28,
            /*5*/   33,  100,  100,  100,   93,   17,   45,  -59,
            /*4*/ -100,  -25,   45,   75,   65,    3,   85,  100,
            /*3*/   60,   50,   65,   75,   40,   55,    0,  100,
            /*2*/  -53,  -15,   15,   20,  -10,   84,   55,  -63,
            /*1*/   25,   43,  -25,  -97,  -23,    0, -100, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::bishop].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  100,  100,   38,   22,   18,  -99,  -26, -100,
            /*7*/  100,   85,  100,   93,  -59,  100,   47,   88,
            /*6*/  100,  100,   -8,   99,  100,   45,   99,   13,
            /*5*/   32,   85,   53,  100,   98,   29,    7,    7,
            /*4*/  -70,   37,   70,  100,  -27,  100,  -44,   -6,
            /*3*/ -100,   45,   55,  100,   75,   75,   99,   48,
            /*2*/   60,   60,   62,   50,   95,   40,   65,  100,
            /*1*/  -53,  -45,  -70,   25,  100, -100,  -53,  -27
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::rook].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/   98, -100, -100,   39, -100, -100, -100,  100,
            /*7*/   90,   97,   99,  100,  100,   99,   96,  100,
            /*6*/   70,   10,   99,   15,  100,  100,   36,   96,
            /*5*/ -100,  -42,   85,  -50,   -3,   62,  100,   20,
            /*4*/  -99,   32,  -71,   85,   73,   -5,   12,  -42,
            /*3*/  -50, -100,   -8,  -48,  -95,  -75,   85,   32,
            /*2*/  -81,  -36,  -50, -100,  -33,    0,  -42, -100,
            /*1*/ -100,  -40,  -50,  -45,  -20,  -40,   25, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::rook].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  100,  100,   33,   15,   35,  -42,   60,   69,
            /*7*/  100,   60,  100,   96,   93,  100,  100,  100,
            /*6*/   41,   99,   28,  100,   60,  -23,    5,  100,
            /*5*/  100,  100,   85,   70,  100,   65,   -5,  100,
            /*4*/   85,    9,   63, -100,  -92,   15,  -97,  -10,
            /*3*/  -10,  -26,   -5,   30,   43,   40,  -85,  -80,
            /*2*/ -100,  -15,  -27,    0,  -77,  -72,    0,    0,
            /*1*/  -30,  -25,   70,   20,  -20,  -20,  -30,  -20
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::queen].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/   54,  100,   14,  100,  100,  -99,   95,  100,
            /*7*/  -98,  -91,   78,  100,   99,  100,  100,  100,
            /*6*/  -77,   75,  -13,   34,  100,  100,  100,  100,
            /*5*/   20,   74, -100,    8,   58,   70,  100,   13,
            /*4*/ -100,   -6,  -10,   17,    8,   -2,   10,   18,
            /*3*/   52,  -42,  -17,    0,  -45,    0,   -5,  -15,
            /*2*/  -99,   27,  -20,  -40,    0,   10,  -16,  100,
            /*1*/  -98, -100, -100,  -67, -100,  -98,  -62,  100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::queen].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  -25,   24,   47,  100,  -50,  100,  -33,  -93,
            /*7*/  100,   67,   -7,  -90,  100,   95,  100,  100,
            /*6*/   32,  -80,  100,  100,  100,   98,   96,  100,
            /*5*/  100,   20,   99,  100,  -20,   42,  -90,  100,
            /*4*/  -25,  -37,   60,   98,  100,  -52,  -10,   92,
            /*3*/   40, -100,    8,   20,   15,   25,   90,   90,
            /*2*/  -90, -100,    0,  -10,  -15, -100,  -25, -100,
            /*1*/  100, -100, -100,  -90,  -83, -100, -100,  -81
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        result[PieceType::king].middlegame = convert_formatted_board({
            // clang-format off
            /*8*/ -100,  -92,   98,  100, -100,  -97,   61,  100,
            /*7*/  100, -100, -100,   99, -100,  -26,  -48,  -96,
            /*6*/  100,   89,   97,  -87,  -98,   99,    5,  -84,
            /*5*/   99,   99,  100,  -99, -100, -100,  -96, -100,
            /*4*/ -100,  -44,  -99, -100,   30, -100, -100, -100,
            /*3*/ -100,   53, -100,   20,  -27, -100,  -48, -100,
            /*2*/  -25, -100,    0,  -98,  -40,  -15,   25,   70,
            /*1*/  -48,   80,   90,  -35,   -5,  -60,  100,   55
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });
        result[PieceType::king].endgame = convert_formatted_board({
            // clang-format off
            /*8*/  -99,  100, -100,  100,   17,  100,  100,  100,
            /*7*/  100,   -7,   95,  100,  -17,   87,  100,   11,
            /*6*/   78,  100,  100,  100,   66,  100,  100,    8,
            /*5*/   70,  100,    3,   93,   73,   56,  100,  -66,
            /*4*/ -100,   23,  -17,   21,   28,   32,   15,  -10,
            /*3*/  -74, -100,    6,  -30,    5,  -25,  -22,  -45,
            /*2*/  -52,   50,  -40,   40,  -20,    0,  -35, -100,
            /*1*/ -100, -100,  -85,  -90,  -95,  -50,  -75, -100
            //       A     B     C     D     E     F     G     H
            // clang-format on
        });

        return result;
    }

    struct EvalSettings {
        static constexpr int32_t king_val = 20'000;
        int32_t queen_val = 1200;
        int32_t rook_val = 650;
        int32_t bishop_val = 402;
        int32_t knight_val = 429;
        static constexpr int32_t pawn_val = 100;

        int32_t blocked_penalty = 25;
        int32_t doubled_penalty = 12;
        int32_t isolated_penalty = 23;
        int32_t backward_penalty = 0;

        int32_t passed_bonus = 60;
        int32_t bishop_pair_bonus = 73;

        int32_t king_shield_2_bonus = 21;
        int32_t king_shield_3_bonus = 11;

        // indexed based off of pawn count
        std::array<int32_t, 9> rook_adj = {39, 50, 45, 45, 11, 5, 2, 8, -11};
        // indexed based off of pawn count
        std::array<int32_t, 9> knight_adj = {-50, 18, 50, -10, 4, -16, -7, 22, 34};

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
    Engine();
    Engine(std::string_view fen, Position::FenParseErr &out_err);

    void operator=(const Engine &) = delete;

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
