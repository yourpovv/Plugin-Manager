#include "PluginWidget.h"

#include <QCheckBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QVBoxLayout>

#include "text.h"

static QString toQ(const std::string& s) {
    return QString::fromUtf8(s.c_str());
}

PluginWidget::PluginWidget(const PluginStatus& status, QWidget* parent)
    : QWidget(parent)
    , status_(status)
    , rawName_(toQ(status.catalog.name))
    , rawDesc_(toQ(status.catalog.description))
{
    setObjectName("pluginCard");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(72);

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(12, 10, 12, 10);
    outer->setSpacing(10);

    checkBox_ = new QCheckBox(this);
    checkBox_->setFixedSize(18, 18);
    checkBox_->setAccessibleName("Select " + rawName_);
    checkBox_->setToolTip("Select for bulk install");
    outer->addWidget(checkBox_, 0, Qt::AlignTop);

    auto* textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(2);

    nameLabel_ = new QLabel(this);
    nameLabel_->setAccessibleName("Plugin name");
    nameLabel_->setStyleSheet("color: #f2f3f5; font-weight: 600; font-size: 13px;");
    nameLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    descLabel_ = new QLabel(this);
    descLabel_->setStyleSheet("color: #b5bac1; font-size: 12px;");
    descLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    metaLabel_ = new QLabel(this);
    metaLabel_->setStyleSheet("color: #80848e; font-size: 11px;");
    metaLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    textCol->addWidget(nameLabel_);
    textCol->addWidget(descLabel_);
    textCol->addWidget(metaLabel_);
    outer->addLayout(textCol, 1);

    auto* rightCol = new QVBoxLayout();
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(6);

    pillLabel_ = new QLabel(this);
    pillLabel_->setAlignment(Qt::AlignCenter);
    pillLabel_->setFixedHeight(18);
    pillLabel_->setMinimumWidth(90);
    if (status_.updateAvailable) {
        pillLabel_->setText("Update available");
        pillLabel_->setProperty("status", "warning");
        pillLabel_->setStyleSheet("background: rgba(88,67,32,0.9); color: #f0b232; border-radius: 9px; font-size: 11px; font-weight: 600; padding: 0 8px;");
    } else if (status_.isInstalled) {
        pillLabel_->setText("Installed");
        pillLabel_->setProperty("status", "success");
        pillLabel_->setStyleSheet("background: rgba(26,77,46,0.9); color: #23a559; border-radius: 9px; font-size: 11px; font-weight: 600; padding: 0 8px;");
    } else {
        pillLabel_->setText("Not installed");
        pillLabel_->setProperty("status", "neutral");
        pillLabel_->setStyleSheet("background: #43464d; color: #b5bac1; border-radius: 9px; font-size: 11px; font-weight: 600; padding: 0 8px;");
    }
    rightCol->addWidget(pillLabel_, 0, Qt::AlignRight);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(6);

    primaryButton_ = new QPushButton(this);
    primaryButton_->setFixedSize(88, 30);
    primaryButton_->setCursor(Qt::PointingHandCursor);
    if (status_.isInstalled && status_.updateAvailable) {
        primaryButton_->setText("Update");
        primaryButton_->setProperty("primary", true);
        primaryButton_->setToolTip("Update to v" + toQ(status_.catalog.version));
    } else if (status_.isInstalled) {
        primaryButton_->setText("Installed");
        primaryButton_->setEnabled(false);
        primaryButton_->setToolTip("Already installed");
    } else {
        primaryButton_->setText("Install");
        primaryButton_->setProperty("primary", true);
        primaryButton_->setToolTip("Install " + rawName_);
    }

    removeButton_ = new QPushButton("Remove", this);
    removeButton_->setFixedSize(88, 30);
    removeButton_->setCursor(Qt::PointingHandCursor);
    removeButton_->setProperty("secondary", true);
    removeButton_->setToolTip("Remove " + rawName_ + " from src/userplugins");
    removeButton_->setVisible(status_.isInstalled);

    btnRow->addStretch();
    if (removeButton_->isVisible())
        btnRow->addWidget(removeButton_, 0, Qt::AlignRight);
    btnRow->addWidget(primaryButton_, 0, Qt::AlignRight);
    rightCol->addLayout(btnRow);
    outer->addLayout(rightCol, 0);

    rebuildMetaText();
    updateElidedTexts();

    connect(checkBox_, &QCheckBox::toggled, this, [this](bool c){
        emit selectionToggled(status_.catalog.id, c);
        style()->unpolish(this);
        style()->polish(this);
        update();
    });
    connect(primaryButton_, &QPushButton::clicked, this, [this]{
        if (status_.isInstalled && !status_.updateAvailable) return;
        emit installRequested(status_.catalog.id);
    });
    connect(removeButton_, &QPushButton::clicked, this, [this]{
        auto res = QMessageBox::question(this, "Remove plugin?",
            "Remove '" + rawName_ + "' from src/userplugins/" + toQ(status_.catalog.id) + "?\nThis cannot be undone.",
            QMessageBox::Yes | QMessageBox::Cancel);
        if (res == QMessageBox::Yes)
            emit removeRequested(status_.catalog.id);
    });
}

void PluginWidget::rebuildMetaText() {
    QStringList parts;
    if (!status_.catalog.author.empty())
        parts << "by " + toQ(status_.catalog.author);
    if (!status_.catalog.version.empty())
        parts << "v" + toQ(status_.catalog.version);
    if (status_.isInstalled && !status_.installedVersion.empty())
        parts << "installed " + toQ(status_.installedVersion);
    rawMeta_ = parts.join("  ·  ");
    if (rawMeta_.isEmpty())
        rawMeta_ = toQ(status_.catalog.id);
}

void PluginWidget::updateElidedTexts() {
    const int availW = width() - 200;
    QFontMetrics fmName(nameLabel_->font());
    QFontMetrics fmDesc(descLabel_->font());
    nameLabel_->setText(fmName.elidedText(rawName_, Qt::ElideRight, qMax(80, availW)));
    descLabel_->setText(fmDesc.elidedText(rawDesc_, Qt::ElideRight, qMax(80, availW)));
    metaLabel_->setText(rawMeta_);
    nameLabel_->setToolTip(rawName_);
    descLabel_->setToolTip(rawDesc_);
    metaLabel_->setToolTip(rawMeta_);
}

void PluginWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    updateElidedTexts();
}

std::string PluginWidget::pluginId() const { return status_.catalog.id; }
bool PluginWidget::isChecked() const { return checkBox_ ? checkBox_->isChecked() : false; }
void PluginWidget::setChecked(bool on) {
    if (!checkBox_) return;
    checkBox_->blockSignals(true);
    checkBox_->setChecked(on);
    checkBox_->blockSignals(false);
    style()->unpolish(this);
    style()->polish(this);
    update();
}
PluginStatus PluginWidget::status() const { return status_; }
bool PluginWidget::matchesFilter(const std::string& q, bool instOnly, bool updOnly) const {
    if (instOnly && !status_.isInstalled) return false;
    if (updOnly && !status_.updateAvailable) return false;
    if (q.empty()) return true;
    return containsInsensitive(status_.catalog.name, q)
        || containsInsensitive(status_.catalog.description, q)
        || containsInsensitive(status_.catalog.author, q)
        || containsInsensitive(status_.catalog.id, q);
}
