#pragma once

#include <QProcess>

// Hide the progress cursor on one terminal until this scope ends.
class TerminalCursor
{
public:
    explicit TerminalCursor(int outputFd);
    ~TerminalCursor();
    void hide();
    void show();

private:
    int outputFd;
    QProcess watcher;
};
