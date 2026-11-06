"""The Python tokenizer must produce exactly the firmware's tokens for the golden corpus."""
import json
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from rigels_studio.text import tokenize_line  # noqa: E402

GOLDEN = pathlib.Path(__file__).parent / "golden"


class TokenizerMatchesFirmware(unittest.TestCase):
    def test_tokens_match_golden(self):
        golden = json.loads((GOLDEN / "golden.json").read_text(encoding="utf-8"))["tokens"]
        lines = (GOLDEN / "corpus.txt").read_text(encoding="utf-8").split("\n")
        ours = [(number, token) for number, line in enumerate(lines) for token in tokenize_line(line)]
        theirs = [(token["line"], token["t"]) for token in golden]
        for index, (mine, firmware) in enumerate(zip(ours, theirs)):
            self.assertEqual(mine, firmware, f"token {index} differs")
        self.assertEqual(len(ours), len(theirs))


if __name__ == "__main__":
    unittest.main()
