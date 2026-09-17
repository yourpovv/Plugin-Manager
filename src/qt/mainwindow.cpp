#include "mainwindow.h"
#include <QApplication>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCursor>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QDir>
#include <QThread>
#include <QMetaObject>
#include <QStyle>
#include <QEvent>
#include <thread>
#include "builder.h"
#include "config.h"
#include "equicord.h"
#include "log.h"
#include "paths.h"
#include "persist.h"
#include "plugin.h"
#include "settingsdialog.h"
static QString toQ(const std::string& s){return QString::fromUtf8(s.c_str());}
static std::string fromQ(const QString& s){return s.toUtf8().toStdString();}
MainWindow::MainWindow(QWidget* p):QMainWindow(p){
 setWindowTitle("Equicord Plugin Manager");
 setWindowIcon(QIcon(":/assets/icon.png"));
 setAcceptDrops(true);
 resize(720,520);
 buildMenuBar();
 buildCentralWidget();
 buildStatusBar();

 appLog::setListener([this]{QMetaObject::invokeMethod(this,[this]{refreshLog();},Qt::QueuedConnection);});
 setupAccessibleNames();
 setupTabOrder();
}
MainWindow::~MainWindow()=default;
void MainWindow::buildMenuBar(){
 auto* file=menuBar()->addMenu("&File");
 file->addAction("Settings…",this,&MainWindow::onBrowseEquicord,QKeySequence("Ctrl+,"));
 file->addAction("Exit",this,&QWidget::close,QKeySequence("Ctrl+Q"));
 auto* view=menuBar()->addMenu("&View");
 view->addAction("Refresh",this,[this]{applyStatus("Ready",false,false);},QKeySequence("Ctrl+R"));
 view->addSeparator();
 {
  auto* act = view->addAction("Logs", this, [this](bool checked){
   if(logView_) logView_->setVisible(checked);
   if(logToggleBtn_) logToggleBtn_->setVisible(checked);
  });
  act->setCheckable(true);
  act->setChecked(false);
  act->setShortcut(QKeySequence("Ctrl+L"));
  act->setToolTip("Show pnpm build/inject output");

  logToggleBtn_ = new QPushButton("Logs", this);
  logToggleBtn_->setProperty("tertiary", true);
  logToggleBtn_->setCheckable(true);
  logToggleBtn_->setVisible(false);

  connect(act, &QAction::toggled, logToggleBtn_, &QPushButton::setChecked);
  connect(logToggleBtn_, &QPushButton::toggled, act, &QAction::setChecked);
 }
 auto* help=menuBar()->addMenu("&Help");
 help->addAction("About",this,[this]{
  QMessageBox box(this);
  box.setWindowTitle("About");
  box.setTextFormat(Qt::RichText);
  box.setText("Equicord Plugin Manager<br><br>"
              "Made to install unofficial Equicord plugins<br>"
              "Created by <a href=\"https://yourpov.dev\">yourpov.dev</a><br><br>"
              "Drop a plugin folder, ZIP/RAR/7Z, or paste a GitHub link<br>"
              "Pick Equicord folder and Discord version in File > Settings, then Install");
  box.setTextInteractionFlags(Qt::TextBrowserInteraction);
  box.exec();
 });
}
void MainWindow::buildCentralWidget(){
 central_=new QWidget(this);setCentralWidget(central_);
 rootLayout_=new QVBoxLayout(central_);
 rootLayout_->setContentsMargins(12,12,12,12);rootLayout_->setSpacing(12);
 topProgress_=new QProgressBar(this);
 topProgress_->setFixedHeight(3);
 topProgress_->setTextVisible(false);
 topProgress_->setVisible(false);
 topProgress_->setRange(0,100);
 topProgress_->setStyleSheet("QProgressBar{background:#2b2d31;border:none;border-radius:1px;}QProgressBar::chunk{background:#5865f2;}");
 rootLayout_->addWidget(topProgress_);
 banner_=new QFrame(this);banner_->setObjectName("banner");banner_->setFrameShape(QFrame::StyledPanel);banner_->setVisible(false);
 auto* bl=new QHBoxLayout(banner_);bl->setContentsMargins(12,8,12,8);bl->setSpacing(8);
 bannerIcon_=new QLabel("⚠",banner_);bannerIcon_->setStyleSheet("color:#f0b232;font-size:14px;");
 bannerLabel_=new QLabel(banner_);bannerLabel_->setWordWrap(true);bannerLabel_->setStyleSheet("color:#f0b232;font-size:12px;");
 bannerChangeBtn_=new QPushButton("Open Settings…",banner_);bannerChangeBtn_->setProperty("secondary",true);
 bl->addWidget(bannerIcon_);bl->addWidget(bannerLabel_,1);bl->addWidget(bannerChangeBtn_);
 rootLayout_->addWidget(banner_);
 connect(bannerChangeBtn_,&QPushButton::clicked,this,&MainWindow::onBrowseEquicord);
 linkEdit_=new QLineEdit(this);
 linkEdit_->setPlaceholderText("Enter a Github link or C:\\path\\to\\plugin");
 linkEdit_->setClearButtonEnabled(true);
 linkEdit_->setMinimumHeight(32);
 linkEdit_->setToolTip("Paste a GitHub folder link (tree) or local path, then click Install");
 rootLayout_->addWidget(linkEdit_);
 connect(linkEdit_,&QLineEdit::textChanged,this,&MainWindow::onSourceChanged);
 connect(linkEdit_,&QLineEdit::returnPressed,this,&MainWindow::onInstallClicked);
 buildDropZone();
 footerBar_=new QHBoxLayout();footerBar_->setContentsMargins(0,0,0,0);
 logToggleBtn_=new QPushButton("Show log",this);logToggleBtn_->setProperty("tertiary",true);logToggleBtn_->setMinimumHeight(28);logToggleBtn_->setVisible(false);
 footerBar_->addWidget(logToggleBtn_);
 footerBar_->addStretch(1);
 installButton_=new QPushButton("Install",this);installButton_->setProperty("primary",true);installButton_->setMinimumHeight(36);installButton_->setMinimumWidth(140);
 footerBar_->addWidget(installButton_);
 rootLayout_->addLayout(footerBar_);
 rootLayout_->setStretchFactor(dropFrame_,1);
 connect(installButton_,&QPushButton::clicked,this,&MainWindow::onInstallClicked);
 connect(logToggleBtn_,&QPushButton::clicked,this,[this]{
  if(logView_ && logView_->isVisible()){logView_->setVisible(false);logToggleBtn_->setText("Show log");}
  else if(logView_){logView_->setVisible(true);logToggleBtn_->setText("Hide log");}
 });

 logView_=new QTextEdit(this);logView_->setReadOnly(true);logView_->setVisible(false);
 logView_->setMaximumHeight(160);
 logView_->setPlaceholderText("Build and inject output appears here as it runs…");
 logView_->setStyleSheet("QTextEdit{background:#18191c;color:#b5bac1;border:1px solid #3f4147;border-radius:8px;font-family:Consolas;font-size:11px;}");
 rootLayout_->addWidget(logView_);
}
void MainWindow::refreshLog(){
 if(!logView_)return;
 QScrollBar* bar=logView_->verticalScrollBar();
 const bool atBottom=!bar||bar->value()>=bar->maximum()-4;
 logView_->setPlainText(QString::fromUtf8(appLog::text().c_str()));
 if(bar&&atBottom)bar->setValue(bar->maximum());
}
void MainWindow::buildDropZone(){
 dropFrame_=new QFrame(this);dropFrame_->setObjectName("emptyPage");dropFrame_->setFrameShape(QFrame::StyledPanel);
 auto* lay=new QVBoxLayout(dropFrame_);lay->setContentsMargins(24,24,24,24);lay->setSpacing(12);lay->addStretch(1);
 dropTitle_=new QLabel("Drop plugin folders, ZIP, RAR or 7Z here - several at once is fine",dropFrame_);dropTitle_->setAlignment(Qt::AlignCenter);dropTitle_->setStyleSheet("color:#f2f3f5;font-size:15px;font-weight:600;");
 pendingLabel_=new QLabel(dropFrame_);pendingLabel_->setAlignment(Qt::AlignCenter);pendingLabel_->setWordWrap(true);pendingLabel_->setStyleSheet("color:#23a559;font-size:12px;font-weight:600;");pendingLabel_->setVisible(false);
 dropBrowseBtn_=new QPushButton("Browse…",dropFrame_);dropBrowseBtn_->setProperty("primary",true);dropBrowseBtn_->setMinimumHeight(32);
 lay->addWidget(dropTitle_);lay->addWidget(pendingLabel_);lay->addWidget(dropBrowseBtn_,0,Qt::AlignHCenter);lay->addStretch(1);
 rootLayout_->addWidget(dropFrame_,1);
 connect(dropBrowseBtn_,&QPushButton::clicked,this,&MainWindow::onBrowsePlugin);
}
void MainWindow::buildStatusBar(){
 statusIcon_=new QLabel();statusIcon_->setFixedWidth(16);statusIcon_->setVisible(false);
 statusLabel_=new QLabel("drop a plugin or paste a GitHub link to install");statusLabel_->setStyleSheet("color:#b5bac1;font-size:12px;");
 progressBar_=new QProgressBar(this);progressBar_->setMaximumHeight(4);progressBar_->setTextVisible(false);progressBar_->setVisible(false);progressBar_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
 progressBar_->setStyleSheet("QProgressBar{background:#2b2d31;border:none;border-radius:2px;}QProgressBar::chunk{background:#5865f2;}");
 statusBar()->addWidget(statusIcon_);
 statusBar()->addWidget(statusLabel_,1);
 statusBar()->addWidget(progressBar_,1);
}
void MainWindow::setupAccessibleNames(){
 linkEdit_->setAccessibleName("Plugin GitHub link or local path");
 dropFrame_->setAccessibleName("Drop plugin here");
 dropBrowseBtn_->setAccessibleName("Browse for plugin");
 installButton_->setAccessibleName("Install plugin");
}
void MainWindow::setupTabOrder(){
 setTabOrder(linkEdit_,dropBrowseBtn_);
 setTabOrder(dropBrowseBtn_,installButton_);
}
void MainWindow::setDropHighlight(bool on){dropFrame_->setProperty("dragOver",on);style()->unpolish(dropFrame_);style()->polish(dropFrame_);dropFrame_->update();}
void MainWindow::dragEnterEvent(QDragEnterEvent* e){if(e->mimeData()->hasUrls()||e->mimeData()->hasText()){e->acceptProposedAction();setDropHighlight(true);}}
void MainWindow::dragMoveEvent(QDragMoveEvent* e){if(e->mimeData()->hasUrls()||e->mimeData()->hasText())e->acceptProposedAction();}
bool MainWindow::eventFilter(QObject* w,QEvent* e){return QMainWindow::eventFilter(w,e);}
void MainWindow::dragLeaveEvent(QDragLeaveEvent* e){Q_UNUSED(e);setDropHighlight(false);}
void MainWindow::dropEvent(QDropEvent* e){
 setDropHighlight(false);
 const QMimeData* m=e->mimeData();
 std::vector<std::string> dropped;
 if(m->hasUrls()){
  for(auto& u:m->urls()){
   QString p=u.isLocalFile()?u.toLocalFile():u.toString();
   if(!p.isEmpty())dropped.push_back(fromQ(p));
  }
 }else if(m->hasText()){
  for(auto& line:m->text().split('\n',Qt::SkipEmptyParts)){
   QString p=line.trimmed();
   if(!p.isEmpty())dropped.push_back(fromQ(p));
  }
 }
 if(!dropped.empty()){
  linkEdit_->setText(dropped.size()==1?toQ(dropped[0]):QString());
  setPendingPlugins(dropped);
 }
 e->acceptProposedAction();
}
void MainWindow::resizeEvent(QResizeEvent* e){QMainWindow::resizeEvent(e);}
void MainWindow::setPendingPlugin(const std::string& path){
 if(path.empty())setPendingPlugins(std::vector<std::string>());
 else setPendingPlugins(std::vector<std::string>{path});
}
void MainWindow::setPendingPlugins(const std::vector<std::string>& paths){
 pendingPlugins_=paths;
 if(paths.empty()){fullPendingPath_.clear();pendingLabel_->setVisible(false);updateInstallButton();return;}
 QStringList all;
 for(auto& p:paths)all<<toQ(p);
 fullPendingPath_=all.join("\n");
 pendingLabel_->setText(paths.size()==1?("Selected: "+all.first()):QString("Selected: %1 plugins").arg(paths.size()));
 pendingLabel_->setToolTip(fullPendingPath_);
 pendingLabel_->setVisible(true);
 updateInstallButton();
}
void MainWindow::updateInstallButton(){
 bool hasEquicord=equicord::validate(cfg::equicordPath).isValid;
 QString linkText=linkEdit_?linkEdit_->text().trimmed():QString();
 bool hasPending=!pendingPlugins_.empty() || !linkText.isEmpty();
 bool busy=jobRunning_;
 dropBrowseBtn_->setProperty("primary",!hasPending);
 dropBrowseBtn_->setProperty("secondary",hasPending);
 installButton_->setVisible(hasPending);
 installButton_->setEnabled(hasEquicord&&hasPending&&!busy);
 if(!hasEquicord)installButton_->setToolTip("Select your Equicord folder in Settings first");
 else if(!hasPending)installButton_->setToolTip("");
 else if(busy)installButton_->setToolTip("Working…");
 else installButton_->setToolTip("Close Discord, install, build and reopen");
 style()->unpolish(dropBrowseBtn_);style()->polish(dropBrowseBtn_);dropBrowseBtn_->update();
 style()->unpolish(installButton_);style()->polish(installButton_);installButton_->update();
}
void MainWindow::applyEquicordState(const QString& path,bool isValid,const QString& message){
 if(isValid){banner_->setVisible(false);applyStatus(message,false,false);}
 else{banner_->setVisible(true);bannerLabel_->setText(message.isEmpty()?"Select your Equicord folder. Open Settings…":message);applyStatus(message,false,true);}
 bool hasPending=!pendingPlugins_.empty() || !linkEdit_->text().trimmed().isEmpty();
 dropBrowseBtn_->setProperty("primary",!hasPending);
 dropBrowseBtn_->setProperty("secondary",hasPending);
 installButton_->setVisible(hasPending);
 style()->unpolish(dropBrowseBtn_);style()->polish(dropBrowseBtn_);dropBrowseBtn_->update();
 updateInstallButton();
}
void MainWindow::applyStatus(const QString& msg,bool isError,bool isWarning){
 statusLabel_->setText(msg);
 bool show=isError||isWarning;
 statusIcon_->setVisible(show);
 if(show)statusIcon_->setText(isError?"✕":"⚠");
 statusIcon_->setToolTip(show?msg:"");
 style()->unpolish(statusLabel_);style()->polish(statusLabel_);statusLabel_->update();
 style()->unpolish(statusIcon_);style()->polish(statusIcon_);statusIcon_->update();
}
void MainWindow::applyBusy(bool busy,const QString& label){
 installButton_->setEnabled(!busy);dropBrowseBtn_->setEnabled(!busy);
 if(!label.isEmpty())statusLabel_->setText(label);
 topProgress_->setVisible(busy);progressBar_->setVisible(busy);
 if(!busy){topProgress_->setValue(0);progressBar_->setValue(0);}
}
void MainWindow::applyProgress(int pct){
 if(pct<0){topProgress_->setVisible(false);progressBar_->setVisible(false);return;}
 topProgress_->setVisible(true);topProgress_->setValue(pct);
 progressBar_->setVisible(true);progressBar_->setValue(pct);
}
std::string MainWindow::pickEquicordFolder(){QString f=QFileDialog::getExistingDirectory(this,"Select your Equicord folder");return fromQ(f);}
std::string MainWindow::pickPluginPathSingleDialog(){
 QMenu menu(this);
 QAction* folderAction=menu.addAction("Plugin folder…");
 QAction* fileAction=menu.addAction("Archive or source file…");
 QPoint where=dropBrowseBtn_?dropBrowseBtn_->mapToGlobal(QPoint(0,dropBrowseBtn_->height())):QCursor::pos();
 QAction* chosen=menu.exec(where);
 if(chosen==folderAction)
  return fromQ(QFileDialog::getExistingDirectory(this,"Select the plugin folder"));
 if(chosen==fileAction)
  return fromQ(QFileDialog::getOpenFileName(this,"Select a plugin archive or source file","",
   "Equicord plugin (*.zip *.rar *.7z *.ts *.tsx *.css);;All files (*.*)"));
 return "";
}
void MainWindow::onBrowseEquicord(){
 SettingsDialog dlg(this);
 dlg.setEquicordPath(toQ(cfg::equicordPath));
 auto chans=builder::discordChannels();
 dlg.setDiscordChoices(chans);
 dlg.setDiscordBranch(toQ(cfg::discordBranch));
 if(dlg.exec()!=QDialog::Accepted)return;
 std::string newPath=fromQ(dlg.equicordPath());
 std::string newBranch=fromQ(dlg.discordBranch());
 if(!newPath.empty())applyEquicordPath(newPath);
 cfg::discordBranch=newBranch;
 persist::Settings s{cfg::equicordPath,cfg::manifestUrl,cfg::autoBuild,cfg::buildDev,cfg::discordBranch};
 persist::save(s);
 applyStatus("Settings saved",false,false);
}
void MainWindow::onBrowsePlugin(){std::string p=pickPluginPathSingleDialog();if(!p.empty()){linkEdit_->setText(toQ(p));setPendingPlugin(p);}}
void MainWindow::onSourceChanged(){updateInstallButton();}
void MainWindow::onInstallClicked(){startInstallFlow();}
std::string MainWindow::workBlocker(){
 if(jobRunning_)return "Wait for current task to finish.";
 auto chk=equicord::validate(cfg::equicordPath);
 if(!chk.isValid)return chk.userMessage;
 bool hasPending=!pendingPlugins_.empty() || !linkEdit_->text().trimmed().isEmpty();
 if(!hasPending)return "Drop a plugin folder, ZIP, RAR or 7Z, click Browse, or paste a GitHub link above.";
 return "";
}
bool MainWindow::ensurePnpmAvailable(){auto t=builder::findTools();if(t.pnpmFound&&t.nodeFound)return true;return autoInstallNodeAndPnpm();}
bool MainWindow::autoInstallNodeAndPnpm(){
 auto tools=builder::findTools();
 if(!tools.nodeFound){
  applyStatus("Node.js not found. installing via winget…",false,true);
  QProcess proc;proc.start("winget",QStringList()<<"install"<<"--id"<<"OpenJS.NodeJS.LTS"<<"-e"<<"--accept-package-agreements"<<"--accept-source-agreements"<<"--silent");proc.waitForFinished(180000);
  tools=builder::findTools();
  if(!tools.nodeFound){QMessageBox::warning(this,"Node.js missing","Node.js is required but auto-install failed.\n\nInstall Node.js LTS from https://nodejs.org/ then click Install again.");return false;}
 }
 tools=builder::findTools();
 if(!tools.pnpmFound){
  applyStatus("pnpm not found. installing…",false,true);
  QProcess proc;proc.start("cmd.exe",QStringList()<<"/C"<<"npm i -g pnpm");proc.waitForFinished(120000);
  if(!builder::findTools().pnpmFound){QProcess p2;p2.start("cmd.exe",QStringList()<<"/C"<<"corepack enable && corepack prepare pnpm@latest --activate");p2.waitForFinished(60000);}
  tools=builder::findTools();
  if(!tools.pnpmFound){QMessageBox::warning(this,"pnpm missing","pnpm auto-install failed.\n\nRun 'npm i -g pnpm' in a terminal, then click Install again.");return false;}
 }
 return builder::findTools().pnpmFound&&builder::findTools().nodeFound;
}
bool MainWindow::closeDiscord(const std::string& branch){

 QString name="Discord.exe";
 if(branch=="ptb")name="DiscordPTB.exe";
 else if(branch=="canary")name="DiscordCanary.exe";
 else if(branch=="development")name="DiscordDevelopment.exe";
 QProcess::execute("taskkill",QStringList()<<"/IM"<<name<<"/F"<<"/T");

 for(int attempt=0;attempt<40;attempt++){
  QProcess probe;
  probe.start("tasklist",QStringList()<<"/FI"<<("IMAGENAME eq "+name)<<"/NH");
  if(!probe.waitForFinished(3000))break;
  if(!QString::fromLocal8Bit(probe.readAllStandardOutput()).contains(name,Qt::CaseInsensitive))return true;
  QThread::msleep(250);
 }
 return false;
}
bool MainWindow::reopenDiscord(const std::string& branch){
 std::string folder;for(auto& c:builder::discordChannels())if(c.branch==branch)folder=c.folder;
 if(folder.empty())return false;
 QString upd=toQ(folder+"\\Update.exe");
 QString exe=toQ(folder+"\\Discord.exe");
 if(QFileInfo::exists(upd)){
  QString en="Discord.exe";if(branch=="ptb")en="DiscordPTB.exe";else if(branch=="canary")en="DiscordCanary.exe";
  QProcess::startDetached(upd,QStringList()<<"--processStart"<<en);return true;
 }
 if(QFileInfo::exists(exe)){QProcess::startDetached(exe,QStringList());return true;}
 QDir dir(toQ(folder));auto apps=dir.entryList(QStringList()<<"app-*",QDir::Dirs);
 for(auto& a:apps){QString p=toQ(folder+"\\"+fromQ(a)+"\\Discord.exe");if(QFileInfo::exists(p)){QProcess::startDetached(p,QStringList());return true;}}
 return false;
}
void MainWindow::applyEquicordPath(const std::string& folder){
 auto chk=equicord::validate(folder);
 cfg::equicordPath=chk.isValid?chk.path:folder;
 persist::Settings s{cfg::equicordPath,cfg::manifestUrl,cfg::autoBuild,cfg::buildDev,cfg::discordBranch};
 persist::save(s);
 applyEquicordState(toQ(chk.path),chk.isValid,toQ(chk.userMessage));
 logLine(chk.isValid?"OK":"ERROR",chk.userMessage);
}
void MainWindow::startInstallFlow(){
 std::string blocker=workBlocker();
 if(!blocker.empty()){applyStatus(toQ(blocker),true,false);return;}
 std::string branch=cfg::discordBranch;if(branch.empty())branch="stable";
 QString linkText=linkEdit_->text().trimmed();
 std::vector<std::string> sources;
 if(pendingPlugins_.size()>1)sources=pendingPlugins_;
 else if(!linkText.isEmpty())sources.push_back(fromQ(linkText));
 else sources=pendingPlugins_;
 const size_t total=sources.size();
 jobRunning_=true;applyBusy(true,total>1?QString("Installing %1 plugins…").arg(total):QString("Installing plugin…"));applyProgress(0);
 if(logView_&&!logView_->isVisible()){logView_->setVisible(true);if(logToggleBtn_)logToggleBtn_->setText("Hide log");}
 std::thread([this,sources,total,branch]{
  if(!ensurePnpmAvailable()){QMetaObject::invokeMethod(this,[this]{applyStatus("Cannot build without Node.js/pnpm.",true,false);applyBusy(false,"");applyProgress(-1);},Qt::QueuedConnection);jobRunning_=false;return;}
  QMetaObject::invokeMethod(this,[this,branch]{applyStatus(toQ("Closing Discord "+branch+"…"),false,false);},Qt::QueuedConnection);
  if(!closeDiscord(branch)){
   logLine("ERROR","Could not close Discord "+branch+".");
   QMetaObject::invokeMethod(this,[this,branch]{applyStatus(toQ("Couldn't close Discord "+branch+". Quit it from the tray icon, then try again."),true,false);applyBusy(false,"");applyProgress(-1);},Qt::QueuedConnection);
   jobRunning_=false;return;
  }
  size_t installed=0;
  Outcome lastFail=ok();
  for(size_t i=0;i<total;i++){
   const int pct=(int)(10+(20*(i+1))/total);
   const QString step=total>1?QString("Copying plugin %1 of %2…").arg(i+1).arg(total):QString("Copying plugin…");
   QMetaObject::invokeMethod(this,[this,step,pct]{applyStatus(step,false,false);applyProgress(pct);},Qt::QueuedConnection);
   auto plugin=plugins::fromSource(sources[i]);
   Outcome res=plugins::install(plugin,cfg::equicordPath,[](const std::string& s){logLine("INFO",s);});
   if(res.succeeded){installed++;logLine("OK",res.userMessage);continue;}
   lastFail=res;
   logLine("ERROR",res.userMessage);
   if(!res.detail.empty())logLine("ERROR",res.detail);
  }
  if(installed==0){
   QMetaObject::invokeMethod(this,[this,lastFail]{applyStatus(toQ(lastFail.userMessage),true,false);applyBusy(false,"");applyProgress(-1);},Qt::QueuedConnection);
   reopenDiscord(branch);jobRunning_=false;return;
  }
  const size_t failed=total-installed;
  if(failed>0)logLine("ERROR",std::to_string(failed)+" of "+std::to_string(total)+" plugins didn't install. Building the rest.");
  QMetaObject::invokeMethod(this,[this]{applyStatus("Building Equicord…",false,false);applyProgress(60);},Qt::QueuedConnection);
  Outcome built=builder::build(cfg::equicordPath,cfg::buildDev,[this](const std::string& l){logLine("BUILD",l);});
  if(!built.succeeded){logLine("ERROR",built.userMessage);QMetaObject::invokeMethod(this,[this,built]{applyStatus(toQ(built.userMessage),true,false);applyBusy(false,"");applyProgress(-1);},Qt::QueuedConnection);reopenDiscord(branch);jobRunning_=false;return;}
  logLine("OK",built.userMessage);
  QMetaObject::invokeMethod(this,[this,branch]{applyStatus(toQ("Injecting into "+branch+"…"),false,false);applyProgress(85);},Qt::QueuedConnection);

  if(!closeDiscord(branch)){
   logLine("ERROR","Could not close Discord "+branch+" before injecting.");
   QMetaObject::invokeMethod(this,[this,branch]{applyStatus(toQ("Built, but Discord "+branch+" wouldn't close, so it can't be patched. Quit it from the tray icon, then click Install again."),true,false);applyBusy(false,"");applyProgress(-1);},Qt::QueuedConnection);
   jobRunning_=false;return;
  }
  Outcome inj=builder::inject(cfg::equicordPath,branch,[this](const std::string& l){logLine("BUILD",l);});
  if(inj.succeeded)logLine("OK",inj.userMessage);else logLine("ERROR",inj.userMessage);
  QMetaObject::invokeMethod(this,[this,inj,branch]{if(inj.succeeded)applyStatus(toQ(inj.userMessage),false,false);else applyStatus(toQ(inj.userMessage),true,false);applyBusy(false,"");applyProgress(100);QTimer::singleShot(800,this,[this]{applyProgress(-1);});},Qt::QueuedConnection);
  QThread::msleep(500);reopenDiscord(branch);jobRunning_=false;
 }).detach();
}
void MainWindow::initialize(){
 std::string saved=cfg::equicordPath;
 if(!saved.empty()){auto chk=equicord::validate(saved);if(!chk.isValid){std::string g=equicord::guessInstallFolder();if(!g.empty()&&equicord::validate(g).isValid)cfg::equicordPath=g;}}else cfg::equicordPath=equicord::guessInstallFolder();
 if(!cfg::equicordPath.empty())applyEquicordPath(cfg::equicordPath);else applyEquicordState("",false,"Select your Equicord folder. Open Settings…");
 if(!ensurePnpmAvailable())applyStatus("pnpm not found. will auto-install on Install",false,true);
 updateInstallButton();
 applyStatus("drop a plugin or paste a GitHub link to install",false,false);
}
