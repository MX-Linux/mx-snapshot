#include "terminalcursor.h"

#include <unistd.h>

TerminalCursor::TerminalCursor(int fd)
    : outputFd(fd)
{
    watcher.setProcessChannelMode(QProcess::ForwardedChannels);
    watcher.setChildProcessModifier([fd] {
        // Stay out of the snapshot's foreground process group so the watcher
        // can still restore the terminal if the entire group is killed.
        if (::setsid() == -1 || ::dup2(fd, STDOUT_FILENO) == -1) {
            ::_exit(1);
        }
    });
    hide();
}

TerminalCursor::~TerminalCursor()
{
    show();
}

void TerminalCursor::hide()
{
    if (!::isatty(outputFd) || qgetenv("TERM") == "dumb" || watcher.state() != QProcess::NotRunning) {
        return;
    }
    // The watcher owns both escape sequences. Its stdin reaches EOF even if
    // the snapshot crashes or is killed without running any application code.
    watcher.start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral(
        R"(trap 'printf "\033[?25h"' 0; printf '\033[?25l'; IFS= read -r line)")});
    watcher.waitForStarted();
}

void TerminalCursor::show()
{
    if (watcher.state() != QProcess::NotRunning) {
        watcher.closeWriteChannel();
        watcher.waitForFinished(-1);
    }
}
