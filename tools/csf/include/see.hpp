#pragma once

#include "engine.hpp"
#include "position.hpp"
#include "tranpostable.hpp"

#include <array>

namespace chess {
class SEEEngine : public IEngine {
  private:
    using Clock = std::chrono::steady_clock;

    static constexpr int32_t INF = 1'500'000;
    static constexpr int32_t MATE_SCORE = 1'000'000;
    static constexpr int32_t MAX_PLY = 256;
    static constexpr int32_t MATE_THRESHOLD = MATE_SCORE - MAX_PLY;

    static constexpr int MAX_KILLER_PLY = MAX_PLY;
    static constexpr int HISTORY_SIZE = 64 * 64;

    static constexpr int32_t ASPIRATION_WINDOW = 25;

    static constexpr int MAX_QPLY = 16;

    static constexpr uint64_t TIME_CHECK_INTERVAL = 1024;

    struct MoveHeuristics {
        std::array<std::array<Move, 2>, SEEEngine::MAX_KILLER_PLY> killers{};
        std::array<std::array<int32_t, SEEEngine::HISTORY_SIZE>, 2> history{};
    };

    struct SearchContext {
        Clock::time_point deadline;
        bool stopped = false;

        chess::TransposTable<64> &tt;

        std::array<std::array<chess::Move, MAX_PLY>, MAX_PLY> pv{};
        std::array<int, MAX_PLY> pv_length{};

        std::array<chess::Move, MAX_PLY> prev_pv{};
        int prev_pv_length = 0;

        MoveHeuristics heuristics;

        uint64_t nodes = 0;
    };

    class MovePicker {
      public:
        MovePicker(
            const Position &pos, const MoveStack<> &legal_moves, const Move &tt_move,
            const Move &pv_move, int ply, Color side, const MoveHeuristics &heuristics
        );

        bool next(Move &move);

      private:
        const Position &m_pos;
        Color m_side;
        const MoveHeuristics &m_heuristics;

        Move m_tt_move{};
        bool m_has_tt_move = false;

        Move m_pv_move{};
        bool m_has_pv_move = false;
        bool m_pv_move_pending = false;

        std::vector<Move> m_good_captures;
        std::vector<Move> m_killers;
        std::vector<Move> m_quiets;
        std::vector<Move> m_bad_captures;

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
    bool should_stop(SearchContext &ctx);
    Move get_tt_move(SearchContext &ctx);
    int32_t capture_score(const Move &move);
    int32_t move_score(const Move &move, const Move &pv_move, const Move &tt_move);
    void order_moves(MoveStack<> &moves, const Move &pv_move, const Move &tt_move);
    void update_pv(SearchContext &ctx, uint16_t ply, const Move &move);
    int32_t quiescence(int32_t alpha, int32_t beta, SearchContext &ctx, int ply, int qply = 0);
    int32_t score_to_tt(int32_t score, uint16_t ply);
    int32_t score_from_tt(int32_t score, uint16_t ply);
    int32_t negamax(uint8_t depth, uint16_t ply, int32_t alpha, int32_t beta, SearchContext &ctx);
    int32_t root_search(
        MoveStack<> &root_moves, uint8_t depth, SearchContext &ctx, int32_t alpha, int32_t beta,
        Move &best_move
    );

  public:
    SEEEngine();
    SEEEngine(std::string_view fen, Position::FenParseErr &out_err);

    void operator=(const SEEEngine &) = delete;

    Position::FenParseErr parse_fen(std::string_view fen) override;

    virtual Position &position() override;
    const Position &position() const override;

    const MoveStack<> &gen_legal_moves() override;

    void do_move(const Move &move) override;
    void undo_move(const Move &move) override;

    int32_t evaluate() const override;

    SearchResult search(std::chrono::milliseconds time) override;
};
} // namespace chess