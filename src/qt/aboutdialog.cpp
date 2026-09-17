#include "aboutdialog.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::AboutDialog) {
    ui->setupUi(this);
    setStyleSheet(
        "QDialog { background:#1e1f22; }"
        "QLabel { color:#b5bac1; }"
        "QPushButton { background:#5865f2; color:#fff; border:none; border-radius:8px; padding:6px 16px; }"
        "QPushButton:hover { background:#4752c4; }"
    );
}

AboutDialog::~AboutDialog() {
    delete ui;
}
