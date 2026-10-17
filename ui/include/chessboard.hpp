#pragma once

#include "piece_color.hpp"
#include "position.hpp"
#include "square.hpp"

#include "wx/popupwin.h"
#include <wx/bitmap.h>
#include <wx/bmpbndl.h>
#include <wx/statbmp.h>
#include <wx/wx.h>

struct SelectedData {
    chess::sqr_t sqr = -1;
    std::vector<chess::Move *> moves;
};

class PromotionPopup : public wxPopupTransientWindow {
  public:
    using promote_func_t = std::function<void(chess::PieceType)>;

  public:
    PromotionPopup(
        wxWindow *parent, chess::Color color,
        const chess::PieceArray<chess::ColorArray<wxBitmap>> &pieces, promote_func_t promote_func
    );

  private:
    promote_func_t m_promote_func;
};

class ChessBoard : public wxPanel {
  public:
    static constexpr int SQUARE_SIZE = 64;
    static constexpr int BOARD_SIZE = SQUARE_SIZE * 8;
    static constexpr int BORDER = 16;
    using do_move_func_t = std::function<void(const chess::Move &move)>;

  private:
    chess::Position &m_pos;
    chess::MoveStack<> &m_moves;
    do_move_func_t m_do_move_func;

    static inline bool s_has_set_pieces = false;
    static inline chess::PieceArray<chess::ColorArray<wxBitmap>> s_piecesx64;
    SelectedData m_selected;

  private:
    void OnPaint(wxPaintEvent &);
    void OnClick(wxMouseEvent &event);

  public:
    void clear_selection();

    ChessBoard(
        wxWindow *parent, chess::Position &pos, chess::MoveStack<> &moves, do_move_func_t do_move
    );
};
