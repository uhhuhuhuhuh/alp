"""
Generate PGN games by playing against Stockfish.

Games are saved in standard PGN format.

Results:
    1-0       White wins
    0-1       Black wins
    1/2-1/2   Draw
"""

import subprocess
import sys
import random
import threading

import chess
import chess.pgn

from concurrent.futures import ThreadPoolExecutor, as_completed

START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

OUTPUT_FILE = "games.pgn"
output_lock = threading.Lock()


class Worker:
    def __init__(self, helper_path: str, time_ms: int):
        self.time_ms = time_ms
        self.is_helper_white = False

        self.fen = START_FEN
        self.start_fen = START_FEN

        self.helper = None
        self.stockfish = None

        self.board = None
        self.game = None

        try:
            self.helper = subprocess.Popen(
                [helper_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
                bufsize=1,
            )

            self.stockfish = subprocess.Popen(
                ["stockfish"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
                bufsize=1,
            )

            self._stockfish_init()

        except Exception:
            self.close()
            raise

    @staticmethod
    def _write(process, command: str):
        if process.poll() is not None:
            raise RuntimeError(f"Process exited: {process.args}")

        if process.stdin is None:
            raise RuntimeError(f"No stdin available: {process.args}")

        process.stdin.write(command + "\n")
        process.stdin.flush()

    @staticmethod
    def _readline(process) -> str:
        if process.stdout is None:
            raise RuntimeError(f"No stdout available: {process.args}")

        line = process.stdout.readline()

        if not line:
            raise RuntimeError(f"Process exited unexpectedly: {process.args}")

        return line.strip()

    def _stockfish_init(self):
        self._write(self.stockfish, "uci")

        while self._readline(self.stockfish) != "uciok":
            pass

        self._write(self.stockfish, "setoption name Threads value 1")
        self._write(self.stockfish, "isready")

        while self._readline(self.stockfish) != "readyok":
            pass

    def run_helper(self, arg: str) -> str:
        self._write(self.helper, arg)
        result = self._readline(self.helper)

        if result.startswith("ERROR:"):
            raise RuntimeError(f"Alp error: {result}")

        return result

    def set_position(self) -> str:
        return self.run_helper("p")

    def run_stockfish(self, fen: str) -> str:
        self._write(self.stockfish, f"position fen {fen}")
        self._write(self.stockfish, f"go movetime {self.time_ms}")

        while True:
            line = self._readline(self.stockfish)

            if line.startswith("bestmove "):
                parts = line.split()

                if len(parts) < 2 or parts[1] == "(none)":
                    raise RuntimeError(f"Invalid Stockfish bestmove: {line}")

                return parts[1]

    def play_game(self):
        self.is_helper_white = random.choice([True, False])

        self.fen = self.set_position()
        self.start_fen = self.fen

        # Initialize the PGN game.
        self.board = chess.Board(self.start_fen)
        self.game = chess.pgn.Game()

        # Set up custom starting positions when necessary.
        if self.start_fen != START_FEN:
            self.game.setup(self.board)

        self.game.headers["Event"] = "Computer Game"
        self.game.headers["Site"] = "?"
        self.game.headers["Date"] = "????.??.??"
        self.game.headers["Round"] = "?"

        self.game.headers["White"] = "Alp" if self.is_helper_white else "Stockfish"

        self.game.headers["Black"] = "Stockfish" if self.is_helper_white else "Alp"

        node = self.game

        while True:
            # Record the current position if needed.
            end = self.run_helper("e")

            if end != "ok":
                if end == "mate":
                    # Side to move is checkmated.
                    winner_is_white = self.fen.split()[1] == "b"
                    score = 1.0 if winner_is_white else 0.0
                else:
                    score = 0.5

                result = "1-0" if score == 1.0 else "0-1" if score == 0.0 else "1/2-1/2"

                self.game.headers["Result"] = result

                return score

            fields = self.fen.split()

            if len(fields) < 2 or fields[1] not in ("w", "b"):
                raise RuntimeError(f"Invalid FEN: {self.fen}")

            is_white_to_move = fields[1] == "w"

            # Decide which engine plays.
            if is_white_to_move == self.is_helper_white:
                move = self.run_helper(str(self.time_ms))
            else:
                move = self.run_stockfish(self.fen)

            # Convert UCI move to a chess.Move.
            chess_move = chess.Move.from_uci(move)

            # Validate the move.
            if chess_move not in self.board.legal_moves:
                raise RuntimeError(
                    f"Illegal move {move} in position " f"{self.board.fen()}"
                )

            # Append the move to the PGN.
            node = node.add_variation(chess_move)

            # Update our local board.
            self.board.push(chess_move)

            # Update the helper's position.
            self.fen = self.run_helper(move)

    def close(self):
        for process in (self.helper, self.stockfish):
            if process is None:
                continue

            if process.poll() is not None:
                self._close_pipes(process)
                continue

            try:
                if process.stdin is not None:
                    process.stdin.close()
            except (OSError, ValueError):
                pass

            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.terminate()

                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            finally:
                self._close_pipes(process)

    @staticmethod
    def _close_pipes(process):
        for pipe in (
            process.stdin,
            process.stdout,
            process.stderr,
        ):
            if pipe is not None:
                try:
                    pipe.close()
                except (OSError, ValueError):
                    pass


def run_game(index: int, helper_path: str, time_ms: int):
    worker = Worker(helper_path, time_ms)

    try:
        score = worker.play_game()

        # Export the completed game as PGN.
        exporter = chess.pgn.StringExporter(
            headers=True,
            variations=True,
            comments=True,
        )

        pgn_text = worker.game.accept(exporter)

        # Write each complete game atomically.
        with output_lock:
            with open(OUTPUT_FILE, "a", encoding="utf-8") as output:
                output.write(pgn_text)
                output.write("\n\n")

        return index, score, len(worker.board.move_stack)

    finally:
        worker.close()


def main():
    if len(sys.argv) not in (4, 5):
        print(f"Usage: {sys.argv[0]} build_dir game_count time_ms [workers]")
        sys.exit(1)

    build_dir = sys.argv[1]

    try:
        game_count = int(sys.argv[2])
        time_ms = int(sys.argv[3])

        max_workers = int(sys.argv[4]) if len(sys.argv) == 5 else min(game_count, 4)

    except ValueError:
        print(
            "game_count, time_ms, and workers must be integers.",
            file=sys.stderr,
        )
        sys.exit(1)

    if game_count < 1 or time_ms < 1 or max_workers < 1:
        print(
            "All arguments must be positive integers.",
            file=sys.stderr,
        )
        sys.exit(1)

    max_workers = min(max_workers, game_count)

    helper_path = f"{build_dir}/tools/games/games_helper"

    # Clear the PGN output file.
    with open(OUTPUT_FILE, "w", encoding="utf-8"):
        pass

    print(f"Running {game_count} games with {max_workers} workers.")
    print(f"Writing PGN games to {OUTPUT_FILE}")

    completed = 0
    failures = 0

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = [
            executor.submit(
                run_game,
                i + 1,
                helper_path,
                time_ms,
            )
            for i in range(game_count)
        ]

        for future in as_completed(futures):
            try:
                index, score, moves = future.result()

                completed += 1

                result = (
                    "White wins"
                    if score == 1.0
                    else "Black wins" if score == 0.0 else "Draw"
                )

                print(f"Game {index}/{game_count}: " f"{result}, moves={moves}")

            except Exception as exc:
                failures += 1

                print(
                    f"Game failed: {exc}",
                    file=sys.stderr,
                    flush=True,
                )

    print("\nResults")
    print(f"Completed: {completed}/{game_count}")
    print(f"Failures:  {failures}")
    print(f"PGN:       {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
