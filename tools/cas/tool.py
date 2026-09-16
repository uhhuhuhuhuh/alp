"""
---Compare against Stockfish---

Finds the exact difference between alp's perft and stockfish's perft
When it finds the difference it reports the fen position and the
change
"""

import re
import subprocess
import sys

has_seen_diff = False

MOVE_RE = re.compile(r"\b([a-h][1-8][a-h][1-8][qrbn]?)\s*:\s*(\d+)\b")
NODES_RE = re.compile(r"\bNodes searched:\s*(\d+)\b")


def parse_perft_output(text: str) -> tuple[int, dict[str, int]]:
    moves = {move: int(nodes) for move, nodes in MOVE_RE.findall(text)}

    match = NODES_RE.search(text)
    total = int(match.group(1)) if match else 0

    return total, moves


def run(command: list[str], input: str | None = None) -> str:
    try:
        result = subprocess.run(
            command,
            input=input,
            capture_output=True,
            text=True,
            check=True,
        )
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {' '.join(command)}", file=sys.stderr)
        if input:
            print(f"Input:\n{input}", file=sys.stderr)
        print(e.stderr, file=sys.stderr)
        sys.exit(e.returncode)

    return result.stdout


def run_alp(fen: str, depth: int) -> tuple[int, dict[str, int]]:
    output = run(["build/alp", fen, str(depth)])
    return parse_perft_output(output)


def run_stockfish(fen: str, depth: int) -> tuple[int, dict[str, int]]:
    output = run(
        ["stockfish"],
        input=f"position fen {fen}\ngo perft {depth}",
    )
    return parse_perft_output(output)


def make_move(fen: str, move: str) -> str:
    return run(["build/alp", fen, move]).strip()


def compare_position(fen: str, depth: int) -> str | None:
    global has_seen_diff
    _, alp_moves = run_alp(fen, depth)
    _, sf_moves = run_stockfish(fen, depth)

    alp_set = set(alp_moves)
    sf_set = set(sf_moves)

    for move in sorted(alp_set - sf_set):
        has_seen_diff = True
        print(f"Extra move: {move} at depth {depth}")
        return None

    for move in sorted(sf_set - alp_set):
        has_seen_diff = True
        print(f"Missing move: {move} at depth {depth}")
        return None

    for move in sorted(alp_set):
        alp_nodes = alp_moves[move]
        sf_nodes = sf_moves[move]

        if alp_nodes != sf_nodes:
            has_seen_diff = True
            print(
                f"Difference at depth {depth}: {move}: Alp={alp_nodes}, Stockfish={sf_nodes}"
            )
            return move

    return None


def main():
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} "fen" depth')
        sys.exit(1)

    fen = sys.argv[1]
    depth = int(sys.argv[2])

    original_fen = fen
    original_depth = depth

    alp_total, _ = run_alp(fen, depth)
    sf_total, _ = run_stockfish(fen, depth)

    while depth > 0:
        move = compare_position(fen, depth)

        if move is None:
            break

        fen = make_move(fen, move)
        depth -= 1

    global has_seen_diff
    print("" if has_seen_diff else "No differences found.\n")
    print(f"Initial FEN: {original_fen}")
    print(f"Depth:       {original_depth}")
    print(f"Stockfish:   {sf_total}")
    print(f"Alp:         {alp_total}")
    print(f"Position:    {fen}")


if __name__ == "__main__":
    main()
