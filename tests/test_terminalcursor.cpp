#include <QCoreApplication>
#include <QScopeGuard>
#include <QtTest>

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "terminalcursor.h"

class TestTerminalCursor : public QObject
{
    Q_OBJECT

private slots:
    void restoresCursor_data()
    {
        QTest::addColumn<int>("signal");
        QTest::newRow("normal-exit") << 0;
        QTest::newRow("ctrl-c") << static_cast<int>(SIGINT);
        QTest::newRow("killed-process-group") << static_cast<int>(SIGKILL);
    }

    void restoresCursor()
    {
        QFETCH(int, signal);
        int master, slave;
        QVERIFY(::openpty(&master, &slave, nullptr, nullptr, nullptr) == 0);
        const auto closePty = qScopeGuard([&] {
            ::close(master);
            ::close(slave);
        });
        QVERIFY(::fcntl(master, F_SETFL, O_NONBLOCK) == 0);
        QProcess client;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("TERM", "xterm");
        client.setProcessEnvironment(env);
        client.setChildProcessModifier([=] {
            ::close(master);
            if (::setsid() == -1 || ::ioctl(slave, TIOCSCTTY, 0) == -1
                || ::dup2(slave, STDIN_FILENO) == -1 || ::dup2(slave, STDERR_FILENO) == -1) {
                ::_exit(2);
            }
            ::close(slave);
        });
        client.start(QCoreApplication::applicationFilePath(), {"--cursor-client"});
        QVERIFY(client.waitForStarted());

        QByteArray output;
        const auto received = [&](const QByteArray &sequence) {
            char bytes[256];
            const ssize_t count = ::read(master, bytes, sizeof(bytes));
            if (count > 0) {
                output.append(bytes, count);
            }
            return output.contains(sequence);
        };
        QTRY_VERIFY_WITH_TIMEOUT(received("\033[?25l"), 3000);
        if (signal == SIGKILL) {
            QCOMPARE(::kill(-static_cast<pid_t>(client.processId()), SIGKILL), 0);
        } else {
            const QByteArray input = signal == SIGINT ? "\003" : "\n";
            QCOMPARE(::write(master, input.constData(), input.size()), input.size());
        }
        QVERIFY(client.state() == QProcess::NotRunning || client.waitForFinished(3000));
        QCOMPARE(client.exitStatus(), signal ? QProcess::CrashExit : QProcess::NormalExit);
        QTRY_VERIFY_WITH_TIMEOUT(received("\033[?25h"), 3000);
    }

    void redirectedProgress()
    {
        QProcess client;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("TERM", "xterm");
        client.setProcessEnvironment(env);
        client.start(QCoreApplication::applicationFilePath(), {"--cursor-client"});
        client.closeWriteChannel();
        QVERIFY(client.waitForFinished(3000));
        QCOMPARE(client.exitCode(), 0);
        QVERIFY(client.readAllStandardError().isEmpty());
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains("--cursor-client")) {
        TerminalCursor cursor(STDERR_FILENO);
        char byte;
        while (::read(STDIN_FILENO, &byte, 1) < 0 && errno == EINTR) {}
        return 0;
    }
    TestTerminalCursor test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_terminalcursor.moc"
