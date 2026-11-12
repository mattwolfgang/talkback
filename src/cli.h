#pragma once

#include <QStringList>

// Commands that need no window, for the keybindings and for agents:
// `talkback speech toggle`, `talkback agent codex off`, `talkback status --json`.
// Returns the exit code, or -1 when the arguments ask for the window.
int runCli(const QStringList &arguments);
