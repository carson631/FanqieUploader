#include "mainwindow.h"

#include "browserlauncher.h"
#include "cdpclient.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
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
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTimeEdit>
#include <QVBoxLayout>

#include <functional>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static const char *kStyle = R"QSS(
QWidget#appBg { background:#F5F6F8; }

QFrame#appHeader {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #FF7A45, stop:1 #D93A0B);
    border:0;
}
QLabel#appTitle    { color:#FFFFFF; font-size:19px; font-weight:700; letter-spacing:1px; }
QLabel#appSubtitle { color:rgba(255,255,255,0.86); font-size:12px; }
QLabel#verTag {
    color:rgba(255,255,255,0.94); border:1px solid rgba(255,255,255,0.5);
    border-radius:10px; padding:2px 10px; font-size:11px;
}

QFrame#card { background:#FFFFFF; border:1px solid #E6E8EC; border-radius:12px; }
QLabel#section { color:#1F2937; font-size:13px; font-weight:700; }
QLabel#hint { color:#8A93A3; font-size:11px; }
QLabel#stat { color:#5B6577; font-size:12px; }

QLabel { color:#25313F; font-size:13px; }

QLineEdit, QComboBox, QDateEdit, QTimeEdit, QSpinBox {
    background:#FBFCFD; border:1px solid #DFE3EA; border-radius:8px;
    padding:5px 9px; font-size:13px; color:#22303F;
    selection-background-color:#FFD9C7;
}
QLineEdit:focus, QComboBox:focus, QDateEdit:focus, QTimeEdit:focus, QSpinBox:focus {
    border:1px solid #FF6B35; background:#FFFFFF;
}
QComboBox::drop-down { border:0; width:22px; }
QComboBox::down-arrow { image:url(:/icons/chevron.png); width:11px; height:11px; }
QComboBox QAbstractItemView {
    background:#FFFFFF; border:1px solid #E6E8EC; outline:0;
    selection-background-color:#FFF0E8; selection-color:#D93A0B;
}
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width:0; border:0; }
QDateEdit::drop-down { border:0; width:22px; }
QDateEdit::down-arrow { image:url(:/icons/chevron.png); width:10px; height:10px; }

QCheckBox::indicator {
    width:15px; height:15px; border:1px solid #D3D9E2; border-radius:4px; background:#FFFFFF;
}
QCheckBox::indicator:hover { border-color:#FFB08C; }
QCheckBox::indicator:checked {
    background:#FF6B35; border-color:#FF6B35; image:url(:/icons/check.png);
}

QPushButton {
    background:#FFFFFF; border:1px solid #DFE3EA; border-radius:8px;
    padding:6px 14px; font-size:13px; color:#2A3A50;
}
QPushButton:hover  { background:#FFF7F2; border-color:#FFC3A6; }
QPushButton:pressed { background:#FFEDE3; }
QPushButton:disabled { color:#AEB6C4; background:#F5F6F8; border-color:#E6E8EC; }

QPushButton#primary {
    background:#FF6B35; border:1px solid #FF6B35; color:#FFFFFF;
    font-weight:600; padding:7px 20px;
}
QPushButton#primary:hover   { background:#F05A22; border-color:#F05A22; }
QPushButton#primary:pressed { background:#D94A15; }
QPushButton#primary:disabled { background:#FFC9B2; border-color:#FFC9B2; color:#FFFFFF; }

QPushButton#danger { color:#C2410C; border-color:#F3C7B4; }
QPushButton#danger:hover { background:#FFF3EC; }

QTableWidget {
    background:#FFFFFF; border:1px solid #EAEDF2; border-radius:10px;
    gridline-color:transparent;
    selection-background-color:#FFF0E8; selection-color:#1B2A3D;
    alternate-background-color:#FCFCFD;
}
QHeaderView::section {
    background:#F7F8FA; color:#5B6577; border:0;
    border-bottom:1px solid #EAEDF2; padding:7px 8px;
    font-size:12px; font-weight:600;
}
QTableCornerButton::section { background:#F7F8FA; border:0; }

QProgressBar { background:#EDEFF3; border:0; border-radius:5px; height:10px; }
QProgressBar::chunk { background:#FF6B35; border-radius:5px; }

QPlainTextEdit#log {
    background:#FBFCFD; border:1px solid #EAEDF2; border-radius:10px;
    color:#3B4A5E; font-family:"Consolas","Courier New"; font-size:12px; padding:6px;
}

QCheckBox { color:#25313F; font-size:13px; spacing:7px; }

QScrollBar:vertical { background:transparent; width:10px; margin:2px; }
QScrollBar::handle:vertical { background:#D2D7E0; border-radius:5px; min-height:24px; }
QScrollBar::handle:vertical:hover { background:#B9C1CF; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
QScrollBar:horizontal { background:transparent; height:10px; margin:2px; }
QScrollBar::handle:horizontal { background:#D2D7E0; border-radius:5px; min-width:24px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }

QSplitter::handle { background:transparent; height:8px; }
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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("番茄章节上传工具"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    setStyleSheet(QString::fromUtf8(kStyle));
    resize(1200, 800);

    loadSiteScript();

    m_cdp = new CdpClient(this);
    m_cdp->setPort(BrowserLauncher::defaultPort());
    m_browser = new BrowserLauncher(this);
    m_worker = new UploadWorker(m_cdp, this);
    m_worker->setSiteScript(m_siteJs);

    buildUi();

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
        if (total > 0)
            m_summary->setText(QStringLiteral("进度 %1 / %2").arg(done).arg(total));
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
    });

    loadSettings();
    setConnected(false, QStringLiteral("未连接"));
    appendLog(QStringLiteral("就绪。首次使用：点「启动浏览器」，在弹出的浏览器里登录番茄作家账号。"));
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

static QFrame *makeCard(const QString &titleText, QVBoxLayout **out)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("card"));
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(8);
    if (!titleText.isEmpty()) {
        auto *t = new QLabel(titleText);
        t->setObjectName(QStringLiteral("section"));
        v->addWidget(t);
    }
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

    // 品牌头
    auto *header = new QFrame;
    header->setObjectName(QStringLiteral("appHeader"));
    header->setFixedHeight(72);
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(22, 10, 22, 10);
    auto *tb = new QVBoxLayout;
    tb->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("番茄章节上传工具"));
    title->setObjectName(QStringLiteral("appTitle"));
    auto *sub = new QLabel(QStringLiteral("番茄小说网 · 作家后台　｜　自动建章 → 纠错 → 全面检测 → 定时发布"));
    sub->setObjectName(QStringLiteral("appSubtitle"));
    tb->addWidget(title);
    tb->addWidget(sub);
    hl->addLayout(tb);
    hl->addStretch(1);
    auto *ver = new QLabel(QStringLiteral("v1.0.1"));
    ver->setObjectName(QStringLiteral("verTag"));
    hl->addWidget(ver, 0, Qt::AlignVCenter);
    root->addWidget(header);

    // 主体
    auto *body = new QWidget;
    auto *bl = new QHBoxLayout(body);
    bl->setContentsMargins(16, 14, 16, 14);
    bl->setSpacing(14);

    // ===== 左列 =====
    auto *left = new QWidget;
    left->setFixedWidth(432);
    auto *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(12);

    // ① 连接
    QVBoxLayout *c1 = nullptr;
    auto *card1 = makeCard(QStringLiteral("① 连接浏览器"), &c1);
    auto *row1 = new QHBoxLayout;
    row1->setSpacing(8);
    m_dot = new QLabel;
    m_dot->setFixedSize(8, 8);
    m_connText = new QLabel(QStringLiteral("未连接"));
    m_connText->setObjectName(QStringLiteral("stat"));
    row1->addWidget(m_dot);
    row1->addWidget(m_connText);
    row1->addStretch(1);
    m_probeBtn = new QPushButton(QStringLiteral("勘察页面"));
    m_connBtn = new QPushButton(QStringLiteral("启动浏览器"));
    m_connBtn->setObjectName(QStringLiteral("primary"));
    row1->addWidget(m_probeBtn);
    row1->addWidget(m_connBtn);
    c1->addLayout(row1);
    auto *h1 = new QLabel(QStringLiteral("登录态保存在独立 profile，只需登录一次；不会影响你日常用的浏览器。"));
    h1->setObjectName(QStringLiteral("hint"));
    h1->setWordWrap(true);
    c1->addWidget(h1);
    connect(m_connBtn, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(m_probeBtn, &QPushButton::clicked, this, [this]() {
        if (!m_cdp->isConnected()) {
            appendLog(QStringLiteral("还没连接，先点「启动浏览器」"));
            return;
        }
        m_worker->probePage();
    });

    // ② 作品
    QVBoxLayout *c2 = nullptr;
    auto *card2 = makeCard(QStringLiteral("② 作品与分卷"), &c2);
    auto *f2 = new QGridLayout;
    f2->setHorizontalSpacing(8);
    f2->setVerticalSpacing(8);
    f2->addWidget(new QLabel(QStringLiteral("书名")), 0, 0);
    m_bookCombo = new QComboBox;
    m_bookCombo->setEditable(true);
    m_bookCombo->setMinimumWidth(240);
    f2->addWidget(m_bookCombo, 0, 1);
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"));
    f2->addWidget(m_refreshBtn, 0, 2);
    f2->addWidget(new QLabel(QStringLiteral("分卷")), 1, 0);
    m_volCombo = new QComboBox;
    m_volCombo->setEditable(true);
    f2->addWidget(m_volCombo, 1, 1, 1, 2);
    f2->setColumnStretch(1, 1);
    c2->addLayout(f2);
    auto *h2 = new QLabel(QStringLiteral("刷新会读取后台的书架与分卷；也可手动填写书名/卷名。"));
    h2->setObjectName(QStringLiteral("hint"));
    h2->setWordWrap(true);
    c2->addWidget(h2);
    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        refreshBooks();
        refreshVolumes();
    });

    // ③ 章节源
    QVBoxLayout *c3 = nullptr;
    auto *card3 = makeCard(QStringLiteral("③ 章节源与排期"), &c3);
    auto *rowd = new QHBoxLayout;
    rowd->setSpacing(8);
    m_dirEdit = new QLineEdit;
    m_dirEdit->setPlaceholderText(QStringLiteral("选择「每章一个 txt」的文件夹"));
    m_browseBtn = new QPushButton(QStringLiteral("选择"));
    rowd->addWidget(m_dirEdit, 1);
    rowd->addWidget(m_browseBtn);
    c3->addLayout(rowd);

    auto *f3 = new QGridLayout;
    f3->setHorizontalSpacing(8);
    f3->setVerticalSpacing(8);

    // 章节范围：只上传目录里指定章号区间的部分（默认全选）
    f3->addWidget(new QLabel(QStringLiteral("章节范围")), 0, 0);
    auto *rowRange = new QHBoxLayout;
    rowRange->setSpacing(6);
    m_fromSpin = new QSpinBox;
    m_fromSpin->setRange(0, 99999);
    m_fromSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_fromSpin->setAlignment(Qt::AlignCenter);
    m_fromSpin->setFixedWidth(84);
    m_toSpin = new QSpinBox;
    m_toSpin->setRange(0, 99999);
    m_toSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_toSpin->setAlignment(Qt::AlignCenter);
    m_toSpin->setFixedWidth(84);
    rowRange->addWidget(new QLabel(QStringLiteral("第")));
    rowRange->addWidget(m_fromSpin);
    rowRange->addWidget(new QLabel(QStringLiteral("章　至　第")));
    rowRange->addWidget(m_toSpin);
    rowRange->addWidget(new QLabel(QStringLiteral("章")));
    m_rangeInfo = new QLabel(QStringLiteral("全部"));
    m_rangeInfo->setObjectName(QStringLiteral("hint"));
    rowRange->addSpacing(6);
    rowRange->addWidget(m_rangeInfo);
    rowRange->addStretch(1);
    f3->addLayout(rowRange, 0, 1, 1, 3);

    f3->addWidget(new QLabel(QStringLiteral("首章日期")), 1, 0);
    m_dateEdit = new QDateEdit;
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_dateEdit->setDate(QDate::currentDate().addDays(1));
    f3->addWidget(m_dateEdit, 1, 1);
    f3->addWidget(new QLabel(QStringLiteral("每天")), 1, 2);
    m_perDay = new QSpinBox;
    m_perDay->setRange(1, 20);
    m_perDay->setValue(3);
    m_perDay->setSuffix(QStringLiteral(" 章"));
    m_perDay->setButtonSymbols(QAbstractSpinBox::NoButtons);
    f3->addWidget(m_perDay, 1, 3);
    f3->addWidget(new QLabel(QStringLiteral("首发时间")), 2, 0);
    m_timeEdit = new QTimeEdit(QTime(19, 0));
    m_timeEdit->setDisplayFormat(QStringLiteral("HH:mm"));
    m_timeEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    f3->addWidget(m_timeEdit, 2, 1);
    f3->addWidget(new QLabel(QStringLiteral("间隔")), 2, 2);
    m_interval = new QSpinBox;
    m_interval->setRange(0, 120);
    m_interval->setValue(5);
    m_interval->setSuffix(QStringLiteral(" 分钟"));
    m_interval->setButtonSymbols(QAbstractSpinBox::NoButtons);
    f3->addWidget(m_interval, 2, 3);
    f3->setColumnStretch(1, 1);
    f3->setColumnStretch(3, 1);
    c3->addLayout(f3);

    auto *h3 = new QLabel(QStringLiteral(
        "排期规则：每天 3 章 → 19:00 / 19:05 / 19:10；第 4 章自动顺延到次日 19:00。"
        "章节范围按文件名里的章号筛选，例如 41 至 80 就只传这 40 章。"));
    h3->setObjectName(QStringLiteral("hint"));
    h3->setWordWrap(true);
    c3->addWidget(h3);

    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::pickFolder);
    connect(m_fromSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { applyRange(false); });
    connect(m_toSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { applyRange(false); });
    connect(m_perDay, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { reschedule(); });
    connect(m_interval, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { reschedule(); });
    connect(m_dateEdit, &QDateEdit::dateChanged, this, [this](const QDate &) { reschedule(); });
    connect(m_timeEdit, &QTimeEdit::timeChanged, this, [this](const QTime &) { reschedule(); });

    // ④ 执行
    QVBoxLayout *c4 = nullptr;
    auto *card4 = makeCard(QStringLiteral("④ 执行"), &c4);
    auto *row4 = new QHBoxLayout;
    row4->setSpacing(8);
    m_startBtn = new QPushButton(QStringLiteral("开始上传"));
    m_startBtn->setObjectName(QStringLiteral("primary"));
    m_stopBtn = new QPushButton(QStringLiteral("停止"));
    m_stopBtn->setObjectName(QStringLiteral("danger"));
    m_stopBtn->setEnabled(false);
    m_stopOnErr = new QCheckBox(QStringLiteral("出错即停"));
    m_stopOnErr->setChecked(true);
    row4->addWidget(m_startBtn);
    row4->addWidget(m_stopBtn);
    row4->addStretch(1);
    row4->addWidget(m_stopOnErr);
    c4->addLayout(row4);
    m_progress = new QProgressBar;
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(10);
    c4->addWidget(m_progress);
    m_summary = new QLabel(QStringLiteral("未加载章节"));
    m_summary->setObjectName(QStringLiteral("stat"));
    c4->addWidget(m_summary);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::startUpload);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::stopUpload);
    connect(m_stopOnErr, &QCheckBox::toggled, this, [this](bool on) {
        m_worker->setStopOnError(on);
    });

    lv->addWidget(card1);
    lv->addWidget(card2);
    lv->addWidget(card3);
    lv->addWidget(card4);
    lv->addStretch(1);

    auto *foot = new QLabel(QStringLiteral("只在本机驱动浏览器，不接触你的账号密码"));
    foot->setObjectName(QStringLiteral("hint"));
    foot->setWordWrap(true);
    lv->addWidget(foot);

    // ===== 右列 =====
    auto *right = new QSplitter(Qt::Vertical);
    right->setChildrenCollapsible(false);

    auto *tCard = new QFrame;
    tCard->setObjectName(QStringLiteral("card"));
    auto *tv = new QVBoxLayout(tCard);
    tv->setContentsMargins(12, 10, 12, 12);
    tv->setSpacing(8);
    auto *tHead = new QHBoxLayout;
    auto *tTitle = new QLabel(QStringLiteral("章节与排期"));
    tTitle->setObjectName(QStringLiteral("section"));
    tHead->addWidget(tTitle);
    tHead->addStretch(1);
    auto *tHint = new QLabel(QStringLiteral("定时时间按上面的规则自动计算，可手工改日期/时间后重新生成"));
    tHint->setObjectName(QStringLiteral("hint"));
    tHead->addWidget(tHint);
    tv->addLayout(tHead);

    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("章号"), QStringLiteral("标题（取自文件名）"),
                                        QStringLiteral("字数"), QStringLiteral("定时发布")});
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 64);
    m_table->setColumnWidth(2, 80);
    m_table->setColumnWidth(3, 150);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(28);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    tv->addWidget(m_table, 1);

    auto *logCard = new QFrame;
    logCard->setObjectName(QStringLiteral("card"));
    auto *lgv = new QVBoxLayout(logCard);
    lgv->setContentsMargins(12, 10, 12, 12);
    lgv->setSpacing(8);
    auto *lgHead = new QHBoxLayout;
    auto *lgTitle = new QLabel(QStringLiteral("运行日志"));
    lgTitle->setObjectName(QStringLiteral("section"));
    lgHead->addWidget(lgTitle);
    lgHead->addStretch(1);
    auto *clearBtn = new QPushButton(QStringLiteral("清空"));
    lgHead->addWidget(clearBtn);
    lgv->addLayout(lgHead);

    m_log = new QPlainTextEdit;
    m_log->setObjectName(QStringLiteral("log"));
    m_log->setReadOnly(true);
    m_log->setUndoRedoEnabled(false);
    m_log->setMaximumBlockCount(4000);
    lgv->addWidget(m_log, 1);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        flushLog();
        m_log->clear();
    });

    right->addWidget(tCard);
    right->addWidget(logCard);
    right->setStretchFactor(0, 3);
    right->setStretchFactor(1, 2);

    bl->addWidget(left);
    bl->addWidget(right, 1);
    root->addWidget(body, 1);

    m_logTimer = new QTimer(this);
    m_logTimer->setInterval(120);
    m_logTimer->setSingleShot(true);
    connect(m_logTimer, &QTimer::timeout, this, &MainWindow::flushLog);

    // 卡片投影
    for (QFrame *c : {card1, card2, card3, card4}) {
        auto *sh = new QGraphicsDropShadowEffect(c);
        sh->setBlurRadius(18);
        sh->setOffset(0, 3);
        sh->setColor(QColor(0, 0, 0, 18));
        c->setGraphicsEffect(sh);
    }

    showEmptyTable();
}

void MainWindow::setConnected(bool on, const QString &text)
{
    m_connText->setText(text);
    m_dot->setStyleSheet(on ? QStringLiteral("background:#16A34A;border-radius:4px;")
                            : QStringLiteral("background:#C7D0DE;border-radius:4px;"));
    m_connBtn->setText(on ? QStringLiteral("断开") : QStringLiteral("启动浏览器"));
    m_probeBtn->setEnabled(on);
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
    const QString expr = QStringLiteral("(function(){if(!window.__fq){") + m_siteJs
                         + QStringLiteral("}return (__fq.books());})()");
    m_cdp->evaluate(expr, [this](bool ok, const QJsonValue &v, const QString &err) {
        if (!ok) {
            appendLog(QStringLiteral("读书架失败：%1").arg(err));
            return;
        }
        const QStringList list = [&v]() {
            QStringList out;
            for (const QJsonValue &x : v.toArray())
                out << x.toString();
            return out;
        }();
        if (list.isEmpty()) {
            appendLog(QStringLiteral("没读到书名——如果当前在章节管理页，先点上方的「番茄小说网」回到作品管理页再刷新"));
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

    m_jobs.clear();
    for (const auto &j : m_allJobs) {
        if (j.no >= lo && j.no <= hi)
            m_jobs.append(j);
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
        QStringLiteral("还没有加载章节　·　先在左侧「③ 章节源与排期」选择章节文件夹，再按需限定章号范围"));
    it->setTextAlignment(Qt::AlignCenter);
    it->setForeground(QColor(0x9A, 0xA3, 0xB2));
    m_table->setItem(0, 0, it);
}

void MainWindow::updateSummary()
{
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
    m_worker->start();
}

void MainWindow::stopUpload()
{
    m_worker->stop();
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}

// ---- 日志 ---------------------------------------------------------------

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
    if (m_log)
        m_log->appendPlainText(m_logBuf.join(QLatin1Char('\n')));
    m_logBuf.clear();
    if (m_logTimer)
        m_logTimer->stop();
}

// ---- 设置 ---------------------------------------------------------------

void MainWindow::saveSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("fanqie"));
    s.setValue(QStringLiteral("book"), m_bookCombo->currentText());
    s.setValue(QStringLiteral("volume"), m_volCombo->currentText());
    s.setValue(QStringLiteral("dir"), m_dirEdit->text());
    s.setValue(QStringLiteral("date"), m_dateEdit->date());
    s.setValue(QStringLiteral("perDay"), m_perDay->value());
    s.setValue(QStringLiteral("time"), m_timeEdit->time());
    s.setValue(QStringLiteral("interval"), m_interval->value());
    s.setValue(QStringLiteral("from"), m_fromSpin->value());
    s.setValue(QStringLiteral("to"), m_toSpin->value());
    s.endGroup();
}

void MainWindow::loadSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("fanqie"));
    const QString dir = s.value(QStringLiteral("dir")).toString();
    m_bookCombo->setCurrentText(s.value(QStringLiteral("book")).toString());
    m_volCombo->setCurrentText(s.value(QStringLiteral("volume")).toString());
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
