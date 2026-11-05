// 真机验证：点【新建章节】→ 接管新标签 → 在新标签里判定编辑页
// 用法：followtest.exe <port> [site.js 路径]
// 不填标题、不发布，跑完关掉自己开的标签（站点上会留一个空草稿章）。
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <cstdio>
#include <functional>

#include "cdpclient.h"

static void say(const QString &s)
{
    printf("%s\n", s.toUtf8().constData());
    fflush(stdout);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const int port = argc > 1 ? QString::fromUtf8(argv[1]).toInt() : 9333;
    const QString sitePath = argc > 2 ? QString::fromUtf8(argv[2])
                                      : QStringLiteral("D:/project/QT/FanqieUploader/js/site.js");

    QFile f(sitePath);
    if (!f.open(QIODevice::ReadOnly)) {
        say(QStringLiteral("FAIL: 读不到 %1").arg(sitePath));
        return 2;
    }
    const QString site = QString::fromUtf8(f.readAll());

    CdpClient cdp;
    cdp.setPort(port);

    std::function<void()>                start;
    std::function<void(const QStringList &)> afterClick;
    std::function<void(const QString &)> onFollowed;

    QObject::connect(&cdp, &CdpClient::log, [](const QString &m) { say(QStringLiteral("[cdp] ") + m); });
    QObject::connect(&cdp, &CdpClient::failed, [](const QString &m) { say(QStringLiteral("[failed] ") + m); });
    QObject::connect(&cdp, &CdpClient::connected, [&](const QString &url, const QString &) {
        say(QStringLiteral("已连接，url=%1").arg(url.left(110)));
        QTimer::singleShot(200, [&]() { start(); });
    });

    start = [&]() {
        cdp.evaluate(site, [&](bool ok, const QJsonValue &, const QString &why) {
            if (!ok) {
                say(QStringLiteral("FAIL 注入 site.js：%1").arg(why));
                QCoreApplication::exit(1);
                return;
            }
            QTimer::singleShot(300, [&]() {
                cdp.fetchTargets([&](bool ok2, const QJsonArray &pages, const QString &why2) {
                    if (!ok2) {
                        say(QStringLiteral("FAIL fetchTargets：%1").arg(why2));
                        QCoreApplication::exit(1);
                        return;
                    }
                    QStringList known;
                    for (const QJsonValue &v : pages) {
                        const QJsonObject o = v.toObject();
                        known << o.value(QStringLiteral("id")).toString();
                        say(QStringLiteral("  现有标签 %1  %2")
                                .arg(o.value(QStringLiteral("id")).toString(),
                                     o.value(QStringLiteral("url")).toString().left(90)));
                    }
                    say(QStringLiteral("点【新建章节】…"));
                    cdp.evaluate(QStringLiteral("__fq.newChapter()"),
                                 [&, known](bool ok3, const QJsonValue &v3, const QString &why3) {
                                     if (!ok3) {
                                         say(QStringLiteral("FAIL newChapter：%1").arg(why3));
                                         QCoreApplication::exit(1);
                                         return;
                                     }
                                     say(QStringLiteral("newChapter 返回：%1")
                                             .arg(QString::fromUtf8(QJsonDocument(
                                                      QJsonArray{v3}).toJson(QJsonDocument::Compact))));
                                     afterClick(known);
                                 });
                });
            });
        });
    };

    afterClick = [&](const QStringList &known) {
        const qint64 t0 = QDateTime::currentMSecsSinceEpoch();
        cdp.followNewTarget(known, QStringLiteral("/publish/"), 9000,
                            [&, t0](bool ok, const QString &tid, const QString &why) {
                                const qint64 dt = QDateTime::currentMSecsSinceEpoch() - t0;
                                if (!ok) {
                                    say(QStringLiteral("FAIL followNewTarget(%1ms)：%2").arg(dt).arg(why));
                                    QCoreApplication::exit(1);
                                    return;
                                }
                                say(QStringLiteral("followNewTarget 成功(%1ms)，新标签 %2").arg(dt).arg(tid));
                                onFollowed(tid);
                            });
    };

    onFollowed = [&](const QString &tid) {
        QTimer::singleShot(2500, [&, tid]() {
            cdp.evaluate(site, [&, tid](bool, const QJsonValue &, const QString &) {
                cdp.evaluate(QStringLiteral("__fq.stage()"),
                             [&, tid](bool ok, const QJsonValue &v, const QString &why) {
                                 if (!ok) {
                                     say(QStringLiteral("FAIL stage()：%1").arg(why));
                                 } else {
                                     const QJsonObject st = v.toObject();
                                     say(QStringLiteral("stage: editor=%1 bodyEditor=%2 manage=%3 publish=%4")
                                             .arg(st.value(QStringLiteral("editor")).toBool())
                                             .arg(st.value(QStringLiteral("bodyEditor")).toBool())
                                             .arg(st.value(QStringLiteral("manage")).toBool())
                                             .arg(st.value(QStringLiteral("publish")).toBool()));
                                     say(st.value(QStringLiteral("editor")).toBool()
                                             ? QStringLiteral("PASS 编辑页在新标签里可识别")
                                             : QStringLiteral("FAIL 连上了新标签但 editor=false"));
                                 }
                                 say(QStringLiteral("关闭测试标签 %1").arg(tid));
                                 cdp.closeTarget(tid);
                                 QTimer::singleShot(600, []() { QCoreApplication::quit(); });
                             });
            });
        });
    };

    QTimer::singleShot(100000, []() { say(QStringLiteral("TIMEOUT")); QCoreApplication::quit(); });
    // 测试从「章节管理页」起跑（编辑页上点不到新建章节）
    cdp.ensureTarget(
        [](const QString &u) {
            return u.contains(QLatin1String("fanqienovel.com"))
                   && !u.contains(QLatin1String("/publish/"));
        },
        [](bool ok, const QString &why) {
            if (!ok)
                say(QStringLiteral("FAIL 找不到章节管理标签：%1").arg(why));
        });
    return app.exec();
}
