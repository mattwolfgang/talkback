# Talkback

![Talkback](docs/screenshot.png)

Talkback picks the voice that reads Claude Code's replies aloud, and turns
spoken replies and Voxtype dictation on and off. It speaks with either of two
local engines:

- [Piper](https://github.com/OHF-Voice/piper1-gpl): fast and light, with
  hundreds of voices to browse, download and try.
- [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M), through
  [kokoro-onnx](https://github.com/thewh1teagle/kokoro-onnx): more natural
  rhythm and intonation, 54 voices, about a second to start. The window
  installs it (about 350 MB under `~/.local/share/talkback/kokoro`), or run
  `talkback kokoro install`.

It has three parts:

- **`talkback-speak`**, a Claude Code Stop hook that reads each finished reply
  aloud with the chosen engine and plays previews (`talkback-kokoro` is the
  Kokoro side). `talkback-speech-text` first turns the
  Markdown into plain sentences: code blocks and tables are skipped, list
  items and headings end like sentences, paths shrink to their file name, and
  symbols like `→` and `e.g.` read as words.
- **`talkback`**, the window: the voice catalog, the two switches, and how the
  voice sounds: speed, expression (how much pitch varies), rhythm (how much
  sound lengths vary), and the pause after each sentence. Voices trained on
  many people (LibriTTS has 904) get a speaker picker, remembered per voice.
- **`talkback <command>`**, the same controls with no window, for keybindings.

Settings live in `~/.config/talkback/settings.conf` and voices in
`~/.local/share/piper-voices/`.

## Shortcuts

| Key | Action |
| --- | --- |
| `Space` | Play the selected voice |
| `Enter` | Use the selected voice, or download it |
| `D` | Download the selected voice |
| `Delete` | Remove the selected voice |
| `↑` `↓` | Move through the voices |
| `/` | Search voices |
| `E` | Switch between Piper and Kokoro |
| `I` | Show downloaded voices only |
| `+` `-` | Speak faster or slower |
| `[` `]` | Previous or next speaker of a multi-speaker voice, and play it |
| `S` | Turn spoken replies on or off |
| `V` | Turn dictation on or off |
| `Esc` | Stop speaking |
| `?` | Show every shortcut |
| `Q` | Quit |

## Commands

```text
talkback speech [on|off|toggle|status] [--notify]
talkback dictation [on|off|toggle|status] [--notify]
talkback engine [piper|kokoro]
talkback voice [NAME]
talkback kokoro install
talkback speaker [N]
talkback say TEXT
talkback stop
talkback hook install
talkback status [--json]
```

Dictation off stops Voxtype's user service, so the dictation keys do nothing
until it's turned back on or you log in again.

## Build and install

```sh
bin/build     # build/talkback
bin/test      # run the tests offscreen
bin/install   # makepkg -fsi, so Talkback shows up in the app launcher
talkback hook install
```

Needs `piper-tts` (AUR) and, for dictation, Voxtype set up under Omarchy's
Setup > Defaults > Dictation.

## Credits

The theme and language handling (`src/theme.cpp`, `src/translator.cpp`)
follow [Omacut](https://github.com/omacom-io/omacut) and
[Monologue](https://github.com/omacom/monologue), MIT licensed,
Copyright (c) David Heinemeier Hansson.

## License

MIT. See [LICENSE](LICENSE).
