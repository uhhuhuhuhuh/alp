#include "benchmarking.hpp"
#include "fen_pos.hpp"
#include "genmoves.hpp"
#include "piece_color.hpp"
#include "position.hpp"

#include "chessboard.hpp"
#include "test.hpp"

#include <string>
#include <thread>

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

class PerftTextBox : public wxTextCtrl {
  public:
    using perft_fn = std::function<void(std::string_view)>;

    PerftTextBox(wxWindow *parent, perft_fn perft, uint16_t depth)
        : wxTextCtrl(
              parent, wxID_ANY, std::to_string(depth), wxPoint(705, 250), wxSize(70, 25),
              wxTE_PROCESS_ENTER
          ),
          m_parse_fen(perft) {
        SetToolTip("Hit Enter/Return after entering the perft depth.");
        Bind(wxEVT_TEXT_ENTER, &PerftTextBox::OnEnter, this);
    }

  private:
    perft_fn m_parse_fen;

    void OnEnter(wxCommandEvent &) { m_parse_fen(GetValue().ToStdString()); }
};

wxDECLARE_EVENT(EVT_PERFT_OUTPUT, wxThreadEvent);
wxDEFINE_EVENT(EVT_PERFT_OUTPUT, wxThreadEvent);

class PerftDialog : public wxDialog {
  private:
    chess::Position m_position;
    std::thread m_thread;
    wxTextCtrl *m_output = nullptr;

  public:
    PerftDialog(wxWindow *parent, const chess::Position &position)
        : wxDialog(parent, wxID_ANY, "Perft", wxDefaultPosition, wxSize(450, 650)),
          m_position(position) {
        SetFont(wxFont(12, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        auto *sizer = new wxBoxSizer(wxVERTICAL);

        m_output = new wxTextCtrl(
            this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY
        );

        sizer->Add(m_output, 1, wxEXPAND | wxALL, 10);

        SetSizer(sizer);

        Bind(EVT_PERFT_OUTPUT, &PerftDialog::on_perft_output, this);
        Bind(wxEVT_CLOSE_WINDOW, &PerftDialog::OnClose, this);
    }

    ~PerftDialog() {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    void run(uint8_t depth) {
        m_output->Clear();

        chess::Position position = m_position;

        m_thread = std::thread([this, depth, position = std::move(position)]() mutable {
            uint64_t nodes = chess::benchmark::perft(position, depth, [this](std::string line) {
                                 auto *event = new wxThreadEvent(EVT_PERFT_OUTPUT);
                                 event->SetString(wxString::FromUTF8(line));
                                 wxQueueEvent(this, event);
                             }).nodes;

            auto *event = new wxThreadEvent();
            event->SetString(wxString::Format("%llu", static_cast<unsigned long long>(nodes)));
            wxQueueEvent(this, event);
        });
    }

  private:
    void OnClose(wxCloseEvent &event) {
        if (m_thread.joinable()) {
            m_thread.join();
        }

        Destroy();
    }
    void on_perft_output(wxThreadEvent &event) { m_output->AppendText(event.GetString() + "\n"); }
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

    chess::Position &m_pos;
    chess::State *&m_st;
    chess::State *const m_init_st;
    chess::MoveStack &m_moves;

  private:
    void handle_game_end() {
        chess::State::GameEnd game_end = m_pos.st->game_end_state;
        if (game_end == chess::State::GameEnd::proceed) {
            return;
        }
        m_moves.clear();
        std::string message, caption;
        switch (game_end) {
        case chess::State::GameEnd::checkmate:
            caption = "Checkmate!";
            if (m_pos.st->active_color == chess::Color::white) {
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
            message = "There have been 50 consecutive moves without a pawn moving or a piece"
                      "captured.";
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
        chess::State *old_st_ptr = m_st;
        m_st = m_init_st;
        chess::State old_init_state = *m_init_st;
        auto err = m_pos.parse_fen(fen, *m_st++);
        if (!handle_fen_parse_err(err)) {
            *m_st = old_init_state;
            m_st = old_st_ptr;
            return;
        }
        m_moves = chess::gen_moves<chess::GenType::legal>(m_pos);
        m_pos.st->game_end_state = m_pos.get_game_end_state(m_moves);
        handle_game_end();
        m_board->clear_selection();
        m_board->Refresh();
    }
    void starting_pos(wxCommandEvent &) {
        parse_fen(chess::fen::starting_pos);
        m_fen_txt_box->SetValue(std::string(chess::fen::starting_pos));
    }

    void run_tests(wxCommandEvent &) {
        TestDialog dialog(this);
        auto test_results = run_perft_tests();
        for (const auto &result : test_results) {
            dialog.add_test_result(result);
        }
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

        PerftDialog dialog(this, m_pos);
        dialog.run(depth);
        dialog.ShowModal();
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
    Frame(chess::Position &pos, chess::State *&st, chess::MoveStack &moves)
        : wxFrame(
              nullptr, wxID_ANY, "Alp Chess", wxDefaultPosition, wxDefaultSize,
              wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER
          ),
          m_pos(pos), m_st(st), m_init_st(st), m_moves(moves) {
        SetClientSize(800, 600);
        SetMinSize(GetSize());
        SetMaxSize(GetSize());

        m_panel = new wxPanel(this);

        m_starting_pos_btn = new wxButton(
            m_panel, wxID_ANY, "Starting Position", wxPoint(650, 145), wxSize(125, 25)
        );
        m_starting_pos_btn->Bind(wxEVT_BUTTON, &Frame::starting_pos, this);
        m_fen_txt_box = new FENTextBox(
            m_panel, [this](std::string_view fen) { parse_fen(fen); }, pos.format_as_fen()
        );

        m_test_btn = new wxButton(m_panel, wxID_ANY, "Run Tests", wxPoint(635, 210));
        m_test_btn->Bind(wxEVT_BUTTON, &Frame::run_tests, this);

        m_perft_txt_box =
            new PerftTextBox(m_panel, [this](std::string_view depth_str) { perft(depth_str); }, 1);

        constexpr int boardPanelSize = ChessBoard::BOARD_SIZE + ChessBoard::BORDER * 2;
        m_board = new ChessBoard(m_panel, pos, moves, [&](const chess::Move &move) {
            pos.do_move(move, *st++);
            moves = chess::gen_moves<chess::GenType::legal>(pos);
            m_fen_txt_box->set_fen(pos.format_as_fen());
            pos.st->game_end_state = pos.get_game_end_state(moves);
            handle_game_end();
        });
        m_board->SetSize(30, 25, boardPanelSize, boardPanelSize);

        m_panel->Bind(wxEVT_PAINT, &Frame::OnPaint, this);
        m_panel->Bind(wxEVT_LEFT_DOWN, &Frame::OnClick, this);
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
            std::cerr << "[BUG] Can't parse starting position FEN\n";
            std::cerr << chess::Position::format_fen_err(err) << '\n';
            return false;
        }
        moves = chess::gen_moves<chess::GenType::legal>(pos);
        Frame *frame = new Frame(pos, st, moves);

        frame->Show(true);

        return true;
    }
};

wxIMPLEMENT_APP(Alp);
