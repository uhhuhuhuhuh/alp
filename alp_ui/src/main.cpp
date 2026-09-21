#include "fen_pos.hpp"
#include "genmoves.hpp"
#include "piece_color.hpp"
#include "position.hpp"
#include "square.hpp"
#include "wx/popupwin.h"

#include <string>

#include <wx/bitmap.h>
#include <wx/bmpbndl.h>
#include <wx/statbmp.h>
#include <wx/wx.h>

class SelectedData {
  public:
    chess::sqr_t sqr = -1;
    std::vector<chess::Move *> moves;
};

class PromotionPopup : public wxPopupTransientWindow {
  public:
    using promote_func_t = std::function<void(chess::PieceType)>;

    PromotionPopup(
        wxWindow *parent, chess::Color color,
        const chess::PieceArray<chess::ColorArray<wxBitmap>> &pieces, promote_func_t promote_func
    )
        : wxPopupTransientWindow(parent), promote_func(promote_func) {

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

  private:
    promote_func_t promote_func;
};

class ChessBoard : public wxPanel {
  public:
    static constexpr int SQUARE_SIZE = 64;
    static constexpr int BOARD_SIZE = SQUARE_SIZE * 8;
    static constexpr int BORDER = 16;
    using do_move_func_t = std::function<void(const chess::Move &move)>;

  private:
    chess::Position &pos;
    chess::MoveStack &moves;
    do_move_func_t do_move_func;

    static inline bool has_set_pieces = false;
    static inline chess::PieceArray<chess::ColorArray<wxBitmap>> piecesx64;
    SelectedData selected;

  private:
    void OnPaint(wxPaintEvent &) {
        wxPaintDC dc(this);

        constexpr int x = BORDER, y = BORDER;

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxColour("#010d26"));
        dc.DrawRectangle(0, 0, BOARD_SIZE + BORDER * 2, BOARD_SIZE + BORDER * 2);

        for (chess::rank_t drawn_rank = 0; drawn_rank < 8; drawn_rank++) {
            chess::rank_t rank = 7 - drawn_rank;
            for (chess::file_t file = 0; file < 8; file++) {
                bool white = (rank + file) % 2 == 0;
                dc.SetBrush(wxBrush(white ? wxColour("#e1e8f7") : wxColour("#364f80")));
                dc.DrawRectangle(
                    x + file * SQUARE_SIZE, y + drawn_rank * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE
                );
                chess::sqr_t sqr = file + rank * 8;
                chess::Piece &p = pos.board[sqr];
                if (p.type == chess::PieceType::none) {
                    continue;
                }

                int piece_x = x + file * SQUARE_SIZE;
                int piece_y = y + drawn_rank * SQUARE_SIZE;

                dc.DrawBitmap(piecesx64[p.type][p.color], piece_x, piece_y, true);
            }
        }

        if (selected.sqr == -1) {
            return;
        }

        chess::rank_t rank = selected.sqr / 8;
        rank = 7 - rank;
        chess::file_t file = selected.sqr % 8;
        int left = x + file * SQUARE_SIZE;
        int top = y + rank * SQUARE_SIZE;

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

        for (auto move_ptr : selected.moves) {
            chess::Move &move = *move_ptr;
            chess::file_t to_file = move.to % 8;
            chess::rank_t to_rank = move.to / 8;

            to_rank = 7 - to_rank;

            int center_x = x + to_file * SQUARE_SIZE + SQUARE_SIZE / 2;
            int center_y = y + to_rank * SQUARE_SIZE + SQUARE_SIZE / 2;
            int radius = SQUARE_SIZE / 6;

            bool is_capture_black = pos.board[move_ptr->to].color == chess::Color::black &&
                                    move.flag == chess::MoveFlag::capture;
            uint8_t rgb = is_capture_black ? 255 : 0;
            dc.SetBrush(wxBrush(wxColour(rgb, rgb, rgb, 70)));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawCircle(center_x, center_y, radius);
        }
    }

    void OnClick(wxMouseEvent &event) {
        const int x = BORDER;
        const int y = BORDER;

        int mouseX = event.GetX();
        int mouseY = event.GetY();

        chess::file_t file = (mouseX - x) / SQUARE_SIZE;
        chess::rank_t drawn_rank = (mouseY - y) / SQUARE_SIZE;
        chess::rank_t rank = 7 - drawn_rank;

        if (file < 0 || file >= 8 || drawn_rank < 0 || drawn_rank >= 8) {
            clear_selection();
            return;
        }

        chess::sqr_t sqr = rank * 8 + file;

        auto move_it = std::find_if(
            selected.moves.begin(), selected.moves.end(), [sqr](chess::Move *move_ptr) {
                return sqr == move_ptr->to;
            }
        );

        if (move_it == selected.moves.end()) {
            selected.sqr = sqr;
            selected.moves.clear();
            for (auto &move : moves) {
                if (move.from == sqr) {
                    selected.moves.push_back(&move);
                }
            }
            Refresh();
            return;
        }

        auto *move = *move_it;
        if (move->promotion_type == chess::PieceType::none) {
            do_move_func(*move);
            clear_selection();
            return;
        }

        PromotionPopup *popup = new PromotionPopup(
            this,
            pos.board[move->from].color,
            piecesx64,
            [this, move](chess::PieceType promotion_type) {
                chess::Move promotion_move = *move;
                promotion_move.promotion_type = promotion_type;
                do_move_func(promotion_move);
                clear_selection();
                Refresh();
            }
        );
        wxPoint popup_pos = this->ClientToScreen(event.GetPosition());
        popup->Position(popup_pos, wxSize(0, 0));
        popup->Popup();
    }

  public:
    void clear_selection() {
        selected.sqr = -1;
        selected.moves.clear();
        Refresh();
    }

    ChessBoard(
        wxWindow *parent, chess::Position &pos, chess::MoveStack &moves, do_move_func_t do_move
    )
        : wxPanel(parent), pos(pos), moves(moves), do_move_func(do_move), selected() {
        Bind(wxEVT_PAINT, &ChessBoard::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &ChessBoard::OnClick, this);

        SetBackgroundColour(*wxWHITE);

        if (has_set_pieces) {
            return;
        }
        has_set_pieces = true;

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
                piecesx64[piece][color] =
                    wxBitmapBundle::FromSVGFile(full_path, wxSize(SQUARE_SIZE, SQUARE_SIZE))
                        .GetBitmap(wxSize(SQUARE_SIZE, SQUARE_SIZE));
            }
        }
    }
};

class ALPlogo {
  public:
    ALPlogo()
        : bitmap(
              wxBitmapBundle::FromSVGFile("resources/ALP.svg", wxSize(200, 100))
                  .GetBitmap(wxSize(200, 100))
          ) {}

    void draw(wxDC &dc) { dc.DrawBitmap(bitmap, 600, 25, true); }

  private:
    wxBitmap bitmap;
};

class FENTextBox : public wxTextCtrl {
  public:
    using parse_func_t = std::function<void(std::string_view)>;

    FENTextBox(wxWindow *parent, parse_func_t parse_fen, const std::string &fen)
        : wxTextCtrl(parent, wxID_ANY, fen, wxPoint(600, 175), wxSize(175, 25), wxTE_PROCESS_ENTER),
          parse_fen(parse_fen) {

        SetToolTip("Hit Enter/Return after pasting the FEN.");
        Bind(wxEVT_TEXT_ENTER, &FENTextBox::OnEnter, this);
    }

    void set_fen(const std::string &fen) { ChangeValue(fen); }

  private:
    parse_func_t parse_fen;

    void OnEnter(wxCommandEvent &) { parse_fen(GetValue().ToStdString()); }
};

class Frame : public wxFrame {
  private:
    wxPanel *panel;
    ChessBoard *board;
    ALPlogo alp;
    FENTextBox *fen_txt_box;
    wxButton *starting_pos_btn;

    chess::Position &pos;
    chess::State *&st;
    chess::State *const init_st;
    chess::MoveStack &moves;

  private:
    void handle_game_end() {
        chess::State::GameEnd game_end = pos.st->game_end_state;
        if (game_end == chess::State::GameEnd::proceed) {
            return;
        }
        moves.clear();
        std::string message, caption;
        switch (game_end) {
        case chess::State::GameEnd::checkmate:
            caption = "Checkmate!";
            if (pos.st->active_color == chess::Color::white) {
                message = "Black has checkmated white! Since white can't avoid check.";
            } else {
                message = "White has checkmated black! Since black can't avoid check.";
            }
            break;
        case chess::State::GameEnd::stalemate:
            caption = "Stalemate!";
            message = "There are no legal moves but there is no check.";
            break;
        case chess::State::GameEnd::dead_pos:
            caption = "Dead Position!";
            message = "There are not enough pieces for either side to checkmate the other side.";
            break;
        case chess::State::GameEnd::threefold:
            caption = "Threefold Repetition!";
            message = "This position has been seen 3 times.";
            break;
        case chess::State::GameEnd::move50:
            caption = "50 Move Rule!";
            message =
                "There have been 50 consecutive moves without a pawn moving or a piece captured.";
            break;
        default: break;
        }
        wxMessageBox(message, caption);
    }
    bool handle_fen_parse_err(chess::Position::FenParseErr err) {
        if (err != chess::Position::FenParseErr::ok) {
            wxMessageBox(chess::Position::format_fen_err(err), "Bad FEN position");
            return false;
        }
        return true;
    }

    void parse_fen(std::string_view fen) {
        chess::State *old_st_ptr = st;
        st = init_st;
        auto err = pos.parse_fen(fen, *st++);
        if (!handle_fen_parse_err(err)) {
            st = old_st_ptr;
            return;
        }
        moves = chess::gen_moves<chess::GenType::legal>(pos);
        pos.st->game_end_state = pos.get_game_end_state(moves);
        handle_game_end();
        board->clear_selection();
        board->Refresh();
    }
    void starting_pos(wxCommandEvent &) {
        parse_fen(chess::fen::starting_pos);
        fen_txt_box->SetValue(std::string(chess::fen::starting_pos));
    }

    void OnPaint(wxPaintEvent &) {
        wxPaintDC dc(panel);
        alp.draw(dc);

        wxFont font(18, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(font);
        dc.DrawText("FEN", wxPoint(602, 140));
    }

    void OnClick(wxMouseEvent &event) {
        board->clear_selection();
        event.Skip();
    }

  public:
    Frame(chess::Position &pos, chess::State *&st, chess::MoveStack &moves)
        : wxFrame(
              nullptr, wxID_ANY, "Alp Chess", wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER
          ),
          pos(pos), st(st), init_st(st), moves(moves) {
        SetClientSize(800, 600);
        SetMinSize(GetSize());
        SetMaxSize(GetSize());

        panel = new wxPanel(this);

        starting_pos_btn =
            new wxButton(panel, wxID_ANY, "Starting Position", wxPoint(650, 145), wxSize(125, 25));
        starting_pos_btn->Bind(wxEVT_BUTTON, &Frame::starting_pos, this);
        fen_txt_box = new FENTextBox(
            panel, [this](std::string_view fen) { parse_fen(fen); }, pos.format_as_fen()
        );

        const int boardPanelSize = ChessBoard::BOARD_SIZE + ChessBoard::BORDER * 2;
        board = new ChessBoard(panel, pos, moves, [&](const chess::Move &move) {
            pos.do_move(move, *st++);
            moves = chess::gen_moves<chess::GenType::legal>(pos);
            fen_txt_box->set_fen(pos.format_as_fen());
            pos.st->game_end_state = pos.get_game_end_state(moves);
            handle_game_end();
        });
        board->SetSize(30, 25, boardPanelSize, boardPanelSize);

        panel->Bind(wxEVT_PAINT, &Frame::OnPaint, this);
        panel->Bind(wxEVT_LEFT_DOWN, &Frame::OnClick, this);
    }
};

class Alp : public wxApp {
  private:
    std::array<chess::State, 256> st_arr{};
    chess::State *st;
    chess::Position pos;
    chess::MoveStack moves;

  public:
    bool OnInit() override {
        st = &st_arr[0];
        auto err = pos.parse_fen(chess::fen::starting_pos, *st++);
        if (err != chess::Position::FenParseErr::ok) {
            std::cout << "[BUG] Can't parse starting position FEN\n";
            return false;
        }
        moves = chess::gen_moves<chess::GenType::legal>(pos);
        Frame *frame = new Frame(pos, st, moves);

        frame->Show(true);

        return true;
    }
};

wxIMPLEMENT_APP(Alp);
