#pragma once

#include <QDialog>
#include <vector>
#include "builder.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

    void setEquicordPath(const QString& path);
    QString equicordPath() const;
    void setDiscordBranch(const QString& branch);
    QString discordBranch() const;
    void setDiscordChoices(const std::vector<builder::DiscordChannel>& channels);

private slots:
    void onBrowseEquicord();

private:
    Ui::SettingsDialog* ui = nullptr;
    QString pendingEquicordPath_;
};
