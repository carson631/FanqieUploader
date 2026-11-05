#include "cdpclient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <memory>

CdpClient::CdpClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_ws, &QWebSocket::connected, this, [this]() {
        log(QStringLiteral("调试通道已连接"));
    });
    connect(&m_ws, &QWebSocket::disconnected, this, [this]() {
        failPending(QStringLiteral("调试连接已断开"));
        emit disconnected();
    });
    connect(&m_ws, &QWebSocket::textMessageReceived, this, &CdpClient::handleMessage);
    connect(&m_ws, &QWebSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError err) {
        // 关编辑标签时连接会被服务端断开，RemoteHostClosed 是正常收尾，
        // 不是故障，别把它当错误弹出来吓人
        if (err == QAbstractSocket::RemoteHostClosedError)
            return;
        emit failed(QStringLiteral("WebSocket 错误：%1").arg(m_ws.errorString()));
    });
}

void CdpClient::scan(const TargetFilter &filter,
                     std::function<void(bool, const QJsonObject &, const QString &)> cb)
{
    QUrl url(QStringLiteral("http://127.0.0.1:%1/json/list").arg(m_port));
    QNetworkRequest req(url);
    req.setTransferTimeout(4000);
    QNetworkReply *reply = m_net.get(req);

    connect(reply, &QNetworkReply::finished, this, [reply, filter, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            cb(false, QJsonObject(),
               QStringLiteral("连不上调试端口：%1（浏览器没带 --remote-debugging-port 启动？）")
                   .arg(reply->errorString()));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            cb(false, QJsonObject(), QStringLiteral("/json/list 返回的不是数组"));
            return;
        }
        QJsonObject hit;
        QStringList seen;
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("type")).toString() != QLatin1String("page"))
                continue;
            const QString u = o.value(QStringLiteral("url")).toString();
            if (u.startsWith(QLatin1String("devtools")))
                continue;
            seen << u;
            if (filter(u)) {
                hit = o;
                break;
            }
        }
        if (hit.isEmpty()) {
            cb(false, QJsonObject(),
               QStringLiteral("没找到番茄页面（当前标签：%1）")
                   .arg(seen.isEmpty() ? QStringLiteral("无") : seen.join(QStringLiteral(" | "))));
            return;
        }
        cb(true, hit, QString());
    });
}

void CdpClient::discoverAndConnect()
{
    if (isConnected()) {
        m_ws.close();
        return;
    }
    scan([](const QString &u) { return u.contains(QLatin1String("fanqienovel.com")); },
         [this](bool ok, const QJsonObject &t, const QString &why) {
             if (!ok) {
                 emit failed(why);
                 return;
             }
             connectToTarget(t);
         });
}

void CdpClient::ensureTarget(const TargetFilter &filter,
                             std::function<void(bool, const QString &)> cb)
{
    scan(filter, [this, cb](bool ok, const QJsonObject &t, const QString &why) {
        if (!ok) {
            cb(false, why);
            return;
        }
        const QString url = t.value(QStringLiteral("url")).toString();
        if (isConnected() && m_url == url) {
            cb(true, QString());
            return;
        }
        log(QStringLiteral("切换标签：%1").arg(url));
        connectToTarget(t);
        // 连接是异步的：等 400ms 后再判一次
        QTimer::singleShot(500, this, [this, cb]() {
            cb(isConnected(), isConnected() ? QString()
                                            : QStringLiteral("切换到新标签失败"));
        });
    });
}

void CdpClient::connectToTarget(const QJsonObject &target)
{
    const QString wsUrl = target.value(QStringLiteral("webSocketDebuggerUrl")).toString();
    if (wsUrl.isEmpty()) {
        emit failed(QStringLiteral("target 没有 webSocketDebuggerUrl"));
        return;
    }
    m_url = target.value(QStringLiteral("url")).toString();
    m_targetId = target.value(QStringLiteral("id")).toString();
    log(QStringLiteral("目标页：%1").arg(m_url));
    if (m_ws.state() != QAbstractSocket::UnconnectedState)
        m_ws.abort();
    m_ws.open(QUrl(wsUrl));

    QTimer::singleShot(300, this, [this, target]() {
        if (!isConnected())
            return;
        evaluate(QStringLiteral("1+1"), [this, target](bool ok, const QJsonValue &, const QString &e) {
            if (!ok) {
                emit failed(QStringLiteral("Runtime.evaluate 失败：%1").arg(e));
                return;
            }
            emit connected(target.value(QStringLiteral("url")).toString(),
                           target.value(QStringLiteral("title")).toString());
        });
    });
}

void CdpClient::close()
{
    if (m_ws.state() != QAbstractSocket::UnconnectedState)
        m_ws.close();
}

void CdpClient::evaluate(const QString &js, EvalCallback cb, int timeoutMs)
{
    if (!isConnected()) {
        cb(false, QJsonValue(), QStringLiteral("调试连接未建立"));
        return;
    }

    const int id = m_nextId++;
    m_pending.insert(id, cb);

    QJsonObject params;
    params.insert(QStringLiteral("expression"), js);
    params.insert(QStringLiteral("returnByValue"), true);
    params.insert(QStringLiteral("awaitPromise"), true);
    params.insert(QStringLiteral("userGesture"), true);

    QJsonObject msg;
    msg.insert(QStringLiteral("id"), id);
    msg.insert(QStringLiteral("method"), QStringLiteral("Runtime.evaluate"));
    msg.insert(QStringLiteral("params"), params);

    m_ws.sendTextMessage(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));

    QTimer::singleShot(timeoutMs, this, [this, id]() {
        auto it = m_pending.find(id);
        if (it == m_pending.end())
            return;
        const EvalCallback cb2 = it.value();
        m_pending.erase(it);
        cb2(false, QJsonValue(), QStringLiteral("JS 执行超时"));
    });
}

void CdpClient::handleMessage(const QString &text)
{
    const QJsonObject msg = QJsonDocument::fromJson(text.toUtf8()).object();
    const int id = msg.value(QStringLiteral("id")).toInt(-1);
    if (id < 0)
        return;

    auto it = m_pending.find(id);
    if (it == m_pending.end())
        return;
    const EvalCallback cb = it.value();
    m_pending.erase(it);

    if (msg.contains(QStringLiteral("error"))) {
        cb(false, QJsonValue(), msg.value(QStringLiteral("error")).toObject()
                                    .value(QStringLiteral("message")).toString());
        return;
    }

    const QJsonObject result = msg.value(QStringLiteral("result")).toObject();
    if (result.contains(QStringLiteral("exceptionDetails"))) {
        const QJsonObject ex = result.value(QStringLiteral("exceptionDetails")).toObject();
        QString detail = ex.value(QStringLiteral("text")).toString();
        const QJsonObject eo = ex.value(QStringLiteral("exception")).toObject();
        if (!eo.value(QStringLiteral("description")).toString().isEmpty())
            detail += QStringLiteral(" · ") + eo.value(QStringLiteral("description")).toString();
        cb(false, QJsonValue(), detail);
        return;
    }

    cb(true, result.value(QStringLiteral("result")).toObject().value(QStringLiteral("value")),
       QString());
}

// ---- 多标签：番茄「新建章节」是 window.open 开新标签 ----

void CdpClient::fetchTargets(ListCb cb)
{
    QUrl url(QStringLiteral("http://127.0.0.1:%1/json/list").arg(m_port));
    QNetworkRequest req(url);
    req.setTransferTimeout(4000);
    QNetworkReply *reply = m_net.get(req);

    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            cb(false, QJsonArray(),
               QStringLiteral("连不上调试端口：%1").arg(reply->errorString()));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            cb(false, QJsonArray(), QStringLiteral("/json/list 返回的不是数组"));
            return;
        }
        QJsonArray pages;
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("type")).toString() != QLatin1String("page"))
                continue;
            const QString u = o.value(QStringLiteral("url")).toString();
            if (u.startsWith(QLatin1String("devtools")))
                continue;
            pages.append(o);
        }
        cb(true, pages, QString());
    });
}

void CdpClient::followNewTarget(
    const QStringList &knownIds, const QString &urlPart, int waitMs,
    std::function<void(bool ok, const QString &targetId, const QString &why)> cb)
{
    const qint64 started  = QDateTime::currentMSecsSinceEpoch();
    const qint64 deadline = started + waitMs;
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, knownIds, urlPart, deadline, started, cb, step]() {
        fetchTargets([this, knownIds, urlPart, deadline, started, cb, step](
                         bool ok, const QJsonArray &pages, const QString &why) {
            if (!ok) {
                cb(false, QString(), why);
                return;
            }
            QJsonObject fallback;           // 新开但不是精确匹配的番茄页
            QString     fallbackId;
            for (const QJsonValue &v : pages) {
                const QJsonObject o = v.toObject();
                const QString id = o.value(QStringLiteral("id")).toString();
                const QString u = o.value(QStringLiteral("url")).toString();
                const bool exact = u.contains(urlPart);
                if (exact && id == m_targetId) {
                    // 原地跳到编辑页（不是新标签），连接没变
                    m_url = u;
                    cb(true, id, QString());
                    return;
                }
                if (knownIds.contains(id))
                    continue;               // 之前就开着的旧标签，不碰
                if (exact) {
                    connectToTarget(o);
                    QTimer::singleShot(800, this, [this, id, cb]() {
                        cb(isConnected(), id,
                           isConnected() ? QString() : QStringLiteral("新标签连不上"));
                    });
                    return;
                }
                // 新标签刚建出来时 URL 可能是空/about:blank，或先落在别的路由上；
                // 记下来，等 2.5 秒还没有精确匹配就先接住它（真正的判定交给 stage()）
                if (fallbackId.isEmpty() && !u.startsWith(QLatin1String("about:"))
                    && u.contains(QLatin1String("fanqienovel.com")))
                    fallback = o, fallbackId = id;
            }

            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (!fallbackId.isEmpty() && now - started >= 2500) {
                connectToTarget(fallback);
                QTimer::singleShot(800, this, [this, fallbackId, cb]() {
                    cb(isConnected(), fallbackId,
                       isConnected() ? QString() : QStringLiteral("新标签连不上"));
                });
                return;
            }
            if (now >= deadline) {
                cb(false, QString(),
                   QStringLiteral("等新标签超时（没出现 URL 含「%1」的页面）").arg(urlPart));
                return;
            }
            QTimer::singleShot(400, this, [step]() { (*step)(); });
        });
    };
    (*step)();
}

void CdpClient::closeTarget(const QString &targetId)
{
    if (targetId.isEmpty())
        return;
    // /json/close/<id> 是 DevTools HTTP 接口，关标签不用另开 WebSocket
    QUrl url(QStringLiteral("http://127.0.0.1:%1/json/close/%2").arg(m_port).arg(targetId));
    QNetworkRequest req(url);
    req.setTransferTimeout(4000);
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, targetId]() {
        reply->deleteLater();
        // 标签可能已经自己关了（提交成功后番茄常常自己关掉编辑页），失败不当回事
        if (reply->error() == QNetworkReply::NoError && m_targetId == targetId)
            log(QStringLiteral("已关闭本章的编辑标签"));
    });
}

void CdpClient::failPending(const QString &why)
{
    const auto callbacks = m_pending;
    m_pending.clear();
    for (const EvalCallback &cb : callbacks)
        cb(false, QJsonValue(), why);
}
