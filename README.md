# Talkback

![Talkback](docs/screenshot.png)

Talkback picks the voice that reads your coding agent's replies aloud (Claude
Code, Codex, Grok, Cursor, Antigravity or OMP), and turns spoken replies and
Voxtype dictation on and off. It speaks with either of two
local engines:

- [Piper](https://github.com/OHF-Voice/piper1-gpl): fast and light, with
  hundreds of voices to browse, download and try.
- [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M), through
  [kokoro-onnx](https://github.com/thewh1teagle/kokoro-onnx): more natural
  rhythm and intonation, 54 voices, about a second to start. The window
  installs it (about 350 MB under `~/.local/share/talkback/kokoro`), or run
  `talkback kokoro install`.

It has three parts:

- **`talkback-speak`**, the hook each agent runs to read its finished reply
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

Two Hyprland keybindings work from anywhere. On Omarchy, `talkback hook
install` and the window's Set up button add them to
`~/.config/hypr/bindings.lua`, skipping any key something else already uses.
The window warns when one isn't bound, and Change keys picks new ones (or run
`talkback keys toggle "SUPER + CTRL + T"`). Talkback finds the lines by their
commands, so you can also edit them by hand. Elsewhere, add the equivalent of:

```lua
o.bind("SUPER + ALT + X", "Toggle spoken replies", "talkback speech toggle --notify")
o.bind("SUPER + ALT + ESCAPE", "Stop speaking", "talkback-speak --stop")
```

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
talkback hook install [claude|codex|grok|cursor|antigravity|omp|all] [--no-keys]
talkback keys [install|toggle KEY|stop KEY]
talkback status [--json]
```

`talkback hook install` with no agent, or `all`, sets up every agent it finds
in your home directory. Each hook lives where that agent looks for it:

| Agent | Hook |
| --- | --- |
| Claude Code | Stop hook in `~/.claude/settings.json` |
| Codex | Stop hook in `~/.codex/hooks.json`; approve it once in Codex's `/hooks` |
| Grok | Stop hook in `~/.grok/hooks/talkback.json` |
| Cursor | `afterAgentResponse` hook in `~/.cursor/hooks.json` |
| Antigravity | Stop hook named `talkback` in `~/.gemini/config/hooks.json` |
| OMP | Extension at `~/.omp/agent/extensions/talkback.ts` |

Grok and Cursor can also run Claude Code's hooks. `talkback-speak` ignores
those runs and uses each agent's own hook, so a reply is read only once.

Dictation off stops Voxtype's user service, so the dictation keys do nothing
until it's turned back on or you log in again.

## Install

Each release has prebuilt x86_64 packages. They include `piper-tts` and
`python-pathvalidate`, which Talkback needs but the Arch repositories don't
have. pacman fetches the other dependencies itself:

```sh
cd "$(mktemp -d)"
url=https://github.com/mattwolfgang/talkback/releases/download/v0.1.0
curl -fsSLO "$url/SHA256SUMS"
for file in $(awk '{print $2}' SHA256SUMS); do curl -fsSLO "$url/$file"; done
sha256sum -c SHA256SUMS
sudo pacman -U *.pkg.tar.zst
talkback hook install
```

The packages aren't signed, so pacman can't install them straight from their
URLs. The commands above download them and check them against `SHA256SUMS`
first.

## Build from source

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
