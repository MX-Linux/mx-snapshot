#include <QtTest/QtTest>

#include "squashfsutils.h"

class TestSquashfsUtils : public QObject
{
    Q_OBJECT

private slots:
    void escapeExcludePath();
    void nestedHomes();
    void configTrust();
    void selectExtraOptions();
    void buildCommandDirect();
    void buildCommandWithUnbuffer();
    void buildCommandOmitsPercentageWhenDisabled();
    void helpListsOption();
    void parsePercentageLine();
};

void TestSquashfsUtils::escapeExcludePath()
{
    QCOMPARE(SquashfsUtils::escapeExcludePath(R"(/test/[year] "x"*?\@(old))"),
             QStringLiteral(R"(/test/\[year] "x"\*\?\\@\(old))"));
}

void TestSquashfsUtils::nestedHomes()
{
    using SquashfsUtils::needsXdgExclusion;
    const QString flat = "/home/alice";
    const QString nested = "/home/staff/alice";
    QVERIFY(!needsXdgExclusion({}, nested, "Documents"));
    QVERIFY(!needsXdgExclusion(nested, nested, "Documents"));
    QVERIFY(!needsXdgExclusion(flat + "/Documents", flat, "Documents"));
    QVERIFY(needsXdgExclusion(nested + "/Documents", nested, "Documents"));
    QVERIFY(needsXdgExclusion(flat + "/Work/Documents", flat, "Documents"));
}

void TestSquashfsUtils::configTrust()
{
    struct stat st {};
    st.st_mode = S_IFREG | 0644;
    const QString gui = "/etc/mx-snapshot.conf";
    QVERIFY(SquashfsUtils::isTrustedSystemConfig(gui, st));
    QVERIFY(SquashfsUtils::isTrustedSystemConfig("/etc/iso-snapshot-cli.conf", st));
    QVERIFY(!SquashfsUtils::isTrustedSystemConfig("/tmp/mx-snapshot.conf", st));
    st.st_uid = 1000;
    QVERIFY(!SquashfsUtils::isTrustedSystemConfig(gui, st));
    st.st_uid = 0;
    for (const auto mode : {S_IFREG | 0664, S_IFREG | 0646, S_IFLNK | 0644, S_IFDIR | 0755}) {
        st.st_mode = mode;
        QVERIFY(!SquashfsUtils::isTrustedSystemConfig(gui, st));
    }
}

void TestSquashfsUtils::selectExtraOptions()
{
    using SquashfsUtils::selectExtraOptions;
    const QString system = "-sort /etc/snapshot-priorities";
    const QStringList expected {"-sort", "/etc/snapshot-priorities"};
    QCOMPARE(selectExtraOptions(system, {}, {}).value(), expected);
    QCOMPARE(selectExtraOptions(system, system, {}).value(), expected);
    QVERIFY(!selectExtraOptions({}, system, {}));
    QVERIFY(!selectExtraOptions(system, "-Xhc", system));
    QVERIFY(!selectExtraOptions({}, "-b", {}));
    QVERIFY(!selectExtraOptions(system, "-p", {}));
    QVERIFY(!selectExtraOptions(system, {}, "-pf"));
    QVERIFY(!selectExtraOptions({}, {}, "-Xstrategy invalid"));
    const QStringList safe {"-b", "256K", "-Xstrategy", "default,fixed", "-no-xattrs"};
    QCOMPARE(selectExtraOptions(system, {}, safe.join(' ')).value(), safe);
    QCOMPARE(selectExtraOptions(system, safe.join(' '), {}).value(), safe);
}

void TestSquashfsUtils::buildCommandDirect()
{
    const SquashfsUtils::Command command = SquashfsUtils::buildCommand({
        .bindRootPath = "/bind-root",
        .outputPath = "/tmp/linuxfs",
        .compression = "zstd",
        .cores = 4,
        .throttle = 75,
        .throttleSupported = true,
        .percentageSupported = true,
        .mksqOpt = {"-Xcompression-level", "15"},
        .excludesFileName = "/tmp/excludes.list",
        .sessionExcludes = {"home/user/Downloads", "home/user/My Files", R"(home/user/Docs "1" \[2026])"},
        .useUnbuffer = false,
    });

    QCOMPARE(command.program, QStringLiteral("mksquashfs"));
    const QStringList expected {
        "/bind-root", "/tmp/linuxfs", "-comp", "zstd", "-processors", "4",
        "-throttle", "75", "-percentage", "-Xcompression-level", "15",
        "-wildcards", "-ef", "/tmp/excludes.list", "-e", "home/user/Downloads", "home/user/My Files",
        R"(home/user/Docs "1" \[2026])",
    };
    QCOMPARE(command.args, expected);
}

void TestSquashfsUtils::buildCommandWithUnbuffer()
{
    const SquashfsUtils::Command command = SquashfsUtils::buildCommand({
        .bindRootPath = "/bind-root",
        .outputPath = "/tmp/linuxfs",
        .compression = "xz",
        .cores = 2,
        .excludesFileName = "/tmp/excludes.list",
        .useUnbuffer = true,
    });

    QCOMPARE(command.program, QStringLiteral("unbuffer"));
    QCOMPARE(command.args.mid(0, 7), (QStringList {"/bin/sh", "-c", R"("$@"; exit $?)", "sh",
                                                   "/usr/bin/mksquashfs", "/bind-root", "/tmp/linuxfs"}));
    QVERIFY(!command.args.contains("-percentage"));
}

void TestSquashfsUtils::buildCommandOmitsPercentageWhenDisabled()
{
    const SquashfsUtils::Command command = SquashfsUtils::buildCommand({
        .bindRootPath = "/bind-root",
        .outputPath = "/tmp/linuxfs",
        .compression = "gzip",
        .cores = 1,
        .percentageSupported = false,
        .excludesFileName = "/tmp/excludes.list",
    });

    QVERIFY(!command.args.contains("-percentage"));
}

void TestSquashfsUtils::helpListsOption()
{
    const QString help = QStringLiteral(
        "-throttle <percentage>\tthrottle I/O\n"
        "-processors <number>\tUse processors\n");

    QVERIFY(SquashfsUtils::helpListsOption(help, QStringLiteral("-throttle")));
    QVERIFY(SquashfsUtils::helpListsOption(help, QStringLiteral("-processors")));
    QVERIFY(!SquashfsUtils::helpListsOption(help, QStringLiteral("-percentage")));
}

void TestSquashfsUtils::parsePercentageLine()
{
    bool ok = false;
    QCOMPARE(SquashfsUtils::parsePercentageLine("6", &ok), 6);
    QVERIFY(ok);
    QCOMPARE(SquashfsUtils::parsePercentageLine(" 12% ", &ok), 12);
    QVERIFY(ok);
    QCOMPARE(SquashfsUtils::parsePercentageLine("\x1B[2K100\r", &ok), 100);
    QVERIFY(ok);
    QCOMPARE(SquashfsUtils::parsePercentageLine("Parallel mksquashfs: Using 8 processors", &ok), 0);
    QVERIFY(!ok);
    QCOMPARE(SquashfsUtils::parsePercentageLine("101", &ok), 0);
    QVERIFY(!ok);
}

QTEST_MAIN(TestSquashfsUtils)
#include "test_squashfsutils.moc"
