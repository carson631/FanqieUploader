#include "mainwindow.h"

#include "browserlauncher.h"
#include "cdpclient.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimeEdit>
#include <QVBoxLayout>

#include <functional>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static const char *kVersion = "1.1.0";

static const char *kStyle = R"QSS(
/* ===== 基础 ================================================== */
QWidget#appBg  { background:#F4F5F7; }
QWidget#leftCol { background:transparent; }
QLabel         { color:#1F2937; }

/* ===== 顶栏 ================================================== */
QFrame#appHeader { background:#FFFFFF; border:0; border-bottom:1px solid #E9EBEF; }
QLabel#appTitle    { color:#111827; font-size:15px; font-weight:600; }
QLabel#appSubtitle { color:#9AA3B0; font-size:11px; }
QLabel#verTag {
    color:#8B93A1; background:#F4F5F7; border:1px solid #E9EBEF;
    border-radius:9px; padding:1px 8px; font-size:10px;
}
QFrame#chip {
    background:#F8F9FB; border:1px solid #E9EBEF; border-radius:11px;
}
QLabel#chipText { color:#6B7280; font-size:11px; }

/* ===== 卡片 ================================================== */
QFrame#card {
    background:#FFFFFF; border:1px solid #E9EBEF; border-radius:10px;
}
QLabel#section     { color:#111827; font-size:13px; font-weight:600; }
QLabel#badge {
    color:#F5591F; background:#FFF2EC; border-radius:9px;
    font-size:11px; font-weight:700;
    qproperty-alignment: AlignCenter;
}
QLabel#hint       { color:#9AA3B0; font-size:11px; }
QLabel#flowHint   { color:#C3CAD4; font-size:11px; }
QLabel#stat       { color:#6B7280; font-size:12px; }
QLabel#fieldLabel { color:#78828F; font-size:12px; }
QLabel#countChip {
    color:#6B7280; background:#F4F5F7; border-radius:8px;
    padding:1px 8px; font-size:11px;
}

/* ===== 输入控件 ============================================== */
QLineEdit, QComboBox, QDateEdit, QTimeEdit, QSpinBox {
    background:#FFFFFF; border:1px solid #DCE0E7; border-radius:6px;
    padding:5px 8px; color:#111827;
    selection-background-color:#FFD9C8; selection-color:#111827;
}
QLineEdit:hover, QComboBox:hover, QDateEdit:hover, QTimeEdit:hover, QSpinBox:hover {
    border-color:#C2C9D4;
}
QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QTimeEdit:focus, QSpinBox:focus {
    border-color:#F5591F;
}
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled {
    color:#AEB6C2; background:#F7F8FA;
}
QComboBox::drop-down, QDateEdit::drop-down { border:0; width:20px; }
QComboBox::down-arrow, QDateEdit::down-arrow {
    image:url(:/icons/chevron.png); width:10px; height:10px;
}
QComboBox QAbstractItemView {
    background:#FFFFFF; border:1px solid #E9EBEF; outline:0; padding:4px;
    selection-background-color:#FFF2EC; selection-color:#D9480F;
}
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width:0; border:0; }

/* ===== 复选框 ================================================ */
QCheckBox { color:#374151; spacing:8px; }
QCheckBox::indicator {
    width:15px; height:15px; border:1px solid #CFD5DE;
    border-radius:4px; background:#FFFFFF;
}
QCheckBox::indicator:hover { border-color:#F0A184; }
QCheckBox::indicator:checked {
    background:#F5591F; border-color:#F5591F; image:url(:/icons/check.png);
}

/* ===== 按钮 ================================================== */
QPushButton {
    background:#FFFFFF; border:1px solid #DCE0E7; border-radius:6px;
    padding:6px 14px; color:#374151;
}
QPushButton:hover   { background:#F9FAFB; border-color:#C2C9D4; }
QPushButton:pressed { background:#F1F3F5; }
QPushButton:disabled { color:#B6BEC9; background:#F7F8FA; border-color:#EDEFF3; }

QPushButton#primary {
    background:#F5591F; border:1px solid #F5591F; color:#FFFFFF;
    font-weight:600; padding:7px 18px;
}
QPushButton#primary:hover    { background:#DF4A12; border-color:#DF4A12; }
QPushButton#primary:pressed  { background:#C53F0E; }
QPushButton#primary:disabled { background:#F8C7B2; border-color:#F8C7B2; color:#FFFFFF; }

QPushButton#ghost {
    background:transparent; border:1px solid transparent;
    color:#6B7280; padding:4px 9px;
}
QPushButton#ghost:hover   { background:#F3F4F6; color:#111827; }
QPushButton#ghost:pressed { background:#E9EBEF; }

QPushButton#danger { color:#C2410C; border-color:#F2CFC0; }
QPushButton#danger:hover   { background:#FEF4EF; border-color:#E9AE93; }
QPushButton#danger:disabled { color:#C7CDD6; border-color:#EDEFF3; }

/* ===== 表格 ================================================== */
QTableWidget {
    background:#FFFFFF; border:1px solid #EFF1F4; border-radius:8px;
    gridline-color:transparent;
    selection-background-color:#FFF2EC; selection-color:#111827;
    alternate-background-color:#FAFBFC;
}
QTableWidget::item { padding:0 10px; border:0; }
QHeaderView::section {
    background:#FBFCFD; color:#9AA3B0; border:0;
    border-bottom:1px solid #EFF1F4; padding:7px 10px;
    font-size:11px; font-weight:600;
}
QTableCornerButton::section { background:#FBFCFD; border:0; }

/* ===== 进度 / 日志 =========================================== */
QProgressBar { background:#E8EBF0; border:0; border-radius:4px; }
QProgressBar::chunk { background:#F5591F; border-radius:4px; }

QPlainTextEdit#log {
    background:#FBFCFD; border:1px solid #EFF1F4; border-radius:8px;
    color:#4B5563; padding:8px;
}

/* ===== 滚动条 / 分隔条 / 状态栏 =============================== */
QScrollArea { background:transparent; border:0; }
QScrollBar:vertical { background:transparent; width:10px; margin:2px 0 2px 0; }
QScrollBar::handle:vertical { background:#D6DBE3; border-radius:4px; min-height:28px; }
QScrollBar::handle:vertical:hover { background:#BFC6D2; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; border:0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
QScrollBar:horizontal { background:transparent; height:10px; margin:0 2px 0 2px; }
QScrollBar::handle:horizontal { background:#D6DBE3; border-radius:4px; min-width:28px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; border:0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background:transparent; }

QSplitter::handle { background:transparent; }

QStatusBar { background:#FAFBFC; border-top:1px solid #E9EBEF; }
QStatusBar::item { border:0; }
QLabel#statBar { color:#9AA3B0; font-size:11px; }

QToolTip {
    background:#1F2937; color:#F3F4F6; border:0; padding:5px 8px; font-size:11px;
}
)QSS";

static const char *kBookUrl = "https://fanqienovel.com/main/writer/book-manage";

// 读文本：优先 UTF-8，出现替换符按 GBK 再解一次
static QString readText(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray raw = f.readAll();
    QString utf8 = QString::fromUtf8(raw);
    int bad = 0;
    for (const QChar &c : utf8)
        if (c == QChar(0xFFFD)) ++bad;
    if (bad == 0)
        return utf8;
#ifdef Q_OS_WIN
    const int wlen = MultiByteToWideChar(936, 0, raw.constData(), raw.size(), nullptr, 0);
    if (wlen > 0) {
        QString gbk(wlen, QChar(0));
        MultiByteToWideChar(936, 0, raw.constData(), raw.size(),
                            reinterpret_cast<wchar_t *>(gbk.data()), wlen);
        return gbk;
    }
#endif
    return utf8;
}

// 正文归一：CRLF → LF，去掉首尾空行（保留行首全角缩进）
static QString cleanBody(const QString &s)
{
    QString t = s;
    t.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    t.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    while (t.endsWith(QLatin1Char('\n')) || t.endsWith(QLatin1Char(' ')))
        t.chop(1);
    while (t.startsWith(QLatin1Char('\n')))
        t.remove(0, 1);
    return t;
}

namespace {

// 书名/卷名的合理性检查：后台导航条被整段抓下来时会带好几个空格，
// 这种值不写进设置、也不从设置里恢复。
bool saneName(const QString &raw)
{
    const QString t = raw.trimmed();
    if (t.isEmpty() || t.size() > 30)
        return false;
    int spaces = 0;
    for (const QChar &c : t) {
        if (c.isSpace())
            ++spaces;
    }
    return spaces < 2;
}

}   // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // 启动耗时剖析（CU_BOOT=1 时把各段耗时追加到 %TEMP%/fqboot.txt）
    QElapsedTimer prof;
    prof.start();

    setWindowTitle(QStringLiteral("番茄章节上传工具"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    // 样式表等控件全建完再一次性应用：提前 set 会让每个新控件在 polish 时
    // 逐条匹配 QSS 规则，78 个控件累起来就是几百毫秒（实测差 ~200ms）
    const qint64 tCss = prof.restart();
    setMinimumSize(1040, 640);
    {
        // 默认尺寸跟着屏幕可用区走：1080p / 2K / 缩放 125% 都不会超出屏幕
        const QRect avail = QGuiApplication::primaryScreen()
                                ? QGuiApplication::primaryScreen()->availableGeometry()
                                : QRect(0, 0, 1280, 800);
        resize(qMin(1260, avail.width() - 60), qMin(820, avail.height() - 60));
    }

    loadSiteScript();
    const qint64 tJs = prof.restart();

    m_cdp = new CdpClient(this);
    m_cdp->setPort(BrowserLauncher::defaultPort());
    m_browser = new BrowserLauncher(this);
    m_worker = new UploadWorker(m_cdp, this);
    m_worker->setSiteScript(m_siteJs);

    buildUi();
    const qint64 tUi = prof.restart();
    setStyleSheet(QString::fromUtf8(kStyle));
    const qint64 tPolish = prof.restart();

    connect(m_cdp, &CdpClient::connected, this, [this](const QString &url, const QString &) {
        setConnected(true, QStringLiteral("已连接"));
        appendLog(QStringLiteral("✓ 已连接：%1").arg(url));
        QTimer::singleShot(1200, this, [this]() { refreshBooks(); });
    });
    connect(m_cdp, &CdpClient::disconnected, this, [this]() {
        setConnected(false, QStringLiteral("未连接"));
    });
    connect(m_cdp, &CdpClient::failed, this, [this](const QString &why) {
        appendLog(QStringLiteral("✗ %1").arg(why));
        setConnected(false, QStringLiteral("未连接"));
    });
    connect(m_cdp, &CdpClient::log, this, [this](const QString &m) { appendLog(m); });

    connect(m_worker, &UploadWorker::log, this, &MainWindow::appendLog);
    connect(m_worker, &UploadWorker::progress, this, [this](int done, int total) {
        m_progress->setMaximum(qMax(1, total));
        m_progress->setValue(done);
        if (total > 0) {
            m_summary->setText(QStringLiteral("进度 %1 / %2").arg(done).arg(total));
            if (m_statusText)
                m_statusText->setText(QStringLiteral("正在上传　%1 / %2").arg(done).arg(total));
        }
    });
    connect(m_worker, &UploadWorker::chapterStart, this, [this](int index, int, const QString &) {
        if (index < m_table->rowCount())
            m_table->selectRow(index);
    });
    connect(m_worker, &UploadWorker::chapterEnd, this, [this](int index, bool ok, const QString &) {
        if (index < 0 || index >= m_table->rowCount())
            return;
        if (auto *it = m_table->item(index, 3)) {
            it->setForeground(ok ? QColor(0x16, 0xA3, 0x4A) : QColor(0xDC, 0x26, 0x26));
        }
    });
    connect(m_worker, &UploadWorker::riskStop, this, [this](const QString &detail) {
        m_startBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        if (m_statusText)
            m_statusText->setText(QStringLiteral("已中断 · 需要人工处理"));
        QMessageBox::warning(
            this, QStringLiteral("检测到风险，已中断"),
            detail + QStringLiteral("\n\n请到浏览器里人工处理这一章；处理完再点「开始上传」。\n"
                                    "本工具不会自动跳过风险章节，避免误发。"));
    });
    connect(m_worker, &UploadWorker::finished, this, [this](int ok, int fail) {
        flushLog();
        m_startBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_summary->setText(QStringLiteral("完成：成功 %1，失败 %2").arg(ok).arg(fail));
        if (m_statusText)
            m_statusText->setText(fail > 0
                                      ? QStringLiteral("已结束 · 成功 %1，失败 %2").arg(ok).arg(fail)
                                      : QStringLiteral("已结束 · 全部成功（%1 章）").arg(ok));
    });

    const QDate buildDay = QDate::fromString(QString::fromLatin1(__DATE__).simplified(),
                                             QStringLiteral("MMM d yyyy"));
    appendLog(QStringLiteral("FanqieUploader v%1　构建 %2 %3")
                  .arg(QString::fromLatin1(kVersion),
                       buildDay.isValid() ? buildDay.toString(QStringLiteral("yyyy-MM-dd"))
                                          : QString::fromLatin1(__DATE__).simplified(),
                       QString::fromLatin1(__TIME__)));
    setConnected(false, QStringLiteral("未连接"));
    appendLog(QStringLiteral("就绪。首次使用：点「启动浏览器」，在弹出的浏览器里登录番茄作家账号。"));
    loadSettings();
    const qint64 tRest = prof.elapsed();

    if (qEnvironmentVariableIsSet("CU_BOOT")) {
        QFile f(QDir::tempPath() + QStringLiteral("/fqboot.txt"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Append)) {
            f.write(QStringLiteral("ctor: style=%1ms js=%2ms worker+ui=%3ms polish=%4ms rest=%5ms\n")
                        .arg(tCss).arg(tJs).arg(tUi).arg(tPolish).arg(tRest).toUtf8());
        }
    }
}

MainWindow::~MainWindow()
{
    flushLog();
}

void MainWindow::loadSiteScript()
{
    const QString disk = QCoreApplication::applicationDirPath() + QStringLiteral("/js/site.js");
    QFile f(disk);
    if (f.open(QIODevice::ReadOnly)) {
        m_siteJs = QString::fromUtf8(f.readAll());
        return;
    }
    QFile r(QStringLiteral(":/js/site.js"));
    if (r.open(QIODevice::ReadOnly))
        m_siteJs = QString::fromUtf8(r.readAll());
}

// ---- 界面 ---------------------------------------------------------------

static QLabel *fieldLabel(const QString &t)
{
    auto *l = new QLabel(t);
    l->setObjectName(QStringLiteral("fieldLabel"));
    return l;
}

// 卡片 = 白底 + 1px 边框 + 标题行（可选数字徽标 + 右侧动作区）
static QFrame *makeCard(const QString &step, const QString &titleText,
                        QVBoxLayout **out, QHBoxLayout **headOut = nullptr)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("card"));
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(16, 13, 16, 15);
    v->setSpacing(10);

    auto *head = new QHBoxLayout;
    head->setSpacing(8);
    head->setContentsMargins(0, 0, 0, 0);
    if (!step.isEmpty()) {
        auto *b = new QLabel(step);
        b->setObjectName(QStringLiteral("badge"));
        b->setFixedSize(18, 18);
        head->addWidget(b, 0, Qt::AlignVCenter);
    }
    auto *t = new QLabel(titleText);
    t->setObjectName(QStringLiteral("section"));
    head->addWidget(t, 0, Qt::AlignVCenter);
    head->addStretch(1);
    v->addLayout(head);

    if (headOut)
        *headOut = head;
    *out = v;
    return card;
}

void MainWindow::buildUi()
{
    auto *bg = new QWidget;
    bg->setObjectName(QStringLiteral("appBg"));
    setCentralWidget(bg);

    auto *root = new QVBoxLayout(bg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ===== 顶栏：品牌 + 全局状态 =====
    auto *header = new QFrame;
    header->setObjectName(QStringLiteral("appHeader"));
    header->setFixedHeight(56);
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(18, 0, 18, 0);
    hl->setSpacing(10);

    auto *logo = new QLabel;
    logo->setFixedSize(26, 26);
    logo->setPixmap(QPixmap(QStringLiteral(":/icons/app.png"))
                        .scaled(26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    hl->addWidget(logo, 0, Qt::AlignVCenter);

    auto *tb = new QVBoxLayout;
    tb->setSpacing(1);
    tb->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(QStringLiteral("番茄章节上传工具"));
    title->setObjectName(QStringLiteral("appTitle"));
    auto *sub = new QLabel(QStringLiteral("番茄小说网 · 作家后台　自动建章 → 纠错 → 全量检测 → 定时发布"));
    sub->setObjectName(QStringLiteral("appSubtitle"));
    tb->addWidget(title);
    tb->addWidget(sub);
    hl->addLayout(tb);

    auto *ver = new QLabel(QStringLiteral("v") + QString::fromLatin1(kVersion));
    ver->setObjectName(QStringLiteral("verTag"));
    hl->addWidget(ver, 0, Qt::AlignVCenter);
    hl->addStretch(1);

    auto *chip = new QFrame;
    chip->setObjectName(QStringLiteral("chip"));
    chip->setFixedHeight(22);
    auto *cl = new QHBoxLayout(chip);
    cl->setContentsMargins(9, 0, 11, 0);
    cl->setSpacing(6);
    m_dot = new QLabel;
    m_dot->setFixedSize(6, 6);
    m_connText = new QLabel(QStringLiteral("未连接"));
    m_connText->setObjectName(QStringLiteral("chipText"));
    cl->addWidget(m_dot, 0, Qt::AlignVCenter);
    cl->addWidget(m_connText, 0, Qt::AlignVCenter);
    hl->addWidget(chip, 0, Qt::AlignVCenter);

    m_probeBtn = new QPushButton(QStringLiteral("勘察页面"));
    m_probeBtn->setObjectName(QStringLiteral("ghost"));
    m_probeBtn->setToolTip(QStringLiteral("把当前页面结构打印到日志，排查选择器时用"));
    m_probeBtn->setEnabled(false);
    hl->addWidget(m_probeBtn, 0, Qt::AlignVCenter);

    m_connBtn = new QPushButton(QStringLiteral("启动浏览器"));
    m_connBtn->setObjectName(QStringLiteral("primary"));
    m_connBtn->setToolTip(QStringLiteral(
        "用独立 profile 拉起 Edge 并接入调试端口（Ctrl+B）\n"
        "登录态只保存在这个 profile 里，不影响你日常用的浏览器。"));
    m_connBtn->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));
    hl->addWidget(m_connBtn, 0, Qt::AlignVCenter);

    connect(m_connBtn, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(m_probeBtn, &QPushButton::clicked, this, [this]() {
        if (!m_cdp->isConnected()) {
            appendLog(QStringLiteral("还没连接，先点「启动浏览器」"));
            return;
        }
        m_worker->probePage();
    });
    root->addWidget(header);

    // ===== 主体 =====
    auto *body = new QWidget;
    auto *bl = new QHBoxLayout(body);
    bl->setContentsMargins(14, 14, 14, 14);
    bl->setSpacing(14);

    // ===== 左列：上面设置区可滚动，底部「执行」常驻 =====
    auto *leftCol = new QWidget;
    leftCol->setFixedWidth(452);
    auto *lcv = new QVBoxLayout(leftCol);
    lcv->setContentsMargins(0, 0, 0, 0);
    lcv->setSpacing(12);

    auto *leftScroll = new QScrollArea;
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    leftScroll->viewport()->setAutoFillBackground(false);

    auto *left = new QWidget;
    left->setObjectName(QStringLiteral("leftCol"));
    auto *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 4, 0);
    lv->setSpacing(12);

    // ① 作品
    QVBoxLayout *c2 = nullptr;
    QHBoxLayout *h2r = nullptr;
    auto *card2 = makeCard(QStringLiteral("1"), QStringLiteral("作品与分卷"), &c2, &h2r);
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"));
    m_refreshBtn->setObjectName(QStringLiteral("ghost"));
    m_refreshBtn->setToolTip(QStringLiteral("从后台重新读取书架与分卷"));
    h2r->addWidget(m_refreshBtn, 0, Qt::AlignVCenter);

    auto *f2 = new QGridLayout;
    f2->setHorizontalSpacing(8);
    f2->setVerticalSpacing(9);
    f2->setColumnMinimumWidth(0, 40);
    f2->addWidget(fieldLabel(QStringLiteral("书名")), 0, 0);
    m_bookCombo = new QComboBox;
    m_bookCombo->setEditable(true);
    f2->addWidget(m_bookCombo, 0, 1);
    f2->addWidget(fieldLabel(QStringLiteral("分卷")), 1, 0);
    m_volCombo = new QComboBox;
    m_volCombo->setEditable(true);
    f2->addWidget(m_volCombo, 1, 1);
    f2->setColumnStretch(1, 1);
    c2->addLayout(f2);
    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        refreshBooks();
        refreshVolumes();
    });

    // ② 章节源
    QVBoxLayout *c3 = nullptr;
    auto *card3 = makeCard(QStringLiteral("2"), QStringLiteral("章节源与排期"), &c3);
    auto *rowd = new QHBoxLayout;
    rowd->setSpacing(8);
    m_dirEdit = new QLineEdit;
    m_dirEdit->setPlaceholderText(QStringLiteral("选择「每章一个 txt」的文件夹"));
    m_browseBtn = new QPushButton(QStringLiteral("选择"));
    m_browseBtn->setToolTip(QStringLiteral("选择章节文件夹（Ctrl+O）"));
    m_browseBtn->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    rowd->addWidget(m_dirEdit, 1);
    rowd->addWidget(m_browseBtn);
    c3->addLayout(rowd);

    auto *f3 = new QGridLayout;
    f3->setHorizontalSpacing(8);
    f3->setVerticalSpacing(9);
    f3->setColumnMinimumWidth(0, 58);
    f3->setColumnMinimumWidth(2, 40);
    f3->setColumnStretch(1, 1);

    // 章节范围：只上传目录里指定章号区间的部分（默认全选）
    f3->addWidget(fieldLabel(QStringLiteral("章节范围")), 0, 0);
    auto *rowRange = new QHBoxLayout;
    rowRange->setSpacing(6);
    m_fromSpin = new QSpinBox;
    m_fromSpin->setRange(0, 99999);
    m_fromSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_fromSpin->setAlignment(Qt::AlignCenter);
    m_fromSpin->setFixedWidth(72);
    m_toSpin = new QSpinBox;
    m_toSpin->setRange(0, 99999);
    m_toSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_toSpin->setAlignment(Qt::AlignCenter);
    m_toSpin->setFixedWidth(72);
    rowRange->addWidget(fieldLabel(QStringLiteral("第")));
    rowRange->addWidget(m_fromSpin);
    rowRange->addWidget(fieldLabel(QStringLiteral("章")));
    rowRange->addWidget(fieldLabel(QStringLiteral("—")));
    rowRange->addWidget(fieldLabel(QStringLiteral("第")));
    rowRange->addWidget(m_toSpin);
    rowRange->addWidget(fieldLabel(QStringLiteral("章")));
    rowRange->addStretch(1);
    m_rangeInfo = new QLabel(QStringLiteral("全部"));
    m_rangeInfo->setObjectName(QStringLiteral("countChip"));
    rowRange->addWidget(m_rangeInfo, 0, Qt::AlignVCenter);
    f3->addLayout(rowRange, 0, 1, 1, 3);

    // 标题格式：番茄后台这本书历来是「第27章 27归队」这种带章号的写法
    m_titleWithNo = new QCheckBox(QStringLiteral("标题带章号（27归队）"));
    m_titleWithNo->setChecked(true);
    m_titleWithNo->setToolTip(QStringLiteral(
        "勾上：标题 = 文件名「27归队」；不勾：标题 = 去掉章号的「归队」。"));
    f3->addWidget(m_titleWithNo, 1, 1, 1, 3);

    f3->addWidget(fieldLabel(QStringLiteral("首章日期")), 2, 0);
    m_dateEdit = new QDateEdit;
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_dateEdit->setDate(QDate::currentDate().addDays(1));
    f3->addWidget(m_dateEdit, 2, 1);
    f3->addWidget(fieldLabel(QStringLiteral("每天")), 2, 2);
    m_perDay = new QSpinBox;
    m_perDay->setRange(1, 20);
    m_perDay->setValue(3);
    m_perDay->setSuffix(QStringLiteral(" 章"));
    m_perDay->setAlignment(Qt::AlignRight);
    m_perDay->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_perDay->setFixedWidth(96);
    f3->addWidget(m_perDay, 2, 3);

    f3->addWidget(fieldLabel(QStringLiteral("首发时间")), 3, 0);
    m_timeEdit = new QTimeEdit(QTime(19, 0));
    m_timeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    m_timeEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    f3->addWidget(m_timeEdit, 3, 1);
    f3->addWidget(fieldLabel(QStringLiteral("间隔")), 3, 2);
    m_interval = new QSpinBox;
    m_interval->setRange(0, 120);
    m_interval->setValue(5);
    m_interval->setSuffix(QStringLiteral(" 分钟"));
    m_interval->setAlignment(Qt::AlignRight);
    m_interval->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_interval->setFixedWidth(96);
    f3->addWidget(m_interval, 3, 3);
    c3->addLayout(f3);

    auto *h3 = new QLabel(QStringLiteral(
        "排期按「每天章数 + 间隔」自动顺延到次日；章节范围按文件名里的章号筛选。"));
    h3->setObjectName(QStringLiteral("hint"));
    h3->setWordWrap(true);
    c3->addWidget(h3);

    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::pickFolder);
    connect(m_dirEdit, &QLineEdit::returnPressed, this, [this]() {
        const QString d = m_dirEdit->text().trimmed();
        if (!d.isEmpty()) {
            m_dirEdit->setText(QDir::toNativeSeparators(d));
            loadChapters(d);
        }
    });
    connect(m_titleWithNo, &QCheckBox::toggled, this, [this](bool) { applyRange(false); });
    connect(m_fromSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { applyRange(false); });
    connect(m_toSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { applyRange(false); });
    connect(m_perDay, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { reschedule(); });
    connect(m_interval, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { reschedule(); });
    connect(m_dateEdit, &QDateEdit::dateChanged, this, [this](const QDate &) { reschedule(); });
    connect(m_timeEdit, &QTimeEdit::timeChanged, this, [this](const QTime &) { reschedule(); });

    // ③ 执行
    QVBoxLayout *c4 = nullptr;
    QHBoxLayout *h4r = nullptr;
    auto *card4 = makeCard(QStringLiteral("3"), QStringLiteral("执行"), &c4, &h4r);
    m_stopOnErr = new QCheckBox(QStringLiteral("出错即停"));
    m_stopOnErr->setChecked(true);
    m_stopOnErr->setToolTip(QStringLiteral("任意一章失败就中止整批，避免带病连发"));
    h4r->addWidget(m_stopOnErr, 0, Qt::AlignVCenter);

    auto *row4 = new QHBoxLayout;
    row4->setSpacing(8);
    m_startBtn = new QPushButton(QStringLiteral("开始上传"));
    m_startBtn->setObjectName(QStringLiteral("primary"));
    m_startBtn->setMinimumWidth(118);
    m_startBtn->setToolTip(QStringLiteral("按上面的排期批量建章并发布（Ctrl+Enter）"));
    m_startBtn->setShortcut(QKeySequence(QStringLiteral("Ctrl+Return")));
    m_stopBtn = new QPushButton(QStringLiteral("停止"));
    m_stopBtn->setObjectName(QStringLiteral("danger"));
    m_stopBtn->setEnabled(false);
    row4->addWidget(m_startBtn);
    row4->addWidget(m_stopBtn);
    row4->addStretch(1);
    c4->addLayout(row4);

    m_progress = new QProgressBar;
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    c4->addWidget(m_progress);

    m_summary = new QLabel(QStringLiteral("未加载章节"));
    m_summary->setObjectName(QStringLiteral("stat"));
    m_summary->setWordWrap(true);
    c4->addWidget(m_summary);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::startUpload);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::stopUpload);
    connect(m_stopOnErr, &QCheckBox::toggled, this, [this](bool on) {
        m_worker->setStopOnError(on);
    });

    // 执行区在滚动区之外（见下方 lcv），这里只排设置卡片
    lv->addWidget(card2);
    lv->addWidget(card3);
    auto *flowHint = new QLabel(QStringLiteral("连接浏览器　→　选择章节文件夹　→　核对排期后开始上传"));
    flowHint->setObjectName(QStringLiteral("flowHint"));
    flowHint->setAlignment(Qt::AlignCenter);
    flowHint->setWordWrap(true);
    lv->addWidget(flowHint, 1);
    leftScroll->setWidget(left);

    lcv->addWidget(leftScroll, 1);
    lcv->addWidget(card4);

    // ===== 右列 =====
    auto *right = new QSplitter(Qt::Vertical);
    right->setChildrenCollapsible(false);
    right->setHandleWidth(12);
    m_split = right;

    auto *tCard = new QFrame;
    tCard->setObjectName(QStringLiteral("card"));
    auto *tv = new QVBoxLayout(tCard);
    tv->setContentsMargins(14, 12, 14, 14);
    tv->setSpacing(10);
    auto *tHead = new QHBoxLayout;
    tHead->setSpacing(8);
    auto *tTitle = new QLabel(QStringLiteral("章节与排期"));
    tTitle->setObjectName(QStringLiteral("section"));
    tHead->addWidget(tTitle, 0, Qt::AlignVCenter);
    m_countLabel = new QLabel(QStringLiteral("空"));
    m_countLabel->setObjectName(QStringLiteral("countChip"));
    tHead->addWidget(m_countLabel, 0, Qt::AlignVCenter);
    tHead->addStretch(1);
    auto *tHint = new QLabel(QStringLiteral("按左侧规则自动计算"));
    tHint->setObjectName(QStringLiteral("hint"));
    tHead->addWidget(tHint, 0, Qt::AlignVCenter);
    tv->addLayout(tHead);

    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("章号"), QStringLiteral("标题（取自文件名）"),
                                        QStringLiteral("字数"), QStringLiteral("定时发布")});
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (auto *hdr = m_table->horizontalHeaderItem(2))
        hdr->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 72);
    m_table->setColumnWidth(2, 88);
    m_table->setColumnWidth(3, 158);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    tv->addWidget(m_table, 1);

    auto *logCard = new QFrame;
    logCard->setObjectName(QStringLiteral("card"));
    auto *lgv = new QVBoxLayout(logCard);
    lgv->setContentsMargins(14, 12, 14, 14);
    lgv->setSpacing(10);
    auto *lgHead = new QHBoxLayout;
    lgHead->setSpacing(8);
    auto *lgTitle = new QLabel(QStringLiteral("运行日志"));
    lgTitle->setObjectName(QStringLiteral("section"));
    lgHead->addWidget(lgTitle, 0, Qt::AlignVCenter);
    lgHead->addStretch(1);
    auto *clearBtn = new QPushButton(QStringLiteral("清空"));
    clearBtn->setObjectName(QStringLiteral("ghost"));
    clearBtn->setToolTip(QStringLiteral("清空日志（Ctrl+L）"));
    clearBtn->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    lgHead->addWidget(clearBtn, 0, Qt::AlignVCenter);
    lgv->addLayout(lgHead);

    m_log = new QPlainTextEdit;
    m_log->setObjectName(QStringLiteral("log"));
    // 等宽字体在这里设（不能写进 QSS）：QSS 的 font-family 会让每个控件
    // polish 时都做一次字体族查找，带引号+逗号的多字体族更是每次都匹配失败
    // —— 实测这一条占了启动的 460ms（polish 466ms → 2ms）
    {
        QFont mono(QStringLiteral("Consolas"));
        mono.setStyleHint(QFont::Monospace);
        mono.setPixelSize(12);
        m_log->setFont(mono);
        m_log->document()->setDefaultFont(mono);
    }
    m_log->setReadOnly(true);
    m_log->setUndoRedoEnabled(false);
    m_log->setMaximumBlockCount(4000);
    m_log->setPlaceholderText(QStringLiteral("等待任务开始…"));
    lgv->addWidget(m_log, 1);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        flushLog();
        m_log->clear();
    });

    right->addWidget(tCard);
    right->addWidget(logCard);
    right->setStretchFactor(0, 5);
    right->setStretchFactor(1, 3);

    bl->addWidget(leftCol);
    bl->addWidget(right, 1);
    root->addWidget(body, 1);

    // 状态栏
    m_statusText = new QLabel(QStringLiteral("就绪"));
    m_statusText->setObjectName(QStringLiteral("statBar"));
    statusBar()->addWidget(m_statusText);
    auto *sbHint = new QLabel(QStringLiteral("只在本机驱动浏览器，不接触你的账号密码"));
    sbHint->setObjectName(QStringLiteral("statBar"));
    statusBar()->addPermanentWidget(sbHint);
    statusBar()->setSizeGripEnabled(false);

    m_logTimer = new QTimer(this);
    m_logTimer->setInterval(120);
    m_logTimer->setSingleShot(true);
    connect(m_logTimer, &QTimer::timeout, this, &MainWindow::flushLog);

    showEmptyTable();
}

void MainWindow::setConnected(bool on, const QString &text)
{
    m_connText->setText(text);
    m_dot->setStyleSheet(on ? QStringLiteral("background:#16A34A;border-radius:3px;")
                            : QStringLiteral("background:#C6CCD6;border-radius:3px;"));
    m_connText->setStyleSheet(on ? QStringLiteral("color:#0F7A3D;")
                                 : QStringLiteral("color:#6B7280;"));
    m_connBtn->setText(on ? QStringLiteral("断开连接") : QStringLiteral("启动浏览器"));
    // 连上以后「断开」是次要动作，不该继续占着主色按钮
    const QString want = on ? QString() : QStringLiteral("primary");
    if (m_connBtn->objectName() != want) {
        m_connBtn->setObjectName(want);
        m_connBtn->style()->unpolish(m_connBtn);
        m_connBtn->style()->polish(m_connBtn);
    }
    m_probeBtn->setEnabled(on);
    if (m_statusText)
        m_statusText->setText(on ? QStringLiteral("已连接浏览器 · 可以开始上传")
                                 : QStringLiteral("就绪"));
}

// ---- 连接 ---------------------------------------------------------------

void MainWindow::toggleConnection()
{
    if (m_cdp->isConnected()) {
        m_cdp->close();
        appendLog(QStringLiteral("已断开调试连接（浏览器仍在运行）"));
        return;
    }

    QString err;
    if (!m_browser->launch(QString::fromLatin1(kBookUrl), BrowserLauncher::defaultPort(), &err)) {
        appendLog(QStringLiteral("✗ %1").arg(err));
        return;
    }
    appendLog(QStringLiteral("浏览器已启动（独立 profile：%1）").arg(BrowserLauncher::profileDir()));
    appendLog(QStringLiteral("等调试端口就绪…首次启动约 3~6 秒"));

    auto left = std::make_shared<int>(25);
    auto timer = new QTimer(this);
    timer->setInterval(600);
    connect(timer, &QTimer::timeout, this, [this, left, timer]() {
        if (m_cdp->isConnected()) {
            timer->stop();
            timer->deleteLater();
            return;
        }
        if (--*left <= 0) {
            timer->stop();
            timer->deleteLater();
            appendLog(QStringLiteral("✗ 调试端口一直没起来。若是第一次，浏览器可能还在初始化，请再点一次「启动浏览器」"));
            return;
        }
        // 端口一通就去 discovery
        if (left && *left % 2 == 0)
            m_cdp->discoverAndConnect();
    });
    timer->start();
    m_cdp->discoverAndConnect();
}

// ---- 读页面信息 ---------------------------------------------------------

void MainWindow::refreshBooks()
{
    if (!m_cdp->isConnected()) {
        appendLog(QStringLiteral("未连接，无法刷新书架"));
        return;
    }
    // 只有作品管理页才有书架；章节管理/编辑页读出来的是导航条文本
    const QString expr = QStringLiteral("(function(){if(!window.__fq){") + m_siteJs
                         + QStringLiteral("}const st=__fq.stage();")
                         + QStringLiteral("if(!st.books)return {__notBooks:true};")
                         + QStringLiteral("return (__fq.books());})()");
    m_cdp->evaluate(expr, [this](bool ok, const QJsonValue &v, const QString &err) {
        if (!ok) {
            appendLog(QStringLiteral("读书架失败：%1").arg(err));
            return;
        }
        if (v.isObject() && v.toObject().value(QStringLiteral("__notBooks")).toBool()) {
            appendLog(QStringLiteral("当前不在作品管理页，跳过读书名（保留原值）"));
            return;
        }
        const QStringList list = [&v]() {
            QStringList out;
            for (const QJsonValue &x : v.toArray())
                out << x.toString();
            return out;
        }();
        if (list.isEmpty()) {
            appendLog(QStringLiteral("没读到书名（当前不在作品管理页就正常，书名保留原值）"));
            return;
        }
        const QString keep = m_bookCombo->currentText();
        m_bookCombo->clear();
        m_bookCombo->addItems(list);
        if (!keep.isEmpty() && list.contains(keep))
            m_bookCombo->setCurrentText(keep);
        appendLog(QStringLiteral("读到 %1 本书：%2").arg(list.size()).arg(list.join(QStringLiteral(" / "))));
    });
}

void MainWindow::refreshVolumes()
{
    if (!m_cdp->isConnected()) {
        appendLog(QStringLiteral("未连接，无法刷新分卷"));
        return;
    }
    const QString expr = QStringLiteral("(function(){if(!window.__fq){") + m_siteJs
                         + QStringLiteral("}return (__fq.volumes());})()");
    m_cdp->evaluate(expr, [this](bool ok, const QJsonValue &v, const QString &err) {
        if (!ok) {
            appendLog(QStringLiteral("读分卷失败：%1").arg(err));
            return;
        }
        const QStringList list = [&v]() {
            QStringList out;
            for (const QJsonValue &x : v.toArray())
                out << x.toString();
            return out;
        }();
        if (list.isEmpty()) {
            appendLog(QStringLiteral("没读到分卷（先进到某本书的「章节管理」页再刷新）"));
            return;
        }
        const QString keep = m_volCombo->currentText();
        m_volCombo->clear();
        m_volCombo->addItems(list);
        if (!keep.isEmpty() && list.contains(keep))
            m_volCombo->setCurrentText(keep);
        appendLog(QStringLiteral("读到 %1 个分卷：%2").arg(list.size()).arg(list.join(QStringLiteral(" / "))));
    });
}

// ---- 章节源 -------------------------------------------------------------

void MainWindow::pickFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择章节文件夹"),
                                                          m_dirEdit->text());
    if (dir.isEmpty())
        return;
    m_dirEdit->setText(QDir::toNativeSeparators(dir));
    loadChapters(dir);
}

void MainWindow::loadChapters(const QString &dirPath)
{
    QDir d(dirPath);
    if (!d.exists()) {
        appendLog(QStringLiteral("✗ 目录不存在：%1").arg(dirPath));
        return;
    }
    QStringList files = d.entryList({QStringLiteral("*.txt")}, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        appendLog(QStringLiteral("✗ 这个目录里没有 .txt 文件"));
        return;
    }

    static const QRegularExpression kNum(QStringLiteral("^\\s*(\\d+)\\s*[\\s_\\.\\-]*(.*)$"));
    static const QRegularExpression kCn(QStringLiteral("^\\s*第([0-9０-９]+)章\\s*(.*)$"));

    m_allJobs.clear();
    int seq = 0;
    for (const QString &f : files) {
        const QFileInfo fi(d.filePath(f));
        const QString base = fi.completeBaseName();
        UploadWorker::Job job;
        job.file = fi.absoluteFilePath();
        job.base = base;                 // 「27归队」——标题带章号时用它
        auto m = kNum.match(base);
        if (m.hasMatch()) {
            job.no = m.captured(1).toInt();
            job.title = m.captured(2).trimmed();
        } else {
            auto m2 = kCn.match(base);
            if (m2.hasMatch()) {
                job.no = m2.captured(1).toInt();
                job.title = m2.captured(2).trimmed();
            } else {
                job.no = ++seq;
                job.title = base.trimmed();
            }
        }
        if (job.no <= 0)
            job.no = ++seq;
        job.body = cleanBody(readText(job.file));
        if (job.body.isEmpty())
            appendLog(QStringLiteral("⚠ 空文件：%1").arg(f));
        m_allJobs.append(job);
    }
    std::sort(m_allJobs.begin(), m_allJobs.end(),
              [](const UploadWorker::Job &a, const UploadWorker::Job &b) { return a.no < b.no; });
    for (int i = 1; i < m_allJobs.size(); ++i) {
        if (m_allJobs[i].no == m_allJobs[i - 1].no) {
            appendLog(QStringLiteral("⚠ 章号重复：%1（%2）")
                          .arg(m_allJobs[i].no)
                          .arg(QFileInfo(m_allJobs[i].file).fileName()));
        }
    }

    // 范围控件跟着目录走：默认全选，用户再改成 41~80 这种子区间
    const int lo = m_allJobs.first().no;
    const int hi = m_allJobs.last().no;
    {
        const QSignalBlocker b1(m_fromSpin);
        const QSignalBlocker b2(m_toSpin);
        m_fromSpin->setRange(lo, hi);
        m_toSpin->setRange(lo, hi);
        m_fromSpin->setValue(lo);
        m_toSpin->setValue(hi);
    }

    qint64 total = 0;
    for (const auto &j : m_allJobs)
        total += j.body.size();
    appendLog(QStringLiteral("已载入 %1 章（第 %2 ~ %3 章），共 %4 字")
                  .arg(m_allJobs.size())
                  .arg(lo)
                  .arg(hi)
                  .arg(total));
    applyRange(true);
}

// 按「章号范围」从 m_allJobs 里筛出 m_jobs，再重排时间表
void MainWindow::applyRange(bool logIt)
{
    const int a = m_fromSpin->value();
    const int b = m_toSpin->value();
    const int lo = qMin(a, b);
    const int hi = qMax(a, b);

    const bool withNo = !m_titleWithNo || m_titleWithNo->isChecked();
    m_jobs.clear();
    for (const auto &j : m_allJobs) {
        if (j.no < lo || j.no > hi)
            continue;
        UploadWorker::Job job = j;
        if (withNo && !j.base.isEmpty())
            job.title = j.base;          // 27归队
        m_jobs.append(job);
    }

    if (m_allJobs.isEmpty()) {
        if (m_rangeInfo)
            m_rangeInfo->setText(QStringLiteral("全部"));
        reschedule();
        return;
    }
    if (m_rangeInfo) {
        const bool all = (lo <= m_allJobs.first().no && hi >= m_allJobs.last().no);
        m_rangeInfo->setText(all
                                 ? QStringLiteral("全部 · %1 章").arg(m_jobs.size())
                                 : QStringLiteral("第 %1~%2 章 · %3 章").arg(lo).arg(hi).arg(m_jobs.size()));
    }
    if (m_jobs.isEmpty()) {
        appendLog(QStringLiteral("⚠ 章号范围 %1~%2 里没有章节").arg(lo).arg(hi));
        reschedule();
        return;
    }
    if (logIt) {
        appendLog(QStringLiteral("本次上传 %1 章：第 %2 章 → 第 %3 章")
                      .arg(m_jobs.size())
                      .arg(m_jobs.first().no)
                      .arg(m_jobs.last().no));
    }
    reschedule();
}

void MainWindow::reschedule()
{
    m_table->clearSpans();
    if (m_jobs.isEmpty()) {
        showEmptyTable();
        m_summary->setText(QStringLiteral("未加载章节"));
        return;
    }
    const QDate start = m_dateEdit->date();
    const int perDay = qMax(1, m_perDay->value());
    const QTime first = m_timeEdit->time();
    const int gap = m_interval->value();

    for (int i = 0; i < m_jobs.size(); ++i) {
        const int day = i / perDay;
        const int slot = i % perDay;
        m_jobs[i].date = start.addDays(day);
        m_jobs[i].time = first.addSecs(slot * gap * 60).toString(QStringLiteral("HH:mm"));
    }

    m_table->setRowCount(m_jobs.size());
    for (int i = 0; i < m_jobs.size(); ++i) {
        const auto &j = m_jobs.at(i);
        auto *c0 = new QTableWidgetItem(QString::number(j.no));
        c0->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        auto *c1 = new QTableWidgetItem(j.title);
        auto *c2 = new QTableWidgetItem(QString::number(j.body.size()));
        c2->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *c3 = new QTableWidgetItem(QStringLiteral("%1 %2")
                                            .arg(j.date.toString(QStringLiteral("MM-dd")), j.time));
        c3->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_table->setItem(i, 0, c0);
        m_table->setItem(i, 1, c1);
        m_table->setItem(i, 2, c2);
        m_table->setItem(i, 3, c3);
    }
    m_table->scrollToTop();
    updateSummary();
}

void MainWindow::showEmptyTable()
{
    m_table->clearContents();
    m_table->clearSpans();
    m_table->setRowCount(1);
    m_table->setSpan(0, 0, 1, 4);
    auto *it = new QTableWidgetItem(
        QStringLiteral("还没有加载章节　·　先在左侧「章节源与排期」选择章节文件夹，再按需限定章号范围"));
    it->setTextAlignment(Qt::AlignCenter);
    it->setForeground(QColor(0x9A, 0xA3, 0xB0));
    m_table->setItem(0, 0, it);
    if (m_countLabel)
        m_countLabel->setText(QStringLiteral("空"));
}

void MainWindow::updateSummary()
{
    if (m_countLabel)
        m_countLabel->setText(m_jobs.isEmpty() ? QStringLiteral("空")
                                               : QStringLiteral("%1 章").arg(m_jobs.size()));
    if (m_jobs.isEmpty()) {
        m_summary->setText(QStringLiteral("未加载章节"));
        return;
    }
    const auto &last = m_jobs.last();
    const auto &first = m_jobs.first();
    const int days = (first.date.daysTo(last.date)) + 1;
    m_summary->setText(QStringLiteral("共 %1 章 · 跨 %2 天 · %3 %4 → %5 %6")
                           .arg(m_jobs.size())
                           .arg(days)
                           .arg(first.date.toString(QStringLiteral("yyyy-MM-dd")), first.time,
                                last.date.toString(QStringLiteral("yyyy-MM-dd")), last.time));
}

// ---- 执行 ---------------------------------------------------------------

void MainWindow::startUpload()
{
    if (m_jobs.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有章节"),
                                 m_allJobs.isEmpty()
                                     ? QStringLiteral("先选择章节文件夹。")
                                     : QStringLiteral("当前「章节范围」里没有章节，请调整起止章号。"));
        return;
    }
    if (!m_cdp->isConnected()) {
        QMessageBox::information(this, QStringLiteral("未连接"),
                                 QStringLiteral("先点「启动浏览器」，登录后连上再开始。"));
        return;
    }
    flushLog();

    if (!m_auto) {
        const auto &f = m_jobs.first();
        const auto &l = m_jobs.last();
        const QString msg =
            QStringLiteral("将上传 %1 章：第 %2 章 → 第 %3 章\n"
                           "定时发布：%4 %5 起，每天 %6 章，间隔 %7 分钟\n"
                           "最后一张：%8 %9\n\n"
                           "开始前请在浏览器里确认：已在「章节管理」页、分卷选择正确。")
                .arg(m_jobs.size())
                .arg(f.no)
                .arg(m_jobs.last().no)
                .arg(f.date.toString(QStringLiteral("yyyy-MM-dd")), f.time)
                .arg(m_perDay->value())
                .arg(m_interval->value())
                .arg(l.date.toString(QStringLiteral("yyyy-MM-dd")), l.time);
        if (QMessageBox::question(this, QStringLiteral("确认开始"), msg) != QMessageBox::Yes)
            return;
    }

    m_worker->setSiteScript(m_siteJs);
    m_worker->setJobs(m_jobs);
    m_worker->setBook(m_bookCombo->currentText().trimmed());
    m_worker->setVolume(m_volCombo->currentText().trimmed());
    m_worker->setStopOnError(m_stopOnErr->isChecked());

    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_progress->setMaximum(qMax(1, m_jobs.size()));
    m_progress->setValue(0);
    if (m_statusText)
        m_statusText->setText(QStringLiteral("正在上传　0 / %1").arg(m_jobs.size()));
    m_worker->start();
}

void MainWindow::stopUpload()
{
    m_worker->stop();
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    if (m_statusText)
        m_statusText->setText(QStringLiteral("已停止"));
}

// ---- 日志 ---------------------------------------------------------------

// 日志行着色：只按语义记号判断，不改日志内容
static QColor logLineColor(const QString &line)
{
    if (line.contains(QChar(0x2717)) || line.contains(QStringLiteral("失败"))
        || line.contains(QStringLiteral("✗")))
        return QColor(0xDC, 0x26, 0x26);
    if (line.contains(QChar(0x2713)) || line.contains(QStringLiteral("成功"))
        || line.contains(QStringLiteral("✓")))
        return QColor(0x0F, 0x7A, 0x3D);
    if (line.contains(QChar(0x26A0)) || line.contains(QStringLiteral("风险"))
        || line.contains(QStringLiteral("⚠")))
        return QColor(0xB4, 0x5D, 0x09);
    return QColor(0x55, 0x60, 0x70);
}

void MainWindow::appendLog(const QString &msg)
{
    m_logBuf << QStringLiteral("[%1] %2")
                    .arg(QTime::currentTime().toString(QStringLiteral("hh:mm:ss")), msg);
    if (m_logBuf.size() >= 200) {
        flushLog();
        return;
    }
    if (m_logTimer && !m_logTimer->isActive())
        m_logTimer->start();
}

void MainWindow::flushLog()
{
    if (m_logBuf.isEmpty())
        return;
    if (m_log) {
        // 按语义着色：✓ 成功绿、✗ 失败红、⚠ 风险琥珀，其余常规灰蓝。
        // 逐条 insertText（而不是 appendPlainText）才能带上 QTextCharFormat。
        // 行首的 [hh:mm:ss] 用更淡的颜色，让正文自己浮出来。
        bool emptyDoc = m_log->document()->isEmpty();
        QTextCursor cur(m_log->document());
        cur.movePosition(QTextCursor::End);
        for (const QString &line : m_logBuf) {
            if (!emptyDoc)
                cur.insertBlock();
            emptyDoc = false;
            QTextCharFormat fmt;
            fmt.setForeground(logLineColor(line));
            int stampEnd = 0;
            if (line.size() > 10 && line.startsWith(QLatin1Char('['))
                && line.at(9) == QLatin1Char(']'))
                stampEnd = 10;
            if (stampEnd > 0) {
                QTextCharFormat dim;
                dim.setForeground(QColor(0xA8, 0xB1, 0xBE));
                cur.insertText(line.left(stampEnd), dim);
                cur.insertText(line.mid(stampEnd), fmt);
            } else {
                cur.insertText(line, fmt);
            }
        }
        if (auto *sb = m_log->verticalScrollBar())
            sb->setValue(sb->maximum());
    }
    m_logBuf.clear();
    if (m_logTimer)
        m_logTimer->stop();
}

// ---- 设置 ---------------------------------------------------------------

void MainWindow::saveSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("fanqie"));
    const QString book = m_bookCombo->currentText().trimmed();
    const QString vol  = m_volCombo->currentText().trimmed();
    s.setValue(QStringLiteral("book"), saneName(book) ? book : QString());
    s.setValue(QStringLiteral("volume"), saneName(vol) ? vol : QString());
    s.setValue(QStringLiteral("dir"), m_dirEdit->text());
    s.setValue(QStringLiteral("date"), m_dateEdit->date());
    s.setValue(QStringLiteral("perDay"), m_perDay->value());
    s.setValue(QStringLiteral("time"), m_timeEdit->time());
    s.setValue(QStringLiteral("interval"), m_interval->value());
    s.setValue(QStringLiteral("titleWithNo"), m_titleWithNo->isChecked());
    s.setValue(QStringLiteral("from"), m_fromSpin->value());
    s.setValue(QStringLiteral("to"), m_toSpin->value());
    // 窗口尺寸/位置与右列分栏比例：正式软件的标配记忆
    s.setValue(QStringLiteral("geometry"), saveGeometry());
    if (m_split)
        s.setValue(QStringLiteral("splitter"), m_split->saveState());
    s.endGroup();
}

void MainWindow::loadSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("fanqie"));
    const QString dir = s.value(QStringLiteral("dir")).toString();
    const QString book = s.value(QStringLiteral("book")).toString();
    const QString vol = s.value(QStringLiteral("volume")).toString();
    if (saneName(book))
        m_bookCombo->setCurrentText(book);
    if (saneName(vol))
        m_volCombo->setCurrentText(vol);
    if (!dir.isEmpty())
        m_dirEdit->setText(dir);
    const QDate d = s.value(QStringLiteral("date")).toDate();
    if (d.isValid())
        m_dateEdit->setDate(d);
    m_perDay->setValue(s.value(QStringLiteral("perDay"), 3).toInt());
    const QTime t = s.value(QStringLiteral("time")).toTime();
    if (t.isValid())
        m_timeEdit->setTime(t);
    m_interval->setValue(s.value(QStringLiteral("interval"), 5).toInt());
    {
        const QSignalBlocker b(m_titleWithNo);
        m_titleWithNo->setChecked(s.value(QStringLiteral("titleWithNo"), true).toBool());
    }
    {
        const QByteArray geo = s.value(QStringLiteral("geometry")).toByteArray();
        if (!geo.isEmpty())
            restoreGeometry(geo);
        const QByteArray sp = s.value(QStringLiteral("splitter")).toByteArray();
        if (m_split && !sp.isEmpty())
            m_split->restoreState(sp);
    }
    s.endGroup();

    if (!dir.isEmpty() && QDir(dir).exists()) {
        loadChapters(dir);
        // 目录载入会把范围重置成 1~N，这里再恢复上次选的区间
        const int f = s.value(QStringLiteral("from"), 0).toInt();
        const int t = s.value(QStringLiteral("to"), 0).toInt();
        if (f > 0 && t > 0 && !m_allJobs.isEmpty()) {
            const QSignalBlocker b1(m_fromSpin);
            const QSignalBlocker b2(m_toSpin);
            m_fromSpin->setValue(qBound(m_fromSpin->minimum(), f, m_fromSpin->maximum()));
            m_toSpin->setValue(qBound(m_toSpin->minimum(), t, m_toSpin->maximum()));
            applyRange(false);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    flushLog();
    saveSettings();
    m_worker->stop();
    QMainWindow::closeEvent(e);
}

void MainWindow::runAutoTest(const QString &shot)
{
    m_auto = true;
    const QString dir = qEnvironmentVariable("CU_DIR");
    if (!dir.isEmpty()) {
        m_dirEdit->setText(QDir::toNativeSeparators(dir));
        loadChapters(dir);
    }
    // 自测用：CU_FROM / CU_TO 可验证章节范围筛选
    const int f = qEnvironmentVariableIntValue("CU_FROM");
    const int t = qEnvironmentVariableIntValue("CU_TO");
    if (f > 0 && t > 0 && !m_allJobs.isEmpty()) {
        const QSignalBlocker b1(m_fromSpin);
        const QSignalBlocker b2(m_toSpin);
        m_fromSpin->setValue(qBound(m_fromSpin->minimum(), f, m_fromSpin->maximum()));
        m_toSpin->setValue(qBound(m_toSpin->minimum(), t, m_toSpin->maximum()));
        applyRange(true);
    }
    if (!qEnvironmentVariable("CU_RUN").isEmpty()) {
        appendLog(QStringLiteral("CU_RUN：自动连接已开着的浏览器并开始上传（CU_DRY 时只填表）"));
        connect(m_cdp, &CdpClient::connected, this, [this]() {
            QTimer::singleShot(700, this, [this]() {
                if (m_startBtn->isEnabled())
                    m_startBtn->click();
            });
        });
        const QString runShot = qEnvironmentVariable("CU_SHOT");
        connect(m_worker, &UploadWorker::finished, this, [this, runShot](int ok, int fail) {
            flushLog();
            appendLog(QStringLiteral("CU_RUN 结束：成功 %1　失败 %2").arg(ok).arg(fail));
            if (!runShot.isEmpty())
                grab().save(runShot);
            // CU_KEEP：结束也不退出，把窗口和浏览器留在原地，方便用 CDP 接着抓页面
            if (qEnvironmentVariableIsSet("CU_KEEP"))
                return;
            QTimer::singleShot(400, qApp, &QApplication::quit);
        });
        // CU_LAUNCH：走用户真实路径——点「启动浏览器」自己拉起 Edge 再连接
        // （不设则复用已经开着的浏览器）
        if (qEnvironmentVariableIsSet("CU_LAUNCH")) {
            QTimer::singleShot(600, this, [this]() { toggleConnection(); });
            return;
        }
        m_cdp->discoverAndConnect();
        return;
    }

    QTimer::singleShot(900, this, [this, shot]() {
        if (!shot.isEmpty()) {
            const QPixmap pm = grab();
            pm.save(shot);
            appendLog(QStringLiteral("自测截图已保存：%1").arg(shot));
        }
        flushLog();
        QTimer::singleShot(150, qApp, &QApplication::quit);
    });
}
