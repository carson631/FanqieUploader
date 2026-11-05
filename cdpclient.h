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
    QString currentTargetId() const { return m_targetId; }

    void discoverAndConnect();          // 找 fanqienovel.com 的 page，连上
    void ensureTarget(const TargetFilter &filter,
                      std::function<void(bool ok, const QString &why)> cb);  // 切 target（开了新 tab 时用）
    void close();

    // 番茄点「新建章节 / 新建卷」是 window.open 开**新标签页**，旧标签还停在列表页。
    // 用法：动作前先 fetchTargets 记下已知 id，动作后 followNewTarget 接住新标签。
    using ListCb = std::function<void(bool ok, const QJsonArray &pages, const QString &why)>;
    void fetchTargets(ListCb cb);
    void followNewTarget(const QStringList &knownIds, const QString &urlPart, int waitMs,
                         std::function<void(bool ok, const QString &targetId, const QString &why)> cb);
    // 关掉自己开出来的编辑标签（走 browser 级会话的 Target.closeTarget）
    void closeTarget(const QString &targetId);

    void evaluate(const QString &js, EvalCallback cb, int timeoutMs = 25000);

    // 把目标标签提到前台。浏览器窗口在后台时 Chromium 会把 setTimeout 深度节流
    // （最长 1 分钟才触发一次），跑长流程的 JS（如设发布定时）就会「卡住」。
    // 不关心回执，发出去即可。
    void bringToFront();

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
    QString               m_targetId;
    int                   m_port = 9333;
    int                   m_nextId = 1;
    QHash<int, EvalCallback> m_pending;
};

#endif // CDPCLIENT_H
