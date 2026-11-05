#include "browserlauncher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QRegularExpression>

BrowserLauncher::BrowserLauncher(QObject *parent) : QObject(parent) {}

QString BrowserLauncher::findBrowser()
{
    // 先用 Edge（Windows 自带，一定有），再找 Chrome
    const QStringList cands = {
        QStringLiteral("C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"),
        QStringLiteral("C:/Program Files/Microsoft/Edge/Application/msedge.exe"),
        QStringLiteral("C:/Program Files/Google/Chrome/Application/chrome.exe"),
        QStringLiteral("C:/Program Files (x86)/Google/Chrome/Application/chrome.exe"),
        QDir::homePath() + QStringLiteral("/AppData/Local/Google/Chrome/Application/chrome.exe"),
    };
    for (const QString &p : cands) {
        if (QFileInfo::exists(p))
            return QDir::toNativeSeparators(p);
    }
    return QString();
}

QString BrowserLauncher::profileDir()
{
    // 用独立的 profile：登录一次长期保留，也不跟用户平时的浏览器抢实例
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString dir = base + QStringLiteral("/browser-profile");
    QDir().mkpath(dir);
    return QDir::toNativeSeparators(dir);
}

bool BrowserLauncher::isRunning() const
{
    if (m_pid <= 0)
        return false;
    QTcpSocket s;
    s.connectToHost(QStringLiteral("127.0.0.1"), defaultPort());
    const bool ok = s.waitForConnected(300);
    s.abort();
    return ok;
}

bool BrowserLauncher::launch(const QString &url, int port, QString *err)
{
    // 端口已经能连上：说明上次起的浏览器还开着，直接复用，别开第二个实例
    {
        QTcpSocket s;
        s.connectToHost(QStringLiteral("127.0.0.1"), port);
        if (s.waitForConnected(250)) {
            s.abort();
            return true;
        }
        s.abort();
    }

    const QString exe = findBrowser();
    if (exe.isEmpty()) {
        if (err)
            *err = QStringLiteral("没找到 Edge 或 Chrome，请先装一个浏览器");
        return false;
    }

    QStringList args;
    args << QStringLiteral("--remote-debugging-port=%1").arg(port)
         << QStringLiteral("--remote-allow-origins=*")
         << (QStringLiteral("--user-data-dir=") + profileDir())
         << QStringLiteral("--no-first-run")
         << QStringLiteral("--no-default-browser-check")
         << QStringLiteral("--disable-features=Translate,msEdgeIdentityFeatures")
         << QStringLiteral("--start-maximized")
         << url;

    qint64 pid = 0;
    const bool ok = QProcess::startDetached(exe, args, QString(), &pid);
    if (!ok) {
        if (err)
            *err = QStringLiteral("启动浏览器失败：%1").arg(exe);
        return false;
    }
    m_pid = pid;
    return true;
}

void BrowserLauncher::closeBrowser()
{
    // 按 user-data-dir 精确定位本工具的浏览器实例，不用 taskkill /IM 误杀用户自己的浏览器
    QProcess p;
    p.start(QStringLiteral("wmic"), {QStringLiteral("process"), QStringLiteral("where"),
                                     QStringLiteral("name='msedge.exe' or name='chrome.exe'"),
                                     QStringLiteral("get"), QStringLiteral("ProcessId,CommandLine"),
                                     QStringLiteral("/format:csv")});
    if (!p.waitForFinished(4000))
        return;
    const QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    const QString key = QDir::fromNativeSeparators(profileDir());
    for (const QString &line : out.split(QLatin1Char('\n'))) {
        if (!line.contains(key, Qt::CaseInsensitive))
            continue;
        const QStringList cols = line.trimmed().split(QLatin1Char(','));
        if (cols.isEmpty())
            continue;
        bool okNum = false;
        const int pid = cols.last().toInt(&okNum);
        if (!okNum || pid <= 0)
            continue;
        QProcess::startDetached(QStringLiteral("taskkill"),
                                {QStringLiteral("/PID"), QString::number(pid), QStringLiteral("/F")});
    }
    m_pid = 0;
}
