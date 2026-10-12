"""
Run games against Stockfish using multiple concurrent workers.
"""

import subprocess
import sys
import random
from concurrent.futures import ThreadPoolExecutor, as_completed

START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"


class Worker:
    def __init__(self, helper_path: str, time_ms: int):
        self.time_ms = time_ms
        self.is_helper_white = False
        self.fen = START_FEN
        self.start_fen = START_FEN

        self.helper = None
        self.stockfish = None

        try:
            self.helper = subprocess.Popen(
                [helper_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                text=True,
                bufsize=1,
            )

            self.stockfish = subprocess.Popen(
                ["stockfish"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
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

        # Give each Stockfish process one search thread so that
        # concurrent games do not oversubscribe the CPU as easily.
        self._write(
            self.stockfish,
            "setoption name Threads value 1",
        )

        self._write(self.stockfish, "isready")

        while self._readline(self.stockfish) != "readyok":
            pass

    def run_helper(self, arg: str) -> str:
        self._write(self.helper, arg)
        result = self._readline(self.helper)

        if result.startswith("ERROR:"):
            raise RuntimeError(f"Helper error: {result}")

        return result

    def set_position(self) -> str:
        return self.run_helper("p")

    def run_stockfish(self, fen: str) -> str:
        self._write(self.stockfish, f"position fen {fen}")
        self._write(
            self.stockfish,
            f"go movetime {self.time_ms}",
        )

        while True:
            line = self._readline(self.stockfish)

            if line.startswith("bestmove "):
                parts = line.split()

                if len(parts) < 2:
                    raise RuntimeError(f"Invalid Stockfish response: {line}")

                return parts[1]

    def play_game(self) -> str:
        self.is_helper_white = random.choice([True, False])

        self.fen = self.set_position()
        self.start_fen = self.fen

        # The active side is the side-to-move field in the FEN.
        fields = self.fen.split()

        if len(fields) < 2 or fields[1] not in ("w", "b"):
            raise RuntimeError(f"Invalid starting FEN: {self.fen}")

        is_white = fields[1] == "w"

        while True:
            end = self.run_helper("e")

            if end != "ok":
                if end == "mate":
                    # The side to move has been checkmated.
                    return "stockfish" if self.is_helper_white == is_white else "helper"

                return "draw"

            if is_white == self.is_helper_white:
                move = self.run_helper(str(self.time_ms))
            else:
                move = self.run_stockfish(self.fen)

            self.fen = self.run_helper(move)
            is_white = not is_white

    def close(self):
        # Close both child processes, including when initialization fails.
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
        for pipe in (process.stdin, process.stdout, process.stderr):
            if pipe is not None:
                try:
                    pipe.close()
                except (OSError, ValueError):
                    pass


def run_game(index: int, helper_path: str, time_ms: int):
    """Run one independent game and return its result and metadata."""
    worker = Worker(helper_path, time_ms)

    try:
        result = worker.play_game()

        return (
            index,
            result,
            worker.start_fen,
            worker.fen,
            worker.is_helper_white,
        )
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

    if game_count < 1:
        print("game_count must be positive.", file=sys.stderr)
        sys.exit(1)

    if time_ms < 1:
        print("time_ms must be positive.", file=sys.stderr)
        sys.exit(1)

    if max_workers < 1:
        print("workers must be positive.", file=sys.stderr)
        sys.exit(1)

    max_workers = min(max_workers, game_count)

    helper_path = f"{build_dir}/tools/games/games_helper"

    helper_wins = 0
    stockfish_wins = 0
    draws = 0
    failures = 0

    print(f"Running {game_count} games with " f"{max_workers} concurrent workers.")

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
                (
                    index,
                    result,
                    start_fen,
                    end_fen,
                    helper_white,
                ) = future.result()

            except Exception as exc:
                failures += 1
                print(
                    f"Game failed: {exc}",
                    file=sys.stderr,
                    flush=True,
                )
                continue

            if result == "helper":
                helper_wins += 1
            elif result == "stockfish":
                stockfish_wins += 1
            else:
                draws += 1

            print(f"Game {index}/{game_count}: {result}")
            print(f"    Starting FEN: {start_fen}")
            print(f"    End FEN:      {end_fen}")
            print(f"    Alp: {'white' if helper_white else 'black'}")

    print("\nResults")
    print(f"Helper wins:    {helper_wins}")
    print(f"Stockfish wins: {stockfish_wins}")
    print(f"Draws:          {draws}")
    print(f"Failures:       {failures}")
    print(f"Completed:      {game_count - failures}/{game_count}")


if __name__ == "__main__":
    main()
