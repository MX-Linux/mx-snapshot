#include "squashfsutils.h"

#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <QRegularExpression>

namespace
{
QStringList splitShellWords(const QString &text)
{
    return text.trimmed().isEmpty() ? QStringList() : QProcess::splitCommand(text);
}
} // namespace

namespace SquashfsUtils
{

bool needsXdgExclusion(const QString &dir, const QString &home, const QString &englishName)
{
    if (dir.isEmpty() || dir == home) {
        return false; // xdg-user-dir returns the user's home for a disabled folder.
    }
    // The default pattern home/*/<EnglishName> covers only one level under /home.
    return QFileInfo(home).path() != "/home" || dir != home + '/' + englishName;
}

bool isTrustedSystemConfig(const QString &path, const struct stat &metadata)
{
    return (path == "/etc/mx-snapshot.conf" || path == "/etc/iso-snapshot-cli.conf")
           && S_ISREG(metadata.st_mode) && metadata.st_uid == 0
           && (metadata.st_mode & (S_IWGRP | S_IWOTH)) == 0;
}

std::optional<QStringList> selectExtraOptions(const QString &systemDefault, const QString &preference,
                                               const QString &argument)
{
    // Older releases copied system defaults to the user file. An identical
    // cached value can inherit the actual administrator-provided string.
    if (argument.isEmpty() && (preference.isEmpty() || preference == systemDefault)) {
        return splitShellWords(systemDefault);
    }
    const QStringList words = splitShellWords(argument.isEmpty() ? preference : argument);
    static const QSet<QString> flags {"-Xhc", "-no-recovery", "-no-xattrs", "-xattrs", "-no-fragments",
                                      "-always-use-fragments", "-no-duplicates", "-noappend"};
    // Match whole operands. mksquashfs validates codec-specific numeric ranges.
    const auto choices = [](const QString &alternatives) {
        return QRegularExpression(QStringLiteral(R"(\A(?:%1)(?:,(?:%1))*\z)").arg(alternatives));
    };
    static const QHash<QString, QRegularExpression> operands {
        {"-b", QRegularExpression(R"(\A[0-9]+[kKmM]?\z)")},
        {"-Xcompression-level", QRegularExpression(R"(\A-?[0-9]+\z)")},
        {"-Xwindow-size", QRegularExpression(R"(\A[0-9]+\z)")},
        {"-Xdict-size", QRegularExpression(R"(\A(?:[0-9]+[kKmM]?|[0-9]+(?:\.[0-9]+)?%)\z)")},
        {"-Xalgorithm", QRegularExpression(R"(\Alzo1x_(?:1|1_11|1_12|1_15|999)\z)")},
        {"-Xstrategy", choices("default|filtered|huffman_only|run_length_encoded|fixed")},
        {"-Xbcj", choices("x86|arm|armthumb|powerpc|sparc|ia64|arm64|riscv")},
    };
    for (qsizetype i = 0; i < words.size(); ++i) {
        if (flags.contains(words.at(i))) {
            continue;
        }
        const auto operand = operands.constFind(words.at(i));
        if (operand == operands.cend() || ++i == words.size() || !operand->match(words.at(i)).hasMatch()) {
            return std::nullopt;
        }
    }
    return words;
}

Command buildCommand(const Options &options)
{
    QStringList squashfsArgs {options.bindRootPath, options.outputPath,
                              "-comp", options.compression,
                              "-processors", QString::number(options.cores)};
    if (options.throttleSupported) {
        squashfsArgs << "-throttle" << QString::number(options.throttle);
    }
    if (options.percentageSupported) {
        squashfsArgs << "-percentage";
    }
    squashfsArgs += options.mksqOpt;
    squashfsArgs << "-wildcards" << "-ef" << options.excludesFileName;

    const QStringList sessionExcludes = splitShellWords(options.sessionExcludes);
    if (!sessionExcludes.isEmpty()) {
        squashfsArgs << "-e";
        squashfsArgs += sessionExcludes;
    }

    Command command;
    command.program = options.useUnbuffer ? QStringLiteral("unbuffer") : QStringLiteral("mksquashfs");
    if (options.useUnbuffer) {
        // Expect's unbuffer loses the exit status of a signalled child.
        // Keep the shell alive to translate that signal into a normal exit.
        command.args << "/bin/sh" << "-c" << R"("$@"; exit $?)" << "sh" << "/usr/bin/mksquashfs";
    }
    command.args += squashfsArgs;
    return command;
}

bool helpListsOption(const QString &helpText, const QString &option)
{
    const QRegularExpression optionLine(
        QStringLiteral("(?:^|\\s)%1(?:\\s|$)").arg(QRegularExpression::escape(option)));
    return optionLine.match(helpText).hasMatch();
}

int parsePercentageLine(const QString &line, bool *ok)
{
    static const QRegularExpression ansiEscape(QStringLiteral("\\x1B\\[[0-?]*[ -/]*[@-~]"));
    static const QRegularExpression squashfsPercentageLine(QStringLiteral("^\\s*(100|\\d{1,2})\\s*%?\\s*$"));

    const QString cleanedLine = QString(line).remove(ansiEscape);
    const QRegularExpressionMatch match = squashfsPercentageLine.match(cleanedLine);
    if (!match.hasMatch()) {
        if (ok) {
            *ok = false;
        }
        return 0;
    }

    bool converted = false;
    const int percentage = match.captured(1).toInt(&converted);
    if (ok) {
        *ok = converted;
    }
    return converted ? percentage : 0;
}

} // namespace SquashfsUtils
