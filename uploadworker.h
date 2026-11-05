#ifndef UPLOADWORKER_H
#define UPLOADWORKER_H

#include <QDate>
#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class CdpClient;

// 逐章推进的状态机。
// 页面那边由 js/site.js 提供「按文本定位」的原子动作，这里只负责节奏、
// 重试、校验与熔断（宁可不传，也不能把定时发布设错导致立刻发出去）。
class UploadWorker : public QObject
{
    Q_OBJECT

public:
    struct Job {
        int     no = 0;
        QString title;     // 纯标题（去掉开头的章号）
        QString base;      // 完整文件名（不含扩展名），如「27归队」
        QString body;
        QString file;
        QDate   date;
        QString time;      // "19:05"
    };

    explicit UploadWorker(CdpClient *cdp, QObject *parent = nullptr);

    void setSiteScript(const QString &js) { m_site = js; }
    void setJobs(const QList<Job> &jobs) { m_jobs = jobs; }
    void setBook(const QString &b) { m_book = b; }
    void setVolume(const QString &v) { m_volume = v; }
    void setStopOnError(bool on) { m_stopOnError = on; }

    void start();
    void stop();
    void probePage();                 // 手动勘察：把当前页面的按钮/输入框打进日志

signals:
    void log(const QString &msg);
    void progress(int done, int total);
    void chapterStart(int index, int no, const QString &title);
    void chapterEnd(int index, bool ok, const QString &msg);
    void riskStop(const QString &detail);
    void finished(int okCount, int failCount);

private:
    enum Step {
        S_Idle,
        S_Prepare,
        S_OpenBook,
        S_Volume,
        S_NewChapter,
        S_Ready,          // 等编辑页真正挂好（骨架→真编辑器）
        S_Fill,           // 只写章号 / 标题
        S_Body,           // 写正文
        S_Check,          // 回读核对，不对就重写

        S_Next,
        S_Pump,
        S_Publish,
        S_PublishWait,
        S_Done
    };

    using JsCb = std::function<void(bool ok, const QJsonValue &v)>;

    void runJs(const QString &call, JsCb cb, int timeoutMs = 25000);
    void pollJs(const QString &call,
                std::function<bool(const QJsonValue &)> pred,
                int intervalMs, int maxTicks,
                std::function<void(bool ok, const QJsonValue &last)> done);
    void gotoStep(Step s);
    void scheduleNext(Step s, int ms);
    void beginChapter(int index);
    // 首章（m_index < 0）由这里正式开章；之后各章走 S_NewChapter
    void enterChapter();
    void failChapter(const QString &why);
    void abortWithDraft(const QString &why, bool risk);   // 先存草稿再中断
    void finishChapter(bool ok, const QString &msg);
    void advanceChapter();
    // 编辑页是「新建章节」打开的独立标签：关掉它并切回章节管理页
    void returnToManage(std::function<void(bool ok)> cb);

    CdpClient *m_cdp = nullptr;
    QString    m_site;
    QList<Job> m_jobs;
    QString    m_book;
    QString    m_volume;
    QStringList m_knownTargets;   // 点「新建章节」前已有的标签 id
    QString    m_editorTarget;    // 本章编辑页所在标签（用于收尾时关掉）

    bool   m_running = false;
    bool   m_stopOnError = true;
    int    m_gen = 0;
    int    m_index = -1;
    int    m_ok = 0;
    int    m_fail = 0;
    int    m_ticks = 0;
    int    m_retry = 0;
    int    m_fillTry = 0;            // 章号/标题重写次数（React 挂载慢时用）
    int    m_bodyTry = 0;            // 正文重写次数
    bool   m_bodyOk = false;         // 本章正文已确认写入
    int    m_wantChars = 0;          // 本章正文的期望字数（去空白）
    int    m_volFix = 0;             // 分卷自动纠正次数，防止来回打转
    int    m_switchRetry = 0;        // 起步时「切回章节管理标签」的尝试次数
    bool   m_stepped = false;        // 本章是否已点过「下一步」
    bool   m_filled = false;         // 本章正文是否已写入（决定异常时要不要存草稿）
    Step   m_step = S_Idle;
    Job    m_cur;
};

#endif // UPLOADWORKER_H
