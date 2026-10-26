#include "hooks.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSaveFile>

namespace {
// OMP has no JSON hooks; it loads TypeScript extensions instead. This one
// hands the reply to talkback-speak in the shape Claude's hook sends.
const char *ompExtension = R"(// Installed by Talkback (talkback hook install omp): reads each finished reply aloud.
import { spawn } from "node:child_process";

export default function (pi) {
  pi.on("agent_end", (event) => {
    if (event.isTerminal === false || event.willContinue === true) return;
    const reply = [...(event.messages ?? [])].reverse().find((message) => message.role === "assistant");
    const content = reply?.content ?? [];
    const text = typeof content === "string"
      ? content
      : content.filter((part) => part.type === "text").map((part) => part.text).join("\n");
    if (!text.trim()) return;
    const child = spawn("talkback-speak", ["--agent", "omp"], { stdio: ["pipe", "ignore", "ignore"] });
    child.on("error", () => {});
    child.stdin.on("error", () => {});
    child.stdin.end(JSON.stringify({ last_assistant_message: text }));
    child.unref();
  });
}
)";

QString command(const QString &agent) {
    return agent == "claude" ? QString("talkback-speak") : "talkback-speak --agent " + agent;
}

QJsonObject handler(const QString &agent) {
    return {{"type", "command"}, {"command", command(agent)}, {"timeout", 10}};
}

bool isOurs(const QJsonValue &hook) {
    const QString command = hook.toObject().value("command").toString();
    return command.contains("talkback-speak") || command.contains("speak-reply.sh");
}

// Claude, Codex and Grok share one shape: hooks.Stop is a list of matcher
// groups, each with its own list of handlers.
bool groupsFormat(const QString &agent) {
    return agent == "claude" || agent == "codex" || agent == "grok";
}

// Handlers listed straight under an event, as Cursor and Antigravity write them.
QJsonArray withoutOurs(const QJsonArray &handlers) {
    QJsonArray kept;
    for (const QJsonValue &hook : handlers)
        if (!isOurs(hook))
            kept.append(hook);
    return kept;
}

bool anyOurs(const QJsonArray &handlers) {
    for (const QJsonValue &hook : handlers)
        if (hook.toObject().value("command").toString().contains("talkback-speak"))
            return true;
    return false;
}

bool fail(QString *error, const QString &message) {
    if (error)
        *error = message;
    return false;
}

bool save(const QString &path, const QByteArray &contents, QString *error) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, file.errorString());
    file.write(contents);
    if (!file.commit())
        return fail(error, file.errorString());
    return true;
}
}

namespace Hooks {
QStringList agents() {
    return {"claude", "codex", "grok", "cursor", "antigravity", "omp"};
}

QString displayName(const QString &agent) {
    static const QMap<QString, QString> names{
        {"claude", "Claude Code"}, {"codex", "Codex"}, {"grok", "Grok"},
        {"cursor", "Cursor"}, {"antigravity", "Antigravity"}, {"omp", "OMP"}};
    return names.value(agent, agent);
}

QString path(const QString &agent, const QString &home) {
    if (agent == "claude")
        return home + "/.claude/settings.json";
    if (agent == "codex")
        return home + "/.codex/hooks.json";
    if (agent == "grok")
        return home + "/.grok/hooks/talkback.json";
    if (agent == "cursor")
        return home + "/.cursor/hooks.json";
    if (agent == "antigravity")
        return home + "/.gemini/config/hooks.json";
    if (agent == "omp")
        return home + "/.omp/agent/extensions/talkback.ts";
    return {};
}

bool detected(const QString &agent, const QString &home) {
    // ~/.gemini alone is the Gemini CLI.
    if (agent == "antigravity")
        return QFileInfo(home + "/.gemini/antigravity-cli").isDir() || QFileInfo(home + "/.gemini/antigravity").isDir();
    static const QMap<QString, QString> dirs{
        {"claude", ".claude"}, {"codex", ".codex"}, {"grok", ".grok"}, {"cursor", ".cursor"}, {"omp", ".omp"}};
    return dirs.contains(agent) && QFileInfo(home + "/" + dirs.value(agent)).isDir();
}

bool installed(const QString &agent, const QString &home) {
    QFile file(path(agent, home));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    if (agent == "omp")
        return file.readAll().contains("talkback-speak");
    return hasHook(agent, QJsonDocument::fromJson(file.readAll()).object());
}

bool install(const QString &agent, const QString &home, QString *error) {
    const QString file = path(agent, home);
    if (file.isEmpty())
        return fail(error, QCoreApplication::translate("Hooks", "Unknown agent: %1").arg(agent));
    if (agent == "omp")
        return save(file, ompExtension, error);

    QJsonObject config;
    QFile current(file);
    if (current.exists()) {
        if (!current.open(QIODevice::ReadOnly))
            return fail(error, QCoreApplication::translate("Hooks", "Couldn't read %1.").arg(file));
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(current.readAll(), &parseError);
        // Never overwrite a settings file we can't read back faithfully.
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return fail(error, QCoreApplication::translate("Hooks", "%1 isn't valid JSON, so it was left alone.").arg(file));
        config = document.object();
    }
    return save(file, QJsonDocument(withHook(agent, config)).toJson(QJsonDocument::Indented), error);
}

bool hasHook(const QString &agent, const QJsonObject &config) {
    if (groupsFormat(agent)) {
        for (const QJsonValue &group : config.value("hooks").toObject().value("Stop").toArray())
            if (anyOurs(group.toObject().value("hooks").toArray()))
                return true;
        return false;
    }
    if (agent == "cursor")
        return anyOurs(config.value("hooks").toObject().value("afterAgentResponse").toArray());
    if (agent == "antigravity")
        return anyOurs(config.value("talkback").toObject().value("Stop").toArray());
    return false;
}

QJsonObject withHook(const QString &agent, QJsonObject config) {
    if (groupsFormat(agent)) {
        QJsonObject hooks = config.value("hooks").toObject();
        QJsonArray stop;
        for (const QJsonValue &group : hooks.value("Stop").toArray()) {
            QJsonObject object = group.toObject();
            const QJsonArray kept = withoutOurs(object.value("hooks").toArray());
            if (kept.isEmpty())
                continue;
            object["hooks"] = kept;
            stop.append(object);
        }
        stop.append(QJsonObject{{"hooks", QJsonArray{handler(agent)}}});
        hooks["Stop"] = stop;
        config["hooks"] = hooks;
    } else if (agent == "cursor") {
        // Cursor's Stop hook doesn't carry the reply; afterAgentResponse does.
        QJsonObject hooks = config.value("hooks").toObject();
        QJsonArray responses = withoutOurs(hooks.value("afterAgentResponse").toArray());
        responses.append(QJsonObject{{"command", command(agent)}, {"timeout", 10}});
        hooks["afterAgentResponse"] = responses;
        config["hooks"] = hooks;
        if (!config.contains("version"))
            config["version"] = 1;
    } else if (agent == "antigravity") {
        // Each top-level key is a named hook, so Talkback's is its own.
        config["talkback"] = QJsonObject{{"Stop", QJsonArray{handler(agent)}}};
    }
    return config;
}
}
