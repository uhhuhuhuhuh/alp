"""
---Compare against Stockfish---

Finds the exact difference between alp's perft and stockfish's perft
When it finds the difference it reports the fen position and the
change
"""

import re
import subprocess
import sys

build_dir = ""

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


def run_cas_helper(fen: str, depth: int) -> tuple[int, dict[str, int]]:
    output = run([f"{build_dir}/tools/cas/cas_helper", fen, str(depth)])
    return parse_perft_output(output)


def run_stockfish(fen: str, depth: int) -> tuple[int, dict[str, int]]:
    output = run(
        ["stockfish"],
        input=f"position fen {fen}\ngo perft {depth}",
    )
    return parse_perft_output(output)


def make_move(fen: str, move: str) -> str:
    return run([f"{build_dir}/tools/cas/cas_helper", fen, move]).strip()


def compare_position(fen: str, depth: int) -> tuple[str, str | None]:
    _, alp_moves = run_cas_helper(fen, depth)
    _, sf_moves = run_stockfish(fen, depth)

    alp_set = set(alp_moves)
    sf_set = set(sf_moves)

    extra = sorted(alp_set - sf_set)
    missing = sorted(sf_set - alp_set)

    if extra:
        print(f"EXTRA MOVE: Alp has {extra[0]}, Stockfish does not.")
        return "extra_move", extra[0]

    if missing:
        print(f"MISSING MOVE: Stockfish has {missing[0]}, Alp does not.")
        return "missing_move", missing[0]

    for move in sorted(alp_set):
        alp_nodes = alp_moves[move]
        sf_nodes = sf_moves[move]

        if alp_nodes != sf_nodes:
            delta = alp_nodes - sf_nodes
            print(
                f"COUNT MISMATCH at depth {depth}: {move}: "
                f"Alp={alp_nodes}, Stockfish={sf_nodes}, Delta={delta:+d}"
            )
            return "count_mismatch", move

    return "no_difference", None


def main():
    if len(sys.argv) != 4:
        print(f'Usage: {sys.argv[0]} build_dir "fen" depth')
        sys.exit(1)

    global build_dir
    build_dir = sys.argv[1]

    original_fen = sys.argv[2]
    original_depth = int(sys.argv[3])

    fen = original_fen
    depth = original_depth
    move_sequence = []

    alp_total, _ = run_cas_helper(fen, depth)
    sf_total, _ = run_stockfish(fen, depth)

    print("=== Initial position ===")
    print(f"FEN:       {original_fen}")
    print(f"Depth:     {original_depth}")
    print(f"Alp nodes: {alp_total}")
    print(f"SF nodes:  {sf_total}")

    if alp_total != sf_total:
        print(f"TOTAL MISMATCH: Delta={alp_total - sf_total:+d}")
    else:
        print("Root totals match.")

    stop_reason = "depth_exhausted"

    while depth > 0:
        print(f"\n=== Comparing position at depth {depth} ===")
        print(f"FEN: {fen}")

        status, move = compare_position(fen, depth)

        if status == "no_difference":
            stop_reason = "no_difference"
            print("STOP: No per-move differences found at this position.")
            break

        if status in ("extra_move", "missing_move"):
            stop_reason = status
            print("STOP: Legal move lists differ.")
            print("The discrepant move was not played.")
            break

        # A shared move has a different subtree count.
        assert move is not None
        move_sequence.append(move)

        print(f"Trace so far: {' '.join(move_sequence)}")

        fen = make_move(fen, move)
        depth -= 1

    print("\n" + "=" * 60)
    print("=== DEBUG SUMMARY ===")
    print(f"Initial FEN:  {original_fen}")
    print(f"Initial depth: {original_depth}")
    print(f"Alp total:    {alp_total}")
    print(f"Stockfish:    {sf_total}")
    print(f"Total delta:  {alp_total - sf_total:+d}")
    print("Move sequence: " + (" ".join(move_sequence) if move_sequence else "(empty)"))
    print(f"Moves traced: {len(move_sequence)}")
    print(f"Stop reason:  {stop_reason}")
    print(f"Final depth:  {depth}")
    print(f"Final FEN:    {fen}")

    if stop_reason == "no_difference":
        print(
            "Interpretation: No differing per-move counts or legal moves "
            "were found at the final position."
        )
        print("Note: This does not prove that the original root totals match.")
    elif stop_reason == "extra_move":
        print(
            "Interpretation: Alp generated a move Stockfish did not. "
            "Inspect legal move generation at the final FEN."
        )
    elif stop_reason == "missing_move":
        print(
            "Interpretation: Stockfish generated a move Alp did not. "
            "Inspect legal move generation at the final FEN."
        )
    elif stop_reason == "depth_exhausted":
        print(
            "Interpretation: The trace reached depth zero. "
            "Inspect the final position and replay the move sequence."
        )


if __name__ == "__main__":
    main()
