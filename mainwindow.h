#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QMainWindow>
#include <QString>
#include <QTimer>

#include "uploadworker.h"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimeEdit;

class CdpClient;
class BrowserLauncher;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 无人值守自测：CU_SELFTEST=1 + CU_SHOT=截图路径 → 截图后自动退出
    void runAutoTest(const QString &shot);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void toggleConnection();
    void refreshBooks();
    void refreshVolumes();
    void pickFolder();
    void startUpload();
    void stopUpload();

private:
    void buildUi();
    void loadSiteScript();
    void setConnected(bool on, const QString &text);
    void loadChapters(const QString &dir);
    void applyRange(bool logIt);
    void reschedule();
    void updateSummary();
    void showEmptyTable();
    void appendLog(const QString &msg);
    void flushLog();
    void saveSettings();
    void loadSettings();

    CdpClient      *m_cdp = nullptr;
    BrowserLauncher *m_browser = nullptr;
    UploadWorker   *m_worker = nullptr;
    QString         m_siteJs;
    QList<UploadWorker::Job> m_allJobs;   // 目录里的全部章节
    QList<UploadWorker::Job> m_jobs;      // 按「章号范围」筛选后要上传的

    // 连接
    QLabel *m_dot = nullptr;
    QLabel *m_connText = nullptr;
    QPushButton *m_connBtn = nullptr;
    QPushButton *m_probeBtn = nullptr;

    // 作品
    QComboBox *m_bookCombo = nullptr;
    QComboBox *m_volCombo = nullptr;
    QPushButton *m_refreshBtn = nullptr;

    // 章节源
    QLineEdit *m_dirEdit = nullptr;
    QPushButton *m_browseBtn = nullptr;
    QSpinBox  *m_fromSpin = nullptr;
    QSpinBox  *m_toSpin = nullptr;
    QLabel    *m_rangeInfo = nullptr;
    QDateEdit *m_dateEdit = nullptr;
    QSpinBox  *m_perDay = nullptr;
    QTimeEdit *m_timeEdit = nullptr;
    QSpinBox  *m_interval = nullptr;

    // 执行
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QCheckBox   *m_stopOnErr = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel      *m_summary = nullptr;

    QTableWidget *m_table = nullptr;
    QPlainTextEdit *m_log = nullptr;

    QStringList m_logBuf;
    QTimer *m_logTimer = nullptr;
    bool m_auto = false;
};

#endif // MAINWINDOW_H
