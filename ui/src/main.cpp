#include "bitboard.hpp"
#include "engine.hpp"
#include "fen_pos.hpp"
#include "piece_color.hpp"
#include "position.hpp"

#include "chessboard.hpp"
#include "perft.hpp"
#include "test.hpp"

#include <string>

#include <wx/spinctrl.h>
#include <wx/wx.h>

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
    using parse_func_fn = std::function<void(std::string_view)>;

    FENTextBox(wxWindow *parent, parse_func_fn parse_fen, const std::string &fen)
        : wxTextCtrl(parent, wxID_ANY, fen, wxPoint(600, 175), wxSize(175, 25), wxTE_PROCESS_ENTER),
          m_parse_fen(parse_fen) {

        SetToolTip("Hit Enter/Return after pasting the FEN.");
        Bind(wxEVT_TEXT_ENTER, &FENTextBox::OnEnter, this);
    }

    void set_fen(const std::string &fen) { ChangeValue(fen); }

  private:
    parse_func_fn m_parse_fen;

    void OnEnter(wxCommandEvent &) { m_parse_fen(GetValue().ToStdString()); }
};

class Frame : public wxFrame {
  private:
    wxPanel *m_panel;
    ChessBoard *m_board;
    ALPlogo m_alp;
    FENTextBox *m_fen_txt_box;
    wxButton *m_starting_pos_btn;
    wxButton *m_test_btn;
    PerftTextBox *m_perft_txt_box;
    wxButton *m_eval_btn;
    wxButton *m_engine_move_btn;

    chess::Engine &m_engine;
    chess::GameEnd m_game_end;

  private:
    void handle_game_end(chess::GameEnd game_end) {
        m_game_end = game_end;
        if (game_end == chess::GameEnd::proceed) { return; }
        m_engine.moves.clear();
        std::string message, caption;
        switch (game_end) {
        case chess::GameEnd::checkmate:
            caption = "Checkmate!";
            if (m_engine.st_stack.back().active_color == chess::Color::white) {
                message = "Black has checkmated white! Since white can't avoid check.";
            } else {
                message = "White has checkmated black! Since black can't avoid check.";
            }
            break;
        case chess::GameEnd::stalemate:
            caption = "Stalemate!";
            message = "There are no legal moves but there is no check.";
            break;
        case chess::GameEnd::dead_pos:
            caption = "Dead Position!";
            message = "There are not enough pieces for either side to checkmate the other side.";
            break;
        case chess::GameEnd::threefold:
            caption = "Threefold Repetition!";
            message = "This position has been seen 3 times.";
            break;
        case chess::GameEnd::move50:
            caption = "50 Move Rule!";
            message = "There have been 50 consecutive moves without a pawn moving or a piece"
                      " captured.";
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
        if (!handle_fen_parse_err(m_engine.parse_fen(fen))) { return; }
        const auto &moves = m_engine.gen_legal_moves();
        m_board->clear_selection();
        m_board->Refresh();
        handle_game_end(m_engine.pos.get_game_end(moves));
    }
    void starting_pos(wxCommandEvent &) {
        parse_fen(chess::fen::starting_pos);
        m_fen_txt_box->SetValue(std::string(chess::fen::starting_pos));
    }

    void run_tests(wxCommandEvent &) {
        TestDialog dialog(this);
        auto test_results = run_perft_tests();
        for (const auto &result : test_results) { dialog.add_test_result(result); }
        dialog.ShowModal();
    }

    void perft(std::string_view sv) {
        std::string depth_str(sv);
        depth_str.erase(
            depth_str.begin(),
            std::find_if(depth_str.begin(), depth_str.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            })
        );

        uint16_t depth;
        try {
            depth = std::stoul(depth_str);
        } catch (std::exception &e) {
            wxMessageBox("The given text is not a positive number", "Bad Depth");
            return;
        }

        PerftDialog dialog(this, m_engine.pos);
        dialog.run(depth);
        dialog.ShowModal();
    }

    void eval(wxCommandEvent &) {
        int32_t score = m_engine.evaluate();
        wxMessageBox(wxString() << score, "Evalution score");
    }

    void do_move(const chess::Move &move) {
        m_engine.do_move(move);
        const auto &moves = m_engine.gen_legal_moves();
        m_fen_txt_box->set_fen(m_engine.pos.format_as_fen());
        Refresh();
        handle_game_end(m_engine.pos.get_game_end(moves));
    }

    void engine_move(wxCommandEvent &) {
        if (m_game_end != chess::GameEnd::proceed) { return; }
        m_board->clear_selection();
        m_board->Refresh();
        using namespace std::chrono_literals;
        auto results = m_engine.search(1000ms);
        do_move(results.best_move);
        std::cout << "Depth Went: " << results.depth << ", Nodes: " << results.nodes << '\n';
    }

    void OnPaint(wxPaintEvent &) {
        wxPaintDC dc(m_panel);
        m_alp.draw(dc);

        wxFont fen_font(18, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(fen_font);
        dc.DrawText("FEN", wxPoint(602, 140));
        wxFont perft_font(14, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(perft_font);
        dc.DrawText("Run Perft", wxPoint(600, 250));
    }

    void OnClick(wxMouseEvent &event) {
        m_board->clear_selection();
        event.Skip();
    }

  public:
    Frame(chess::Engine &engine)
        : wxFrame(
              nullptr, wxID_ANY, "Alp Chess", wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER
          ),
          m_engine(engine) {
        m_game_end = m_engine.pos.get_game_end(m_engine.gen_legal_moves());

        SetClientSize(800, 600);
        SetMinSize(GetSize());
        SetMaxSize(GetSize());

        m_panel = new wxPanel(this);

        m_starting_pos_btn = new wxButton(
            m_panel, wxID_ANY, "Starting Position", wxPoint(650, 145), wxSize(125, 25)
        );
        m_starting_pos_btn->Bind(wxEVT_BUTTON, &Frame::starting_pos, this);
        m_fen_txt_box = new FENTextBox(
            m_panel, [this](std::string_view fen) { parse_fen(fen); }, m_engine.pos.format_as_fen()
        );

        m_test_btn = new wxButton(m_panel, wxID_ANY, "Run Tests", wxPoint(635, 210));
        m_test_btn->Bind(wxEVT_BUTTON, &Frame::run_tests, this);

        m_perft_txt_box =
            new PerftTextBox(m_panel, [this](std::string_view depth_str) { perft(depth_str); }, 1);

        m_eval_btn = new wxButton(m_panel, wxID_ANY, "Evaluate Position", wxPoint(615, 285));
        m_eval_btn->Bind(wxEVT_BUTTON, &Frame::eval, this);

        m_engine_move_btn = new wxButton(m_panel, wxID_ANY, "Engine Move", wxPoint(625, 325));
        m_engine_move_btn->Bind(wxEVT_BUTTON, &Frame::engine_move, this);

        constexpr int boardPanelSize = ChessBoard::BOARD_SIZE + ChessBoard::BORDER * 2;
        m_board =
            new ChessBoard(m_panel, m_engine.pos, m_engine.moves, [&](const chess::Move &move) {
                do_move(move);
            });
        m_board->SetSize(30, 25, boardPanelSize, boardPanelSize);

        m_panel->Bind(wxEVT_PAINT, &Frame::OnPaint, this);
        m_panel->Bind(wxEVT_LEFT_DOWN, &Frame::OnClick, this);
    }
};

class Alp : public wxApp {
  private:
    chess::Engine engine;
    chess::Position::FenParseErr err;

  public:
    Alp() : engine(chess::fen::starting_pos, err) {}
    bool OnInit() override {
        if (err != chess::Position::FenParseErr::ok) {
            std::cerr << "[BUG] Can't parse starting position FEN\n";
            std::cerr << chess::Position::format_fen_err(err) << '\n';
            return false;
        }
        engine.gen_legal_moves();
        Frame *frame = new Frame(engine);

        frame->Show(true);

        return true;
    }
};

wxIMPLEMENT_APP(Alp);
