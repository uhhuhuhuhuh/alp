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
    struct EvalSettings {
        static constexpr int32_t king_val = 20'000;
        int32_t queen_val = 700;
        int32_t rook_val = 350;
        int32_t bishop_val = 200;
        int32_t knight_val = 200;
        static constexpr int32_t pawn_val = 100;

        int32_t blocked_penalty = 36;
        int32_t doubled_penalty = 60;
        int32_t isolated_penalty = 0;
        int32_t backward_penalty = 4;
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

        chess::TransposTable<64> &tt;

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
