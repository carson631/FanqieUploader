#ifndef CDPCLIENT_H
#define CDPCLIENT_H

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QObject>
#include <QWebSocket>

#include <functional>

// 极简 DevTools 协议客户端
//   1) HTTP GET /json/list 找到番茄作家后台那个 tab
//   2) WebSocket 连上去，用 Runtime.evaluate 在页面里跑 JS 拿返回值
// 不用 QWebEngine 的原因：MinGW 版 Qt 根本没编 WebEngine 模块（aqt 里查不到），
// 而 CDP 方案还能直接复用用户浏览器里已登录的账号。
class CdpClient : public QObject
{
    Q_OBJECT

public:
    using EvalCallback = std::function<void(bool ok, const QJsonValue &value, const QString &err)>;
    using TargetFilter = std::function<bool(const QString &url)>;

    explicit CdpClient(QObject *parent = nullptr);

    void setPort(int port) { m_port = port; }
    int  port() const { return m_port; }

    bool isConnected() const { return m_ws.state() == QAbstractSocket::ConnectedState; }
    QString currentUrl() const { return m_url; }

    void discoverAndConnect();          // 找 fanqienovel.com 的 page，连上
    void ensureTarget(const TargetFilter &filter,
                      std::function<void(bool ok, const QString &why)> cb);  // 切 target（开了新 tab 时用）
    void close();

    void evaluate(const QString &js, EvalCallback cb, int timeoutMs = 25000);

signals:
    void log(const QString &msg);
    void connected(const QString &url, const QString &title);
    void disconnected();
    void failed(const QString &why);

private:
    void connectToTarget(const QJsonObject &target);
    void scan(const TargetFilter &filter,
              std::function<void(bool ok, const QJsonObject &target, const QString &why)> cb);
    void handleMessage(const QString &text);
    void failPending(const QString &why);

    QNetworkAccessManager m_net;
    QWebSocket            m_ws;
    QString               m_url;
    int                   m_port = 9333;
    int                   m_nextId = 1;
    QHash<int, EvalCallback> m_pending;
};

#endif // CDPCLIENT_H
