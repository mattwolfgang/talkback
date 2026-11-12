"""Holds Talkback's translations to Omarchy's rules (docs/i18n.md upstream):
python3 tests/i18n_test.py

Every language has a complete catalog for the window and for the bar plugin,
each translation carries its English placeholders, a counted string has
exactly the plural forms its language uses, and the binary and the desktop
entry include every language."""

import json
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests"))
from i18n_strings import app_strings  # noqa: E402

APP = ROOT / "src" / "i18n"
BAR = ROOT / "bar-plugin" / "i18n"
REGISTRY = pathlib.Path("/usr/share/omarchy/default/i18n/locales.json")


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def codes():
    return sorted(p.stem for p in APP.glob("*.json"))


def placeholders(text):
    return sorted(re.findall(r"%[0-9n]", text))


# The categories Translator::pluralCategory (src/translator.cpp) can return
# for whole counts in each language; English and anything unlisted use one
# and other.
PLURALS = {
    **dict.fromkeys(["ja", "ko", "zh-CN", "zh-TW", "th", "vi", "id", "ms"], {"other"}),
    **dict.fromkeys(["fr", "pt-BR", "ca", "es-MX", "it", "pt"], {"one", "many", "other"}),
    **dict.fromkeys(["cs", "sk", "hr", "sr", "lt", "ro"], {"one", "few", "other"}),
    **dict.fromkeys(["pl", "ru", "uk"], {"one", "few", "many"}),
    "sl": {"one", "two", "few", "other"},
    "lv": {"zero", "one", "other"},
    "ar": {"zero", "one", "two", "few", "many", "other"},
    "he": {"one", "two", "other"},
    "ga": {"one", "two", "few", "many", "other"},
}


class AppCatalogs(unittest.TestCase):
    def test_every_omarchy_language_has_both_catalogs(self):
        if REGISTRY.exists():
            self.assertEqual(codes(), sorted(load(REGISTRY)))
        self.assertEqual(sorted(p.stem for p in BAR.glob("*.json")), codes())

    def test_each_translation_covers_exactly_the_shown_strings(self):
        source = set(app_strings())
        english = load(APP / "en.json")
        self.assertLessEqual(set(english), source, "en.json carries only plural forms")
        for code in codes():
            if code == "en":
                continue
            catalog = load(APP / f"{code}.json")
            with self.subTest(code=code):
                self.assertEqual(sorted(source - set(catalog)), [], "missing")
                self.assertEqual(sorted(set(catalog) - source), [], "no longer in the source")

    def test_placeholders_and_plural_forms(self):
        for code in codes():
            for english, value in load(APP / f"{code}.json").items():
                with self.subTest(code=code, string=english):
                    if isinstance(value, dict):
                        want = {"one", "other"} if code == "en" else PLURALS.get(code, {"one", "other"})
                        self.assertEqual(set(value), want)
                        # A form may say the number as a word, but the general ones keep it.
                        for form in {"few", "many", "other"} & set(value):
                            self.assertIn("%n", value[form])
                    else:
                        self.assertTrue(value.strip())
                        self.assertEqual(placeholders(value), placeholders(english))


class BarCatalogs(unittest.TestCase):
    def test_keys_and_placeholders_match_english(self):
        english = load(BAR / "en.json")
        for code in codes():
            catalog = load(BAR / f"{code}.json")
            with self.subTest(code=code):
                self.assertEqual(sorted(catalog), sorted(english))
                for key, value in catalog.items():
                    self.assertTrue(value.strip(), key)
                    self.assertEqual(placeholders(value), placeholders(english[key]), key)

    def test_every_key_the_widget_asks_for_exists(self):
        source = (ROOT / "bar-plugin" / "BarWidget.qml").read_text(encoding="utf-8")
        used = set(re.findall(r'strings\.t\("([^"]+)"\)', source))
        self.assertTrue(used)
        self.assertEqual(sorted(used - set(load(BAR / "en.json"))), [])
        self.assertEqual(sorted(set(load(BAR / "en.json")) - used), [], "unused keys")


class Packaging(unittest.TestCase):
    def test_binary_includes_every_catalog(self):
        qrc = (ROOT / "src" / "resources.qrc").read_text(encoding="utf-8")
        for code in codes():
            self.assertIn(f'alias="i18n/{code}.json"', qrc)
            self.assertIn(f'alias="bar-plugin/i18n/{code}.json"', qrc)

    def test_desktop_entry_is_translated(self):
        desktop = (ROOT / "pkgbuild" / "talkback.desktop").read_text(encoding="utf-8")
        for code in codes():
            if code != "en":
                locale = {"no": "nb"}.get(code, code.replace("-", "_"))
                self.assertIn(f"Comment[{locale}]=", desktop)


if __name__ == "__main__":
    unittest.main()
