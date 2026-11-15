#include "chessboard.hpp"

#include "constbb.hpp"
#include "genmoves.hpp"
#include "piece_color.hpp"

#include "wx/popupwin.h"
#include <wx/bitmap.h>
#include <wx/bmpbndl.h>
#include <wx/statbmp.h>
#include <wx/wx.h>

PromotionPopup::PromotionPopup(
    wxWindow *parent, chess::Color color,
    const chess::PieceArray<chess::ColorArray<wxBitmap>> &pieces, promote_func_t promote_func
)
    : wxPopupTransientWindow(parent), m_promote_func(promote_func) {

    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);

    constexpr std::array<chess::PieceType, 4> options = {
        chess::PieceType::queen,
        chess::PieceType::rook,
        chess::PieceType::bishop,
        chess::PieceType::knight
    };

    for (chess::PieceType piece : options) {
        wxBitmapButton *btn = new wxBitmapButton(
            this, wxID_ANY, pieces[piece][color], wxDefaultPosition, wxSize(64, 64)
        );

        btn->Bind(wxEVT_BUTTON, [this, piece, promote_func](wxCommandEvent &) {
            promote_func(piece);
            Dismiss();
        });

        sizer->Add(btn, 0, wxALL, 5);
    }

    SetSizerAndFit(sizer);
}

void ChessBoard::OnPaint(wxPaintEvent &) {
    wxPaintDC dc(this);

    constexpr int x = BORDER, y = BORDER;

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxColour("#010d26"));
    dc.DrawRectangle(0, 0, BOARD_SIZE + BORDER * 2, BOARD_SIZE + BORDER * 2);

    for (chess::rank_t rank = 7; rank >= 0; rank--) {
        chess::rank_t drawn_rank = flip_persp(7 - rank);
        for (chess::file_t file = 0; file < 8; file++) {
            chess::file_t drawn_file = flip_persp(file);
            chess::sqr_t sqr = file + rank * 8;
            bool light = chess::bb::bb(sqr) & chess::bb::light_sqrs();
            dc.SetBrush(wxBrush(light ? wxColour("#e1e8f7") : wxColour("#364f80")));
            dc.DrawRectangle(
                x + drawn_file * SQUARE_SIZE, y + drawn_rank * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE
            );

            if (sqr == m_prev_move.from || sqr == m_prev_move.to) {
                dc.SetBrush(wxBrush(wxColour("#28a99e80")));
                dc.DrawRectangle(
                    x + drawn_file * SQUARE_SIZE,
                    y + drawn_rank * SQUARE_SIZE,
                    SQUARE_SIZE,
                    SQUARE_SIZE
                );
            }

            chess::Piece &p = m_pos.board[sqr];
            if (p.type == chess::PieceType::none) { continue; }

            int piece_x = x + drawn_file * SQUARE_SIZE;
            int piece_y = y + drawn_rank * SQUARE_SIZE;

            dc.DrawBitmap(s_piecesx64[p.type][p.color], piece_x, piece_y, true);
        }
    }

    if (m_selected.sqr == -1) { return; }

    chess::rank_t drawn_rank = m_selected.sqr / 8;
    drawn_rank = flip_persp(7 - drawn_rank);
    chess::file_t drawn_file = flip_persp(m_selected.sqr % 8);
    int left = x + drawn_file * SQUARE_SIZE;
    int top = y + drawn_rank * SQUARE_SIZE;

    int selectionBorder = SQUARE_SIZE * 0.075;

    dc.SetBrush(wxBrush("#932f2f"));

    dc.DrawRectangle(left, top, SQUARE_SIZE, selectionBorder);
    dc.DrawRectangle(left, top + SQUARE_SIZE - selectionBorder, SQUARE_SIZE, selectionBorder);
    dc.DrawRectangle(
        left, top + selectionBorder, selectionBorder, SQUARE_SIZE - selectionBorder * 2
    );
    dc.DrawRectangle(
        left + SQUARE_SIZE - selectionBorder,
        top + selectionBorder,
        selectionBorder,
        SQUARE_SIZE - selectionBorder * 2
    );

    for (auto move_ptr : m_selected.moves) {
        chess::Move &move = *move_ptr;
        chess::file_t to_file = move.to % 8;
        chess::rank_t to_rank = move.to / 8;

        to_rank = 7 - to_rank;

        int center_x = x + flip_persp(to_file) * SQUARE_SIZE + SQUARE_SIZE / 2;
        int center_y = y + flip_persp(to_rank) * SQUARE_SIZE + SQUARE_SIZE / 2;
        int radius = SQUARE_SIZE / 6;

        bool is_capture_black = m_pos.board[move_ptr->to].color == chess::Color::black &&
                                move.flag == chess::MoveFlag::capture;
        uint8_t rgb = is_capture_black ? 255 : 0;
        dc.SetBrush(wxBrush(wxColour(rgb, rgb, rgb, 140)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawCircle(center_x, center_y, radius);
    }
}

void ChessBoard::OnClick(wxMouseEvent &event) {
    const int x = BORDER;
    const int y = BORDER;

    int mouseX = event.GetX();
    int mouseY = event.GetY();

    chess::file_t drawn_file = (mouseX - x) / SQUARE_SIZE;
    chess::rank_t drawn_rank = (mouseY - y) / SQUARE_SIZE;

    if (drawn_file < 0 || drawn_file >= 8 || drawn_rank < 0 || drawn_rank >= 8) {
        clear_selection();
        return;
    }

    drawn_rank = flip_persp(drawn_rank);
    drawn_file = flip_persp(drawn_file);
    chess::rank_t rank = 7 - drawn_rank;
    chess::file_t file = drawn_file;

    chess::sqr_t sqr = rank * 8 + file;

    auto move_it = std::find_if(
        m_selected.moves.begin(), m_selected.moves.end(), [sqr](chess::Move *move_ptr) {
            return sqr == move_ptr->to;
        }
    );

    if (move_it == m_selected.moves.end()) {
        m_selected.sqr = sqr;
        m_selected.moves.clear();
        for (auto &move : m_moves) {
            if (move.from == sqr) { m_selected.moves.push_back(&move); }
        }
        Refresh();
        return;
    }

    auto *move = *move_it;
    if (move->promotion_type == chess::PieceType::none) {
        m_prev_move = {.from = move->from, .to = move->to};
        m_do_move_func(*move);
        auto after_moves = chess::gen_moves<chess::GenType::legal>(m_pos);
        clear_selection();
        return;
    }

    PromotionPopup *popup = new PromotionPopup(
        this,
        m_pos.board[move->from].color,
        s_piecesx64,
        [this, move](chess::PieceType promotion_type) {
            chess::Move promotion_move = *move;
            promotion_move.promotion_type = promotion_type;
            m_prev_move = {.from = promotion_move.from, .to = promotion_move.to};
            m_do_move_func(promotion_move);
            clear_selection();
            Refresh();
        }
    );
    wxPoint popup_pos = this->ClientToScreen(event.GetPosition());
    popup->Position(popup_pos, wxSize(0, 0));
    popup->Popup();
}

void ChessBoard::clear_selection() {
    m_selected.sqr = -1;
    m_selected.moves.clear();
    Refresh();
}

ChessBoard::ChessBoard(
    wxWindow *parent, chess::Position &pos, chess::MoveStack<> &moves, do_move_func_t do_move
)
    : wxPanel(parent), m_pos(pos), m_moves(moves), m_do_move_func(do_move), m_selected() {
    Bind(wxEVT_PAINT, &ChessBoard::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &ChessBoard::OnClick, this);

    SetBackgroundColour(*wxWHITE);

    if (s_has_set_pieces) { return; }
    s_has_set_pieces = true;

    std::unordered_map<chess::PieceType, std::string> piece_names = {
        {chess::PieceType::pawn,   "pawn"  },
        {chess::PieceType::knight, "knight"},
        {chess::PieceType::bishop, "bishop"},
        {chess::PieceType::rook,   "rook"  },
        {chess::PieceType::queen,  "queen" },
        {chess::PieceType::king,   "king"  },
    };
    std::unordered_map<chess::Color, std::string> colors = {
        {chess::Color::white, "white"},
        {chess::Color::black, "black"},
    };

    std::string root = "resources/chess_pieces/";
    for (const auto &[piece, piece_name] : piece_names) {
        for (const auto &[color, color_str] : colors) {
            std::string full_path = root + color_str + '/' + piece_name + ".svg";
            s_piecesx64[piece][color] =
                wxBitmapBundle::FromSVGFile(full_path, wxSize(SQUARE_SIZE, SQUARE_SIZE))
                    .GetBitmap(wxSize(SQUARE_SIZE, SQUARE_SIZE));
        }
    }
}
