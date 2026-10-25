import argparse
import chess
import chess.pgn


def convert_pgn_to_tsv(
    input_path: str, output_path: str, game_limit: int | None = None, step: int = 1
) -> None:
    positions_written = 0
    games_processed = 0

    with open(input_path, "r", encoding="utf-8") as pgn_file, open(
        output_path, "w", encoding="utf-8"
    ) as tsv_file:
        while True:
            if game_limit is not None and games_processed >= game_limit:
                break

            game = chess.pgn.read_game(pgn_file)
            if game is None:
                break

            result = game.headers.get("Result", "*")
            result_scores = {
                "1-0": 1.0,
                "1/2-1/2": 0.5,
                "0-1": 0.0,
            }

            if result not in result_scores:
                continue

            white_result = result_scores[result]
            board = game.board()
            positions = [board.fen()]

            for move in game.mainline_moves():
                board.push(move)
                positions.append(board.fen())

            for i in range(0, len(positions), step):
                fen = positions[i]
                tsv_file.write(f"{fen}\t{white_result}\n")
                positions_written += 1

            games_processed += 1

    print(f"Games processed: {games_processed}")
    print(f"Positions written: {positions_written}")
    print(f"Output file: {output_path}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert PGN chess games into FEN/result TSV samples."
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
        help="Maximum number of valid games to process (default: all games)",
    )
    parser.add_argument(
        "--step",
        type=int,
        default=1,
        help="Save every Nth position (default: 1, save all positions)",
    )

    args = parser.parse_args()
    convert_pgn_to_tsv(args.input, args.output, args.game_limit, args.step)


if __name__ == "__main__":
    main()
