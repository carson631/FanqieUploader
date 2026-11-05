#include "uploadworker.h"
#include "cdpclient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace {

// 把 C++ 字符串安全地塞进 JS 表达式
QString jsStr(const QString &s)
{
    QString e = s;
    e.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    e.replace(QLatin1Char('"'), QLatin1String("\\\""));
    e.replace(QLatin1Char('\n'), QLatin1String("\\n"));
    e.replace(QLatin1Char('\r'), QLatin1String("\\r"));
    e.replace(QChar(0x2028), QLatin1String("\\u2028"));
    e.replace(QChar(0x2029), QLatin1String("\\u2029"));
    return QLatin1Char('"') + e + QLatin1Char('"');
}

QString stepsText(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &s : v.toArray())
        out << s.toString();
    return out.join(QStringLiteral(" / "));
}

} // namespace

UploadWorker::UploadWorker(CdpClient *cdp, QObject *parent)
    : QObject(parent), m_cdp(cdp) {}

void UploadWorker::runJs(const QString &call, JsCb cb)
{
    if (!m_cdp || !m_cdp->isConnected()) {
        if (cb)
            cb(false, QJsonValue(QStringLiteral("调试连接未建立")));
        return;
    }
    // 页面导航后 window.__fq 会消失，这里自动补注入
    const QString expr = QStringLiteral("(function(){if(!window.__fq){") + m_site
                         + QStringLiteral("}return (") + call + QStringLiteral(");})()");
    m_cdp->evaluate(expr, [cb](bool ok, const QJsonValue &v, const QString &err) {
        if (!cb)
            return;
        if (!ok) {
            cb(false, QJsonValue(QStringLiteral("JS 失败：") + err));
            return;
        }
        cb(true, v);
    });
}

void UploadWorker::pollJs(const QString &call,
                          std::function<bool(const QJsonValue &)> pred,
                          int intervalMs, int maxTicks,
                          std::function<void(bool, const QJsonValue &)> done)
{
    const int gen = m_gen;
    auto tick = std::make_shared<std::function<void(int)>>();
    auto last = std::make_shared<QJsonValue>();

    *tick = [this, gen, call, pred, intervalMs, maxTicks, done, tick, last](int n) {
        if (gen != m_gen || !m_running)
            return;
        runJs(call, [this, gen, pred, intervalMs, maxTicks, done, tick, last, n](
                        bool ok, const QJsonValue &v) {
            if (gen != m_gen || !m_running)
                return;
            *last = v;
            if (ok && pred(v)) {
                done(true, v);
                return;
            }
            if (n + 1 >= maxTicks) {
                done(false, *last);
                return;
            }
            QTimer::singleShot(intervalMs, this, [tick, n]() { (*tick)(n + 1); });
        });
    };
    (*tick)(0);
}

void UploadWorker::start()
{
    if (m_running)
        return;
    if (m_jobs.isEmpty()) {
        emit log(QStringLiteral("没有可上传的章节"));
        emit finished(0, 0);
        return;
    }
    if (!m_cdp || !m_cdp->isConnected()) {
        emit log(QStringLiteral("还没连上浏览器，先点「启动浏览器并连接」"));
        emit finished(0, 0);
        return;
    }
    m_running = true;
    ++m_gen;
    m_ok = m_fail = 0;
    m_index = -1;
    m_retry = 0;
    emit progress(0, m_jobs.size());
    emit log(QStringLiteral("=== 开始：共 %1 章 ===").arg(m_jobs.size()));
    gotoStep(S_Prepare);
}

void UploadWorker::stop()
{
    if (!m_running)
        return;
    ++m_gen;
    m_running = false;
    m_step = S_Idle;
    emit log(QStringLiteral("=== 已手动停止（成功 %1，失败 %2）===").arg(m_ok).arg(m_fail));
}

void UploadWorker::probePage()
{
    runJs(QStringLiteral("__fq.probe()"), [this](bool ok, const QJsonValue &v) {
        if (!ok) {
            emit log(QStringLiteral("勘察失败：%1").arg(v.toString()));
            return;
        }
        const QJsonObject o = v.toObject();
        const QJsonObject st = o.value(QStringLiteral("stage")).toObject();
        emit log(QStringLiteral("── 页面勘察 ──"));
        emit log(QStringLiteral("地址：%1").arg(o.value(QStringLiteral("url")).toString()));
        emit log(QStringLiteral("状态：章节管理=%1　新建页=%2　纠错页=%3　发布设置=%4　书列表=%5")
                     .arg(st.value(QStringLiteral("manage")).toBool() ? QStringLiteral("是")
                                                                      : QStringLiteral("否"),
                          st.value(QStringLiteral("editor")).toBool() ? QStringLiteral("是")
                                                                      : QStringLiteral("否"),
                          st.value(QStringLiteral("correct")).toBool() ? QStringLiteral("是")
                                                                       : QStringLiteral("否"),
                          st.value(QStringLiteral("publish")).toBool() ? QStringLiteral("是")
                                                                       : QStringLiteral("否"),
                          st.value(QStringLiteral("books")).toBool() ? QStringLiteral("是")
                                                                     : QStringLiteral("否")));
        QStringList btns;
        for (const QJsonValue &b : o.value(QStringLiteral("buttons")).toArray())
            btns << b.toString();
        emit log(QStringLiteral("可见按钮：%1").arg(btns.join(QStringLiteral(" | "))));
        QStringList ins;
        for (const QJsonValue &iv : o.value(QStringLiteral("inputs")).toArray()) {
            const QJsonObject io = iv.toObject();
            ins << QStringLiteral("[%1]%2=%3")
                       .arg(io.value(QStringLiteral("type")).toString(),
                            io.value(QStringLiteral("ph")).toString(),
                            io.value(QStringLiteral("val")).toString());
        }
        emit log(QStringLiteral("输入框：%1").arg(ins.isEmpty() ? QStringLiteral("无")
                                                              : ins.join(QStringLiteral(" | "))));
        emit log(QStringLiteral("有正文编辑器：%1")
                     .arg(o.value(QStringLiteral("hasEditor")).toBool() ? QStringLiteral("是")
                                                                        : QStringLiteral("否")));
    });
}

void UploadWorker::scheduleNext(Step s, int ms)
{
    const int gen = m_gen;
    QTimer::singleShot(ms, this, [this, gen, s]() {
        if (gen != m_gen || !m_running)
            return;
        gotoStep(s);
    });
}

void UploadWorker::failChapter(const QString &why)
{
    finishChapter(false, why);
}

// 出异常（风险提示 / 发布失败）时：先把本章存成草稿，再弹警告并中断。
// 顺序很重要——先存草稿再中断，否则页面已经被弹窗占住，内容就白填了。
void UploadWorker::abortWithDraft(const QString &why, bool risk)
{
    m_running = false;
    ++m_gen;                                  // 丢弃所有在途回调

    if (!m_cdp || !m_cdp->isConnected()) {
        emit log(QStringLiteral("  ✗ %1").arg(why));
        if (risk) {
            emit riskStop(why);
        } else {
            ++m_fail;
            emit chapterEnd(m_index, false, why);
        }
        emit progress(m_ok + m_fail, m_jobs.size());
        emit finished(m_ok, m_fail);
        return;
    }

    emit log(QStringLiteral("  异常：%1").arg(why));
    emit log(QStringLiteral("  正在把本章存成草稿，避免丢内容…"));

    runJs(QStringLiteral("__fq.saveDraft()"), [this, why, risk](bool ok, const QJsonValue &v) {
        const QJsonObject o = v.toObject();
        const bool saved = ok && o.value(QStringLiteral("ok")).toBool();
        if (saved) {
            emit log(QStringLiteral("  ✓ 已点「存草稿」，本章内容保留在草稿箱"));
        } else {
            QStringList bs;
            for (const QJsonValue &b : o.value(QStringLiteral("buttons")).toArray())
                bs << b.toString();
            emit log(QStringLiteral("  ⚠ 没能点到「存草稿」，请到浏览器里手动确认本章内容"));
            if (!bs.isEmpty())
                emit log(QStringLiteral("    可见按钮：%1").arg(bs.join(QStringLiteral(" | "))));
        }
        if (risk) {
            emit riskStop(why + (saved ? QStringLiteral("\n\n（已自动存为草稿）")
                                       : QStringLiteral("\n\n（没能自动存草稿，请手动保存）")));
        } else {
            ++m_fail;
            emit chapterEnd(m_index, false,
                            why + (saved ? QStringLiteral("（已存草稿）")
                                         : QStringLiteral("（存草稿失败，请人工核对）")));
        }
        emit progress(m_ok + m_fail, m_jobs.size());
        emit finished(m_ok, m_fail);
    });
}

void UploadWorker::finishChapter(bool ok, const QString &msg)
{
    // 填过正文之后的任何失败，都先存草稿再中断（不让人白写）
    if (!ok && m_filled) {
        abortWithDraft(msg, false);
        return;
    }

    if (ok) {
        ++m_ok;
        emit log(QStringLiteral("  ✓ %1").arg(msg));
    } else {
        ++m_fail;
        emit log(QStringLiteral("  ✗ %1").arg(msg));
    }
    emit chapterEnd(m_index, ok, msg);
    emit progress(m_ok + m_fail, m_jobs.size());

    if (!m_running)
        return;

    if (!ok && m_stopOnError) {
        m_running = false;
        ++m_gen;
        emit log(QStringLiteral("已停止：出现失败，人工确认页面状态后再继续（成功 %1，失败 %2）")
                     .arg(m_ok).arg(m_fail));
        emit finished(m_ok, m_fail);
        return;
    }
    const int gen = m_gen;
    QTimer::singleShot(1200, this, [this, gen]() {
        if (gen != m_gen || !m_running)
            return;
        advanceChapter();
    });
}

void UploadWorker::advanceChapter()
{
    if (!m_running)
        return;
    ++m_index;
    if (m_index >= m_jobs.size()) {
        if (!m_editorTarget.isEmpty()) {
            m_cdp->closeTarget(m_editorTarget);
            m_editorTarget.clear();
        }
        m_running = false;
        ++m_gen;
        emit progress(m_ok + m_fail, m_jobs.size());
        emit log(QStringLiteral("=== 全部结束：成功 %1，失败 %2 ===").arg(m_ok).arg(m_fail));
        emit finished(m_ok, m_fail);
        return;
    }
    const int gen = m_gen;
    returnToManage([this, gen](bool) {
        if (gen != m_gen || !m_running)
            return;
        beginChapter(m_index);
    });
}

// 本章提交完：关掉自己开出来的编辑标签，把调试连接切回章节管理页。
// 不关的话 150 章会攒出 150 个标签，而且下一章会在错误的标签上找「新建章节」。
void UploadWorker::returnToManage(std::function<void(bool ok)> cb)
{
    const QString editor = m_editorTarget;
    m_editorTarget.clear();
    if (!editor.isEmpty())
        m_cdp->closeTarget(editor);

    QTimer::singleShot(editor.isEmpty() ? 0 : 800, this, [this, cb]() {
        m_cdp->ensureTarget(
            [](const QString &u) {
                return u.contains(QLatin1String("fanqienovel.com"))
                       && !u.contains(QLatin1String("/publish/"));
            },
            [this, cb](bool ok, const QString &why) {
                if (!ok)
                    emit log(QStringLiteral("  ⚠ 没切回章节管理页：%1").arg(why));
                else
                    emit log(QStringLiteral("  已回到章节管理页"));
                cb(ok);
            });
    });
}

// 首章必须在填表前真正「开章」，否则 m_cur 是默认值：
// 章号 0、标题空、正文空 -> 会把空值填进页面，然后卡在「下一步」。
void UploadWorker::enterChapter()
{
    if (m_index < 0)
        beginChapter(0);
    else
        gotoStep(S_NewChapter);
}

void UploadWorker::beginChapter(int index)
{
    // 必须把 m_index 同步成当前章下标：否则首章（经 enterChapter 进来）m_index
    // 一直是 -1，advanceChapter 的 ++m_index 慢一拍，第一章会被传两遍。
    m_index = index;
    m_cur = m_jobs.at(index);
    m_retry = 0;
    m_fillTry = 0;
    m_bodyTry = 0;
    m_bodyOk = false;
    m_volFix = 0;
    m_ticks = 0;
    m_switchRetry = 0;
    m_stepped = false;
    m_filled = false;
    // 页面统计的「正文字数」是去掉所有空白的，这里按同样的口径算期望值
    m_wantChars = 0;
    for (const QChar &c : m_cur.body) {
        if (c.isSpace() || c == QChar(0x3000) || c == QChar(0x00A0))
            continue;
        ++m_wantChars;
    }
    emit log(QStringLiteral("── 第 %1 章《%2》%3 字，定时 %4 %5")
                 .arg(m_cur.no)
                 .arg(m_cur.title)
                 .arg(m_cur.body.size())
                 .arg(m_cur.date.toString(QStringLiteral("yyyy-MM-dd")), m_cur.time));
    emit chapterStart(index, m_cur.no, m_cur.title);
    gotoStep(S_NewChapter);
}

void UploadWorker::gotoStep(Step s)
{
    m_step = s;
    const int gen = m_gen;

    switch (s) {
    case S_Idle:
    case S_Done:
        break;

    case S_Prepare: {
        runJs(QStringLiteral("__fq.stage()"), [this, gen](bool ok, const QJsonValue &v) {
            if (gen != m_gen || !m_running)
                return;
            if (!ok) {
                emit log(QStringLiteral("读不到页面状态：%1").arg(v.toString()));
                if (++m_retry < 6) {
                    scheduleNext(S_Prepare, 1500);
                    return;
                }
                emit log(QStringLiteral("✗ 放弃：请确认浏览器里打开了番茄作家后台并且已登录"));
                m_running = false;
                emit finished(m_ok, m_fail);
                return;
            }
            m_retry = 0;
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("manage")).toBool()) {
                emit log(QStringLiteral("已在章节管理页"));
                gotoStep(m_index < 0 ? S_Volume : S_NewChapter);
                // 注意：S_Volume 结束时统一走 enterChapter()，首章在那里才真正开章
                return;
            }
            if (o.value(QStringLiteral("editor")).toBool()
                || o.value(QStringLiteral("publish")).toBool()) {
                // 常见情况：调试连接还停在上一章自动打开的编辑标签上。
                // 先尝试切回章节管理那个标签（不关任何标签，避免丢用户手写内容）。
                if (m_switchRetry < 2) {
                    ++m_switchRetry;
                    emit log(QStringLiteral("⚠ 当前停在编辑页/发布页，尝试切回章节管理标签…"));
                    returnToManage([this, gen](bool ok2) {
                        if (gen != m_gen || !m_running)
                            return;
                        if (ok2) {
                            gotoStep(S_Prepare);
                            return;
                        }
                        emit log(QStringLiteral("✗ 只有编辑页标签，没有可用的章节管理页，请手动切回去再开始"));
                        m_running = false;
                        emit finished(m_ok, m_fail);
                    });
                    return;
                }
                emit log(QStringLiteral("⚠ 当前停在编辑页/发布页，为避免重复提交，请先手动回到「章节管理」页"));
                m_running = false;
                emit finished(m_ok, m_fail);
                return;
            }
            if (o.value(QStringLiteral("books")).toBool()) {
                gotoStep(S_OpenBook);
                return;
            }
            // 不认识页面：尝试导航到作品管理
            emit log(QStringLiteral("当前不在作家后台，正在打开作品管理页…"));
            runJs(QStringLiteral("__fq.navigate('https://fanqienovel.com/main/writer/book-manage')"),
                  [this, gen](bool, const QJsonValue &) {
                      if (gen != m_gen || !m_running)
                          return;
                      ++m_gen;                    // 导航会换 target，等页面加载
                      const int g2 = m_gen;
                      QTimer::singleShot(3500, this, [this, g2]() {
                          if (g2 != m_gen || !m_running)
                              return;
                          gotoStep(S_Prepare);
                      });
                  });
        });
        break;
    }

    case S_OpenBook: {
        if (m_book.isEmpty()) {
            emit log(QStringLiteral("没填书名：请在浏览器里手动进入目标书的「章节管理」页（最多等 60 秒）"));
            pollJs(QStringLiteral("__fq.stage()"),
                   [](const QJsonValue &v) { return v.toObject().value(QStringLiteral("manage")).toBool(); },
                   1500, 40, [this, gen](bool ok, const QJsonValue &) {
                       if (gen != m_gen || !m_running)
                           return;
                       if (ok) {
                           gotoStep(S_Volume);
                           return;
                       }
                       failChapter(QStringLiteral("等不到章节管理页，已停止"));
                   });
            break;
        }
        runJs(QStringLiteral("__fq.openBook(%1)").arg(jsStr(m_book)),
              [this, gen](bool ok, const QJsonValue &v) {
                  if (gen != m_gen || !m_running)
                      return;
                  const QJsonObject o = v.toObject();
                  if (!ok || !o.value(QStringLiteral("ok")).toBool()) {
                      QString why = o.value(QStringLiteral("why")).toString();
                      if (why.isEmpty())
                          why = v.toString();
                      if (++m_retry < 3) {
                          emit log(QStringLiteral("打开书籍失败（%1），重试 %2/3").arg(why).arg(m_retry));
                          scheduleNext(S_OpenBook, 1500);
                          return;
                      }
                      QStringList bs;
                      for (const QJsonValue &b : o.value(QStringLiteral("buttons")).toArray())
                          bs << b.toString();
                      QStringList bk;
                      for (const QJsonValue &b : o.value(QStringLiteral("books")).toArray())
                          bk << b.toString();
                      emit log(QStringLiteral("页面上没找到「章节管理」按钮。可见按钮：%1").arg(
                          bs.join(QStringLiteral(" | "))));
                      if (!bk.isEmpty())
                          emit log(QStringLiteral("识别到的书名：%1").arg(bk.join(QStringLiteral(" / "))));
                      failChapter(QStringLiteral("打不开书籍的章节管理页"));
                      return;
                  }
                  m_retry = 0;
                  emit log(QStringLiteral("已点击「章节管理」，进入中…"));
                  pollJs(QStringLiteral("__fq.stage()"),
                         [](const QJsonValue &v) {
                             return v.toObject().value(QStringLiteral("manage")).toBool();
                         },
                         800, 20, [this, gen](bool ok2, const QJsonValue &) {
                             if (gen != m_gen || !m_running)
                                 return;
                             if (ok2)
                                 gotoStep(S_Volume);
                             else
                                 failChapter(QStringLiteral("点进章节管理页超时"));
                         });
              });
        break;
    }

    case S_Volume: {
        if (m_volume.isEmpty()) {
            emit log(QStringLiteral("未指定分卷，沿用页面上当前选中的卷"));
            enterChapter();
            break;
        }
        runJs(QStringLiteral("__fq.pickVolume(%1)").arg(jsStr(m_volume)),
              [this, gen](bool ok, const QJsonValue &v) {
                  if (gen != m_gen || !m_running)
                      return;
                  const QJsonObject o = v.toObject();
                  if (ok && o.value(QStringLiteral("ok")).toBool()) {
                      emit log(QStringLiteral("已选卷：%1").arg(o.value(QStringLiteral("picked")).toString()));
                      enterChapter();
                      return;
                  }
                  const QString why = ok ? o.value(QStringLiteral("why")).toString() : v.toString();
                  if (why == QLatin1String("no-volume")) {
                      emit log(QStringLiteral("页面上没有分卷下拉（可能是单卷书），跳过选卷"));
                      enterChapter();
                      return;
                  }
                  QStringList opts;
                  for (const QJsonValue &x : o.value(QStringLiteral("options")).toArray())
                      opts << x.toString();
                  emit log(QStringLiteral("选卷失败（%1）。可选卷：%2")
                               .arg(why, opts.isEmpty() ? QStringLiteral("无") : opts.join(QStringLiteral(" / "))));
                  if (++m_retry < 3) {
                      scheduleNext(S_Volume, 1200);
                      return;
                  }
                  failChapter(QStringLiteral("选不到目标分卷"));
              });
        break;
    }

    case S_NewChapter: {
        // 每章开始前核对卷，防止页面刷新后回落默认卷而传错地方
        runJs(QStringLiteral("__fq.currentVolume?__fq.currentVolume():''"),
              [this, gen](bool ok, const QJsonValue &v) {
                  if (gen != m_gen || !m_running)
                      return;
                  const QString cur = ok ? v.toString() : QString();
                  if (!m_volume.isEmpty() && !cur.isEmpty() && cur.indexOf(m_volume) < 0) {
                      if (m_volFix < 3) {
                          ++m_volFix;
                          emit log(QStringLiteral("当前卷是「%1」，与目标卷不一致，重新选卷（%2/3）")
                                       .arg(cur).arg(m_volFix));
                          gotoStep(S_Volume);
                          return;
                      }
                      emit log(QStringLiteral("⚠ 分卷反复对不上（当前「%1」），继续按当前卷上传").arg(cur));
                  }
                  m_volFix = 0;
                  // 番茄的「新建章节」是 window.open 开新标签：先记下现有标签 id，
                  // 点完再去找新出现的那个，否则会一直在旧标签（章节列表）上等编辑页。
                  m_cdp->fetchTargets([this, gen](bool, const QJsonArray &pages, const QString &) {
                      if (gen != m_gen || !m_running)
                          return;
                      m_knownTargets.clear();
                      for (const QJsonValue &pv : pages)
                          m_knownTargets << pv.toObject().value(QStringLiteral("id")).toString();

                      runJs(QStringLiteral("__fq.newChapter()"), [this, gen](bool ok2, const QJsonValue &v2) {
                      if (gen != m_gen || !m_running)
                          return;
                      const QJsonObject o = v2.toObject();
                      if (!ok2 || !o.value(QStringLiteral("ok")).toBool()) {
                          const QString why = ok2 ? o.value(QStringLiteral("why")).toString()
                                                  : v2.toString();
                          if (++m_retry < 4) {
                              emit log(QStringLiteral("点「新建章节」失败（%1），重试 %2/4").arg(why).arg(m_retry));
                              scheduleNext(S_NewChapter, 1800);
                              return;
                          }
                          QStringList bs;
                          for (const QJsonValue &b : o.value(QStringLiteral("buttons")).toArray())
                              bs << b.toString();
                          emit log(QStringLiteral("可见按钮：%1").arg(bs.join(QStringLiteral(" | "))));
                          failChapter(QStringLiteral("点不到「新建章节」"));
                          return;
                      }
                      m_retry = 0;

                      m_cdp->followNewTarget(
                          m_knownTargets, QStringLiteral("/publish/"), 9000,
                          [this, gen](bool okT, const QString &tid, const QString &whyT) {
                              if (gen != m_gen || !m_running)
                                  return;
                              if (okT) {
                                  m_editorTarget = tid;
                                  emit log(QStringLiteral("  编辑页已在新标签打开，调试连接跟过去了"));
                              } else {
                                  emit log(QStringLiteral("  ⚠ %1（继续在原标签里等编辑页）").arg(whyT));
                              }
                              gotoStep(S_Ready);
                          });
                      });
                  });
              });
        break;
    }

    case S_Ready: {
        // 番茄编辑页是 SPA：先出一版骨架，再换成真编辑器。太早写进去会被重建冲掉，
        // 表现就是「日志说写好了，页面上是空的，点下一步报标题不能为空」。
        // 这里要求：章号框 + 标题框 + 正文编辑器都在，且连续 500ms 指向同一批元素。
        pollJs(QStringLiteral("__fq.editStable()"),
               [](const QJsonValue &v) {
                   const QJsonObject o = v.toObject();
                   return o.value(QStringLiteral("ready")).toBool()
                          && o.value(QStringLiteral("stableMs")).toInt() >= 500;
               },
               320, 40, [this, gen](bool ok, const QJsonValue &last) {
                   if (gen != m_gen || !m_running)
                       return;
                   if (ok) {
                       emit log(QStringLiteral("  编辑页已就绪"));
                       gotoStep(S_Fill);
                   } else {
                       const QJsonObject o = last.toObject();
                       failChapter(QStringLiteral("编辑页没就绪（%1）")
                                       .arg(o.value(QStringLiteral("why")).toString()));
                   }
               });
        break;
    }

    case S_Fill: {
        if (m_cur.body.trimmed().isEmpty()) {
            failChapter(QStringLiteral("第 %1 章正文是空的（检查源 txt）").arg(m_cur.no));
            break;
        }
        runJs(QStringLiteral("__fq.fillChapter(%1,%2)")
                  .arg(m_cur.no)
                  .arg(jsStr(m_cur.title)),
              [this, gen](bool ok, const QJsonValue &v) {
                  if (gen != m_gen || !m_running)
                      return;
                  const QJsonObject o = v.toObject();
                  if (!ok || !o.value(QStringLiteral("ok")).toBool()) {
                      QStringList ins;
                      for (const QJsonValue &x : o.value(QStringLiteral("inputs")).toArray()) {
                          const QJsonObject io = x.toObject();
                          ins << QStringLiteral("[%1]%2").arg(
                              io.value(QStringLiteral("type")).toString(),
                              io.value(QStringLiteral("ph")).toString());
                      }
                      if (++m_fillTry <= 4) {
                          emit log(QStringLiteral("  填表没成功（%1），重试 %2/4")
                                       .arg(ok ? o.value(QStringLiteral("why")).toString()
                                               : v.toString())
                                       .arg(m_fillTry));
                          scheduleNext(S_Fill, 700);
                          return;
                      }
                      failChapter(QStringLiteral("找不到章号/标题输入框。页面输入框：%1")
                                      .arg(ins.join(QStringLiteral(" | "))));
                      return;
                  }
                  emit log(QStringLiteral("  章号=%1　标题=%2")
                               .arg(o.value(QStringLiteral("no")).toVariant().toString(),
                                    o.value(QStringLiteral("title")).toString()));
                  // 立刻回读不可靠（重渲染会冲掉），等 650ms 再由 S_Check 核对
                  scheduleNext(S_Check, 650);
              });
        break;
    }

    case S_Body: {
        runJs(QStringLiteral("__fq.fillBody(%1)").arg(jsStr(m_cur.body)),
              [this, gen](bool ok, const QJsonValue &v) {
                  if (gen != m_gen || !m_running)
                      return;
                  const QJsonObject r = v.toObject();
                  if (!ok || !r.value(QStringLiteral("ok")).toBool()) {
                      if (++m_bodyTry <= 3) {
                          emit log(QStringLiteral("  正文还没写进去，重试 %1/3").arg(m_bodyTry));
                          scheduleNext(S_Body, 800);
                          return;
                      }
                      failChapter(QStringLiteral("正文写不进去（找不到编辑器）"));
                      return;
                  }
                  const int got = r.value(QStringLiteral("chars")).toInt();
                  const int want = r.value(QStringLiteral("want")).toInt();
                  emit log(QStringLiteral("  正文写入 %1/%2 字（%3）")
                               .arg(got)
                               .arg(want)
                               .arg(r.value(QStringLiteral("used")).toString()));
                  if (want > 0 && got < static_cast<int>(want * 0.9)) {
                      if (++m_bodyTry <= 3) {
                          emit log(QStringLiteral("  字数偏少，重写 %1/3").arg(m_bodyTry));
                          scheduleNext(S_Body, 800);
                          return;
                      }
                      failChapter(QStringLiteral("正文写入后字数对不上（%1/%2），疑似被编辑器截断")
                                      .arg(got)
                                      .arg(want));
                      return;
                  }
                  m_bodyTry = 0;
                  m_bodyOk = true;
                  m_filled = true;      // 之后出任何岔子都要先存草稿
                  if (qEnvironmentVariableIsSet("CU_DRY")) {
                      emit log(QStringLiteral("CU_DRY：填表完成，测试到此为止（未点下一步、未发布）"));
                      if (!m_editorTarget.isEmpty())
                          m_cdp->closeTarget(m_editorTarget);
                      m_running = false;
                      ++m_gen;
                      emit finished(m_ok, m_fail);
                      return;
                  }
                  scheduleNext(S_Check, 500);
              });
        break;
    }

    case S_Check: {
        // 回读页面：章号/标题/正文都还在，才允许点「下一步」。
        runJs(QStringLiteral("__fq.formState()"), [this, gen](bool ok, const QJsonValue &v) {
            if (gen != m_gen || !m_running)
                return;
            const QJsonObject o = v.toObject();
            const QString gotNo = ok ? o.value(QStringLiteral("no")).toString().trimmed() : QString();
            const QString gotTitle =
                ok ? o.value(QStringLiteral("title")).toString().trimmed() : QString();
            const int chars = ok ? o.value(QStringLiteral("chars")).toInt() : 0;
            const bool noOk = (gotNo == QString::number(m_cur.no));
            const bool titleOk = (gotTitle == m_cur.title.trimmed());

            if (!noOk || !titleOk) {
                m_bodyOk = false;
                if (++m_fillTry <= 5) {
                    emit log(QStringLiteral("  页面回读不符（章号「%1」标题「%2」），重写 %3/5")
                                 .arg(gotNo, gotTitle)
                                 .arg(m_fillTry));
                    gotoStep(S_Fill);
                    return;
                }
                failChapter(QStringLiteral("章号/标题写不进去（回读 章号「%1」标题「%2」）")
                                .arg(gotNo, gotTitle));
                return;
            }

            if (!m_bodyOk) {
                gotoStep(S_Body);
                return;
            }
            if (m_wantChars > 0 && chars < static_cast<int>(m_wantChars * 0.9)) {
                m_bodyOk = false;
                if (++m_bodyTry <= 3) {
                    emit log(QStringLiteral("  正文回读只有 %1 字（应为 %2），重写 %3/3")
                                 .arg(chars)
                                 .arg(m_wantChars)
                                 .arg(m_bodyTry));
                    gotoStep(S_Body);
                    return;
                }
                failChapter(QStringLiteral("正文回读字数不对（%1/%2）").arg(chars).arg(m_wantChars));
                return;
            }

            m_fillTry = 0;
            emit log(QStringLiteral("  已核对：章号=%1　标题=%2　正文=%3 字")
                         .arg(gotNo, gotTitle)
                         .arg(chars));
            gotoStep(S_Next);
        });
        break;
    }

    case S_Next: {
        runJs(QStringLiteral("__fq.next()"), [this, gen](bool ok, const QJsonValue &v) {
            if (gen != m_gen || !m_running)
                return;
            const QJsonObject o = v.toObject();
            if (!ok || !o.value(QStringLiteral("ok")).toBool()) {
                const QString why = ok ? o.value(QStringLiteral("why")).toString() : v.toString();
                if (++m_retry < 8) {
                    emit log(QStringLiteral("「下一步」暂时点不动（%1），重试 %2/8").arg(why).arg(m_retry));
                    scheduleNext(S_Next, 1500);
                    return;
                }
                QStringList bs;
                for (const QJsonValue &b : o.value(QStringLiteral("buttons")).toArray())
                    bs << b.toString();
                emit log(QStringLiteral("可见按钮：%1").arg(bs.join(QStringLiteral(" | "))));
                failChapter(QStringLiteral("点不到「下一步」"));
                return;
            }
            m_retry = 0;
            m_ticks = 0;
            m_stepped = true;
            emit log(QStringLiteral("  已点「下一步」"));
            scheduleNext(S_Pump, 1200);
        });
        break;
    }

    case S_Pump: {
        runJs(QStringLiteral("__fq.autoStep()"), [this, gen](bool ok, const QJsonValue &v) {
            if (gen != m_gen || !m_running)
                return;
            if (!ok) {
                if (++m_ticks > 100) {
                    failChapter(QStringLiteral("检查流程超时：%1").arg(v.toString()));
                    return;
                }
                scheduleNext(S_Pump, 900);
                return;
            }
            const QJsonObject o = v.toObject();
            const QString act = o.value(QStringLiteral("act")).toString();
            ++m_ticks;

            if (act == QLatin1String("risk")) {
                const QString detail =
                    QStringLiteral("第 %1 章《%2》风险提示 %3 条，已中止上传")
                        .arg(m_cur.no)
                        .arg(m_cur.title)
                        .arg(o.value(QStringLiteral("risks")).toInt());
                abortWithDraft(detail, true);
                return;
            }
            if (act == QLatin1String("publish")) {
                emit log(QStringLiteral("  已进入发布设置"));
                gotoStep(S_Publish);
                return;
            }
            if (act == QLatin1String("cancelTypo")) {
                emit log(QStringLiteral("  弹窗「错别字未修改」→ 取消提交，转智能纠错"));
                scheduleNext(S_Pump, 1000);
                return;
            }
            if (act == QLatin1String("replaceNext")) {
                emit log(QStringLiteral("  智能纠错 → 替换全部 + 下一步"));
                scheduleNext(S_Pump, 1300);
                return;
            }
            if (act == QLatin1String("next")) {
                emit log(QStringLiteral("  智能纠错 → 下一步%s")
                             .arg(o.value(QStringLiteral("next")).toBool() ? QString()
                                                                            : QStringLiteral("（没点到）")));
                scheduleNext(S_Pump, 1300);
                return;
            }
            if (act == QLatin1String("fullDetect")) {
                emit log(QStringLiteral("  全面检测中，等待结果…"));
                scheduleNext(S_Pump, 3000);
                return;
            }

            // act == wait
            // 先看有没有番茄自己的校验提示（标题/正文不达标时它只会弹个红条，
            // 页面不会推进，干等下去只会超时，不如直接把原因报出来）
            if (m_ticks % 4 == 0) {
                runJs(QStringLiteral("__fq.tipText?__fq.tipText():''"),
                      [this, gen](bool okT, const QJsonValue &vT) {
                          if (gen != m_gen || !m_running)
                              return;
                          const QString tip = okT ? vT.toString().trimmed() : QString();
                          if (!tip.isEmpty())
                              abortWithDraft(QStringLiteral("第 %1 章没通过番茄校验：%2")
                                                 .arg(m_cur.no)
                                                 .arg(tip),
                                             false);
                      });
            }
            const QJsonObject st = o.value(QStringLiteral("stage")).toObject();
            // 注意：这里**不能**再"随便挑一个 /main/writer 标签"——
            // 现在章节管理页和编辑页是两个标签，挑错了就会在列表页上等发布设置。
            if (m_ticks == 6 || m_ticks == 20 || m_ticks == 45) {
                emit log(QStringLiteral("  …等待页面推进（管理=%1 编辑=%2 纠错=%3 发布=%4）")
                             .arg(st.value(QStringLiteral("manage")).toBool() ? QStringLiteral("是")
                                                                              : QStringLiteral("否"),
                                  st.value(QStringLiteral("editor")).toBool() ? QStringLiteral("是")
                                                                              : QStringLiteral("否"),
                                  st.value(QStringLiteral("correct")).toBool() ? QStringLiteral("是")
                                                                               : QStringLiteral("否"),
                                  st.value(QStringLiteral("publish")).toBool() ? QStringLiteral("是")
                                                                               : QStringLiteral("否")));
            }
            if (m_ticks > 120) {
                failChapter(QStringLiteral("等检测/发布设置超时"));
                return;
            }
            scheduleNext(S_Pump, 900);
        });
        break;
    }

    case S_Publish: {
        runJs(QStringLiteral("__fq.publishState()"), [this, gen](bool ok, const QJsonValue &v) {
            if (gen != m_gen || !m_running)
                return;
            const QJsonObject o = v.toObject();
            if (!ok || !o.value(QStringLiteral("shown")).toBool()) {
                if (++m_retry < 5) {
                    scheduleNext(S_Publish, 1200);
                    return;
                }
                failChapter(QStringLiteral("发布设置弹窗没出现"));
                return;
            }
            m_retry = 0;
            const QString date = m_cur.date.toString(QStringLiteral("yyyy-MM-dd"));
            runJs(QStringLiteral("__fq.setPublish(%1,%2)").arg(jsStr(date), jsStr(m_cur.time)),
                  [this, gen, date](bool ok2, const QJsonValue &v2) {
                      if (gen != m_gen || !m_running)
                          return;
                      const QJsonObject r = v2.toObject();
                      if (ok2)
                          emit log(QStringLiteral("  发布设置：%1").arg(stepsText(r.value(QStringLiteral("steps")))));

                      // 定时没设对就绝不能点「确认发布」——那会立刻发出去
                      if (!ok2 || !r.value(QStringLiteral("ok")).toBool()) {
                          const QString dv = r.value(QStringLiteral("dateVal")).toString();
                          const QString tv = r.value(QStringLiteral("timeVal")).toString();
                          failChapter(QStringLiteral("定时发布时间没设成功（读回：日期「%1」时间「%2」，期望 %3 %4）"
                                                     "，已中止本章，避免立即发布")
                                          .arg(dv, tv, m_cur.date.toString(QStringLiteral("yyyy-MM-dd")),
                                               m_cur.time));
                          return;
                      }
                      emit log(QStringLiteral("  定时：%1 %2（读回校验通过）")
                                   .arg(r.value(QStringLiteral("dateVal")).toString(),
                                        r.value(QStringLiteral("timeVal")).toString()));

                      // 自测钩子：停在发布设置（保留弹窗供 CDP 抓 DOM），绝不点「确认发布」
                      if (qEnvironmentVariableIsSet("CU_PUBSTOP")) {
                          emit log(QStringLiteral("CU_PUBSTOP：停在发布设置，未点确认发布（弹窗保留）"));
                          m_running = false;
                          ++m_gen;
                          emit finished(m_ok, m_fail);
                          return;
                      }

                      runJs(QStringLiteral("__fq.confirmPublish()"), [this, gen](bool ok3, const QJsonValue &v3) {
                          if (gen != m_gen || !m_running)
                              return;
                          const QJsonObject c = v3.toObject();
                          if (!ok3 || !c.value(QStringLiteral("ok")).toBool()) {
                              QStringList bs;
                              for (const QJsonValue &b : c.value(QStringLiteral("buttons")).toArray())
                                  bs << b.toString();
                              emit log(QStringLiteral("可见按钮：%1").arg(bs.join(QStringLiteral(" | "))));
                              failChapter(QStringLiteral("点不到「确认发布」"));
                              return;
                          }
                          emit log(QStringLiteral("  已点「确认发布」，等回执…"));
                          m_ticks = 0;
                          scheduleNext(S_PublishWait, 2000);
                      });
                  });
        });
        break;
    }

    case S_PublishWait: {
        runJs(QStringLiteral("__fq.published()"), [this, gen](bool ok, const QJsonValue &v) {
            if (gen != m_gen || !m_running)
                return;
            const QJsonObject o = ok ? v.toObject() : QJsonObject();
            if (o.value(QStringLiteral("success")).toBool() || o.value(QStringLiteral("back")).toBool()) {
                finishChapter(true, QStringLiteral("已提交，定时 %1 %2")
                                        .arg(m_cur.date.toString(QStringLiteral("yyyy-MM-dd")), m_cur.time));
                return;
            }
            if (++m_ticks > 40) {
                // 有可能其实已经发出去了，只是没抓到回执——不当成成功，也不悄悄跳过
                abortWithDraft(QStringLiteral("点「确认发布」后 60 秒没抓到回执，结果不确定，已中止（请到后台核对是否已生成该章）"),
                               false);
                return;
            }
            scheduleNext(S_PublishWait, 1500);
        });
        break;
    }
    }
}
