#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace SquashfsUtils
{

struct Command {
    QString program;
    QStringList args;
};

struct Options {
    QString bindRootPath;
    QString outputPath;
    QString compression;
    uint cores = 1;
    uint throttle = 0;
    bool throttleSupported = false;
    bool percentageSupported = false;
    QStringList mksqOpt;
    QString excludesFileName;
    QString sessionExcludes;
    bool useUnbuffer = false;
};

[[nodiscard]] std::optional<QStringList> selectExtraOptions(const QString &systemDefault, const QString &preference,
                                                             const QString &argument);
[[nodiscard]] Command buildCommand(const Options &options);
[[nodiscard]] bool helpListsOption(const QString &helpText, const QString &option);
[[nodiscard]] int parsePercentageLine(const QString &line, bool *ok = nullptr);

} // namespace SquashfsUtils
