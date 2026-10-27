import argparse
import chess
import chess.pgn


def parse_rating(game, color: str) -> int | None:
    value = game.headers.get(f"{color}Elo", "")
    try:
        return int(value)
    except (ValueError, TypeError):
        return None


def parse_base_time(game) -> float | None:
    """Return the initial time per player, in seconds, if available."""
    value = game.headers.get("TimeControl", "")

    if not value or value == "-":
        return None

    first_period = value.split(":")[0]
    if "/" in first_period:
        first_period = first_period.split("/")[-1]

    base_time = first_period.split("+")[0]

    try:
        return float(base_time)
    except ValueError:
        return None


def game_passes_filters(
    game,
    min_rating: int | None,
    max_rating: int | None,
    min_base_time: float | None,
    max_base_time: float | None,
    time_control: str | None,
) -> bool:
    white_elo = parse_rating(game, "White")
    black_elo = parse_rating(game, "Black")

    if min_rating is not None or max_rating is not None:
        # Exclude games with missing or unrated player ratings.
        if white_elo is None or black_elo is None:
            return False

        if min_rating is not None:
            if min(white_elo, black_elo) < min_rating:
                return False

        if max_rating is not None:
            if max(white_elo, black_elo) > max_rating:
                return False

    if time_control is not None:
        if game.headers.get("TimeControl", "") != time_control:
            return False

    if min_base_time is not None or max_base_time is not None:
        base_time = parse_base_time(game)

        # Exclude games whose time control cannot be parsed.
        if base_time is None:
            return False

        if min_base_time is not None and base_time < min_base_time:
            return False

        if max_base_time is not None and base_time > max_base_time:
            return False

    return True


def convert_pgn_to_tsv(
    input_path: str,
    output_path: str,
    game_limit: int | None = None,
    step: int = 1,
    min_rating: int | None = None,
    max_rating: int | None = None,
    min_base_time: float | None = None,
    max_base_time: float | None = None,
    time_control: str | None = None,
) -> None:
    positions_written = 0
    positions_seen = 0
    games_examined = 0
    games_processed = 0
    games_filtered = 0

    if step < 1:
        raise ValueError("Step must be at least 1.")

    if game_limit is not None and game_limit < 1:
        raise ValueError("Game limit must be at least 1.")

    result_scores = {
        "1-0": 1.0,
        "1/2-1/2": 0.5,
        "0-1": 0.0,
    }

    with open(input_path, "r", encoding="utf-8") as pgn_file, open(
        output_path, "w", encoding="utf-8"
    ) as tsv_file:

        while True:
            if game_limit is not None and games_processed >= game_limit:
                break

            game = chess.pgn.read_game(pgn_file)
            if game is None:
                break

            games_examined += 1

            result = game.headers.get("Result", "*")
            if result not in result_scores:
                games_filtered += 1
                continue

            if not game_passes_filters(
                game,
                min_rating,
                max_rating,
                min_base_time,
                max_base_time,
                time_control,
            ):
                games_filtered += 1
                continue

            white_result = result_scores[result]
            board = game.board()

            # Sample every Nth position within each accepted game.
            for ply, move in enumerate(game.mainline_moves(), start=1):
                board.push(move)
                positions_seen += 1

                if ply % step == 0:
                    tsv_file.write(f"{board.fen()}\t{white_result}\n")
                    positions_written += 1

            games_processed += 1

    print(f"Games examined: {games_examined}")
    print(f"Games filtered out: {games_filtered}")
    print(f"Games processed: {games_processed}")
    print(f"Positions examined: {positions_seen}")
    print(f"Positions written: {positions_written}")
    print(f"Output file: {output_path}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert PGN games into filtered FEN/result TSV samples."
    )

    parser.add_argument("input", help="Input PGN file")
    parser.add_argument(
        "-o",
        "--output",
        default="positions.tsv",
        help="Output TSV file (default: positions.tsv)",
    )
    parser.add_argument(
        "--game-limit",
        type=int,
        default=None,
        help="Maximum number of accepted games to process",
    )
    parser.add_argument(
        "--step",
        type=int,
        default=1,
        help="Save every Nth position within each game",
    )
    parser.add_argument(
        "--min-rating",
        type=int,
        default=None,
        help="Minimum rating required for BOTH players",
    )
    parser.add_argument(
        "--max-rating",
        type=int,
        default=None,
        help="Maximum rating allowed for BOTH players",
    )
    parser.add_argument(
        "--min-base-time",
        type=float,
        default=None,
        help="Minimum initial clock time in seconds",
    )
    parser.add_argument(
        "--max-base-time",
        type=float,
        default=None,
        help="Maximum initial clock time in seconds",
    )
    parser.add_argument(
        "--time-control",
        default=None,
        help='Exact PGN TimeControl header match, e.g. "600+0"',
    )

    args = parser.parse_args()

    convert_pgn_to_tsv(
        input_path=args.input,
        output_path=args.output,
        game_limit=args.game_limit,
        step=args.step,
        min_rating=args.min_rating,
        max_rating=args.max_rating,
        min_base_time=args.min_base_time,
        max_base_time=args.max_base_time,
        time_control=args.time_control,
    )


if __name__ == "__main__":
    main()
