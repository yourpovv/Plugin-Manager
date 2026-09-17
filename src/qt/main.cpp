#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStyleFactory>
#include "config.h"
#include "mainwindow.h"
#include "persist.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Equicord Plugin Manager");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/assets/icon.png"));

    app.setStyle(QStyleFactory::create("Fusion"));
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    persist::Settings settings = persist::load();
    cfg::equicordPath = settings.equicordPath;
    cfg::manifestUrl = settings.manifestUrl;
    cfg::autoBuild = settings.autoBuild;
    cfg::buildDev = settings.buildDev;
    cfg::discordBranch = settings.discordBranch;

    MainWindow w;
    w.initialize();
    w.show();
    return app.exec();
}
