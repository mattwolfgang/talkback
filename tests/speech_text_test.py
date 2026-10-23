"""Tests for bin/talkback-speech-text: python3 tests/speech_text_test.py"""

import importlib.machinery
import importlib.util
import pathlib
import unittest

path = pathlib.Path(__file__).resolve().parent.parent / "bin" / "talkback-speech-text"
loader = importlib.machinery.SourceFileLoader("speech_text", str(path))
spec = importlib.util.spec_from_loader("speech_text", loader)
speech_text = importlib.util.module_from_spec(spec)
loader.exec_module(speech_text)
say = speech_text.speech_text


class SpeechTextTests(unittest.TestCase):
    def test_list_items_become_sentences(self):
        self.assertEqual(say("Two things:\n- first\n- second!\n"), "Two things. first. second!")

    def test_everything_is_one_line(self):
        self.assertNotIn("\n", say("# Title\n\nOne.\nStill one.\n\nTwo"))

    def test_code_blocks_and_tables_are_skipped(self):
        self.assertEqual(say("Run:\n```bash\nrm -rf /\n```\n| a | b |\n|---|---|\nDone"), "Run. Done.")

    def test_paths_read_as_their_last_part(self):
        self.assertEqual(say("Edit `~/.config/hypr/bindings.lua` now"), "Edit bindings.lua now.")
        self.assertEqual(say("It lives in ~/.local/share/piper-voices/."), "It lives in piper-voices.")

    def test_links_keep_their_words(self):
        self.assertEqual(say("See [the repo](https://github.com/x/y)."), "See the repo.")
        self.assertEqual(say("Go to https://www.example.com/a/b"), "Go to example.com.")

    def test_symbols_and_abbreviations_read_as_words(self):
        self.assertEqual(say("1.5× faster, e.g. on/off → done"), "1.5 times faster, for example on or off to done.")

    def test_emphasis_is_dropped_but_snake_case_kept(self):
        self.assertEqual(say("**Bold** and _it_ but my_var stays"), "Bold and it but my_var stays.")


if __name__ == "__main__":
    unittest.main()
