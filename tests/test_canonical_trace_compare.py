from pathlib import Path
import tempfile
import unittest

from tools.qa.compare_canonical_trace import compare_trace_files, read_trace


def trace(*frames: str) -> str:
    return "tetrisphere-guest-trace-v1\n" + "".join(
        "tetrisphere-canonical-v1\n" + frame + "end-canonical-frame\n"
        for frame in frames
    )


class CanonicalTraceCompareTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)

    def write(self, name: str, contents: str) -> Path:
        path = Path(self.directory.name) / name
        path.write_text(contents, encoding="ascii")
        return path

    def test_equal_and_first_field_divergence(self):
        left = self.write("left.trace", trace(
            "tick=0000000000000000\nglobal_rng_word=0000000000000001\n",
            "tick=0000000000000001\nglobal_rng_word=0000000000000002\n",
        ))
        right = self.write("right.trace", left.read_text(encoding="ascii"))
        self.assertIsNone(compare_trace_files(left, right))
        right.write_text(trace(
            "tick=0000000000000000\nglobal_rng_word=0000000000000001\n",
            "tick=0000000000000001\nglobal_rng_word=0000000000000003\n",
        ), encoding="ascii")
        difference = compare_trace_files(left, right)
        self.assertEqual((difference.frame, difference.field,
                          difference.expected, difference.actual),
                         (1, "global_rng_word", 2, 3))

    def test_truncated_or_duplicate_frame_is_rejected(self):
        truncated = self.write("truncated.trace",
                               "tetrisphere-guest-trace-v1\n"
                               "tetrisphere-canonical-v1\n"
                               "tick=0000000000000000\n")
        with self.assertRaisesRegex(ValueError, "end-canonical-frame"):
            list(read_trace(truncated))
        duplicate = self.write("duplicate.trace", trace(
            "tick=0000000000000000\ntick=0000000000000000\n"))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            list(read_trace(duplicate))


if __name__ == "__main__":
    unittest.main()
