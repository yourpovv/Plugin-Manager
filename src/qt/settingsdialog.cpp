#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <QFileDialog>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::SettingsDialog) {
    ui->setupUi(this);
    setStyleSheet(
        "QDialog { background:#1e1f22; }"
        "QLabel { color:#b5bac1; }"
        "QLineEdit { background:#18191c; color:#f2f3f5; border:1px solid #3f4147; border-radius:8px; padding:6px 10px; }"
        "QComboBox { background:#18191c; color:#f2f3f5; border:1px solid #3f4147; border-radius:8px; padding:6px 10px; }"
        "QComboBox:focus { border:1px solid #5865f2; }"
        "QComboBox QAbstractItemView { background:#2b2d31; color:#f2f3f5; selection-background-color:#404249; border:1px solid #3f4147; }"
        "QPushButton { background:#2b2d31; color:#f2f3f5; border:1px solid #3f4147; border-radius:8px; padding:6px 16px; min-width:80px; }"
        "QPushButton[primary=\"true\"] { background:#5865f2; color:#fff; border:none; }"
        "QPushButton[primary=\"true\"]:hover { background:#4752c4; }"
    );
    ui->equicordEdit->setReadOnly(true);
    ui->equicordLabel->setBuddy(ui->equicordEdit);
    ui->discordLabel->setBuddy(ui->discordCombo);
    connect(ui->equicordBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseEquicord);
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

void SettingsDialog::setEquicordPath(const QString& path) {
    pendingEquicordPath_ = path;
    ui->equicordEdit->setText(path);
    ui->equicordEdit->setToolTip(path);
}

QString SettingsDialog::equicordPath() const {
    return pendingEquicordPath_.isEmpty() ? ui->equicordEdit->text().trimmed() : pendingEquicordPath_;
}

void SettingsDialog::setDiscordBranch(const QString& branch) {
    for (int i=0;i<ui->discordCombo->count();++i) {
        if (ui->discordCombo->itemData(i).toString()==branch) {
            ui->discordCombo->setCurrentIndex(i);
            return;
        }
    }
}

QString SettingsDialog::discordBranch() const {
    return ui->discordCombo->currentData().toString();
}

void SettingsDialog::setDiscordChoices(const std::vector<builder::DiscordChannel>& channels) {
    ui->discordCombo->clear();
    for (auto& c : channels) {
        QString lbl = QString::fromUtf8(c.label.c_str());
        if (!c.installed) lbl += " (not installed)";
        else if (c.patched) lbl += " (patched)";
        ui->discordCombo->addItem(lbl, QString::fromUtf8(c.branch.c_str()));
    }
}

void SettingsDialog::onBrowseEquicord() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select your Equicord folder", pendingEquicordPath_);
    if (!dir.isEmpty()) {
        pendingEquicordPath_ = dir;
        ui->equicordEdit->setText(dir);
        ui->equicordEdit->setToolTip(dir);
    }
}
