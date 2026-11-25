#pragma once

#include <QString>
#include <QStringList>

#include <optional>
#include <sys/stat.h>

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

[[nodiscard]] bool needsXdgExclusion(const QString &dir, const QString &home, const QString &englishName);
// Supply lstat metadata so the final path component cannot be a symlink.
[[nodiscard]] bool isTrustedSystemConfig(const QString &path, const struct stat &metadata);
[[nodiscard]] std::optional<QStringList> selectExtraOptions(const QString &systemDefault, const QString &preference,
                                                             const QString &argument);
[[nodiscard]] Command buildCommand(const Options &options);
[[nodiscard]] bool helpListsOption(const QString &helpText, const QString &option);
[[nodiscard]] int parsePercentageLine(const QString &line, bool *ok = nullptr);

} // namespace SquashfsUtils
