#pragma once

#include <QMainWindow>
#include <atomic>
#include <string>
#include <vector>

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QFrame;
class QVBoxLayout;
class QHBoxLayout;
class QTextEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void initialize();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onBrowseEquicord();
    void onBrowsePlugin();
    void onInstallClicked();
    void onSourceChanged();

    void applyEquicordState(const QString& path, bool isValid, const QString& message);
    void applyStatus(const QString& message, bool isError, bool isWarning);
    void applyBusy(bool busy, const QString& label);
    void applyProgress(int percent);

private:
    void buildMenuBar();
    void buildCentralWidget();
    void buildDropZone();
    void buildStatusBar();
    void setupAccessibleNames();
    void setupTabOrder();
    void setDropHighlight(bool on);
    void updateInstallButton();
    bool ensurePnpmAvailable();
    bool autoInstallNodeAndPnpm();
    bool closeDiscord(const std::string& branch);
    bool reopenDiscord(const std::string& branch);
    std::string pickEquicordFolder();
    std::string pickPluginPathSingleDialog();
    std::string workBlocker();
    void applyEquicordPath(const std::string& folder);
    void setPendingPlugin(const std::string& path);
    void setPendingPlugins(const std::vector<std::string>& paths);
    void startInstallFlow();
    void refreshLog();

    QWidget* central_ = nullptr;
    QVBoxLayout* rootLayout_ = nullptr;

    QFrame* banner_ = nullptr;
    QLabel* bannerIcon_ = nullptr;
    QLabel* bannerLabel_ = nullptr;
    QPushButton* bannerChangeBtn_ = nullptr;

    QLineEdit* linkEdit_ = nullptr;

    QFrame* dropFrame_ = nullptr;
    QLabel* dropTitle_ = nullptr;
    QLabel* pendingLabel_ = nullptr;
    QPushButton* dropBrowseBtn_ = nullptr;

    QHBoxLayout* footerBar_ = nullptr;
    QPushButton* installButton_ = nullptr;

    QLabel* statusIcon_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QProgressBar* topProgress_ = nullptr;
    QTextEdit* logView_ = nullptr;
    QPushButton* logToggleBtn_ = nullptr;
    QString fullPendingPath_;
    std::vector<std::string> pendingPlugins_;
    std::atomic<bool> jobRunning_{false};
};
