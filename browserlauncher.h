#ifndef BROWSERLAUNCHER_H
#define BROWSERLAUNCHER_H

#include <QObject>
#include <QString>

// 用独立的 user-data-dir 启动 Edge/Chrome，并打开远程调试端口。
// 独立 profile 的好处：登录一次长期有效，且不干扰用户日常浏览器。
class BrowserLauncher : public QObject
{
    Q_OBJECT

public:
    explicit BrowserLauncher(QObject *parent = nullptr);

    static QString findBrowser();          // 返回 exe 路径，找不到返回空
    static QString profileDir();           // 专用 profile 目录（%LOCALAPPDATA%）
    static int defaultPort() { return 9333; }

    // 启动（或复用已在跑的实例）并打开 url
    bool launch(const QString &url, int port, QString *err);
    bool isRunning() const;
    void closeBrowser();

private:
    qint64 m_pid = 0;
};

#endif // BROWSERLAUNCHER_H
