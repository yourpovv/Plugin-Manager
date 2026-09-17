#pragma once

#include <QWidget>
#include <string>
#include <vector>
#include "plugin.h"

class QCheckBox;
class QLabel;
class QPushButton;

class PluginWidget : public QWidget {
    Q_OBJECT
public:
    explicit PluginWidget(const PluginStatus& status, QWidget* parent = nullptr);
    ~PluginWidget() override = default;

    std::string pluginId() const;
    bool isChecked() const;
    void setChecked(bool on);
    PluginStatus status() const;
    bool matchesFilter(const std::string& query, bool installedOnly, bool updatesOnly) const;

signals:
    void installRequested(const std::string& pluginId);
    void removeRequested(const std::string& pluginId);
    void selectionToggled(const std::string& pluginId, bool checked);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildMetaText();
    void updateElidedTexts();

    PluginStatus status_;
    QCheckBox* checkBox_ = nullptr;
    QLabel* nameLabel_ = nullptr;
    QLabel* descLabel_ = nullptr;
    QLabel* metaLabel_ = nullptr;
    QLabel* pillLabel_ = nullptr;
    QPushButton* primaryButton_ = nullptr;
    QPushButton* removeButton_ = nullptr;

    QString rawName_;
    QString rawDesc_;
    QString rawMeta_;
};
