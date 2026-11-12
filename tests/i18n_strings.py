"""The English strings Talkback shows, pulled from its sources: what every
catalog in src/i18n is checked against."""
import glob
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STRING = r'"((?:[^"\\]|\\.)*)"'


def unescape(text):
    return json.loads('"' + text + '"')


def app_strings():
    found = set()
    for path in glob.glob(os.path.join(ROOT, "src", "*.qml")):
        source = open(path, encoding="utf-8").read()
        found.update(unescape(m) for m in re.findall(r"qsTr\(\s*" + STRING, source))
    for path in glob.glob(os.path.join(ROOT, "src", "*.cpp")):
        source = open(path, encoding="utf-8").read()
        found.update(unescape(m) for m in re.findall(r"\btr\(\s*" + STRING, source))
        found.update(unescape(m) for m in re.findall(r"translate\(\s*\"[^\"]*\"\s*,\s*" + STRING, source))
    return sorted(found)


if __name__ == "__main__":
    print(json.dumps(app_strings(), ensure_ascii=False, indent=1))
