#pragma once

#include <QJsonObject>
#include <QStringList>

// The hooks that run talkback-speak when a coding agent finishes a reply.
// Each agent keeps its hooks in its own file and format; installing touches
// only Talkback's entry and leaves every other hook alone.
namespace Hooks {
// claude, codex, grok, cursor, antigravity, omp
QStringList agents();
QString displayName(const QString &agent);
// The file Talkback's hook lives in, under `home`.
QString path(const QString &agent, const QString &home);
// Whether the agent has a configuration directory, so it's worth hooking.
bool detected(const QString &agent, const QString &home);
bool installed(const QString &agent, const QString &home);
// False with `error` set when the file can't be read back faithfully or written.
bool install(const QString &agent, const QString &home, QString *error);

// For the agents whose hooks live in a shared JSON file.
bool hasHook(const QString &agent, const QJsonObject &config);
// The config with one Talkback hook, replacing any earlier one (including the
// speak-reply.sh this app grew out of).
QJsonObject withHook(const QString &agent, QJsonObject config);
}
