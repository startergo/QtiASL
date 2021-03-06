#include "mainwindow.h"
#include "MyTabBar.h"
#include "MyTabPage.h"
#include "MyTabPopup.h"
#include "filesystemwatcher.h"
#include "mytabwidget.h"
#include "ui_mainwindow.h"

#include <QTabBar>
#include <QTabWidget>

bool loading = false;
bool thread_end = true;
bool break_run = false;
bool show_s = true;
bool show_d = true;
bool show_m = true;
bool show_n = false;
int s_count = 0;
int m_count = 0;
int d_count = 0;
int n_count = 0;
QsciScintilla* textEditBack;

QsciScintilla* miniDlgEdit;
miniDialog* miniDlg;
bool textEditScroll = false;
bool miniEditWheel = false;

//QVector<QsciScintilla*> textEditList;
QVector<QString> openFileList;

bool SelfSaved = false;
bool ReLoad = false;

QList<QTreeWidgetItem*> twitems;
QList<QTreeWidgetItem*> tw_scope;
QList<QTreeWidgetItem*> tw_device;
QList<QTreeWidgetItem*> tw_method;
QList<QTreeWidgetItem*> tw_name;
QList<QTreeWidgetItem*> tw_list;
QTreeWidget* treeWidgetBak;

QString fileName;
QVector<QString> filelist;
QWidgetList wdlist;
QscilexerCppAttach* textLexer;

bool zh_cn = false;

QString dragFileName;
int rowDrag;
int colDrag;
int vs;
int hs;

extern MainWindow* mw_one;

int red;

thread_one::thread_one(QObject* parent)
    : QThread(parent)
{
}

MiniEditor::MiniEditor(QWidget* parent)
    : QsciScintilla(parent)
{
    //setWindowFlags(Qt::FramelessWindowHint);
    //this->setMargins(0);

    connect(this, &QsciScintilla::cursorPositionChanged, this, &MiniEditor::miniEdit_cursorPositionChanged);
}

MaxEditor::MaxEditor(QWidget* parent)
    : QsciScintilla(parent)
{
}

CodeEditor::CodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    loadLocal();

    CurVerison = "1.0.50";
    ver = "QtiASL V" + CurVerison + "        ";
    setWindowTitle(ver);

    //获取背景色
    QPalette pal = this->palette();
    QBrush brush = pal.window();
    red = brush.color().red();

    QDir dir;
    if (dir.mkpath(QDir::homePath() + "/.config/QtiASL/")) { }

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    mythread = new thread_one();
    connect(mythread, &thread_one::over, this, &MainWindow::dealover);

    dlg = new dlgDecompile(this);

    init_statusBar();

    init_menu();

    init_recentFiles();

    font.setFamily("SauceCodePro Nerd Font");
#ifdef Q_OS_WIN32
    font.setPointSize(9);
    regACPI_win();
    ui->actionKextstat->setEnabled(false);
    ui->toolBar->setIconSize(QSize(22, 22));
    win = true;
#endif

#ifdef Q_OS_LINUX
    font.setPointSize(11);
    ui->actionKextstat->setEnabled(false);
    ui->actionGenerate->setEnabled(false);
    ui->toolBar->setIconSize(QSize(20, 20));
    linuxOS = true;
#endif

#ifdef Q_OS_MAC
    font.setPointSize(13);
    ui->actionGenerate->setEnabled(true);
    ui->toolBar->setIconSize(QSize(20, 20));
    //ui->actionCheckUpdate->setVisible(false);
    mac = true;
#endif
    init_info_edit();

    init_miniEdit();

    init_treeWidget();

    ui->tabWidget_textEdit->setDocumentMode(true);

    ui->tabWidget_textEdit->tabBar()->installEventFilter(this); //安装事件过滤器以禁用鼠标滚轮切换标签页
    connect(ui->tabWidget_textEdit, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));
    ui->tabWidget_textEdit->setIconSize(QSize(8, 8));

    textEdit = new QsciScintilla;

    init_edit(textEdit);

    ui->dockWidgetContents->layout()->setMargin(0); //成员列表
    ui->gridLayout->setMargin(0);
    ui->gridLayout_8->setMargin(0);

    ui->centralwidget->layout()->setMargin(0);
    ui->centralwidget->layout()->setSpacing(0);

    //删除titleba
    QWidget* lTitleBar = ui->dockWidget_5->titleBarWidget();
    QWidget* lEmptyWidget = new QWidget();
    ui->dockWidget_5->setTitleBarWidget(lEmptyWidget);
    delete lTitleBar;
    ui->dockWidgetContents_5->layout()->setMargin(0);
    ui->gridLayout_10->setMargin(0);
    ui->gridLayout_10->setSpacing(0);
    ui->gridLayout_10->addWidget(miniEdit);

    //splitterMain = new QSplitter(Qt::Horizontal, this);
    //QSplitter* splitterRight = new QSplitter(Qt::Horizontal, splitterMain);
    //splitterRight->setOpaqueResize(true);
    //splitterRight->addWidget(ui->tabWidget_textEdit);
    //splitterRight->addWidget(miniEdit);

    //设置鼠标追踪
    ui->centralwidget->setMouseTracking(true);
    this->setMouseTracking(true);
    ui->toolBar->setMouseTracking(true);
    ui->dockWidget_6->setMouseTracking(true);
    ui->statusbar->setMouseTracking(true);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timer_linkage()));

    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));

    init_filesystem();

    init_toolbar();

    loadTabFiles();

    loadFindString();

    One = false;
}

MainWindow::~MainWindow()
{
    delete ui;

    mythread->quit();
    mythread->wait();
}

void MainWindow::loadTabFiles()
{

    loading = true;

    //读取标签页
    QString qfile = QDir::homePath() + "/.config/QtiASL/QtiASL.ini";
    QFileInfo fi(qfile);

    if (fi.exists()) {

        QSettings Reg(qfile, QSettings::IniFormat);
        int count = Reg.value("count").toInt();

        //minimap
        if (Reg.contains("minimap"))
            ui->actionMinimap->setChecked(Reg.value("minimap").toBool());
        else
            ui->actionMinimap->setChecked(true);
        if (ui->actionMinimap->isChecked())
            ui->dockWidget_5->setVisible(true);
        else
            ui->dockWidget_5->setVisible(false);

        bool file_exists = false;

        if (count == 0) {
            newFile();
            file_exists = true;
        }

        for (int i = 0; i < count; i++) {
            QString file = Reg.value(QString::number(i) + "/file").toString();

            if (file == tr("untitled") + ".dsl") {
                newFile();
                file_exists = true;
            }

            QFileInfo fi(file);
            if (fi.exists()) {

                int row, col;
                row = Reg.value(QString::number(i) + "/row").toInt();
                col = Reg.value(QString::number(i) + "/col").toInt();

                loadFile(file, row, col);

                int vs, hs;
                vs = Reg.value(QString::number(i) + "/vs").toInt();
                hs = Reg.value(QString::number(i) + "/hs").toInt();

                QWidget* pWidget = ui->tabWidget_textEdit->currentWidget();

                QsciScintilla* currentEdit = new QsciScintilla;
                currentEdit = (QsciScintilla*)pWidget->children().at(editNumber);

                currentEdit->verticalScrollBar()->setSliderPosition(vs);
                currentEdit->horizontalScrollBar()->setSliderPosition(hs);

                currentEdit->setFocus();

                file_exists = true;
            }
        }

        int tab_total = ui->tabWidget_textEdit->tabBar()->count(); //以实际存在的为准

        if (!file_exists)
            newFile();

        int ci = Reg.value("ci").toInt();

        if (ci < tab_total) {
            ui->tabWidget_textEdit->setCurrentIndex(ci);
            on_tabWidget_textEdit_tabBarClicked(ci);
        } else {
            ui->tabWidget_textEdit->setCurrentIndex(tab_total - 1);
            on_tabWidget_textEdit_tabBarClicked(tab_total - 1);
        }

    } else
        newFile();

    loading = false;
}

void MainWindow::about()
{
    QFileInfo appInfo(qApp->applicationFilePath());
    QString str;

    str = tr("Last modified: ");

    QString last = str + appInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    QString str1 = "<a style='color:blue;' href = https://github.com/ic005k/QtiASL>QtiASL IDE</a><br><br>";

    QMessageBox::about(this, "About", str1 + last);
}

QString MainWindow::openFile(QString fileName)
{

    QFileInfo fInfo(fileName);

    if (fInfo.suffix() == "aml" || fInfo.suffix() == "dat") {
        //如果之前这个文件被打开过，则返回
        QString str = fInfo.path() + "/" + fInfo.baseName() + ".dsl";

        for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {
            QWidget* pWidget = ui->tabWidget_textEdit->widget(i);

            lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout

            if (str == lblCurrentFile->text()) {
                return str;
            }
        }

        SelfSaved = true; //aml转换成dsl的时候，不进行文件内容更改监测提醒

        QFileInfo appInfo(qApp->applicationDirPath());

        Decompile = new QProcess;

        //显示信息窗口并初始化表头
        ui->dockWidget_6->setHidden(false);
        InfoWinShow = true;
        //标记tab头
        int info_count = 0;

        ui->tabWidget->setTabText(1, tr("Errors") + " (" + QString::number(info_count) + ")");
        ui->tabWidget->setTabText(2, tr("Warnings") + " (" + QString::number(info_count) + ")");
        ui->tabWidget->setTabText(3, tr("Remarks") + " (" + QString::number(info_count) + ")");
        ui->tabWidget->setTabText(4, tr("Scribble"));
        ui->actionInfo_win->setChecked(true);

        ui->listWidget->clear();
        ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i1.png"), tr("BasicInfo")));
        ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i2.png"), ui->tabWidget->tabBar()->tabText(1)));
        ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i3.png"), ui->tabWidget->tabBar()->tabText(2)));
        ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i4.png"), ui->tabWidget->tabBar()->tabText(3)));
        ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i5.png"), ui->tabWidget->tabBar()->tabText(4)));

        QString name;
        //设置文件过滤器
        QStringList nameFilters;

        if (fInfo.suffix() == "aml") {
            name = "/*.aml";
            //设置文件过滤格式
            nameFilters << "*.aml";
        }
        if (fInfo.suffix() == "dat") {
            name = "/*.dat";
            //设置文件过滤格式
            nameFilters << "*.dat";
        }

        QDir dir(fInfo.path());

        //将过滤后的文件名称存入到files列表中
        QStringList files = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);

        int count = files.count();

        if (!ui->chkAll->isChecked()) {
            count = 1;
            files.clear();
            files.append(fInfo.fileName());
        }

        for (int i = 0; i < count; i++) {

            QString dfile = fInfo.path() + "/" + files.at(i);

            try {

#ifdef Q_OS_WIN32

                Decompile->start(appInfo.filePath() + "/iasl.exe", QStringList() << "-d" << dfile);
#endif

#ifdef Q_OS_LINUX

                Decompile->start(appInfo.filePath() + "/iasl", QStringList() << "-d" << dfile);

#endif

#ifdef Q_OS_MAC

                Decompile->start(appInfo.filePath() + "/iasl", QStringList() << "-d" << dfile);
#endif

                connect(Decompile, SIGNAL(finished(int)), this, SLOT(readDecompileResult(int)));

#ifdef Q_OS_WIN32

                Decompile->execute(appInfo.filePath() + "/iasl.exe", QStringList() << "-d" << dfile);
#endif

#ifdef Q_OS_LINUX

                Decompile->execute(appInfo.filePath() + "/iasl", QStringList() << "-d" << dfile);

#endif

#ifdef Q_OS_MAC

                Decompile->execute(appInfo.filePath() + "/iasl", QStringList() << "-d" << dfile);
#endif
            } catch (...) {
                qDebug() << "error";
                Decompile->terminate();
            }
            //qDebug() << files.at(i);

        } //for

        Decompile->terminate();

        fileName = fInfo.path() + "/" + fInfo.baseName() + ".dsl";
    }

    QFileInfo fi(fileName);
    if (fi.suffix().toLower() == "dsl") {
        ui->actionWrapWord->setChecked(false); //取消自动换行，影响dsl文件开启速度
        textEdit->setWrapMode(QsciScintilla::WrapNone);
        //miniEdit->setWrapMode(QsciScintilla::WrapNone);
    }

    return fileName;
}

void MainWindow::loadFile(const QString& fileName, int row, int col)
{

    loading = true;

    /*如果之前文件已打开，则返回已打开的文件*/
    for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {
        QWidget* pWidget = ui->tabWidget_textEdit->widget(i);

        lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout

        if (fileName == lblCurrentFile->text()) {
            ui->tabWidget_textEdit->setCurrentIndex(i);

            if (!ReLoad) {
                on_tabWidget_textEdit_tabBarClicked(i);
                return;
            } else {

                textEdit = getCurrentEditor(i);
            }
        }
    }

    if (!ReLoad)
        newFile();

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Application"),
            tr("Cannot read file %1:\n%2.")
                .arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return;
    }

    QTextStream in(&file);
#ifndef QT_NO_CURSOR
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
#endif

    QString text;
    //关于是否采用GBK编码的方式，再考虑
    /*QTextCodec* gCodec = QTextCodec::codecForName("GBK");
    if(gCodec)
    {
        text = gCodec->toUnicode(file.readAll());
    }
    else
        text = QString::fromUtf8(file.readAll());*/

    int ColNum, RowNum;
    if (ReLoad) //记录重装前的行号
    {

        textEdit->getCursorPosition(&RowNum, &ColNum);
    }

    //text = QString::fromUtf8(file.readAll());
    if (ui->actionUTF_8->isChecked())
        in.setCodec("UTF-8");
    if (ui->actionGBK->isChecked())
        in.setCodec("GBK");
    text = in.readAll();
    textEdit->setText(text);
    miniEdit->clear();
    miniEdit->setText(text);
    file.close();

    if (row != -1 && col != -1) {
        textEdit->setCursorPosition(row, col);
    }

    if (ReLoad) //文本重装之后刷新树并回到之前的位置
    {
        refresh_tree(textEdit);
        textEdit->setCursorPosition(RowNum, ColNum);
    }

#ifndef QT_NO_CURSOR
    QGuiApplication::restoreOverrideCursor();
#endif

    if (!ReLoad) {
        FileSystemWatcher::addWatchPath(fileName); //监控这个文件的变化
    }

    //给当前tab里面的lbl赋值
    QWidget* pWidget = ui->tabWidget_textEdit->currentWidget();
    lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout
    lblCurrentFile->setText(fileName);
    QFileInfo ft(fileName);
    ui->tabWidget_textEdit->tabBar()->setTabToolTip(ui->tabWidget_textEdit->currentIndex(), ft.fileName());

    //为拖拽tab准备拖动后的标题名
    ui->tabWidget_textEdit->currentWidget()->setWindowTitle("        " + ft.fileName());

    setCurrentFile(fileName);
    statusBar()->showMessage("                                              " + tr("File loaded"), 2000);

    ui->editShowMsg->clear();

    ui->treeWidget->clear();
    ui->treeWidget->repaint();
    lblLayer->setText("");
    lblMsg->setText("");

    ReLoad = false;

    QIcon icon(":/icon/d.png");
    ui->tabWidget_textEdit->tabBar()->setTabIcon(ui->tabWidget_textEdit->currentIndex(), icon);
    One = false;

    //最近打开的文件
    QSettings settings;
    QFileInfo fInfo(fileName);
    QCoreApplication::setOrganizationName("ic005k");
    QCoreApplication::setOrganizationDomain("github.com/ic005k");
    QCoreApplication::setApplicationName("QtiASL");
    settings.setValue("currentDirectory", fInfo.absolutePath());
    //qDebug() << settings.fileName(); //最近打开的文件所保存的位置
    m_recentFiles->setMostRecentFile(fileName);

    loading = false;
}

void MainWindow::setCurrentFile(const QString& fileName)
{
    curFile = fileName;
    textEdit->setModified(false);
    setWindowModified(false);

    shownName = curFile;
    if (curFile.isEmpty())
        shownName = tr("untitled") + ".dsl";

    setWindowFilePath(shownName);

    setWindowTitle(ver + shownName);

    ui->actionGo_to_previous_error->setEnabled(false);
    ui->actionGo_to_the_next_error->setEnabled(false);

    //初始化fsm
    QFileInfo f(shownName);
    ui->treeView->setRootIndex(model->index(f.path()));
    fsm_Index = model->index(f.path());
    set_return_text(f.path());
    ui->treeView->setCurrentIndex(model->index(shownName)); //并设置当前条目为打开的文件
    ui->treeView->setFocus();

    if (f.suffix().toLower() == "dsl" || f.suffix().toLower() == "cpp" || f.suffix().toLower() == "c") {
        ui->actionWrapWord->setChecked(false); //取消自动换行，影响dsl文件开启速度
        textEdit->setWrapMode(QsciScintilla::WrapNone);

        //设置编译功能使能
        ui->actionCompiling->setEnabled(true);

    } else {

        //设置编译功能屏蔽
        ui->actionCompiling->setEnabled(false);

        ui->dockWidget_6->setHidden(true);
    }

    ui->tabWidget_textEdit->setTabText(ui->tabWidget_textEdit->currentIndex(), f.fileName());
}

void MainWindow::set_return_text(QString text)
{

    QFontMetrics elideFont(ui->btnReturn->font());
    ui->btnReturn->setText(elideFont.elidedText(text, Qt::ElideLeft, ui->tabWidget_misc->width() - 100)); //省略号显示在左边
}

void MainWindow::Open()
{

    QStringList fileNames = QFileDialog::getOpenFileNames(this, "DSDT", "", "DSDT(*.aml *.dsl *.dat);;All(*.*)");
    for (int i = 0; i < fileNames.count(); i++) {
        if (!fileNames.at(i).isEmpty()) {

            loadFile(openFile(fileNames.at(i)), -1, -1);
            fileName = fileNames.at(i);
        }
    }
}

bool MainWindow::maybeSave(QString info)
{
    if (!textEdit->isModified())
        return true;

    //QMessageBox::StandardButton ret;
    int ret;
    if (!zh_cn) {

        ret = QMessageBox::warning(this, tr("Application"),
            tr("The document has been modified.\n"
               "Do you want to save your changes?\n\n")
                + info,
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    } else {
        QMessageBox box(QMessageBox::Warning, "QtiASL", "文件内容已修改，是否保存？\n\n" + info);
        box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        box.setButtonText(QMessageBox::Save, QString("保 存"));
        box.setButtonText(QMessageBox::Cancel, QString("取 消"));
        box.setButtonText(QMessageBox::Discard, QString("放 弃"));
        ret = box.exec();
    }

    switch (ret) {
    case QMessageBox::Save:
        return Save();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

bool MainWindow::Save()
{

    if (curFile.isEmpty()) {
        return SaveAs();
    } else {

        return saveFile(curFile);
    }
}

bool MainWindow::SaveAs()
{

    QFileDialog dialog;
    QString fn = dialog.getSaveFileName(this, "DSDT", "", "DSDT(*.dsl);;All(*.*)");
    if (fn.isEmpty())
        return false;

    //另存时，先移除当前的文件监控
    if (curFile != "") {
        QWidget* pWidget = ui->tabWidget_textEdit->widget(ui->tabWidget_textEdit->currentIndex());

        lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout
        FileSystemWatcher::removeWatchPath(lblCurrentFile->text());
    }

    //去重
    for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {

        QWidget* pWidget = ui->tabWidget_textEdit->widget(i);

        lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout

        if (fn == lblCurrentFile->text()) {

            ui->tabWidget_textEdit->removeTab(i);
        }
    }

    return saveFile(fn);
}

bool MainWindow::saveFile(const QString& fileName)
{

    QString errorMessage;

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QSaveFile file(fileName);
    if (file.open(QFile::WriteOnly | QFile::Text)) {

        QTextStream out(&file);
        out << textEdit->text();
        if (!file.commit()) {
            errorMessage = tr("Cannot write file %1:\n%2.")
                               .arg(QDir::toNativeSeparators(fileName), file.errorString());
        }
    } else {
        errorMessage = tr("Cannot open file %1 for writing:\n%2.")
                           .arg(QDir::toNativeSeparators(fileName), file.errorString());
    }
    QGuiApplication::restoreOverrideCursor();

    if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("Application"), errorMessage);
        return false;
    }

    //添加文件的监控
    bool add = true;

    for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {
        QWidget* pWidget = ui->tabWidget_textEdit->widget(i);

        lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout
        if (fileName == lblCurrentFile->text()) {
            add = false;
        }
    }
    if (add) {
        FileSystemWatcher::addWatchPath(fileName);
    }

    //ui->tabWidget_textEdit->tabBar()->setTabTextColor(textNumber, QColor(0, 0, 0));
    QIcon icon(":/icon/d.png");
    ui->tabWidget_textEdit->tabBar()->setTabIcon(ui->tabWidget_textEdit->currentIndex(), icon);
    One = false;

    //刷新文件路径
    QWidget* pWidget = ui->tabWidget_textEdit->widget(ui->tabWidget_textEdit->currentIndex());

    lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout
    lblCurrentFile->setText(fileName);
    QFileInfo ft(fileName);
    ui->tabWidget_textEdit->tabBar()->setTabToolTip(ui->tabWidget_textEdit->currentIndex(), ft.fileName());

    setCurrentFile(fileName);

    statusBar()->showMessage(tr("File saved"), 2000);

    SelfSaved = true; //文件监控提示

    ui->actionSave->setEnabled(false);

    return true;
}

void MainWindow::getACPITables(bool ssdt)
{

    QFileInfo appInfo(qApp->applicationDirPath());
    qDebug() << appInfo.filePath();

    QProcess dump;
    QProcess iasl;

    QString acpiDir = QDir::homePath() + "/Desktop/ACPI Tables/";

    QDir dir;
    if (dir.mkpath(acpiDir)) { }
    if (dir.mkpath(acpiDir + "temp/")) { }

    //设置文件过滤器
    QStringList nameFilters;

#ifdef Q_OS_WIN32
    dir.setCurrent(acpiDir);
    dump.execute(appInfo.filePath() + "/acpidump.exe", QStringList() << "-b");

    dir.setCurrent(acpiDir + "temp/");
    dump.execute(appInfo.filePath() + "/acpidump.exe", QStringList() << "-b");

    //设置文件过滤格式
    nameFilters << "ssdt*.dat";
    //将过滤后的文件名称存入到files列表中
    QStringList ssdtFiles = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);

    dir.setCurrent(acpiDir);
    if (!ssdt)
        iasl.execute(appInfo.filePath() + "/iasl.exe", QStringList() << "-e" << ssdtFiles << "-d"
                                                                     << "dsdt.dat");

#endif

#ifdef Q_OS_LINUX
    QStringList ssdtFiles;
    dump.execute(appInfo.filePath() + "/acpidump", QStringList() << "-b");
    //iasl.execute(appInfo.filePath() + "/iasl", QStringList() << "-d"
    //                                                         << "dsdt.dat");

#endif

#ifdef Q_OS_MAC
    dir.setCurrent(acpiDir);
    dump.execute(appInfo.filePath() + "/patchmatic", QStringList() << "-extractall" << acpiDir);

    dir.setCurrent(acpiDir + "temp/");
    dump.execute(appInfo.filePath() + "/patchmatic", QStringList() << "-extract" << acpiDir + "temp/");

    //设置文件过滤格式
    nameFilters << "ssdt*.aml";
    //将过滤后的文件名称存入到files列表中
    QStringList ssdtFiles = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);

    dir.setCurrent(acpiDir);
    if (!ssdt)
        iasl.execute(appInfo.filePath() + "/iasl", QStringList() << "-e" << ssdtFiles << "-d"
                                                                 << "dsdt.aml");

#endif

    deleteDirfile(acpiDir + "temp/");

    //获取当前加载的SSDT列表
    QCoreApplication::setOrganizationName("ic005k");
    QCoreApplication::setOrganizationDomain("github.com/ic005k");
    QCoreApplication::setApplicationName("SSDT");

    m_ssdtFiles->setNumOfRecentFiles(ssdtFiles.count()); //最多显示最近的文件个数

    for (int i = ssdtFiles.count() - 1; i > -1; i--) {
        qDebug() << ssdtFiles.at(i);

        QFileInfo fInfo(acpiDir + ssdtFiles.at(i));
        QSettings settings;
        settings.setValue("currentDirectory", fInfo.absolutePath());
        //qDebug() << settings.fileName(); //最近打开的文件所保存的位置
        m_ssdtFiles->setMostRecentFile(acpiDir + ssdtFiles.at(i));
    }

    if (!ssdt) {
        loadFile(acpiDir + "dsdt.dsl", -1, -1);

        QString dirAcpi = "file:" + acpiDir;
        QDesktopServices::openUrl(QUrl(dirAcpi, QUrl::TolerantMode));
    }
}

bool MainWindow::DeleteDirectory(const QString& path)
{
    if (path.isEmpty()) {
        return false;
    }

    QDir dir(path);
    if (!dir.exists()) {
        return true;
    }

    dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    QFileInfoList fileList = dir.entryInfoList();
    foreach (QFileInfo fi, fileList) {
        if (fi.isFile()) {
            fi.dir().remove(fi.fileName());
        } else {
            DeleteDirectory(fi.absoluteFilePath());
        }
    }
    return dir.rmpath(dir.absolutePath());
}

int MainWindow::deleteDirfile(QString dirName)
{
    QDir directory(dirName);
    if (!directory.exists()) {
        return true;
    }

    QString srcPath = QDir::toNativeSeparators(dirName);
    if (!srcPath.endsWith(QDir::separator()))
        srcPath += QDir::separator();

    QStringList fileNames = directory.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    bool error = false;
    for (QStringList::size_type i = 0; i != fileNames.size(); ++i) {
        QString filePath = srcPath + fileNames.at(i);
        QFileInfo fileInfo(filePath);
        if (fileInfo.isFile() || fileInfo.isSymLink()) {
            QFile::setPermissions(filePath, QFile::WriteOwner);
            if (!QFile::remove(filePath)) {
                error = true;
            }
        } else if (fileInfo.isDir()) {
            if (!deleteDirfile(filePath)) {
                error = true;
            }
        }
    }

    if (!directory.rmdir(QDir::toNativeSeparators(directory.path()))) {
        error = true;
    }
    return !error;
}

void MainWindow::btnGenerate_clicked()
{
    getACPITables(false);
}

void MainWindow::btnCompile_clicked()
{
    QFileInfo cf_info(curFile);
    if (cf_info.suffix().toLower() != "dsl" && cf_info.suffix().toLower() != "cpp" && cf_info.suffix().toLower() != "c") {
        return;
    }

    QFileInfo appInfo(qApp->applicationDirPath());
    co = new QProcess;

    if (!curFile.isEmpty())
        Save();

    lblMsg->setText(tr("Compiling..."));

    qTime.start();

    if (cf_info.suffix().toLower() == "dsl") {
        QString op = ui->cboxCompilationOptions->currentText().trimmed();

#ifdef Q_OS_WIN32
        co->start(appInfo.filePath() + "/iasl.exe", QStringList() << op << curFile);
#endif

#ifdef Q_OS_LINUX
        co->start(appInfo.filePath() + "/iasl", QStringList() << op << curFile);
#endif

#ifdef Q_OS_MAC
        co->start(appInfo.filePath() + "/iasl", QStringList() << op << curFile);
#endif

        connect(co, SIGNAL(finished(int)), this, SLOT(readResult(int)));
    }

    //cpp
    if (cf_info.suffix().toLower() == "cpp") {
        QDir::setCurrent(QFileInfo(curFile).path());
        QString tName = QFileInfo(curFile).path() + "/" + QFileInfo(curFile).baseName();
        co->start("g++", QStringList() << curFile << "-o" << tName);

        connect(co, SIGNAL(finished(int)), this, SLOT(readCppResult(int)));
    }

    //c
    if (cf_info.suffix().toLower() == "c") {
        QDir::setCurrent(QFileInfo(curFile).path());
        QString tName = QFileInfo(curFile).path() + "/" + QFileInfo(curFile).baseName();
        co->start("gcc", QStringList() << curFile << "-o" << tName);

        connect(co, SIGNAL(finished(int)), this, SLOT(readCppResult(int)));
    }

    /*仅供测试*/
    //connect(co , SIGNAL(readyReadStandardOutput()) , this , SLOT(readResult(int)));
    //QByteArray res = co.readAllStandardOutput(); //获取标准输出
    //qDebug() << "Out" << QString::fromLocal8Bit(res);
}

void MainWindow::setMark()
{
    //回到第一行
    QTextBlock block = ui->editShowMsg->document()->findBlockByNumber(0);
    ui->editShowMsg->setTextCursor(QTextCursor(block));

    //将"error"高亮
    QString search_text = "Error";
    if (search_text.trimmed().isEmpty()) {
        QMessageBox::information(this, tr("Empty search field"), tr("The search field is empty."));
    } else {
        QTextDocument* document = ui->editShowMsg->document();
        bool found = false;
        QTextCursor highlight_cursor(document);
        QTextCursor cursor(document);

        cursor.beginEditBlock();
        QTextCharFormat color_format(highlight_cursor.charFormat());
        color_format.setForeground(Qt::red);
        color_format.setBackground(Qt::yellow);

        while (!highlight_cursor.isNull() && !highlight_cursor.atEnd()) {
            //查找指定的文本，匹配整个单词
            highlight_cursor = document->find(search_text, highlight_cursor, QTextDocument::FindCaseSensitively);
            if (!highlight_cursor.isNull()) {
                if (!found)
                    found = true;

                highlight_cursor.mergeCharFormat(color_format);
            }
        }

        cursor.endEditBlock();
    }
}

void MainWindow::readDecompileResult(int exitCode)
{
    loading = true;

    QString result, result1;
    result = QString::fromUtf8(Decompile->readAllStandardOutput());
    result1 = QString::fromUtf8(Decompile->readAllStandardError());

    ui->editShowMsg->clear();
    ui->editShowMsg->append(result);
    ui->editShowMsg->append(result1);

    if (exitCode == 0) {
        //成功

        //标记tab头
        int info_count = 0;

        ui->tabWidget->setTabText(1, tr("Errors") + " (" + QString::number(info_count) + ")");

        ui->tabWidget->setTabText(2, tr("Warnings") + " (" + QString::number(info_count) + ")");

        ui->tabWidget->setTabText(3, tr("Remarks") + " (" + QString::number(info_count) + ")");

        ui->tabWidget->setTabText(4, tr("Scribble"));

        ui->tabWidget->setCurrentIndex(0);

    } else {
    }

    loading = false;
}

void MainWindow::readCppRunResult(int exitCode)
{
    if (exitCode == 0) {

        QString result;

        result = QString::fromUtf8(co->readAll());

        ui->editShowMsg->append(result);
    }
}

void MainWindow::readCppResult(int exitCode)
{
    ui->editShowMsg->clear();
    ui->editErrors->clear();
    ui->editRemarks->clear();
    ui->editWarnings->clear();

    //标记tab头
    ui->tabWidget->setTabText(1, tr("Errors"));
    ui->tabWidget->setTabText(2, tr("Warnings"));
    ui->tabWidget->setTabText(3, tr("Remarks"));

    QString result, result2;

    result = QString::fromUtf8(co->readAll());
    result2 = QString::fromUtf8(co->readAllStandardError());

    float a = qTime.elapsed() / 1000.00;
    lblMsg->setText(tr("Compiled") + "(" + QTime::currentTime().toString() + "    " + QString::number(a, 'f', 2) + " s)");

    //清除所有标记
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERDELETEALL);

    if (exitCode == 0) {

        ui->editShowMsg->append(result);
        ui->editShowMsg->append(result2);

        ui->actionGo_to_previous_error->setEnabled(false);
        ui->actionGo_to_the_next_error->setEnabled(false);
        ui->tabWidget->setCurrentIndex(0);
        ui->listWidget->setCurrentRow(0);

        co = new QProcess;
        QString tName = QFileInfo(curFile).path() + "/" + QFileInfo(curFile).baseName();
        if (win)
            tName = tName + ".exe";

        QStringList strList;
        co->start(tName, strList);

        connect(co, SIGNAL(finished(int)), this, SLOT(readCppRunResult(int)));

        if (!zh_cn)
            QMessageBox::information(this, "QtiASL", "Compilation successful.");
        else {
            QMessageBox message(QMessageBox::Information, "QtiASL", tr("Compilation successful."));
            message.setStandardButtons(QMessageBox::Ok);
            message.setButtonText(QMessageBox::Ok, QString(tr("Ok")));
            message.exec();
        }
    } else {
        ui->actionGo_to_previous_error->setEnabled(true);
        ui->actionGo_to_the_next_error->setEnabled(true);

        ui->editErrors->append(result);
        ui->editErrors->append(result2);
        ui->tabWidget->setCurrentIndex(1);
        ui->listWidget->setCurrentRow(1);

        //回到第一行
        QTextBlock block = ui->editErrors->document()->findBlockByNumber(0);
        ui->editErrors->setTextCursor(QTextCursor(block));

        goCppNextError();
    }

    ui->dockWidget_6->setHidden(false);
    InfoWinShow = true;
    ui->actionInfo_win->setChecked(true);
}

/*读取编译结果信息dsl*/
void MainWindow::readResult(int exitCode)
{
    loading = true;

    textEditTemp->clear();

    QString result, result2;
    /*QTextCodec* gCodec = QTextCodec::codecForName("GBK");

    if(gCodec)
    {
        result = gCodec->toUnicode(co->readAll());
        result2 = gCodec->toUnicode(co->readAllStandardError());
    }
    else
    {
        result = QString::fromUtf8(co->readAll());
        result2 = QString::fromUtf8(co->readAllStandardError());
    }*/

    result = QString::fromUtf8(co->readAll());
    result2 = QString::fromUtf8(co->readAllStandardError());

    textEditTemp->append(result);

    textEditTemp->append(result2);

    //分离基本信息
    ui->editShowMsg->clear();
    QVector<QString> list;
    for (int i = 0; i < textEditTemp->document()->lineCount(); i++) {
        QString str = textEditTemp->document()->findBlockByNumber(i).text();

        list.push_back(str);
        QString str_sub = str.trimmed();
        if (str_sub.mid(0, 5) == "Error" || str_sub.mid(0, 7) == "Warning" || str_sub.mid(0, 6) == "Remark") {
            for (int j = 0; j < i - 2; j++)
                ui->editShowMsg->append(list.at(j));

            break;
        }
    }

    //分离信息
    separ_info("Warning", ui->editWarnings);
    separ_info("Remark", ui->editRemarks);
    separ_info("Error", ui->editErrors);
    separ_info("Optimization", ui->editOptimizations);

    //回到第一行
    QTextBlock block = ui->editErrors->document()->findBlockByNumber(0);
    ui->editErrors->setTextCursor(QTextCursor(block));

    //清除所有标记
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERDELETEALL);

    float a = qTime.elapsed() / 1000.00;
    lblMsg->setText(tr("Compiled") + "(" + QTime::currentTime().toString() + "    " + QString::number(a, 'f', 2) + " s)");

    if (exitCode == 0) {

        ui->actionGo_to_previous_error->setEnabled(false);
        ui->actionGo_to_the_next_error->setEnabled(false);
        ui->tabWidget->setCurrentIndex(0);
        ui->listWidget->setCurrentRow(0);
        ui->listWidget->setFocus();

        if (!zh_cn)
            QMessageBox::information(this, "QtiASL", "Compilation successful.");
        else {
            QMessageBox message(QMessageBox::Information, "QtiASL", tr("Compilation successful."));
            message.setStandardButtons(QMessageBox::Ok);
            message.setButtonText(QMessageBox::Ok, QString(tr("Ok")));
            message.exec();
        }

    } else {
        ui->actionGo_to_previous_error->setEnabled(true);
        ui->actionGo_to_the_next_error->setEnabled(true);
        ui->tabWidget->setCurrentIndex(1);
        ui->listWidget->setCurrentRow(1);
        ui->listWidget->setFocus();

        on_btnNextError();
    }

    ui->dockWidget_6->setHidden(false);
    InfoWinShow = true;
    ui->actionInfo_win->setChecked(true);

    loading = false;
}

void MainWindow::textEdit_cursorPositionChanged()
{
    set_currsor_position(textEdit);

    if (!loading) {

        int i = ui->tabWidget_textEdit->currentIndex();

        if (!textEdit->isModified()) {
            QIcon icon(":/icon/d.png");
            ui->tabWidget_textEdit->tabBar()->setTabIcon(i, icon);
            ui->actionSave->setEnabled(false);
        }

        if (textEdit->isModified()) {
            QIcon icon(":/icon/s.png");
            ui->tabWidget_textEdit->tabBar()->setTabIcon(i, icon);
            ui->actionSave->setEnabled(true);
        }
    }
}

void MainWindow::set_currsor_position(QsciScintilla* textEdit)
{

    int ColNum, RowNum;
    textEdit->getCursorPosition(&RowNum, &ColNum);

    QString msg = tr("Row") + " : " + QString::number(RowNum + 1) + "    " + tr("Column") + " : " + QString::number(ColNum);

    locationLabel->setText(msg);

    locationLabel->setAlignment(Qt::AlignCenter);
    locationLabel->setMinimumSize(locationLabel->sizeHint());
    statusBar()->setStyleSheet(QString("QStatusBar::item{border: 0px}")); // 设置不显示label的边框
    statusBar()->setSizeGripEnabled(false); //设置是否显示右边的大小控制点

    //联动treeWidget
    mem_linkage(ui->treeWidget, RowNum);
}

void MainWindow::miniEdit_cursorPositionChanged()
{

    int ColNum, RowNum;
    miniEdit->getCursorPosition(&RowNum, &ColNum);
    textEdit->setCursorPosition(RowNum, ColNum);

    if (!ui->editFind->hasFocus())
        textEdit->setFocus();

    miniEdit->clearFocus();
}

/*换行之后，1s后再刷新成员树*/
void MainWindow::timer_linkage()
{
    if (!loading) {

        on_btnRefreshTree();

        timer->stop();
    }
}

/*单击文本任意位置，当前代码块与成员树进行联动*/
void MainWindow::mem_linkage(QTreeWidget* tw, int RowNum)
{

    //int RowNum, ColNum;
    //textEdit->getCursorPosition(&RowNum , &ColNum);

    /*进行联动的条件：装载文件没有进行&成员树不为空&不是始终在同一行里面*/
    if (!loading && tw->topLevelItemCount() > 0 && preRow != RowNum) {
        int treeSn = 0;
        QTreeWidgetItemIterator it(tw);
        textEditBack->setCursorPosition(RowNum, 0); //后台进行

        preRow = RowNum;

        for (int j = RowNum; j > -1; j--) //从当前行往上寻找Scope、Device、Method
        {
            QString str = textEditBack->text(j).trimmed();
            if (str.mid(0, 5) == "Scope" || str.mid(0, 5) == "Devic" || str.mid(0, 5) == "Metho") {

                while (*it) {
                    treeSn = (*it)->text(1).toInt();

                    if (treeSn == j) {
                        tw->setCurrentItem((*it));
                        //状态栏上显示层次结构
                        lblLayer->setText(getLayerName((*it)));
                        //editLayer->setText(getLayerName((*it)));

                        break;
                    }

                    ++it;
                }

                break;
            }
        }

        //qDebug() << ColNum << RowNum;
    }
}

/*行号区域的宽度：目前在主编辑框内已弃用，为编译输出信息显示预留*/
int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    //int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    return 0;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent* e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

    //![extraAreaPaintEvent_0]

    //![extraAreaPaintEvent_1]
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    //![extraAreaPaintEvent_1]

    //![extraAreaPaintEvent_2]
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(Qt::yellow).lighter(160);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void MainWindow::on_btnReplace()
{

    textEdit->replace(ui->editReplace->text());
}

void MainWindow::ReplaceAll()
{
    /*loading = true;

    int total = 0;

    QString str = ui->editFind->text().trimmed();
    if (!textEdit->findFirst(str, true, CaseSensitive, false, true, true))
        return;

    for (int i = 0; i < 100; i++) {
        on_btnReplaceFind();
        total++;

        if (!textEdit->findFirst(str, true, CaseSensitive, false, true, true))
            break;
    }
    //qInfo() << total;
    loading = false;*/

    if (ui->editReplace->text().trimmed() == "")
        return;

    int row, col;
    textEdit->getCursorPosition(&row, &col);

    QString searchtext = ui->editFind->currentText().trimmed();
    QString replacetext = ui->editReplace->text().trimmed();
    QString document = textEdit->text();
    if (!ui->chkCaseSensitive->isChecked())
        document.replace(searchtext, replacetext, Qt::CaseInsensitive);
    else
        document.replace(searchtext, replacetext);

    textEdit->setText(document);

    textEdit->setCursorPosition(row, col);
}

void MainWindow::forEach(QString str, QString strReplace)
{
    if (textEdit->findFirst(str, true, CaseSensitive, false, true, true)) {

        textEdit->replace(strReplace);
    }
}

void MainWindow::on_btnFindNext()
{

    clearSearchHighlight(textEdit);

    QString str = ui->editFind->currentText().trimmed();
    //正则、大小写、匹配整个词、循环查找、向下或向上：目前已开启向下的循环查找

    if (textEdit->findFirst(str, true, CaseSensitive, false, true, true)) {

        if (red < 55) {

            QPalette palette;
            palette.setColor(QPalette::Text, Qt::white);
            ui->editFind->setPalette(palette);

            palette = ui->editFind->palette();
            palette.setColor(QPalette::Base, QColor(50, 50, 50, 255));
            ui->editFind->setPalette(palette);

        } else {

            QPalette palette;
            palette.setColor(QPalette::Text, Qt::black);
            ui->editFind->setPalette(palette);

            palette = ui->editFind->palette();
            palette.setColor(QPalette::Base, Qt::white);
            ui->editFind->setPalette(palette);
        }

    } else {
        if (str.count() > 0) {

            //字色
            QPalette palette;
            palette.setColor(QPalette::Text, Qt::white);
            ui->editFind->setPalette(palette);

            palette = ui->editFind->palette();
            palette.setColor(QPalette::Base, QColor(255, 70, 70));
            ui->editFind->setPalette(palette);
        }
    }

    find_down = true;
    find_up = false;

    highlighsearchtext(str);
}

void MainWindow::on_btnFindPrevious()
{

    clearSearchHighlight(textEdit);

    QString name = ui->editFind->currentText().trimmed();
    std::string str = name.toStdString();
    const char* ch = str.c_str();

    int flags;
    if (CaseSensitive)
        flags = QsciScintilla::SCFIND_MATCHCASE | QsciScintilla::SCFIND_REGEXP;
    else
        flags = QsciScintilla::SCFIND_REGEXP;

    textEdit->SendScintilla(QsciScintilla::SCI_SEARCHANCHOR);
    if (textEdit->SendScintilla(QsciScintilla::SCI_SEARCHPREV, flags, ch) == -1) {

    } else {
        if (red < 55) {

        } else {
        }
    }

    QScrollBar* vscrollbar = new QScrollBar;
    vscrollbar = textEdit->verticalScrollBar();

    QScrollBar* hscrollbar = new QScrollBar;
    hscrollbar = textEdit->horizontalScrollBar();

    int row, col, vs_pos, hs_pos;
    vs_pos = vscrollbar->sliderPosition();
    textEdit->getCursorPosition(&row, &col);
    if (row < vs_pos)
        vscrollbar->setSliderPosition(row - 5);

    hs_pos = hscrollbar->sliderPosition();
    QPainter p(this);
    QFontMetrics fm = p.fontMetrics();
    QString t = textEdit->text(row).mid(0, col);
    int char_w;
    //char_w = fm.horizontalAdvance(t); //一个字符的宽度
    char_w = fm.averageCharWidth();
    qDebug() << col;
    if (char_w < textEdit->viewport()->width())
        hscrollbar->setSliderPosition(0);
    else
        hscrollbar->setSliderPosition(char_w); // + fm.horizontalAdvance(name));

    //qDebug() << col << textEditList.at(textNumber)->horizontalScrollBar()->sliderPosition();

    find_down = false;
    find_up = true;

    highlighsearchtext(name);
}

void MainWindow::on_treeWidget_itemClicked(QTreeWidgetItem* item, int column)
{

    if (column == 0 && !loading) {
        int lines = item->text(1).toInt();
        textEdit->setCursorPosition(lines, 0);
        textEdit->setFocus();
    }
}

void MainWindow::treeWidgetBack_itemClicked(QTreeWidgetItem* item, int column)
{

    if (column == 0) {
        int lines = item->text(1).toInt();
        textEdit->setCursorPosition(lines, 0);
        textEdit->setFocus();
    }
}

void MainWindow::on_editShowMsg_cursorPositionChanged()
{
    set_cursor_line_color(ui->editShowMsg);
}

void MainWindow::set_cursor_line_color(QTextEdit* edit)
{
    QList<QTextEdit::ExtraSelection> extraSelection;
    QTextEdit::ExtraSelection selection;
    QColor lineColor = QColor(255, 255, 0, 50);
    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = edit->textCursor();
    //selection.cursor.clearSelection();
    extraSelection.append(selection);
    edit->setExtraSelections(extraSelection);
}

void MainWindow::goCppPreviousError()
{
    miniDlg->close();

    const QTextCursor cursor = ui->editErrors->textCursor();

    int RowNum = cursor.blockNumber() - 2;

    QTextBlock block = ui->editErrors->document()->findBlockByNumber(RowNum);
    ui->editErrors->setTextCursor(QTextCursor(block));

    bool yes = false;

    for (int i = RowNum; i > -1; i--) {
        QTextBlock block = ui->editErrors->document()->findBlockByNumber(i);
        ui->editErrors->setTextCursor(QTextCursor(block));

        QString str = ui->editErrors->document()->findBlockByLineNumber(i).text();
        QString sub = str.trimmed();

        if (sub.contains(curFile)) {

            yes = true;

            QTextBlock block = ui->editErrors->document()->findBlockByNumber(i + 1);
            ui->editErrors->setTextCursor(QTextCursor(block));

            QList<QTextEdit::ExtraSelection> extraSelection;
            QTextEdit::ExtraSelection selection;
            //QColor lineColor = QColor(Qt::gray).lighter(150);
            QColor lineColor = QColor(Qt::red);
            selection.format.setForeground(Qt::white);
            selection.format.setBackground(lineColor);
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            selection.cursor = ui->editErrors->textCursor();
            selection.cursor.clearSelection();
            extraSelection.append(selection);
            ui->editErrors->setExtraSelections(extraSelection);

            //定位到错误行
            getCppErrorLine(i + 1);

            ui->tabWidget->setCurrentIndex(1);
            ui->listWidget->setCurrentRow(1);

            break;
        }
    }

    if (!yes) {
        goCppNextError();
    }
}

void MainWindow::goCppNextError()
{
    miniDlg->close();

    const QTextCursor cursor = ui->editErrors->textCursor();
    int RowNum = cursor.blockNumber();

    QTextBlock block = ui->editErrors->document()->findBlockByNumber(RowNum);
    ui->editErrors->setTextCursor(QTextCursor(block));

    bool yes = false;

    for (int i = RowNum; i < ui->editErrors->document()->lineCount(); i++) {
        QTextBlock block = ui->editErrors->document()->findBlockByNumber(i);
        ui->editErrors->setTextCursor(QTextCursor(block));

        QString str = ui->editErrors->document()->findBlockByLineNumber(i).text();
        QString sub = str.trimmed();

        if (sub.contains(curFile)) {

            yes = true;

            QTextBlock block = ui->editErrors->document()->findBlockByNumber(i + 1);
            ui->editErrors->setTextCursor(QTextCursor(block));

            QList<QTextEdit::ExtraSelection> extraSelection;
            QTextEdit::ExtraSelection selection;
            QColor lineColor = QColor(Qt::red);
            selection.format.setForeground(Qt::white);
            selection.format.setBackground(lineColor);
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            selection.cursor = ui->editErrors->textCursor();
            selection.cursor.clearSelection();
            extraSelection.append(selection);
            ui->editErrors->setExtraSelections(extraSelection);

            //定位到错误行
            getCppErrorLine(i + 1);

            ui->tabWidget->setCurrentIndex(1);
            ui->listWidget->setCurrentRow(1);

            break;
        }
    }

    if (!yes) {
        goCppPreviousError();
    }
}

void MainWindow::on_btnNextError()
{
    miniDlg->close();

    const QTextCursor cursor = ui->editErrors->textCursor();
    int RowNum = cursor.blockNumber();

    QTextBlock block = ui->editErrors->document()->findBlockByNumber(RowNum);
    ui->editErrors->setTextCursor(QTextCursor(block));

    for (int i = RowNum + 1; i < ui->editErrors->document()->lineCount(); i++) {
        QTextBlock block = ui->editErrors->document()->findBlockByNumber(i);
        ui->editErrors->setTextCursor(QTextCursor(block));

        QString str = ui->editErrors->document()->findBlockByLineNumber(i).text();
        QString sub = str.trimmed();

        if (sub.mid(0, 5) == "Error") {
            QTextBlock block = ui->editErrors->document()->findBlockByNumber(i);
            ui->editErrors->setTextCursor(QTextCursor(block));

            QList<QTextEdit::ExtraSelection> extraSelection;
            QTextEdit::ExtraSelection selection;
            QColor lineColor = QColor(Qt::red);
            selection.format.setForeground(Qt::white);
            selection.format.setBackground(lineColor);
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            selection.cursor = ui->editErrors->textCursor();
            selection.cursor.clearSelection();
            extraSelection.append(selection);
            ui->editErrors->setExtraSelections(extraSelection);

            //定位到错误行
            getErrorLine(i);

            ui->tabWidget->setCurrentIndex(1);

            break;
        }

        if (i == ui->editShowMsg->document()->lineCount() - 1)
            on_btnPreviousError();
    }
}

void MainWindow::on_btnPreviousError()
{
    miniDlg->close();

    const QTextCursor cursor = ui->editErrors->textCursor();
    int RowNum = cursor.blockNumber();

    QTextBlock block = ui->editErrors->document()->findBlockByNumber(RowNum);
    ui->editErrors->setTextCursor(QTextCursor(block));

    for (int i = RowNum - 1; i > -1; i--) {
        QTextBlock block = ui->editErrors->document()->findBlockByNumber(i);
        ui->editErrors->setTextCursor(QTextCursor(block));

        QString str = ui->editErrors->document()->findBlockByLineNumber(i).text();
        QString sub = str.trimmed();

        if (sub.mid(0, 5) == "Error") {
            QTextBlock block = ui->editErrors->document()->findBlockByNumber(i);
            ui->editErrors->setTextCursor(QTextCursor(block));

            QList<QTextEdit::ExtraSelection> extraSelection;
            QTextEdit::ExtraSelection selection;
            //QColor lineColor = QColor(Qt::gray).lighter(150);
            QColor lineColor = QColor(Qt::red);
            selection.format.setForeground(Qt::white);
            selection.format.setBackground(lineColor);
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            selection.cursor = ui->editErrors->textCursor();
            selection.cursor.clearSelection();
            extraSelection.append(selection);
            ui->editErrors->setExtraSelections(extraSelection);

            //定位到错误行
            getErrorLine(i);

            ui->tabWidget->setCurrentIndex(1);

            break;
        }

        if (i == 0)
            on_btnNextError();
    }
}

void MainWindow::gotoLine(QTextEdit* edit)
{
    QString text, str2, str3;
    int line = 0;
    bool skip = true;
    const QTextCursor cursor = edit->textCursor();
    int RowNum = cursor.blockNumber();

    text = edit->document()->findBlockByNumber(RowNum).text().trimmed();

    if (text != "") {
        for (int j = 3; j < text.count(); j++) {

            if (text.mid(j, 1) == ":") {
                str2 = text.mid(0, j);
                skip = false;
                break;
            }
        }

        if (skip) {
            //再看看上一行
            text = edit->document()->findBlockByNumber(RowNum - 1).text().trimmed();
            if (text != "") {
                for (int j = 3; j < text.count(); j++) {

                    if (text.mid(j, 1) == ":") {
                        str2 = text.mid(0, j);

                        break;
                    }
                }
            }
        }

        for (int k = str2.count(); k > 0; k--) {
            if (str2.mid(k - 1, 1) == " ") {
                str3 = str2.mid(k, str2.count() - k);

                //定位到错误行
                line = str3.toInt();
                textEdit->setCursorPosition(line - 1, 0);

                QString strLine = textEdit->text(line - 1);

                for (int i = 0; i < strLine.count(); i++) {
                    QString strSub = strLine.trimmed().mid(0, 1);
                    if (strLine.mid(i, 1) == strSub) {
                        textEdit->setCursorPosition(line - 1, i);
                        break;
                    }
                }

                for (int i = 0; i < strLine.count(); i++) {
                    if (strLine.mid(i, 1) == "(") {
                        textEdit->setCursorPosition(line - 1, i + 1);
                        break;
                    }
                }

                textEdit->setFocus();

                break;
            }
        }
    }
}

void MainWindow::setErrorMarkers(int linenr)
{
    //SCI_MARKERGET 参数用来设置标记，默认为圆形标记
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERGET, linenr - 1);
    //SCI_MARKERSETFORE，SCI_MARKERSETBACK设置标记前景和背景标记
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERSETFORE, 0, QColor(Qt::red));
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERSETBACK, 0, QColor(Qt::red));
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERADD, linenr - 1);
    //下划线
    //textEdit->SendScintilla(QsciScintilla::SCI_STYLESETUNDERLINE, linenr, true);
    //textEdit->SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 0, QsciScintilla::SC_MARK_UNDERLINE);
}
void MainWindow::getCppErrorLine(int i)
{
    //定位到错误行
    QString str1 = ui->editErrors->document()->findBlockByLineNumber(i).text().trimmed();
    QString str2, str3, str4;
    if (str1 != "") {
        for (int j = 0; j < str1.count(); j++) {

            if (str1.mid(j, 1) == ":") {
                str2 = str1.mid(j + 1, str1.count() - j);
                //qDebug() << str2;
                break;
            }
        }

        for (int k = 0; k < str2.count(); k++) {
            if (str2.mid(k, 1) == ":") {
                str3 = str2.mid(0, k);
                str4 = str2.mid(k + 1, str2.count() - k);
                //qDebug() << str3 << str4;
                int linenr = str3.toInt();
                int col = 0;

                for (int n = 0; n < str4.count(); n++) {
                    if (str4.mid(n, 1) == ":") {
                        str4 = str4.mid(0, n);
                        //qDebug() << str4;
                        col = str4.toInt();
                        break;
                    }
                }

                //定位到错误行
                textEdit->setCursorPosition(linenr - 1, col - 1);
                setErrorMarkers(linenr);
                textEdit->setFocus();

                break;
            }
        }
    }
}

void MainWindow::getErrorLine(int i)
{
    //定位到错误行
    QString str1 = ui->editErrors->document()->findBlockByLineNumber(i - 1).text().trimmed();
    QString str2, str3;
    if (str1 != "") {
        for (int j = 3; j < str1.count(); j++) {

            if (str1.mid(j, 1) == ":") {
                str2 = str1.mid(0, j);

                break;
            }
        }

        for (int k = str2.count(); k > 0; k--) {
            if (str2.mid(k - 1, 1) == " ") {
                str3 = str2.mid(k, str2.count() - k);

                int linenr = str3.toInt();

                //定位到错误行
                textEdit->setCursorPosition(linenr - 1, 0);

                QString strLine = textEdit->text(linenr - 1);
                for (int i = 0; i < strLine.count(); i++) {
                    QString strSub = strLine.trimmed().mid(0, 1);
                    if (strLine.mid(i, 1) == strSub) {
                        textEdit->setCursorPosition(linenr - 1, i);
                        break;
                    }
                }
                for (int i = 0; i < strLine.count(); i++) {
                    if (strLine.mid(i, 1) == "(") {
                        textEdit->setCursorPosition(linenr - 1, i + 1);
                        break;
                    }
                }

                setErrorMarkers(linenr);
                textEdit->setFocus();

                break;
            }
        }
    }
}

void MainWindow::on_editShowMsg_selectionChanged()
{
    QString row = ui->editShowMsg->textCursor().selectedText();
    int row_num = row.toUInt();
    if (row_num > 0) {

        textEdit->setCursorPosition(row_num - 1, 0);

        textEdit->setFocus();
    }
}

void MainWindow::textEdit_textChanged()
{

    if (!loading) {
    }
}

void MainWindow::editFind_returnPressed()
{

    on_btnFindNext();
}

const char* QscilexerCppAttach::keywords(int set) const
{
    //if(set == 1 || set == 3)
    //    return QsciLexerCPP::keywords(set);

    if (set == 1)
        return "and and_eq asm auto bitand bitor bool break case "
               "catch char class compl const const_cast continue "
               "default delete do double dynamic_cast else enum "
               "explicit export extern false float for friend goto if "
               "inline int long mutable namespace new not not_eq "
               "operator or or_eq private protected public register "
               "reinterpret_cast return short signed sizeof static "
               "static_cast struct switch template this throw true "
               "try typedef typeid typename union unsigned using "
               "virtual void volatile wchar_t while xor xor_eq "

               "External Scope Device Method Name If While Break Return ElseIf Switch Case Else "
               "Default Field OperationRegion Package DefinitionBlock Offset CreateDWordField CreateByteField "
               "CreateBitField CreateWordField CreateQWordField Buffer ToInteger ToString ToUUID ToUuid ToHexString ToDecimalString ToBuffer ToBcd"
               "CondRefOf FindSetLeftBit FindSetRightBit FromBcd Function CreateField "

               "Acquire Add Alias And "
               "BankField AccessAs CondRefOf ExtendedMemory ExtendedSpace "
               "BreakPoint Concatenate ConcatenateResTemplate Connection Continue CopyObject DataTableRegion Debug Decrement DerefOf "
               "Divide Dma Arg0 Arg1 Arg2 Arg3 Arg4 Arg5 Arg6 "
               "DWordIo DWordIO EisaId EndDependentFn Event ExtendedIo Fatal FixedDma FixedIo GpioInt GpioIo "
               "Increment Index IndexField Interrupt Io IO Irq IRQ IrqNoFlags "
               "LAnd LEqual LGreater LGreaterEqual LLess LLessEqual LNot LNotEqual Load LOr Match Mid Mod Multiply "
               "Mutex NAnd NoOp NOr Not Notify ObjectType Or PowerResource Revision "
               "Memory32Fixed "
               "DWordMemory Local0 Local1 Local2 Local3 Local4 Local5 Local6 Local7 "
               "DWordSpace One Ones Processor QWordIo QWordIO Memory24 Memory32 VendorLong VendorShort Wait WordBusNumber WordIo WordSpace "
               "I2cSerialBusV2 Include LoadTable QWordMemory QWordSpace RawDataBuffer RefOf Register Release Reset ResourceTemplate ShiftLeft ShiftRight Signal SizeOf Sleep "
               "SpiSerialBusV2 Stall StartDependentFn StartDependentFnNoPri Store Subtract ThermalZone Timer ToBcd UartSerialBusV2 Unicode Unload "
               "Xor Zero ";

    if (set == 2)
        return "SubDecode PosDecode AttribBytes SubDecode PosDecode ReadWrite ReadOnly Width8bit Width16bit Width32bit Width64bit Width128bit Width256bit "
               "UserDefRegionSpace SystemIO SystemMemory TypeTranslation TypeStatic AttribRawBytes AttribRawProcessBytes Serialized NotSerialized "
               "key dict array TypeA TypeB TypeF AnyAcc ByteAcc Cacheable WriteCombining Prefetchable NonCacheable PullDefault PullUp PullDown PullNone "
               "MethodObj UnknownObj IntObj DeviceObj MutexObj PkgObj FieldUnitObj StrObj Edge Level ActiveHigh ActiveLow ActiveBoth "
               "BuffObj EventObj OpRegionObj PowerResObj ProcessorObj ThermalZoneObj BuffFieldObj DDBHandleObj None ReturnArg PolarityHigh PolarityLow ThreeWireMode FourWireMode "
               "MinFixed MinNotFixed MaxFixed MaxNotFixed ResourceConsumer ResourceProducer MinFixed MinNotFixed MaxFixed MaxNotFixed ClockPolarityLow ClockPolarityHigh "
               "ResourceConsumer ResourceProducer SubDecode PosDecode MaxFixed MaxNotFixed GeneralPurposeIo GenericSerialBus FFixedHW ClockPhaseFirst ClockPhaseSecond "
               "MTR MEQ MLE MLT MGE MGT WordAcc DWordAcc QWordAcc BufferAcc Lock NoLock AddressRangeMemory AddressRangeReserved AddressRangeNVS AddressRangeACPI FlowControlHardware "
               "AttribQuick AttribSendReceive AttribByte AttribWord AttribBlock AttribProcessCall AttribBlockProcessCall IoRestrictionNone IoRestrictionInputOnly IoRestrictionOutputOnly IoRestrictionNoneAndPreserve "
               "Preserve WriteAsOnes WriteAsZeros Compatibility BusMaster NotBusMaster Transfer8 Transfer16 Transfer8_16 DataBitsFive DataBitsSix DataBitsSeven ParityTypeOdd ParityTypeEven FlowControlNone FlowControlXon "
               "ResourceConsumer ResourceProducer SubDecode PosDecode MinFixed MinNotFixed PCI_Config EmbeddedControl SMBus SystemCMOS PciBarTarget IPMI BigEndian LittleEndian ParityTypeNone ParityTypeSpace ParityTypeMark "
               "ISAOnlyRanges NonISAOnlyRanges EntireRange TypeTranslation TypeStatic SparseTranslation DenseTranslation DataBitsEight DataBitsNine StopBitsZero StopBitsOne StopBitsOnePlusHalf StopBitsTwo "
               "Exclusive SharedAndWake ExclusiveAndWake Shared ControllerInitiated DeviceInitiated AddressingMode7Bit AddressingMode10Bit Decode16 Decode10 ";

    if (set == 3)
        return "a addindex addtogroup anchor arg attention author b "
               "brief bug c class code date def defgroup deprecated "
               "dontinclude e em endcode endhtmlonly endif "
               "endlatexonly endlink endverbatim enum example "
               "exception f$ f[ f] file fn hideinitializer "
               "htmlinclude htmlonly if image include ingroup "
               "internal invariant interface latexonly li line link "
               "mainpage name namespace nosubgrouping note overload "
               "p page par param post pre ref relates remarks return "
               "retval sa section see showinitializer since skip "
               "skipline struct subsection test throw todo typedef "
               "union until var verbatim verbinclude version warning "
               "weakgroup $ @ \\ & < > # { }";

    return 0;
}

QString findKey(QString str, QString str_sub, int f_null)
{
    int total, tab_count;
    QString strs, space;
    tab_count = 0;
    for (int i = 0; i < str.count(); i++) {
        if (str.mid(i, 1) == str_sub) {
            strs = str.mid(0, i);

            for (int j = 0; j < strs.count(); j++) {
                if (strs.mid(j, 1) == "\t") {
                    tab_count = tab_count + 1;
                }
                //qDebug() <<"\t个数：" << strs.mid(j, 1) << tab_count;
            }

            int str_space = strs.count() - tab_count;
            total = str_space + tab_count * 4 - f_null;

            for (int k = 0; k < total; k++)
                space = space + " ";

            break;
        }
    }

    return space;
}

void MainWindow::textEdit_linesChanged()
{
    if (!loading)

        timer->start(1000);
}

void thread_one::run()
{

    if (break_run) {

        return;
    }

    thread_end = false;

    //refreshTree();//之前预留，准备弃用
    getMemberTree(textEditBack);

    //emit over();

    QMetaObject::invokeMethod(this, "over");
}

/*线程结束后对成员树进行数据刷新*/
void MainWindow::dealover()
{

    //update_ui_tw();//之前预留，准备弃用
    update_ui_tree();

    thread_end = true;
    break_run = false;
}

void MainWindow::update_member(bool show, QString str_void, QList<QTreeWidgetItem*> tw_list)
{
    if (!show) {

        tw_list.clear();
        for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
            QString str = ui->treeWidget->topLevelItem(i)->text(0).trimmed();
            if (str.mid(0, str_void.count()) == str_void) {
                tw_list.append(ui->treeWidget->takeTopLevelItem(i));
                i = -1;
            }
        }
    } else {
        //if(tw_list.count() > 0)
        //{
        ui->treeWidget->addTopLevelItems(tw_list);
        ui->treeWidget->sortItems(1, Qt::AscendingOrder);

        //}
        //else
        //    on_btnRefreshTree();

        qDebug() << tw_list.count();
    }
}

void MainWindow::update_ui_tree()
{

    if (break_run) {
        return;
    }

    ui->treeWidget->clear();
    ui->treeWidget->update();
    ui->treeWidget->addTopLevelItems(tw_list);
    ui->treeWidget->expandAll();

    QString lbl = "Scope(" + QString::number(s_count) + ")  " + "Device(" + QString::number(d_count) + ")  " + "Method(" + QString::number(m_count) + ")"; //  + "N(" + QString::number(n_count) + ")"
    ui->treeWidget->setHeaderLabel(lbl);
    ui->lblMembers->setText(lbl);

    ui->tabWidget_misc->tabBar()->setTabText(0, lbl);

    ui->treeWidget->update();

    float a = qTime.elapsed() / 1000.00;
    lblMsg->setText(tr("Refresh completed") + "(" + QTime::currentTime().toString() + "    " + QString::number(a, 'f', 2) + " s)");

    textEdit_cursorPositionChanged();

    QFileInfo fi(curFile);
    if (fi.suffix().toLower() == "dsl") {
        ui->treeWidget->setHidden(false);
    }
}

void MainWindow::update_ui_tw()
{
    ui->treeWidget->clear();

    ui->treeWidget->update();

    ui->treeWidget->addTopLevelItems(twitems);

    ui->treeWidget->sortItems(1, Qt::AscendingOrder); //排序

    ui->treeWidget->setIconSize(QSize(12, 12));

    ui->treeWidget->setHeaderLabel("S(" + QString::number(s_count) + ")  " + "D(" + QString::number(d_count) + ")  " + "M(" + QString::number(m_count) + ")  " + "N(" + QString::number(n_count) + ")");
    ui->treeWidget->update();

    float a = qTime.elapsed() / 1000.00;
    lblMsg->setText("Refresh completed(" + QTime::currentTime().toString() + "    " + QString::number(a, 'f', 2) + " s)");

    textEdit_cursorPositionChanged();

    QFileInfo fi(curFile);
    if (fi.suffix().toLower() == "dsl") {
        ui->treeWidget->setHidden(false);
    }
}

void MainWindow::refresh_tree(QsciScintilla* textEdit)
{
    if (!thread_end) {
        break_run = true;
        //lblMsg->setText("Refresh interrupted");
        mythread->quit();
        mythread->wait();

        /*等待线程结束,以使最后一次刷新可以完成*/
        while (!thread_end) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        }
    }

    //将textEdit的内容读到后台
    textEditBack->clear();
    textEditBack->setText(textEdit->text());

    lblMsg->setText(tr("Refreshing..."));

    qTime.start();
    mythread->start();
}

void MainWindow::on_btnRefreshTree()
{
    refresh_tree(textEdit);

    syncMiniEdit();
}

void MainWindow::syncMiniEdit()
{
    int row, col;
    textEdit->getCursorPosition(&row, &col);
    miniEdit->clear();
    miniEdit->setText(textEdit->text());
    miniEdit->setCursorPosition(row, col);
}

QString getMemberName(QString str_member, QsciScintilla* textEdit, int RowNum)
{
    //int RowNum, ColNum;
    QString sub;
    //textEdit->getCursorPosition(&RowNum, &ColNum);

    sub = textEdit->text(RowNum).trimmed();

    QString str_end;
    if (sub.mid(0, str_member.count()) == str_member) {

        for (int i = 0; i < sub.count(); i++) {
            if (sub.mid(i, 1) == ")") {
                str_end = sub.mid(0, i + 1);
                break;
            }
        }
    }

    return str_end;
}

void MainWindow::set_mark(int linenr)
{
    //SCI_MARKERGET 参数用来设置标记，默认为圆形标记
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERGET, linenr);
    //SCI_MARKERSETFORE，SCI_MARKERSETBACK设置标记前景和背景标记
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERSETFORE, 0, QColor(Qt::red));
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERSETBACK, 0, QColor(Qt::red));
    textEdit->SendScintilla(QsciScintilla::SCI_MARKERADD, linenr);
}

int getBraceScope(int start, int count, QsciScintilla* textEdit)
{
    int dkh1 = 0;
    int scope_end = 0;
    bool end = false;
    /*start-1,从当前行就开始解析，囊括Scope(){等这种紧跟{的写法*/
    for (int s = start - 1; s < count; s++) {

        QString str = textEdit->text(s).trimmed();

        for (int t = 0; t < str.count(); t++) {

            if (str.mid(0, 2) != "/*" && str.mid(0, 2) != "//") {

                if (str.mid(t, 1) == "{") {
                    dkh1++;
                }
                if (str.mid(t, 1) == "}") {
                    dkh1--;

                    if (dkh1 == 0) {
                        //范围结束
                        int row, col;
                        textEdit->getCursorPosition(&row, &col);
                        scope_end = s + 1;
                        end = true;
                        //qDebug() << "范围结束" << scope_end;
                        break;
                    }
                }
            }
        }

        if (end) {

            break;
        }
    }

    /*如果没有找到匹配的}，则返回开始位置的下一行，否则会进行无限循环*/
    if (!end)
        return start + 1;

    return scope_end;
}

bool chkMemberName(QString str, QString name)
{

    if (str.trimmed().mid(0, name.count()) == name)
        return true;

    return false;
}

void addSubItem(int start, int end, QsciScintilla* textEdit, QString Name, QTreeWidgetItem* iTop)
{

    textEdit->setCursorPosition(start, 0);

    for (int sdds1 = start; sdds1 < end; sdds1++) {
        if (break_run)
            break;

        QString str = textEdit->text(sdds1).trimmed();

        if (chkMemberName(str, Name)) {

            QTreeWidgetItem* iSub = new QTreeWidgetItem(QStringList() << getMemberName(Name, textEdit, sdds1) << QString("%1").arg(sdds1, 7, 10, QChar('0')));

            if (Name == "Device") {
                iSub->setIcon(0, QIcon(":/icon/d.png"));
                d_count++;
            }
            if (Name == "Scope") {
                iSub->setIcon(0, QIcon(":/icon/s.png"));
                s_count++;
            }
            if (Name == "Method") {
                iSub->setIcon(0, QIcon(":/icon/m.png"));
                m_count++;
            }

            iTop->addChild(iSub);
        }
    }
}

QTreeWidgetItem* addChildItem(int row, QsciScintilla* textEdit, QString Name, QTreeWidgetItem* iTop)
{
    QTreeWidgetItem* iSub = new QTreeWidgetItem(QStringList() << getMemberName(Name, textEdit, row) << QString("%1").arg(row, 7, 10, QChar('0')));
    if (Name == "Device") {
        iSub->setIcon(0, QIcon(":/icon/d.png"));
        d_count++;
    }
    if (Name == "Scope") {
        iSub->setIcon(0, QIcon(":/icon/s.png"));
        s_count++;
    }
    if (Name == "Method") {
        iSub->setIcon(0, QIcon(":/icon/m.png"));
        m_count++;
    }

    iTop->addChild(iSub);

    return iSub;
}

void getMemberTree(QsciScintilla* textEdit)
{

    if (break_run) {
        return;
    }

    loading = true;

    tw_list.clear();

    s_count = 0;
    m_count = 0;
    d_count = 0;
    n_count = 0;

    QString str_member;

    int count; //总行数

    QTreeWidgetItem* twItem0;
    count = textEdit->lines();

    for (int j = 0; j < count; j++) {
        if (break_run)
            break;

        str_member = textEdit->text(j).trimmed();

        //根"Scope"
        if (chkMemberName(str_member, "Scope")) {

            twItem0 = new QTreeWidgetItem(QStringList() << getMemberName(str_member, textEdit, j) << QString("%1").arg(j, 7, 10, QChar('0')));
            twItem0->setIcon(0, QIcon(":/icon/s.png"));
            //tw->addTopLevelItem(twItem0);
            tw_list.append(twItem0);

            s_count++;

            int c_fw_start = j + 1;
            int c_fw_end = getBraceScope(c_fw_start, count, textEdit);

            //再往下找内部成员

            for (int d = c_fw_start; d < c_fw_end; d++) {

                if (break_run)
                    break;

                QString str = textEdit->text(d).trimmed();

                //Scope-->Device
                if (chkMemberName(str, "Device")) {

                    QTreeWidgetItem* twItem1 = addChildItem(d, textEdit, "Device", twItem0);

                    int d2_start = d + 1;
                    int d2_end = getBraceScope(d2_start, count, textEdit);

                    for (int m2 = d2_start; m2 < d2_end; m2++) {
                        if (break_run)
                            break;

                        QString str = textEdit->text(m2).trimmed();
                        //Scope-->Device-->Method
                        if (chkMemberName(str, "Method")) {

                            QTreeWidgetItem* twItem2 = addChildItem(m2, textEdit, "Method", twItem1);
                            if (twItem2) { }
                        }

                        //Scope-->Device-->Device
                        if (chkMemberName(str, "Device")) {

                            QTreeWidgetItem* twItem2 = addChildItem(m2, textEdit, "Device", twItem1);

                            int start = m2 + 1;
                            int end = getBraceScope(start, count, textEdit);

                            for (int sddm1 = start; sddm1 < end; sddm1++) {
                                if (break_run)
                                    break;

                                QString str = textEdit->text(sddm1).trimmed();

                                //Scope-->Device-->Device-->Method
                                if (chkMemberName(str, "Method")) {

                                    QTreeWidgetItem* twItem3 = addChildItem(sddm1, textEdit, "Method", twItem2);
                                    if (twItem3) { }
                                }

                                //Scope-->Device-->Device-->Scope
                                if (chkMemberName(str, "Scope")) {

                                    QTreeWidgetItem* twItem3 = addChildItem(sddm1, textEdit, "Scope", twItem2);

                                    int start_sdds = sddm1 + 1;
                                    int end_sdds = getBraceScope(start_sdds, count, textEdit);

                                    for (int sdds = start_sdds; sdds < end_sdds; sdds++) {
                                        if (break_run) {
                                            break;
                                        }

                                        QString str = textEdit->text(sdds).trimmed();

                                        //S--D--D--S--S
                                        if (chkMemberName(str, "Scope")) {

                                            QTreeWidgetItem* twItem4 = addChildItem(sdds, textEdit, "Scope", twItem3);
                                            if (twItem4) { }

                                            int start_sddss = sdds + 1;
                                            int end_sddss = getBraceScope(start_sddss, count, textEdit);
                                            for (int sddss = start_sddss; sddss < end_sddss; sddss++) {
                                                if (break_run) {
                                                    break;
                                                }
                                                QString str = textEdit->text(sddss);

                                                //S--D--D--S--S--S
                                                if (chkMemberName(str, "Scope")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sddss, textEdit, "Scope", twItem4);
                                                    if (twItem5) { }
                                                }

                                                //S--D--D--S--S--D
                                                if (chkMemberName(str, "Device")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sddss, textEdit, "Device", twItem4);
                                                    if (twItem5) { }
                                                }

                                                //S--D--D--S--S--M
                                                if (chkMemberName(str, "Method")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sddss, textEdit, "Method", twItem4);
                                                    if (twItem5) { }
                                                }
                                            }

                                            sdds = end_sddss - 1;
                                        }

                                        //S--D--D--S--M
                                        if (chkMemberName(str, "Method")) {

                                            QTreeWidgetItem* twItem4 = addChildItem(sdds, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }

                                        //Scope-->Device-->Device-->Scope-->Device
                                        if (chkMemberName(str, "Device")) {

                                            QTreeWidgetItem* twItem4 = addChildItem(sdds, textEdit, "Device", twItem3);
                                            if (twItem4) { }

                                            int start_sddsd = sdds + 1;
                                            int end_sddsd = getBraceScope(start_sddsd, count, textEdit);
                                            for (int sddsd = start_sddsd; sddsd < end_sddsd; sddsd++) {
                                                if (break_run) {
                                                    break;
                                                }
                                                QString str = textEdit->text(sddsd);

                                                //S--D--D--S--D--S
                                                if (chkMemberName(str, "Scope")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sddsd, textEdit, "Scope", twItem4);
                                                    if (twItem5) { }
                                                }

                                                //S--D--D--S--D--D
                                                if (chkMemberName(str, "Device")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sddsd, textEdit, "Device", twItem4);
                                                    if (twItem5) { }
                                                }

                                                //S--D--D--S--D--M
                                                if (chkMemberName(str, "Method")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sddsd, textEdit, "Method", twItem4);
                                                    if (twItem5) { }
                                                }
                                            }

                                            sdds = end_sddsd - 1;
                                        }
                                    }

                                    sddm1 = end_sdds - 1;
                                }

                                //Scope-->Device-->Device-->Device
                                if (chkMemberName(str, "Device")) {

                                    QTreeWidgetItem* twItem3 = addChildItem(sddm1, textEdit, "Device", twItem2);

                                    int start3 = sddm1 + 1;
                                    int end3 = getBraceScope(start3, count, textEdit);

                                    for (int sddd = start3; sddd < end3; sddd++) {
                                        QString str = textEdit->text(sddd).trimmed();

                                        //Scope-->Device-->Device-->Device-->Method
                                        if (chkMemberName(str, "Method")) {

                                            QTreeWidgetItem* twItem4 = addChildItem(sddd, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }

                                        //Scope-->Device-->Device-->Device-->Scope
                                        if (chkMemberName(str, "Scope")) {

                                            QTreeWidgetItem* twItem4 = addChildItem(sddd, textEdit, "Scope", twItem3);
                                            if (twItem4) { }

                                            int start_sddds = sddd + 1;
                                            int end_sddds = getBraceScope(start_sddds, count, textEdit);
                                            for (int sddds = start_sddds; sddds < end_sddds; sddds++) {
                                                if (break_run) {
                                                    break;
                                                }
                                                QString str = textEdit->text(sddds);

                                                //S--D--D--D--S--S
                                                if (chkMemberName(str, "Scope")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sddds, textEdit, "Scope", twItem4);
                                                    if (twItem5) { }
                                                }

                                                //S--D--D--D--S--D
                                                if (chkMemberName(str, "Device")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sddds, textEdit, "Device", twItem4);
                                                    if (twItem5) { }
                                                }

                                                //S--D--D--D--S--M
                                                if (chkMemberName(str, "Method")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sddds, textEdit, "Method", twItem4);
                                                    if (twItem5) { }
                                                }
                                            }

                                            sddd = end_sddds - 1;
                                        }

                                        //Scope-->Device-->Device-->Device-->Device
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(sddd, textEdit, "Device", twItem3);
                                            if (twItem4) { }

                                            int start_sdddd = sddd + 1;
                                            int end_sdddd = getBraceScope(start_sdddd, count, textEdit);
                                            for (int sdddd = start_sdddd; sdddd < end_sdddd; sdddd++) {
                                                if (break_run) {
                                                    break;
                                                }
                                                QString str = textEdit->text(sdddd);

                                                //S--D--D--D--D--S
                                                if (chkMemberName(str, "Scope")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdddd, textEdit, "Scope", twItem4);
                                                    if (twItem5) { }
                                                }

                                                //S--D--D--D--D--D
                                                if (chkMemberName(str, "Device")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdddd, textEdit, "Device", twItem4);
                                                    if (twItem5) { }
                                                }

                                                //S--D--D--D--D--M
                                                if (chkMemberName(str, "Method")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdddd, textEdit, "Method", twItem4);
                                                    if (twItem5) { }
                                                }
                                            }

                                            sddd = end_sdddd - 1;
                                        }
                                    }

                                    sddm1 = end3 - 1;
                                }
                            }

                            m2 = end - 1;
                        }

                        //Scope-->Device-->Scope
                        if (chkMemberName(str, "Scope")) {
                            QTreeWidgetItem* twItem2 = addChildItem(m2, textEdit, "Scope", twItem1);
                            if (twItem2) { }

                            int start_sds = m2 + 1;
                            int end_sds = getBraceScope(start_sds, count, textEdit);

                            for (int sds = start_sds; sds < end_sds; sds++) {
                                if (break_run)
                                    break;

                                QString str = textEdit->text(sds).trimmed();

                                //Scope-->Device-->Scope-->Scope
                                if (chkMemberName(str, "Scope")) {

                                    QTreeWidgetItem* twItem3 = addChildItem(sds, textEdit, "Scope", twItem2);
                                    if (twItem3) { }

                                    int start_sdss = sds + 1;
                                    int end_sdss = getBraceScope(start_sdss, count, textEdit);
                                    for (int sdss = start_sdss; sdss < end_sdss; sdss++) {
                                        if (break_run) {
                                            break;
                                        }
                                        QString str = textEdit->text(sdss);

                                        //S--D--S--S--S
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(sdss, textEdit, "Scope", twItem3);
                                            if (twItem4) { }

                                            int start_sdsss = sdss + 1;
                                            int end_sdsss = getBraceScope(start_sdsss, count, textEdit);
                                            for (int sdsss = start_sdsss; sdsss < end_sdsss; sdsss++) {
                                                if (break_run) {
                                                    break;
                                                }
                                                QString str = textEdit->text(sdsss);

                                                //S--D--S--S--S--S
                                                if (chkMemberName(str, "Scope")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdsss, textEdit, "Scope", twItem4);
                                                    if (twItem5) { }

                                                    int start_sdssss = sdsss + 1;
                                                    int end_sdssss = getBraceScope(start_sdssss, count, textEdit);
                                                    for (int sdssss = start_sdssss; sdssss < end_sdssss; sdssss++) {
                                                        if (break_run) {
                                                            break;
                                                        }
                                                        QString str = textEdit->text(sdssss);

                                                        //S--D--S--S--S--S--S
                                                        if (chkMemberName(str, "Scope")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdssss, textEdit, "Scope", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--S--S--S--D
                                                        if (chkMemberName(str, "Device")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdssss, textEdit, "Device", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--S--S--S--M
                                                        if (chkMemberName(str, "Method")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdssss, textEdit, "Method", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                    }

                                                    sdsss = end_sdssss - 1;
                                                }
                                                //S--D--S--S--S--D
                                                if (chkMemberName(str, "Device")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdsss, textEdit, "Device", twItem4);
                                                    if (twItem5) { }

                                                    int start_sdsssd = sdsss + 1;
                                                    int end_sdsssd = getBraceScope(start_sdsssd, count, textEdit);
                                                    for (int sdsssd = start_sdsssd; sdsssd < end_sdsssd; sdsssd++) {
                                                        if (break_run) {
                                                            break;
                                                        }
                                                        QString str = textEdit->text(sdsssd);

                                                        //S--D--S--S--S--D--S
                                                        if (chkMemberName(str, "Scope")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdsssd, textEdit, "Scope", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--S--S--D--D
                                                        if (chkMemberName(str, "Device")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdsssd, textEdit, "Device", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--S--S--D--M
                                                        if (chkMemberName(str, "Method")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdsssd, textEdit, "Method", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                    }

                                                    sdsss = end_sdsssd - 1;
                                                }
                                                //S--D--S--S--S--M
                                                if (chkMemberName(str, "Method")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdsss, textEdit, "Method", twItem4);
                                                    if (twItem5) { }
                                                }
                                            }
                                            sdss = end_sdsss - 1;
                                        }

                                        //S--D--S--S--D
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(sdss, textEdit, "Device", twItem3);
                                            if (twItem4) { }

                                            int start_sdssd = sdss + 1;
                                            int end_sdssd = getBraceScope(start_sdssd, count, textEdit);
                                            for (int sdssd = start_sdssd; sdssd < end_sdssd; sdssd++) {
                                                if (break_run) {
                                                    break;
                                                }
                                                QString str = textEdit->text(sdssd);

                                                //S--D--S--S--D--S
                                                if (chkMemberName(str, "Scope")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdssd, textEdit, "Scope", twItem4);
                                                    if (twItem5) { }

                                                    int start_sdssds = sdssd + 1;
                                                    int end_sdssds = getBraceScope(start_sdssds, count, textEdit);
                                                    for (int sdssds = start_sdssds; sdssds < end_sdssds; sdssds++) {
                                                        if (break_run) {
                                                            break;
                                                        }
                                                        QString str = textEdit->text(sdssds);

                                                        //S--D--S--S--D--S--S
                                                        if (chkMemberName(str, "Scope")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdssds, textEdit, "Scope", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--S--D--S--D
                                                        if (chkMemberName(str, "Device")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdssds, textEdit, "Device", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--S--D--S--M
                                                        if (chkMemberName(str, "Method")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdssds, textEdit, "Method", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                    }

                                                    sdssd = end_sdssds - 1;
                                                }
                                                //S--D--S--S--D--D
                                                if (chkMemberName(str, "Device")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdssd, textEdit, "Device", twItem4);
                                                    if (twItem5) { }

                                                    int start_sdssdd = sdssd + 1;
                                                    int end_sdssdd = getBraceScope(start_sdssdd, count, textEdit);
                                                    for (int sdssdd = start_sdssdd; sdssdd < end_sdssdd; sdssdd++) {
                                                        if (break_run) {
                                                            break;
                                                        }
                                                        QString str = textEdit->text(sdssdd);

                                                        //S--D--S--S--D--D--S
                                                        if (chkMemberName(str, "Scope")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdssdd, textEdit, "Scope", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--S--D--D--D
                                                        if (chkMemberName(str, "Device")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdssdd, textEdit, "Device", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--S--D--D--M
                                                        if (chkMemberName(str, "Method")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdssdd, textEdit, "Method", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                    }

                                                    sdssd = end_sdssdd - 1;
                                                }
                                                //S--D--S--S--D--M
                                                if (chkMemberName(str, "Method")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdssd, textEdit, "Method", twItem4);
                                                    if (twItem5) { }
                                                }
                                            }

                                            sdss = end_sdssd - 1;
                                        }

                                        //S--D--S--S--M
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(sdss, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }
                                    }

                                    sds = end_sdss - 1;
                                }

                                //Scope-->Device-->Scope-->Device
                                if (chkMemberName(str, "Device")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(sds, textEdit, "Device", twItem2);
                                    if (twItem3) { }

                                    int start4 = sds + 1;
                                    int end4 = getBraceScope(start4, count, textEdit);

                                    for (int m4 = start4; m4 < end4; m4++) {

                                        if (break_run)
                                            break;

                                        QString str = textEdit->text(m4).trimmed();

                                        //Scope-->Device-->Scope-->Device-->Method
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(m4, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }

                                        //Scope-->Device-->Scope-->Device-->Device
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(m4, textEdit, "Device", twItem3);
                                            if (twItem4) { }

                                            int start_sdsdd = m4 + 1;
                                            int end_sdsdd = getBraceScope(start_sdsdd, count, textEdit);
                                            for (int sdsdd = start_sdsdd; sdsdd < end_sdsdd; sdsdd++) {
                                                if (break_run) {
                                                    break;
                                                }
                                                QString str = textEdit->text(sdsdd);

                                                //S--D--S--D--D--S
                                                if (chkMemberName(str, "Scope")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdsdd, textEdit, "Scope", twItem4);
                                                    if (twItem5) { }
                                                }
                                                //S--D--S--D--D--D
                                                if (chkMemberName(str, "Device")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdsdd, textEdit, "Device", twItem4);
                                                    if (twItem5) { }
                                                }
                                                //S--D--S--D--D--M
                                                if (chkMemberName(str, "Method")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdsdd, textEdit, "Method", twItem4);
                                                    if (twItem5) { }
                                                }
                                            }

                                            m4 = end_sdsdd - 1;
                                        }

                                        //Scope-->Device-->Scope-->Device-->Scope
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(m4, textEdit, "Scope", twItem3);
                                            if (twItem4) { }

                                            int start_sdsds = m4 + 1;
                                            int end_sdsds = getBraceScope(start_sdsds, count, textEdit);
                                            for (int sdsds = start_sdsds; sdsds < end_sdsds; sdsds++) {
                                                if (break_run) {
                                                    break;
                                                }
                                                QString str = textEdit->text(sdsds);

                                                //S--D--S--D--S--S
                                                if (chkMemberName(str, "Scope")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdsds, textEdit, "Scope", twItem4);
                                                    if (twItem5) { }

                                                    int start_sdsdss = sdsds + 1;
                                                    int end_sdsdss = getBraceScope(start_sdsdss, count, textEdit);
                                                    for (int sdsdss = start_sdsdss; sdsdss < end_sdsdss; sdsdss++) {
                                                        if (break_run) {
                                                            break;
                                                        }
                                                        QString str = textEdit->text(sdsdss);

                                                        //S--D--S--D--S--S--S
                                                        if (chkMemberName(str, "Scope")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdsdss, textEdit, "Scope", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--D--S--S--D
                                                        if (chkMemberName(str, "Device")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdsdss, textEdit, "Device", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--D--S--S--M
                                                        if (chkMemberName(str, "Method")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdsdss, textEdit, "Method", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                    }

                                                    sdsds = end_sdsdss - 1;
                                                }
                                                //S--D--S--D--S--D
                                                if (chkMemberName(str, "Device")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdsds, textEdit, "Device", twItem4);
                                                    if (twItem5) { }

                                                    int start_sdsdsd = sdsds + 1;
                                                    int end_sdsdsd = getBraceScope(start_sdsdsd, count, textEdit);
                                                    for (int sdsdsd = start_sdsdsd; sdsdsd < end_sdsdsd; sdsdsd++) {
                                                        if (break_run) {
                                                            break;
                                                        }
                                                        QString str = textEdit->text(sdsdsd);

                                                        //S--D--S--D--S--D--S
                                                        if (chkMemberName(str, "Scope")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdsdsd, textEdit, "Scope", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--D--S--D--D
                                                        if (chkMemberName(str, "Device")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdsdsd, textEdit, "Device", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                        //S--D--S--D--S--D--M
                                                        if (chkMemberName(str, "Method")) {
                                                            QTreeWidgetItem* twItem6 = addChildItem(sdsdsd, textEdit, "Method", twItem5);
                                                            if (twItem6) { }
                                                        }
                                                    }

                                                    sdsds = end_sdsdsd - 1;
                                                }
                                                //S--D--S--D--S--M
                                                if (chkMemberName(str, "Method")) {
                                                    QTreeWidgetItem* twItem5 = addChildItem(sdsds, textEdit, "Method", twItem4);
                                                    if (twItem5) { }
                                                }
                                            }

                                            m4 = end_sdsds - 1;
                                        }
                                    }

                                    sds = end4 - 1;
                                }

                                //Scope-->Device-->Scope-->Method
                                if (chkMemberName(str, "Method")) {

                                    QTreeWidgetItem* twItem3 = addChildItem(sds, textEdit, "Method", twItem2);
                                    if (twItem3) { }
                                }
                            }

                            m2 = end_sds - 1;
                        }
                    }

                    d = d2_end - 1;
                }

                //S--S
                if (chkMemberName(str, "Scope")) {
                    QTreeWidgetItem* twItem1 = addChildItem(d, textEdit, "Scope", twItem0);
                    if (twItem1) { }

                    int start_ss = d + 1;
                    int end_ss = getBraceScope(start_ss, count, textEdit);
                    for (int ss = start_ss; ss < end_ss; ss++) {
                        if (break_run) {
                            break;
                        }
                        QString str = textEdit->text(ss);

                        //S--S--S
                        if (chkMemberName(str, "Scope")) {
                            QTreeWidgetItem* twItem2 = addChildItem(ss, textEdit, "Scope", twItem1);
                            if (twItem2) { }

                            int start_sss = ss + 1;
                            int end_sss = getBraceScope(start_sss, count, textEdit);
                            for (int sss = start_sss; sss < end_sss; sss++) {
                                if (break_run) {
                                    break;
                                }
                                QString str = textEdit->text(sss);

                                //S--S--S--S
                                if (chkMemberName(str, "Scope")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(sss, textEdit, "Scope", twItem2);
                                    if (twItem3) { }
                                }
                                //S--S--S--D
                                if (chkMemberName(str, "Device")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(sss, textEdit, "Device", twItem2);
                                    if (twItem3) { }
                                }
                                //S--S--S--M
                                if (chkMemberName(str, "Method")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(sss, textEdit, "Method", twItem2);
                                    if (twItem3) { }
                                }
                            }

                            ss = end_sss - 1;
                        }
                        //S--S--D
                        if (chkMemberName(str, "Device")) {
                            QTreeWidgetItem* twItem2 = addChildItem(ss, textEdit, "Device", twItem1);
                            if (twItem2) { }

                            int start_ssd = ss + 1;
                            int end_ssd = getBraceScope(start_ssd, count, textEdit);
                            for (int ssd = start_ssd; ssd < end_ssd; ssd++) {
                                if (break_run) {
                                    break;
                                }
                                QString str = textEdit->text(ssd);

                                //S--S--D--S
                                if (chkMemberName(str, "Scope")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(ssd, textEdit, "Scope", twItem2);
                                    if (twItem3) { }
                                }
                                //S--S--D--D
                                if (chkMemberName(str, "Device")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(ssd, textEdit, "Device", twItem2);
                                    if (twItem3) { }
                                }
                                //S--S--D--M
                                if (chkMemberName(str, "Method")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(ssd, textEdit, "Method", twItem2);
                                    if (twItem3) { }
                                }
                            }

                            ss = end_ssd - 1;
                        }
                        //S--S--M
                        if (chkMemberName(str, "Method")) {
                            QTreeWidgetItem* twItem2 = addChildItem(ss, textEdit, "Method", twItem1);
                            if (twItem2) { }
                        }
                    }

                    d = end_ss - 1;
                }

                //S--M
                if (chkMemberName(str, "Method")) {

                    QTreeWidgetItem* twItem1 = addChildItem(d, textEdit, "Method", twItem0);
                    if (twItem1) { }
                }
            }

            j = c_fw_end - 1;
        }

        //根下的"Method"
        if (chkMemberName(str_member, "Method")) {

            QTreeWidgetItem* twItem0 = new QTreeWidgetItem(QStringList() << getMemberName(str_member, textEdit, j) << QString("%1").arg(j, 7, 10, QChar('0')));
            twItem0->setIcon(0, QIcon(":/icon/m.png"));
            //tw->addTopLevelItem(twItem0);
            tw_list.append(twItem0);

            m_count++;
        }

        //根下的"Device"
        if (chkMemberName(str_member, "Device")) {

            QTreeWidgetItem* twItem0 = new QTreeWidgetItem(QStringList() << getMemberName(str_member, textEdit, j) << QString("%1").arg(j, 7, 10, QChar('0')));
            twItem0->setIcon(0, QIcon(":/icon/d.png"));
            //tw->addTopLevelItem(twItem0);
            tw_list.append(twItem0);

            d_count++;

            int start_d = j + 1;
            int end_d = getBraceScope(start_d, count, textEdit);

            //qDebug() << start_d << end_d;

            for (int d = start_d; d < end_d; d++) {
                if (break_run)
                    break;

                QString str = textEdit->text(d);

                //D--S
                if (chkMemberName(str, "Scope")) {
                    QTreeWidgetItem* twItem1 = addChildItem(d, textEdit, "Scope", twItem0);
                    if (twItem1) { }

                    int start_ds = d + 1;
                    int end_ds = getBraceScope(start_ds, count, textEdit);
                    for (int ds = start_ds; ds < end_ds; ds++) {
                        if (break_run) {
                            break;
                        }
                        QString str = textEdit->text(ds);

                        //D--S--S
                        if (chkMemberName(str, "Scope")) {
                            QTreeWidgetItem* twItem2 = addChildItem(ds, textEdit, "Scope", twItem1);
                            if (twItem2) { }

                            int start_dss = ds + 1;
                            int end_dss = getBraceScope(start_dss, count, textEdit);
                            for (int dss = start_dss; dss < end_dss; dss++) {
                                if (break_run) {
                                    break;
                                }
                                QString str = textEdit->text(dss);

                                //D--S--S--S
                                if (chkMemberName(str, "Scope")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(dss, textEdit, "Scope", twItem2);
                                    if (twItem3) { }

                                    int start_dsss = dss + 1;
                                    int end_dsss = getBraceScope(start_dsss, count, textEdit);
                                    for (int dsss = start_dsss; dsss < end_dsss; dsss++) {
                                        if (break_run) {
                                            break;
                                        }
                                        QString str = textEdit->text(dsss);

                                        //D--S--S--S--S
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dsss, textEdit, "Scope", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--S--S--S--D
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dsss, textEdit, "Device", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--S--S--S--M
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dsss, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }
                                    }

                                    dss = end_dsss - 1;
                                }
                                //D--S--S--D
                                if (chkMemberName(str, "Device")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(dss, textEdit, "Device", twItem2);
                                    if (twItem3) { }

                                    int start_dssd = dss + 1;
                                    int end_dssd = getBraceScope(start_dssd, count, textEdit);
                                    for (int dssd = start_dssd; dssd < end_dssd; dssd++) {
                                        if (break_run) {
                                            break;
                                        }
                                        QString str = textEdit->text(dssd);

                                        //D--S--S--D--S
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dssd, textEdit, "Scope", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--S--S--D--D
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dssd, textEdit, "Device", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--S--S--D--M
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dssd, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }
                                    }

                                    dss = end_dssd - 1;
                                }
                                //D--S--S--M
                                if (chkMemberName(str, "Method")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(dss, textEdit, "Method", twItem2);
                                    if (twItem3) { }
                                }
                            }

                            ds = end_dss - 1;
                        }
                        //D--S--D
                        if (chkMemberName(str, "Device")) {
                            QTreeWidgetItem* twItem2 = addChildItem(ds, textEdit, "Device", twItem1);
                            if (twItem2) { }

                            int start_dsd = ds + 1;
                            int end_dsd = getBraceScope(start_dsd, count, textEdit);
                            for (int dsd = start_dsd; dsd < end_dsd; dsd++) {
                                if (break_run) {
                                    break;
                                }
                                QString str = textEdit->text(dsd);

                                //D--S--D--S
                                if (chkMemberName(str, "Scope")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(dsd, textEdit, "Scope", twItem2);
                                    if (twItem3) { }

                                    /*下一个子层*/
                                    int start_dsds = dsd + 1;
                                    int end_dsds = getBraceScope(start_dsds, count, textEdit);
                                    for (int dsds = start_dsds; dsds < end_dsds; dsds++) {
                                        if (break_run) {
                                            break;
                                        }
                                        QString str = textEdit->text(dsds);

                                        //D--S--D--S--S
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dsds, textEdit, "Scope", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--S--D--S--D
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dsds, textEdit, "Device", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--S--D--S--M
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dsds, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }
                                    }

                                    dsd = end_dsds - 1;
                                }
                                //D--S--D--D
                                if (chkMemberName(str, "Device")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(dsd, textEdit, "Device", twItem2);
                                    if (twItem3) { }

                                    /*下一个子层*/
                                    int start_dsdd = dsd + 1;
                                    int end_dsdd = getBraceScope(start_dsdd, count, textEdit);
                                    for (int dsdd = start_dsdd; dsdd < end_dsdd; dsdd++) {
                                        if (break_run) {
                                            break;
                                        }
                                        QString str = textEdit->text(dsdd);

                                        //D--S--D--D--S
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dsdd, textEdit, "Scope", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--S--D--D--D
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dsdd, textEdit, "Device", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--S--D--D--M
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dsdd, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }
                                    }

                                    dsd = end_dsdd - 1;
                                }
                                //D--S--D--M
                                if (chkMemberName(str, "Method")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(dsd, textEdit, "Method", twItem2);
                                    if (twItem3) { }
                                }
                            }

                            ds = end_dsd - 1;
                        }
                        //D--S--M
                        if (chkMemberName(str, "Method")) {
                            QTreeWidgetItem* twItem2 = addChildItem(ds, textEdit, "Method", twItem1);
                            if (twItem2) { }
                        }
                    }

                    d = end_ds - 1;
                }

                //D--D
                if (chkMemberName(str, "Device")) {
                    QTreeWidgetItem* twItem1 = addChildItem(d, textEdit, "Device", twItem0);
                    if (twItem1) { }

                    int start_dd = d + 1;
                    int end_dd = getBraceScope(start_dd, count, textEdit);
                    for (int dd = start_dd; dd < end_dd; dd++) {
                        if (break_run) {
                            break;
                        }
                        QString str = textEdit->text(dd);

                        //D--D--S
                        if (chkMemberName(str, "Scope")) {
                            QTreeWidgetItem* twItem2 = addChildItem(dd, textEdit, "Scope", twItem1);
                            if (twItem2) { }

                            /*下一个子层*/
                            int start_dds = dd + 1;
                            int end_dds = getBraceScope(start_dds, count, textEdit);
                            for (int dds = start_dds; dds < end_dds; dds++) {
                                if (break_run) {
                                    break;
                                }
                                QString str = textEdit->text(dds);

                                //D--D--S--S
                                if (chkMemberName(str, "Scope")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(dds, textEdit, "Scope", twItem2);
                                    if (twItem3) { }

                                    /*下一个子层*/
                                    int start_ddss = dds + 1;
                                    int end_ddss = getBraceScope(start_ddss, count, textEdit);
                                    for (int ddss = start_ddss; ddss < end_ddss; ddss++) {
                                        if (break_run) {
                                            break;
                                        }
                                        QString str = textEdit->text(ddss);

                                        //D--D--S--S--S
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(ddss, textEdit, "Scope", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--D--S--S--D
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(ddss, textEdit, "Device", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--D--S--S--M
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(ddss, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }
                                    }

                                    dds = end_ddss - 1;
                                }
                                //D--D--S--D
                                if (chkMemberName(str, "Device")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(dds, textEdit, "Device", twItem2);
                                    if (twItem3) { }

                                    /*下一个子层*/
                                    int start_ddsd = dds + 1;
                                    int end_ddsd = getBraceScope(start_ddsd, count, textEdit);
                                    for (int ddsd = start_ddsd; ddsd < end_ddsd; ddsd++) {
                                        if (break_run) {
                                            break;
                                        }
                                        QString str = textEdit->text(ddsd);

                                        //D--D--S--D--S
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(ddsd, textEdit, "Scope", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--D--S--D--D
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(ddsd, textEdit, "Device", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--D--S--D--M
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(ddsd, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }
                                    }

                                    dds = end_ddsd - 1;
                                }
                                //D--D--S--M
                                if (chkMemberName(str, "Method")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(dds, textEdit, "Method", twItem2);
                                    if (twItem3) { }
                                }
                            }

                            dd = end_dds - 1;
                        }

                        //D--D--D
                        if (chkMemberName(str, "Device")) {
                            QTreeWidgetItem* twItem2 = addChildItem(dd, textEdit, "Device", twItem1);
                            if (twItem2) { }

                            int start_ddd = dd + 1;
                            int end_ddd = getBraceScope(start_ddd, count, textEdit);
                            for (int ddd = start_ddd; ddd < end_ddd; ddd++) {
                                if (break_run) {
                                    break;
                                }
                                QString str = textEdit->text(ddd);

                                //D--D--D--S
                                if (chkMemberName(str, "Scope")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(ddd, textEdit, "Scope", twItem2);
                                    if (twItem3) { }

                                    int start_ddds = ddd + 1;
                                    int end_ddds = getBraceScope(start_ddds, count, textEdit);
                                    for (int ddds = start_ddds; ddds < end_ddds; ddds++) {
                                        if (break_run) {
                                            break;
                                        }
                                        QString str = textEdit->text(ddds);

                                        //D--D--D--S--S
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(ddds, textEdit, "Scope", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--D--D--S--D
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(ddds, textEdit, "Device", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--D--D--S--M
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(ddds, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }
                                    }

                                    ddd = end_ddds - 1;
                                }
                                //D--D--D--D
                                if (chkMemberName(str, "Device")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(ddd, textEdit, "Device", twItem2);
                                    if (twItem3) { }

                                    int start_dddd = ddd + 1;
                                    int end_dddd = getBraceScope(start_dddd, count, textEdit);
                                    for (int dddd = start_dddd; dddd < end_dddd; dddd++) {
                                        if (break_run) {
                                            break;
                                        }
                                        QString str = textEdit->text(dddd);

                                        //D--D--D--D--S
                                        if (chkMemberName(str, "Scope")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dddd, textEdit, "Scope", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--D--D--D--D
                                        if (chkMemberName(str, "Device")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dddd, textEdit, "Device", twItem3);
                                            if (twItem4) { }
                                        }
                                        //D--D--D--D--M
                                        if (chkMemberName(str, "Method")) {
                                            QTreeWidgetItem* twItem4 = addChildItem(dddd, textEdit, "Method", twItem3);
                                            if (twItem4) { }
                                        }
                                    }

                                    ddd = end_dddd - 1;
                                }
                                //D--D--D--M
                                if (chkMemberName(str, "Method")) {
                                    QTreeWidgetItem* twItem3 = addChildItem(ddd, textEdit, "Method", twItem2);
                                    if (twItem3) { }
                                }
                            }

                            dd = end_ddd - 1;
                        }

                        //D--D--M
                        if (chkMemberName(str, "Method")) {
                            QTreeWidgetItem* twItem2 = addChildItem(dd, textEdit, "Method", twItem1);
                            if (twItem2) { }
                        }
                    }

                    d = end_dd - 1;
                }

                //D--M
                if (chkMemberName(str, "Method")) {
                    QTreeWidgetItem* twItem1 = addChildItem(d, textEdit, "Method", twItem0);
                    if (twItem1) { }
                }
            }

            j = end_d - 1;
        }
    }

    loading = false;
}

void refreshTree()
{
    loading = true;

    twitems.clear();

    s_count = 0;
    m_count = 0;
    d_count = 0;
    n_count = 0;

    //枚举"Scope"
    getMembers("Scope", textEditBack);

    //枚举"Device"
    getMembers("Device", textEditBack);

    //枚举"Method"
    getMembers("Method", textEditBack);

    //枚举"Name"
    getMembers("Name", textEditBack);

    loading = false;
}

void getMembers(QString str_member, QsciScintilla* textEdit)
{

    if (break_run)
        return;

    QString str;
    int RowNum, ColNum;
    int count; //总行数
    QTreeWidgetItem* twItem0;

    count = textEdit->lines();

    //回到第一行
    textEdit->setCursorPosition(0, 0);

    for (int j = 0; j < count; j++) {
        if (break_run) {

            break;
        }

        //正则、区分大小写、匹配整个单词、循环搜索
        if (textEdit->findFirst(str_member, true, true, true, false)) {

            textEdit->getCursorPosition(&RowNum, &ColNum);

            str = textEdit->text(RowNum);

            QString space = findKey(str, str_member.mid(0, 1), 0);

            QString sub = str.trimmed();

            bool zs = false; //当前行是否存在注释
            for (int k = ColNum; k > -1; k--) {
                if (str.mid(k - 2, 2) == "//" || str.mid(k - 2, 2) == "/*") {
                    zs = true;

                    break;
                }
            }

            QString str_end;
            if (sub.mid(0, str_member.count()) == str_member && !zs) {

                for (int i = 0; i < sub.count(); i++) {
                    if (sub.mid(i, 1) == ")") {
                        str_end = sub.mid(0, i + 1);

                        twItem0 = new QTreeWidgetItem(QStringList() << space + str_end << QString("%1").arg(RowNum, 7, 10, QChar('0'))); //QString::number(RowNum));

                        if (str_member == "Scope" && show_s) {

                            twItem0->setIcon(0, QIcon(":/icon/s.png"));
                            QFont f;
                            f.setBold(true);
                            twItem0->setFont(0, f);

                            twitems.append(twItem0);

                            s_count++;
                        }
                        if (str_member == "Method" && show_m) {

                            twItem0->setIcon(0, QIcon(":/icon/m.png"));

                            twitems.append(twItem0);

                            m_count++;
                        }
                        if (str_member == "Name" && show_n) {

                            twItem0->setIcon(0, QIcon(":/icon/n.png"));

                            twitems.append(twItem0);

                            n_count++;
                        }
                        if (str_member == "Device" && show_d) {

                            twItem0->setIcon(0, QIcon(":/icon/d.png"));

                            twitems.append(twItem0);

                            d_count++;
                        }

                        break;
                    }
                }
            }
        } else
            break;
    }
}

void MainWindow::on_MainWindow_destroyed()
{
}

void MainWindow::init_info_edit()
{

    ui->dockWidgetContents_6->layout()->setMargin(0);
    ui->dockWidgetContents_6->layout()->setSpacing(0);
    ui->gridLayout_2->setMargin(0);
    ui->gridLayout_3->setMargin(0);
    ui->gridLayout_4->setMargin(0);
    ui->gridLayout_5->setMargin(0);
    ui->gridLayout_6->setMargin(0);
    ui->gridLayout_13->setMargin(0);

    ui->listWidget->setFrameShape(QListWidget::NoFrame);
    //ui->listWidget->setGeometry(ui->listWidget->x(), 20, ui->listWidget->width(), ui->listWidget->height());
    ui->listWidget->setSpacing(0);

    ui->listWidget->setIconSize(QSize(20, 20));

    //ui->listWidget->setViewMode(QListView::IconMode);
    //ui->listWidget->setViewMode(QListWidget::IconMode);
    ui->listWidget->setViewMode(QListView::ListMode);

    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i1.png"), tr("BasicInfo")));
    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i2.png"), tr("Errors")));
    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i3.png"), tr("Warnings")));
    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i4.png"), tr("Remarks")));
    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i5.png"), tr("Scribble")));

    ui->tabWidget->tabBar()->setHidden(true);
    ui->tabWidget->setCurrentIndex(0);
    ui->listWidget->setCurrentRow(0);

    textEditTemp = new QTextEdit();

    ui->editShowMsg->setLineWrapMode(ui->editShowMsg->NoWrap);
    ui->editShowMsg->setReadOnly(true);

    ui->editErrors->setLineWrapMode(ui->editErrors->NoWrap);
    ui->editErrors->setReadOnly(true);

    ui->editWarnings->setLineWrapMode(ui->editWarnings->NoWrap);
    ui->editWarnings->setReadOnly(true);

    ui->editRemarks->setLineWrapMode(ui->editRemarks->NoWrap);
    ui->editRemarks->setReadOnly(true);

    ui->editOptimizations->setLineWrapMode(ui->editOptimizations->NoWrap);
    ui->editOptimizations->setReadOnly(true);
    ui->tabWidget->removeTab(5); //No need to "optimize" this for now

    ui->dockWidget_6->setHidden(true);

    //Loading scribble board files
    ui->editScribble->setPlaceholderText(tr("This is a scribble board to temporarily record something, and the content will be saved and loaded automatically."));
    QString fileScribble = QDir::homePath() + "/.config/QtiASL/Scribble.txt";
    QFileInfo fi(fileScribble);
    if (fi.exists()) {
        QFile file(fileScribble);
        if (!file.open(QFile::ReadOnly | QFile::Text)) {
            QMessageBox::warning(this, tr("Application"),
                tr("Cannot read file %1:\n%2.")
                    .arg(QDir::toNativeSeparators(fileName), file.errorString()));

        } else {

            QTextStream in(&file);
            in.setCodec("UTF-8");
            QString text = in.readAll();
            ui->editScribble->setPlainText(text);
        }
    }
}

void MainWindow::init_recentFiles()
{
    //最近打开的文件
    //Mac:"/Users/../Library/Preferences/com.github-com-ic005k.QtiASL.plist"
    //Win:"\\HKEY_CURRENT_USER\\Software\\ic005k\\QtiASL"
    QCoreApplication::setOrganizationName("ic005k");
    QCoreApplication::setOrganizationDomain("github.com/ic005k");
    QCoreApplication::setApplicationName("QtiASL");

    m_recentFiles = new RecentFiles(this);
    m_recentFiles->attachToMenuAfterItem(ui->menu_File, tr("Open"), SLOT(recentOpen(QString)));
    m_recentFiles->setNumOfRecentFiles(25); //最多显示最近的文件个数

    //SSDT list
    QCoreApplication::setOrganizationName("ic005k");
    QCoreApplication::setOrganizationDomain("github.com/ic005k");
    QCoreApplication::setApplicationName("SSDT");

    m_ssdtFiles = new RecentFiles(this);
    m_ssdtFiles->setTitle(tr("Current SSDT List"));

    if (!linuxOS) {
        m_ssdtFiles->attachToMenuAfterItem(ui->menu_Edit, tr("Generate ACPI tables"), SLOT(recentOpen(QString)));

        getACPITables(true); //获取SSDT列表
    }
}

void MainWindow::init_toolbar()
{
    ui->actionNew->setIcon(QIcon(":/icon/new.png"));
    ui->toolBar->addAction(ui->actionNew);

    ui->actionOpen->setIcon(QIcon(":/icon/open.png"));
    ui->toolBar->addAction(ui->actionOpen);

    ui->actionSave->setIcon(QIcon(":/icon/save.png"));
    ui->toolBar->addAction(ui->actionSave);

    ui->actionSaveAs->setIcon(QIcon(":/icon/saveas.png"));
    ui->toolBar->addAction(ui->actionSaveAs);

    ui->toolBar->addSeparator();

    ui->toolBar->addWidget(ui->chkAll);
    ui->actionDSDecompile->setIcon(QIcon(":/icon/bat.png"));
    ui->toolBar->addAction(ui->actionDSDecompile);

    ui->toolBar->addSeparator();
    ui->toolBar->addWidget(ui->chkCaseSensitive);
    ui->toolBar->addWidget(ui->editFind);
    ui->editFind->setFixedWidth(320);
    ui->editFind->lineEdit()->setPlaceholderText(tr("Find") + "  (" + tr("History entries") + ": " + QString::number(ui->editFind->count()) + ")");
    ui->editFind->lineEdit()->setClearButtonEnabled(true);
    //ui->editFind->setAutoCompletionCaseSensitivity(Qt::CaseSensitive);

    setEditFindCompleter();

    connect(ui->editFind->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::editFind_returnPressed);

    lblCount = new QLabel(this);
    lblCount->setText("0");
    ui->toolBar->addWidget(lblCount);

    ui->actionFindPrevious->setIcon(QIcon(":/icon/fp.png"));
    ui->toolBar->addAction(ui->actionFindPrevious);

    ui->actionFindNext->setIcon(QIcon(":/icon/fn.png"));
    ui->toolBar->addAction(ui->actionFindNext);

    ui->toolBar->addSeparator();
    ui->toolBar->addWidget(ui->editReplace);
    //ui->editReplace->setFixedWidth(ui->editFind->width());

    ui->actionReplace->setIcon(QIcon(":/icon/re.png"));
    ui->toolBar->addAction(ui->actionReplace);

    ui->actionReplace_Find->setIcon(QIcon(":/icon/rf.png"));
    ui->toolBar->addAction(ui->actionReplace_Find);

    ui->actionFind->setIcon(QIcon(":/icon/fn.png"));
    ui->toolBar->addAction(ui->actionFind);

    ui->actionReplaceAll->setIcon(QIcon(":/icon/ra.png"));
    ui->toolBar->addAction(ui->actionReplaceAll);

    ui->toolBar->addSeparator();
    ui->toolBar->addWidget(ui->cboxCompilationOptions);
    ui->actionGo_to_previous_error->setIcon(QIcon(":/icon/1.png"));
    ui->toolBar->addAction(ui->actionGo_to_previous_error);

    ui->actionCompiling->setIcon(QIcon(":/icon/2.png"));
    ui->toolBar->addAction(ui->actionCompiling);

    ui->actionGo_to_the_next_error->setIcon(QIcon(":/icon/3.png"));
    ui->toolBar->addAction(ui->actionGo_to_the_next_error);

    ui->toolBar->addSeparator();
    ui->actionRefreshTree->setIcon(QIcon(":/icon/r.png"));
    ui->toolBar->addAction(ui->actionRefreshTree);
}

void MainWindow::init_menu()
{

    //File
    ui->actionNew->setShortcut(tr("ctrl+n"));
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::newFile);

    ui->actionOpen->setShortcut(tr("ctrl+o"));
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::Open);

    ui->actionSave->setShortcut(tr("ctrl+s"));
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::Save);

    ui->actionSaveAs->setShortcut(tr("ctrl+shift+s"));
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::SaveAs);

    ui->actionOpen_directory->setShortcut(tr("ctrl+0"));
    connect(ui->actionOpen_directory, &QAction::triggered, this, &MainWindow::on_actionOpenDir);

    //Edit
    ui->actionGenerate->setShortcut(tr("ctrl+g"));
    connect(ui->actionGenerate, &QAction::triggered, this, &MainWindow::btnGenerate_clicked);

    //ui->actionDSDecompile->setShortcut(tr("ctrl+l"));
    connect(ui->actionDSDecompile, &QAction::triggered, this, &MainWindow::ds_Decompile);

    ui->actionCompiling->setShortcut(tr("ctrl+m"));
    connect(ui->actionCompiling, &QAction::triggered, this, &MainWindow::btnCompile_clicked);

    ui->actionRefreshTree->setShortcut(tr("ctrl+r"));
    connect(ui->actionRefreshTree, &QAction::triggered, this, &MainWindow::on_btnRefreshTree);

    ui->actionFindPrevious->setShortcut(tr("ctrl+p"));
    connect(ui->actionFindPrevious, &QAction::triggered, this, &MainWindow::on_btnFindPrevious);

    ui->actionFindNext->setShortcut(tr("ctrl+f"));
    connect(ui->actionFindNext, &QAction::triggered, this, &MainWindow::on_btnFindNext);
    //ui->actionFind->setShortcut(tr("ctrl+f")); //注意：win下不起作用
    connect(ui->actionFind, &QAction::triggered, this, &MainWindow::on_btnFindNext);

    ui->actionReplace->setShortcut(tr("ctrl+k"));
    connect(ui->actionReplace, &QAction::triggered, this, &MainWindow::on_btnReplace);

    ui->actionReplace_Find->setShortcut(tr("ctrl+j"));
    connect(ui->actionReplace_Find, &QAction::triggered, this, &MainWindow::on_btnReplaceFind);
    connect(ui->actionReplaceAll, &QAction::triggered, this, &MainWindow::ReplaceAll);

    ui->actionGo_to_previous_error->setShortcut(tr("ctrl+alt+e"));
    connect(ui->actionGo_to_previous_error, &QAction::triggered, this, &MainWindow::on_PreviousError);

    ui->actionGo_to_the_next_error->setShortcut(tr("ctrl+e"));
    connect(ui->actionGo_to_the_next_error, &QAction::triggered, this, &MainWindow::on_NextError);

    connect(ui->actionKextstat, &QAction::triggered, this, &MainWindow::kextstat);

    //Preference
    ui->actionFont_2->setShortcut(tr("ctrl+9"));
    connect(ui->actionFont_2, &QAction::triggered, this, &MainWindow::set_font);

    ui->actionWrapWord->setShortcut(tr("ctrl+w"));
    connect(ui->actionWrapWord, &QAction::triggered, this, &MainWindow::set_wrap);

    connect(ui->actionClear_search_history, &QAction::triggered, this, &MainWindow::on_clearFindText);

    QActionGroup* AG = new QActionGroup(this);
    AG->addAction(ui->actionUTF_8);
    AG->addAction(ui->actionGBK);

    ui->actionUTF_8->setCheckable(true);
    ui->actionGBK->setCheckable(true);

    connect(AG, &QActionGroup::triggered,
        [=]() mutable {
            if (ui->actionUTF_8->isChecked() == true) {
                lblEncoding->setText("UTF-8");

            } else if (ui->actionGBK->isChecked() == true) {
                lblEncoding->setText("GBK");
            }
        });

    //View
    connect(ui->actionMembers_win, &QAction::triggered, this, &MainWindow::view_mem_list);
    ui->actionMembers_win->setShortcut(tr("ctrl+1"));

    connect(ui->actionInfo_win, &QAction::triggered, this, &MainWindow::view_info);
    ui->actionInfo_win->setShortcut(tr("ctrl+2"));

    connect(ui->actionMinimap, &QAction::triggered, this, &MainWindow::on_miniMap);
    ui->actionMinimap->setShortcut(tr("ctrl+3"));

    //Help
    connect(ui->actionCheckUpdate, &QAction::triggered, this, &MainWindow::CheckUpdate);
    connect(ui->actioniasl_usage, &QAction::triggered, this, &MainWindow::iaslUsage);
    connect(ui->actionUser_Guide, &QAction::triggered, this, &MainWindow::userGuide);
    connect(ui->actionAbout_1, &QAction::triggered, this, &MainWindow::about);
    ui->actionAbout_1->setMenuRole(QAction::AboutRole);

    QIcon icon;

    icon.addFile(":/icon/return.png");
    ui->btnReturn->setIcon(icon);

    ui->cboxCompilationOptions->addItem("-f");
    ui->cboxCompilationOptions->addItem("-tp");
    ui->cboxCompilationOptions->setEditable(true);

    //读取编译参数
    QString qfile = QDir::homePath() + "/.config/QtiASL/QtiASL.ini";
    QFileInfo fi(qfile);

    if (fi.exists()) {
        //QSettings Reg(qfile, QSettings::NativeFormat);
        QSettings Reg(qfile, QSettings::IniFormat); //全平台都采用ini格式
        QString op = Reg.value("options").toString().trimmed();
        if (op.count() > 0)
            ui->cboxCompilationOptions->setCurrentText(op);

        //编码
        //ui->actionUTF_8->setChecked(Reg.value("utf-8").toBool());
        if (ui->actionUTF_8->isChecked())
            lblEncoding->setText("UTF-8");

        //ui->actionGBK->setChecked(Reg.value("gbk").toBool());
        if (ui->actionGBK->isChecked())
            lblEncoding->setText("GBK");
    }

    //设置编译功能屏蔽
    ui->actionCompiling->setEnabled(false);
}

void MainWindow::setLexer(QsciLexer* textLexer, QsciScintilla* textEdit)
{

    //获取背景色
    QPalette pal = this->palette();
    QBrush brush = pal.window();
    red = brush.color().red();

    //设置行号栏宽度、颜色、字体
    QFont m_font;

#ifdef Q_OS_WIN32
    textEdit->setMarginWidth(0, 80);
    m_font.setPointSize(9);
#endif

#ifdef Q_OS_LINUX
    textEdit->setMarginWidth(0, 60);
    m_font.setPointSize(12);
#endif

#ifdef Q_OS_MAC
    textEdit->setMarginWidth(0, 60);
    m_font.setPointSize(13);
#endif

    textEdit->setMarginType(0, QsciScintilla::NumberMargin);
    textEdit->setMarginLineNumbers(0, true);

    m_font.setFamily(font.family());
    textEdit->setMarginsFont(m_font);
    if (red < 55) //暗模式，mac下为50
    {
        textEdit->setMarginsBackgroundColor(QColor(50, 50, 50));
        textEdit->setMarginsForegroundColor(Qt::white);

    } else {
        textEdit->setMarginsBackgroundColor(brush.color());
        textEdit->setMarginsForegroundColor(Qt::black);
    }

    if (red < 55) //暗模式，mac下为50
    {

        //设置光标所在行背景色
        textEdit->setCaretLineBackgroundColor(QColor(180, 180, 0));
        textEdit->setCaretLineFrameWidth(1);
        textEdit->setCaretLineVisible(true);

        textLexer->setColor(QColor(30, 190, 30), QsciLexerCPP::CommentLine); //"//"注释颜色
        textLexer->setColor(QColor(30, 190, 30), QsciLexerCPP::Comment);

        textLexer->setColor(QColor(210, 210, 210), QsciLexerCPP::Identifier);
        textLexer->setColor(QColor(245, 150, 147), QsciLexerCPP::Number);
        textLexer->setColor(QColor(100, 100, 250), QsciLexerCPP::Keyword);
        textLexer->setColor(QColor(210, 32, 240), QsciLexerCPP::KeywordSet2);
        textLexer->setColor(QColor(245, 245, 245), QsciLexerCPP::Operator);
        textLexer->setColor(QColor(84, 235, 159), QsciLexerCPP::DoubleQuotedString); //双引号
    } else {

        textEdit->setCaretLineBackgroundColor(QColor(255, 255, 0, 50));
        textEdit->setCaretLineFrameWidth(0);
        textEdit->setCaretLineVisible(true);

        textLexer->setColor(QColor(30, 190, 30), QsciLexerCPP::CommentLine); //"//"注释颜色
        textLexer->setColor(QColor(30, 190, 30), QsciLexerCPP::Comment);

        textLexer->setColor(QColor(255, 0, 0), QsciLexerCPP::Number);
        textLexer->setColor(QColor(0, 0, 255), QsciLexerCPP::Keyword);
        textLexer->setColor(QColor(0, 0, 0), QsciLexerCPP::Identifier);
        textLexer->setColor(QColor(210, 0, 210), QsciLexerCPP::KeywordSet2);
        textLexer->setColor(QColor(20, 20, 20), QsciLexerCPP::Operator);
        textLexer->setColor(QColor(205, 38, 38), QsciLexerCPP::DoubleQuotedString); //双引号
    }

    //匹配大小括弧
    textEdit->setBraceMatching(QsciScintilla::SloppyBraceMatch);
    //textEdit->setBraceMatching(QsciScintilla::StrictBraceMatch));//不推荐
    if (red > 55) //亮模式，mac下阈值为50
    {
        textEdit->setMatchedBraceBackgroundColor(QColor(Qt::green));
        textEdit->setMatchedBraceForegroundColor(QColor(Qt::red));
    }

    //设置括号等自动补全
    textEdit->setAutoIndent(true);
    textEdit->setTabIndents(true); //true如果行前空格数少于tabWidth，补齐空格数,false如果在文字前tab同true，如果在行首tab，则直接增加tabwidth个空格

    //代码提示
    QsciAPIs* apis = new QsciAPIs(textLexer);
    if (apis->load(":/data/apis.txt")) {

    } else
        apis->add(QString("Device"));

    apis->prepare();

    //设置自动补全
    textEdit->setCaretLineVisible(true);
    // Ascii|None|All|Document|APIs
    //禁用自动补全提示功能、所有可用的资源、当前文档中出现的名称都自动补全提示、使用QsciAPIs类加入的名称都自动补全提示
    textEdit->setAutoCompletionSource(QsciScintilla::AcsAll); //自动补全,对于所有Ascii字符
    textEdit->setAutoCompletionCaseSensitivity(false); //大小写敏感度
    textEdit->setAutoCompletionThreshold(2); //从第几个字符开始出现自动补全的提示
    //textEdit->setAutoCompletionReplaceWord(false);//是否用补全的字符串替代光标右边的字符串

    //设置缩进参考线
    textEdit->setIndentationGuides(true);
    //textEdit->setIndentationGuidesBackgroundColor(QColor(Qt::white));
    //textEdit->setIndentationGuidesForegroundColor(QColor(Qt::red));

    //设置光标颜色
    if (red < 55) //暗模式，mac下为50
        textEdit->setCaretForegroundColor(QColor(Qt::white));
    else
        textEdit->setCaretForegroundColor(QColor(Qt::black));
    textEdit->setCaretWidth(2);

    //自动折叠区域
    textEdit->setMarginType(3, QsciScintilla::SymbolMargin);
    textEdit->setMarginLineNumbers(3, false);
    textEdit->setMarginWidth(3, 15);
    textEdit->setMarginSensitivity(3, true);
    textEdit->setFolding(QsciScintilla::BoxedTreeFoldStyle); //折叠样式
    if (red < 55) //暗模式，mac下为50
    {
        textEdit->setFoldMarginColors(Qt::gray, Qt::black);
        //textEdit->setMarginsForegroundColor(Qt::red);  //行号颜色
        textEdit->SendScintilla(QsciScintilla::SCI_SETFOLDFLAGS, 16); //设置折叠标志
        //textEdit->SendScintilla(QsciScintilla::SCI_SETFOLDMARGINCOLOUR,Qt::red);
    } else {
        textEdit->setFoldMarginColors(Qt::gray, Qt::white); //折叠栏颜色
        //textEdit->setMarginsForegroundColor(Qt::blue); //行号颜色
        textEdit->SendScintilla(QsciScintilla::SCI_SETFOLDFLAGS, 16); //设置折叠标志
    }

    /*断点设置区域,为后面可能会用到的功能预留*/
    /*textEdit->setMarginType(1, QsciScintilla::SymbolMargin);
    textEdit->setMarginLineNumbers(1, false);
    textEdit->setMarginWidth(1,20);
    textEdit->setMarginSensitivity(1, true);    //设置是否可以显示断点
    textEdit->setMarginsBackgroundColor(QColor("#bbfaae"));
    textEdit->setMarginMarkerMask(1, 0x02);
    connect(textEdit, SIGNAL(marginClicked(int, int, Qt::KeyboardModifiers)),this,
            SLOT(on_margin_clicked(int, int, Qt::KeyboardModifiers)));
    textEdit->markerDefine(QsciScintilla::Circle, 1);
    textEdit->setMarkerBackgroundColor(QColor("#ee1111"), 1);*/
    //单步执行显示区域
    /*textEdit->setMarginType(2, QsciScintilla::SymbolMargin);
    textEdit->setMarginLineNumbers(2, false);
    textEdit->setMarginWidth(2, 20);
    textEdit->setMarginSensitivity(2, false);
    textEdit->setMarginMarkerMask(2, 0x04);
    textEdit->markerDefine(QsciScintilla::RightArrow, 2);
    textEdit->setMarkerBackgroundColor(QColor("#eaf593"), 2);*/
}

void MainWindow::init_miniEdit()
{

    miniEdit = new MiniEditor;
    miniDlg = new miniDialog(this);
    miniDlgEdit->setFont(font);
    miniDlg->close();

#ifdef Q_OS_WIN32

    ui->dockWidget_5->setFixedWidth(155);

#endif

#ifdef Q_OS_LINUX
    ui->dockWidget_5->setFixedWidth(120);

#endif

#ifdef Q_OS_MAC
    ui->dockWidget_5->setFixedWidth(115);

#endif

    miniEdit->setContextMenuPolicy(Qt::NoContextMenu);
    miniEdit->setMarginWidth(0, 0);
    miniEdit->setMargins(0);
    miniEdit->setReadOnly(1);
    miniEdit->SendScintilla(QsciScintillaBase::SCI_SETCURSOR, 0, 7);

    miniEdit->setWrapMode(QsciScintilla::WrapNone);

    QFont minifont;
    minifont.setFamily(font.family());
    minifont.setPointSize(1);

    QscilexerCppAttach* miniLexer = new QscilexerCppAttach;
    miniEdit->setLexer(miniLexer);
    miniLexer->setFont(minifont);

    if (red < 55) //暗模式，mac下为50
    {

        miniLexer->setColor(QColor(30, 190, 30), QsciLexerCPP::CommentLine); //"//"注释颜色
        miniLexer->setColor(QColor(30, 190, 30), QsciLexerCPP::Comment);

        miniLexer->setColor(QColor(210, 210, 210), QsciLexerCPP::Identifier);
        miniLexer->setColor(QColor(245, 150, 147), QsciLexerCPP::Number);
        miniLexer->setColor(QColor(100, 100, 250), QsciLexerCPP::Keyword);
        miniLexer->setColor(QColor(210, 32, 240), QsciLexerCPP::KeywordSet2);
        miniLexer->setColor(QColor(245, 245, 245), QsciLexerCPP::Operator);
        miniLexer->setColor(QColor(84, 235, 159), QsciLexerCPP::DoubleQuotedString); //双引号
    } else {

        miniLexer->setColor(QColor(30, 190, 30), QsciLexerCPP::CommentLine); //"//"注释颜色
        miniLexer->setColor(QColor(30, 190, 30), QsciLexerCPP::Comment);

        miniLexer->setColor(QColor(255, 0, 0), QsciLexerCPP::Number);
        miniLexer->setColor(QColor(0, 0, 255), QsciLexerCPP::Keyword);
        miniLexer->setColor(QColor(0, 0, 0), QsciLexerCPP::Identifier);
        miniLexer->setColor(QColor(210, 0, 210), QsciLexerCPP::KeywordSet2);
        miniLexer->setColor(QColor(20, 20, 20), QsciLexerCPP::Operator);
        miniLexer->setColor(QColor(205, 38, 38), QsciLexerCPP::DoubleQuotedString); //双引号
    }

    connect(miniEdit, &QsciScintilla::cursorPositionChanged, this, &MainWindow::miniEdit_cursorPositionChanged);
    //connect(miniEdit->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(setValue()));
    connect(miniEdit->verticalScrollBar(), SIGNAL(valueChanged(int)), miniEdit, SLOT(miniEdit_verticalScrollBarChanged()));

    //水平滚动棒
    miniEdit->SendScintilla(QsciScintilla::SCI_SETSCROLLWIDTH, -1);
    miniEdit->SendScintilla(QsciScintilla::SCI_SETSCROLLWIDTHTRACKING, false);
    miniEdit->horizontalScrollBar()->setHidden(true);

    //miniEdit->SendScintilla(QsciScintilla::SCI_SETMOUSEDOWNCAPTURES, 0, true);

    //接受文件拖放打开
    miniEdit->setAcceptDrops(false);
    this->setAcceptDrops(true);
}

void MainWindow::init_edit(QsciScintilla* textEdit)
{

    textEditBack = new QsciScintilla();

    textEdit->setWrapMode(QsciScintilla::WrapNone);

    //设置编码为UTF-8
    textEdit->SendScintilla(QsciScintilla::SCI_SETCODEPAGE, QsciScintilla::SC_CP_UTF8);

    textEdit->setTabWidth(4);

    //水平滚动条:暂时关闭下面两行代码，否则没法设置水平滚动条的数据
    //textEdit->SendScintilla(QsciScintilla::SCI_SETSCROLLWIDTH, textEdit->viewport()->width());
    //textEdit->SendScintilla(QsciScintilla::SCI_SETSCROLLWIDTHTRACKING, true);

    connect(textEdit, &QsciScintilla::cursorPositionChanged, this, &MainWindow::textEdit_cursorPositionChanged);
    connect(textEdit, &QsciScintilla::textChanged, this, &MainWindow::textEdit_textChanged);
    connect(textEdit, &QsciScintilla::linesChanged, this, &MainWindow::textEdit_linesChanged);

    connect(textEdit->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(setValue2()));

    textEdit->setFont(font);
    textEdit->setMarginsFont(font);

    textLexer = new QscilexerCppAttach;
    textEdit->setLexer(textLexer);

    //读取字体
    QString qfile = QDir::homePath() + "/.config/QtiASL/QtiASL.ini";
    QFileInfo fi(qfile);

    if (fi.exists()) {
        //QSettings Reg(qfile, QSettings::NativeFormat);
        QSettings Reg(qfile, QSettings::IniFormat); //全平台都采用ini格式
        if (Reg.value("FontName").toString() != "") {
            font.setFamily(Reg.value("FontName").toString());
            font.setPointSize(Reg.value("FontSize").toInt());
            font.setBold(Reg.value("FontBold").toBool());
            font.setItalic(Reg.value("FontItalic").toBool());
            font.setUnderline(Reg.value("FontUnderline").toBool());
        }
    }

    textLexer->setFont(font);

    setLexer(textLexer, textEdit);

    //接受文件拖放打开
    textEdit->setAcceptDrops(false);
    this->setAcceptDrops(true);

    ui->editReplace->setClearButtonEnabled(true);

    if (red < 55) {

        QPalette palette;
        palette = ui->editFind->palette();
        palette.setColor(QPalette::Base, QColor(50, 50, 50));
        palette.setColor(QPalette::Text, Qt::white); //字色
        ui->editFind->setPalette(palette);

    } else {

        QPalette palette;
        palette = ui->editFind->palette();
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::Text, Qt::black); //字色
        ui->editFind->setPalette(palette);
    }
}

void MainWindow::init_treeWidget()
{

    int w;
    QScreen* screen = QGuiApplication::primaryScreen();
    w = screen->size().width();
    //ui->tabWidget_misc->setMinimumWidth(w / 3 - 80);
    //ui->tabWidget_misc->setStyleSheet("QTabBar::tab {width:0px;}");

    treeWidgetBak = new QTreeWidget;

    //ui->treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    //设置水平滚动条
    ui->treeWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->treeWidget->header()->setStretchLastSection(true);

    ui->treeWidget->setFont(font);
    QFont hFont;
    hFont.setPointSize(font.pointSize() - 1);
    ui->treeWidget->header()->setFont(hFont);

    ui->treeWidget->setHeaderHidden(true);

    ui->treeWidget->setColumnCount(2);
    ui->treeWidget->setColumnHidden(1, true);

    //ui->treeWidget->setColumnWidth(0, ui->tab_misc1->width() - 230);
    ui->treeWidget->setColumnWidth(1, 100);
    ui->treeWidget->setHeaderItem(new QTreeWidgetItem(QStringList() << tr("Members") << "Lines"));

    ui->treeWidget->setStyle(QStyleFactory::create("windows")); //连接的虚线
    ui->treeWidget->setIconSize(QSize(12, 12));

    setVScrollBarStyle(red);

    ui->treeWidget->installEventFilter(this);
    ui->treeWidget->setAlternatingRowColors(true); //底色交替显示
    ui->treeWidget->setStyleSheet("QTreeWidget::item:hover{background-color:rgba(127,255,0,50)}"
                                  "QTreeWidget::item:selected{background-color:rgba(30, 100, 255, 255); color:rgba(255,255,255,255)}");

    //connect(ui->treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::treeWidgetBack_itemClicked);

    //右键菜单
    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu); //给控件设置上下文菜单策略
    QMenu* menu = new QMenu(this);

    QAction* actionExpandAll = new QAction(tr("Expand all"), this);
    menu->addAction(actionExpandAll);
    connect(actionExpandAll, &QAction::triggered, this, &MainWindow::on_actionExpandAll);

    QAction* actionCollapseAll = new QAction(tr("Collapse all"), this);
    menu->addAction(actionCollapseAll);
    connect(actionCollapseAll, &QAction::triggered, this, &MainWindow::on_actionCollapseAll);

    connect(ui->treeWidget, &QTreeWidget::customContextMenuRequested, [=](const QPoint& pos) {
        Q_UNUSED(pos);
        //qDebug() << pos; //参数pos用来传递右键点击时的鼠标的坐标
        menu->exec(QCursor::pos());
    });

    ui->lblMembers->setHidden(true);
    ui->dockWidget->setWindowTitle(tr("Members"));
}

void MainWindow::init_filesystem()
{

    ui->treeView->installEventFilter(this); //安装事件过滤器
    ui->treeView->setAlternatingRowColors(true); //不同的底色交替显示

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu); //给控件设置上下文菜单策略
    QMenu* menu = new QMenu(this);
    QAction* actionOpenDir = new QAction(tr("Open directory"), this);
    menu->addAction(actionOpenDir);
    connect(actionOpenDir, &QAction::triggered, this, &MainWindow::on_actionOpenDir);
    connect(ui->treeView, &QTreeView::customContextMenuRequested, [=](const QPoint& pos) {
        Q_UNUSED(pos);
        //qDebug() << pos; //参数pos用来传递右键点击时的鼠标的坐标
        menu->exec(QCursor::pos());
    });

    model = new QFileSystemModel;

#ifdef Q_OS_WIN32
    model->setRootPath("");

#endif

#ifdef Q_OS_LINUX
    model->setRootPath("");

#endif

#ifdef Q_OS_MAC
    model->setRootPath("/Volumes");

#endif

    ui->treeView->setModel(model);
    ui->treeView->setColumnWidth(3, 200); //注意顺序
    ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents); //表头列宽自适应

    ui->treeView->setAnimated(false);
    ui->treeView->setIndentation(20);
    ui->treeView->setSortingEnabled(true);
    //const QSize availableSize = ui->treeView->screen()->availableGeometry().size();
    const QSize availableSize = ui->treeView->geometry().size();
    ui->treeView->resize(availableSize / 2);
    ui->treeView->setColumnWidth(0, ui->treeView->width() / 3);

    ui->tabWidget_misc->setCurrentIndex(0);

    //读取之前的目录
    QString qfile = QDir::homePath() + "/.config/QtiASL/QtiASL.ini";

    QFileInfo fi(qfile);

    if (fi.exists()) {

        QSettings Reg(qfile, QSettings::IniFormat); //全平台都采用ini格式

        QString dir = Reg.value("dir").toString().trimmed();
        QString btn = Reg.value("btn").toString().trimmed();
        ui->treeView->setRootIndex(model->index(dir));
        fsm_Index = model->index(dir);
        ui->btnReturn->setText(btn);
        //set_return_text(dir);

        //读取成员列表窗口的宽度和信息显示窗口的高度
        int m_w = Reg.value("members_win", 375).toInt();
        resizeDocks({ ui->dockWidget }, { m_w }, Qt::Horizontal);

        int i_h = Reg.value("info_win", 150).toInt();
        resizeDocks({ ui->dockWidget_6 }, { i_h }, Qt::Vertical);
    }
}

void MainWindow::separ_info(QString str_key, QTextEdit* editInfo)
{

    editInfo->clear();

    QTextBlock block = textEditTemp->document()->findBlockByNumber(0);
    textEditTemp->setTextCursor(QTextCursor(block));

    int info_count = 0;
    for (int i = 0; i < textEditTemp->document()->lineCount(); i++) {
        QTextBlock block = textEditTemp->document()->findBlockByNumber(i);
        textEditTemp->setTextCursor(QTextCursor(block));

        QString str = textEditTemp->document()->findBlockByLineNumber(i).text();
        QString sub = str.trimmed();

        if (sub.mid(0, str_key.count()) == str_key) {

            QString str0 = textEditTemp->document()->findBlockByNumber(i - 1).text();
            editInfo->append(str0);

            editInfo->append(str);
            editInfo->append("");

            info_count++;
        }
    }

    //标记tab头

    if (str_key == "Error") {
        ui->tabWidget->setTabText(1, tr("Errors") + " (" + QString::number(info_count) + ")");
    }

    if (str_key == "Warning") {
        ui->tabWidget->setTabText(2, tr("Warnings") + " (" + QString::number(info_count) + ")");
    }

    if (str_key == "Remark") {
        ui->tabWidget->setTabText(3, tr("Remarks") + " (" + QString::number(info_count) + ")");
    }

    if (str_key == "Optimization")
        ui->tabWidget->setTabText(4, tr("Scribble"));

    ui->listWidget->clear();
    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i1.png"), tr("BasicInfo")));
    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i2.png"), ui->tabWidget->tabBar()->tabText(1)));
    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i3.png"), ui->tabWidget->tabBar()->tabText(2)));
    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i4.png"), ui->tabWidget->tabBar()->tabText(3)));
    ui->listWidget->addItem(new QListWidgetItem(QIcon(":/icon/i5.png"), ui->tabWidget->tabBar()->tabText(4)));
}

void MainWindow::on_editErrors_cursorPositionChanged()
{
    if (!loading) {
        set_cursor_line_color(ui->editErrors);
        gotoLine(ui->editErrors);
    }
}

void MainWindow::on_editWarnings_cursorPositionChanged()
{
    if (!loading) {
        set_cursor_line_color(ui->editWarnings);
        gotoLine(ui->editWarnings);
    }
}

void MainWindow::on_editRemarks_cursorPositionChanged()
{
    if (!loading) {
        set_cursor_line_color(ui->editRemarks);
        gotoLine(ui->editRemarks);
    }
}

void MainWindow::on_editOptimizations_cursorPositionChanged()
{
    set_cursor_line_color(ui->editOptimizations);
    gotoLine(ui->editOptimizations);
}

void MainWindow::regACPI_win()
{
    QString appPath = qApp->applicationFilePath();

    QString dir = qApp->applicationDirPath();
    //注意路径的替换
    appPath.replace("/", "\\");
    QString type = "QtiASL";
    QSettings* regType = new QSettings("HKEY_CLASSES_ROOT\\.dsl", QSettings::NativeFormat);
    QSettings* regIcon = new QSettings("HKEY_CLASSES_ROOT\\.dsl\\DefaultIcon", QSettings::NativeFormat);
    QSettings* regShell = new QSettings("HKEY_CLASSES_ROOT\\QtiASL\\shell\\open\\command", QSettings::NativeFormat);

    QSettings* regType1 = new QSettings("HKEY_CLASSES_ROOT\\.aml", QSettings::NativeFormat);
    QSettings* regIcon1 = new QSettings("HKEY_CLASSES_ROOT\\.aml\\DefaultIcon", QSettings::NativeFormat);
    QSettings* regShell1 = new QSettings("HKEY_CLASSES_ROOT\\QtiASL\\shell\\open\\command", QSettings::NativeFormat);

    regType->remove("Default");
    regType->setValue("Default", type);

    regType1->remove("Default");
    regType1->setValue("Default", type);

    regIcon->remove("Default");
    // 0 使用当前程序内置图标
    regIcon->setValue("Default", appPath + ",1");

    regIcon1->remove("Default");
    // 0 使用当前程序内置图标
    regIcon1->setValue("Default", appPath + ",0");

    // 百分号问题
    QString shell = "\"" + appPath + "\" ";
    shell = shell + "\"%1\"";

    regShell->remove("Default");
    regShell->setValue("Default", shell);

    regShell1->remove("Default");
    regShell1->setValue("Default", shell);

    delete regIcon;
    delete regShell;
    delete regType;

    delete regIcon1;
    delete regShell1;
    delete regType1;
    // 通知系统刷新
#ifdef Q_OS_WIN32
    //::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST|SHCNF_FLUSH, 0, 0);
#endif
}

void MainWindow::closeEvent(QCloseEvent* event)
{

    //存储编译选项
    QString qfile = QDir::homePath() + "/.config/QtiASL/QtiASL.ini";
    QFile file(qfile);
    //QSettings Reg(qfile, QSettings::NativeFormat);
    QSettings Reg(qfile, QSettings::IniFormat); //全平台都采用ini格式
    Reg.setValue("options", ui->cboxCompilationOptions->currentText().trimmed());

    //存储搜索历史文本
    int count = ui->editFind->count();

    if (count > 200)
        count = 200;
    for (int i = 0; i < count; i++) {
        Reg.setValue("FindText" + QString::number(i + 1), ui->editFind->itemText(i));
    }
    Reg.setValue("countFindText", count);

    //存储minimap
    Reg.setValue("minimap", ui->actionMinimap->isChecked());

    //存储编码选项
    Reg.setValue("utf-8", ui->actionUTF_8->isChecked());
    Reg.setValue("gbk", ui->actionGBK->isChecked());

    //存储成员列表的宽度和信息窗口高度
    Reg.setValue("members_win", ui->dockWidget->width());
    if (InfoWinShow) //不显示不存储
        Reg.setValue("info_win", ui->dockWidget_6->height());

    //存储当前的目录结构
    QWidget* pWidget = ui->tabWidget_textEdit->widget(ui->tabWidget_textEdit->currentIndex());

    lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout

    QFileInfo f(lblCurrentFile->text());
    Reg.setValue("dir", f.path());
    Reg.setValue("btn", ui->btnReturn->text());
    Reg.setValue("ci", ui->tabWidget_textEdit->currentIndex()); //存储当前活动的标签页

    //检查文件是否修改，需要保存
    for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {

        ui->tabWidget_textEdit->setCurrentIndex(i);
        pWidget = ui->tabWidget_textEdit->widget(i);
        textEdit = getCurrentEditor(i);

        lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber);

        if (lblCurrentFile->text() == tr("untitled") + ".dsl")
            curFile = "";
        else
            curFile = lblCurrentFile->text();

        if (getCurrentEditor(i)->isModified()) {

            int choice;
            if (!zh_cn) {

                choice = QMessageBox::warning(this, tr("Application"),
                    tr("The document has been modified.\n"
                       "Do you want to save your changes?\n\n")
                        + ui->tabWidget_textEdit->tabBar()->tabText(i),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            } else {
                QMessageBox message(QMessageBox::Warning, "QtiASL", "文件内容已修改，是否保存？\n\n" + ui->tabWidget_textEdit->tabBar()->tabText(i));
                message.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
                message.setButtonText(QMessageBox::Save, QString("保 存"));
                message.setButtonText(QMessageBox::Cancel, QString("取 消"));
                message.setButtonText(QMessageBox::Discard, QString("放 弃"));
                message.setDefaultButton(QMessageBox::Save);
                choice = message.exec();
            }

            switch (choice) {
            case QMessageBox::Save:
                Save();
                event->accept();
                break;
            case QMessageBox::Discard:
                event->accept();
                break;
            case QMessageBox::Cancel:
                event->ignore();
                break;
            }
        } else {
            event->accept();
        }
    }

    //Save tabs
    Reg.setValue("count", ui->tabWidget_textEdit->tabBar()->count());

    for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {

        pWidget = ui->tabWidget_textEdit->widget(i);
        lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout

        getCurrentEditor(i)->getCursorPosition(&rowDrag, &colDrag);
        vs = getCurrentEditor(i)->verticalScrollBar()->sliderPosition();
        hs = getCurrentEditor(i)->horizontalScrollBar()->sliderPosition();

        Reg.setValue(QString::number(i) + "/" + "file", lblCurrentFile->text());
        Reg.setValue(QString::number(i) + "/" + "row", rowDrag);
        Reg.setValue(QString::number(i) + "/" + "col", colDrag);
        Reg.setValue(QString::number(i) + "/" + "vs", vs);
        Reg.setValue(QString::number(i) + "/" + "hs", hs);
    }

    //Save scribble board
    QString errorMessage;
    QSaveFile fileScribble(QDir::homePath() + "/.config/QtiASL/Scribble.txt");
    if (fileScribble.open(QFile::WriteOnly | QFile::Text)) {
        QTextStream out(&fileScribble);
        out << ui->editScribble->document()->toPlainText();

        if (!fileScribble.commit()) {
            errorMessage = tr("Cannot write file %1:\n%2.")
                               .arg(QDir::toNativeSeparators(fileName), file.errorString());
        }
    }

    //In multi-window, close the form and delete yourself
    for (int i = 0; i < wdlist.count(); i++) {
        if (this == wdlist.at(i)) {
            wdlist.removeAt(i);
            filelist.removeAt(i);
        }
    }

    //关闭线程
    if (!thread_end) {
        break_run = true;
        mythread->quit();
        mythread->wait();
    }
}

void MainWindow::recentOpen(QString filename)
{

    loadFile(openFile(filename), -1, -1);
}

void MainWindow::ssdtOpen(QString filename)
{

    loadFile(openFile(filename), -1, -1);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{

    if (e->mimeData()->hasFormat("text/uri-list")) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* e)
{
    QList<QUrl> urls = e->mimeData()->urls();
    if (urls.isEmpty()) {
        return;
    }

    for (int i = 0; i < urls.count(); i++) {
        QString fileName = urls.at(i).toLocalFile();
        if (fileName.isEmpty()) {
            return;
        }

        loadFile(openFile(fileName), -1, -1);
    }
}

void MainWindow::init_statusBar()
{
    //状态栏
    locationLabel = new QLabel(this);
    statusBar()->addWidget(locationLabel);

    lblLayer = new QLabel(this);
    QPalette label_palette;
    label_palette.setColor(QPalette::Background, QColor(30, 100, 255, 255));
    label_palette.setColor(QPalette::WindowText, Qt::white);
    lblLayer->setAutoFillBackground(true);
    lblLayer->setPalette(label_palette);
    lblLayer->setText(tr(" Layer "));
    lblLayer->setTextInteractionFlags(Qt::TextSelectableByMouse); //允许选择其中的文字

    editLayer = new QLineEdit();
    editLayer->setReadOnly(true);
    ui->statusbar->addPermanentWidget(lblLayer);

    lblEncoding = new QLabel(this);
    lblEncoding->setText("UTF-8");
    ui->statusbar->addPermanentWidget(lblEncoding);

    QComboBox* cboxEncoding = new QComboBox;
    cboxEncoding->addItem("UTF-8");
    cboxEncoding->addItem("GBK");
    ui->statusbar->layout()->setMargin(0);
    ui->statusbar->layout()->setSpacing(0);
    //ui->statusbar->addPermanentWidget(cboxEncoding);

    lblMsg = new QLabel(this);
    ui->statusbar->addPermanentWidget(lblMsg);
}

void MainWindow::newFile()
{

    if (!thread_end) {
        break_run = true; //通知打断线程
        mythread->quit();
        mythread->wait();
    }

    loading = true;

    ui->treeWidget->clear();
    s_count = 0;
    d_count = 0;
    m_count = 0;
    QString lblMembers = "Scope(" + QString::number(s_count) + ")  " + "Device(" + QString::number(d_count) + ")  " + "Method(" + QString::number(m_count) + ")";
    ui->treeWidget->setHeaderLabel(lblMembers);
    ui->lblMembers->setText(lblMembers);

    ui->tabWidget_misc->tabBar()->setTabText(0, lblMembers);

    ui->editShowMsg->clear();
    ui->editErrors->clear();
    ui->editWarnings->clear();
    ui->editRemarks->clear();

    textEdit = new QsciScintilla(this);
    //textEdit = new MaxEditor;

    init_edit(textEdit);

    //QWidget* page = new QWidget(this);
    MyTabPage* page = new MyTabPage;

    QVBoxLayout* vboxLayout = new QVBoxLayout(page);
    vboxLayout->setMargin(0);
    vboxLayout->addWidget(textEdit);
    QLabel* lbl = new QLabel(tr("untitled") + ".dsl");
    vboxLayout->addWidget(lbl);
    lbl->setHidden(true);

    ui->tabWidget_textEdit->addTab(page, tr("untitled") + ".dsl");
    //ui->tabWidget_textEdit->appendNormalPage(page);

    ui->tabWidget_textEdit->setCurrentIndex(ui->tabWidget_textEdit->tabBar()->count() - 1);
    ui->tabWidget_textEdit->setTabsClosable(true);

    QIcon icon(":/icon/d.png");
    ui->tabWidget_textEdit->tabBar()->setTabIcon(ui->tabWidget_textEdit->tabBar()->count() - 1, icon);
    One = false;

    curFile = "";
    shownName = "";
    setWindowTitle(ver + tr("untitled") + ".dsl");

    textEdit->clear();
    textEditBack->clear();
    miniEdit->clear();

    lblLayer->setText("");
    lblMsg->setText("");

    ui->treeWidget->setHidden(false);

    loading = false;
}

void MainWindow::on_btnReplaceFind()
{
    on_btnReplace();
    if (find_down)
        on_btnFindNext();
    if (find_up)
        on_btnFindPrevious();
}

void MainWindow::on_chkCaseSensitive_clicked()
{
}

void MainWindow::on_chkCaseSensitive_clicked(bool checked)
{
    CaseSensitive = checked;
    on_btnFindNext();
}

/*菜单：设置字体*/
void MainWindow::set_font()
{

    bool ok;
    QFontDialog fd;

    font = fd.getFont(&ok, font);

    if (ok) {

        for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {

            textLexer->setFont(font);
            getCurrentEditor(i)->setLexer(textLexer);
            setLexer(textLexer, getCurrentEditor(i));

            QFont m_font;
            m_font.setFamily(font.family());
            getCurrentEditor(i)->setMarginsFont(m_font);
        }

        miniDlgEdit->setFont(font);

        //存储字体信息
        QString qfile = QDir::homePath() + "/.config/QtiASL/QtiASL.ini";
        QFile file(qfile);

        QSettings Reg(qfile, QSettings::IniFormat);
        Reg.setValue("FontName", font.family());
        Reg.setValue("FontSize", font.pointSize());
        Reg.setValue("FontBold", font.bold());
        Reg.setValue("FontItalic", font.italic());
        Reg.setValue("FontUnderline", font.underline());
    }
}

/*菜单：是否自动换行*/
void MainWindow::set_wrap()
{

    if (ui->actionWrapWord->isChecked()) {
        textEdit->setWrapMode(QsciScintilla::WrapWord);
        miniEdit->setWrapMode(QsciScintilla::WrapWord);
        miniDlgEdit->setWrapMode(QsciScintilla::WrapWord);
    } else {

        textEdit->setWrapMode(QsciScintilla::WrapNone);
        miniEdit->setWrapMode(QsciScintilla::WrapNone);
        miniDlgEdit->setWrapMode(QsciScintilla::WrapNone);
    }
}

/*重载窗体重绘事件*/
void MainWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    //获取背景色,用来刷新软件使用中，系统切换亮、暗模式
    QPalette pal = this->palette();
    QBrush brush = pal.window();
    int c_red = brush.color().red();
    if (c_red != red) {
        //注意：1.代码折叠线的颜色 2.双引号输入时的背景色
        for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {

            init_edit(getCurrentEditor(i));
        }

        setVScrollBarStyle(c_red);
    }

    if (ui->actionMembers_win->isChecked()) {
        if (ui->dockWidget->isHidden())
            ui->actionMembers_win->setChecked(false);
    }

    if (ui->actionInfo_win->isChecked()) {
        if (ui->dockWidget_6->isHidden())
            ui->actionInfo_win->setChecked(false);
    }
}

void MainWindow::setVScrollBarStyle(int red)
{
    if (mac) {
        if (red < 55) {

            ui->treeWidget->verticalScrollBar()->setStyleSheet("QScrollBar:vertical"
                                                               "{"
                                                               "width:9px;"
                                                               "background:rgba(255,255,255,0%);"
                                                               "margin:0px,0px,0px,0px;"
                                                               "padding-top:9px;"
                                                               "padding-bottom:9px;"
                                                               "}"

                                                               "QScrollBar::handle:vertical"
                                                               "{"
                                                               "width:9px;"
                                                               "background:rgba(150,150,150,50%);"
                                                               " border-radius:4px;"
                                                               "min-height:20;"
                                                               "}"

                                                               "QScrollBar::handle:vertical:hover"
                                                               "{"
                                                               "width:9px;"
                                                               "background:rgba(150,150,150,50%);"
                                                               " border-radius:4px;"
                                                               "min-height:20;"
                                                               "}"

            );

        } else {

            ui->treeWidget->verticalScrollBar()->setStyleSheet("QScrollBar:vertical"
                                                               "{"
                                                               "width:9px;"
                                                               "background:rgba(255,255,255,0%);"
                                                               "margin:0px,0px,0px,0px;"
                                                               "padding-top:9px;"
                                                               "padding-bottom:9px;"
                                                               "}"

                                                               "QScrollBar::handle:vertical"
                                                               "{"
                                                               "width:9px;"
                                                               "background:rgba(0,0,0,25%);"
                                                               " border-radius:4px;"
                                                               "min-height:20;"
                                                               "}"

                                                               "QScrollBar::handle:vertical:hover"
                                                               "{"
                                                               "width:9px;"
                                                               "background:rgba(0,0,0,50%);"
                                                               " border-radius:4px;"
                                                               "min-height:20;"
                                                               "}"

            );
        }
    }
}

int MainWindow::treeCount(QTreeWidget* tree, QTreeWidgetItem* parent)
{
    Q_ASSERT(tree != NULL);

    int count = 0;
    if (parent == 0) {
        int topCount = tree->topLevelItemCount();
        for (int i = 0; i < topCount; i++) {
            QTreeWidgetItem* item = tree->topLevelItem(i);
            if (item->isExpanded()) {
                count += treeCount(tree, item);
            }
        }
        count += topCount;
    } else {
        int childCount = parent->childCount();
        for (int i = 0; i < childCount; i++) {
            QTreeWidgetItem* item = parent->child(i);
            if (item->isExpanded()) {
                count += treeCount(tree, item);
            }
        }
        count += childCount;
    }
    return count;
}

int MainWindow::treeCount(QTreeWidget* tree)
{
    Q_ASSERT(tree != NULL);

    int count = 0;

    int topCount = tree->topLevelItemCount();
    for (int i = 0; i < topCount; i++) {
        QTreeWidgetItem* item = tree->topLevelItem(i);
        if (item->isExpanded()) {
            count += treeCount(tree, item);
        }
    }
    count += topCount;

    return count;
}

/*获取当前条目的所有上级条目*/
QString MainWindow::getLayerName(QTreeWidgetItem* hItem)
{
    if (!hItem)
        return "";

    QString str0 = hItem->text(0); //记录初始值，即为当前被选中的条目值
    QString str;
    char sername[255];
    memset(sername, 0, 255);
    QVector<QString> list;
    QTreeWidgetItem* phItem = hItem->parent(); //获取当前item的父item
    if (!phItem) { // 根节点
        QString qstr = hItem->text(0);
        QByteArray ba = qstr.toLatin1(); //实现QString和 char *的转换
        const char* cstr = ba.data();
        strcpy(sername, cstr);
    } else {
        while (phItem) {

            QString qstr = phItem->text(0);
            QByteArray ba = qstr.toLatin1(); //实现QString和char *的转换
            const char* cstr = ba.data();
            strcpy(sername, cstr);
            phItem = phItem->parent();

            list.push_back(sername);
        }
    }
    for (int i = 0; i < list.count(); i++) {
        str = list.at(i) + " --> " + str;
    }

    return " " + str + str0 + " ";
}

void MainWindow::kextstat()
{
    pk = new QProcess;
    QStringList cs1;
    pk->start("kextstat", cs1);
    connect(pk, SIGNAL(finished(int)), this, SLOT(readKextstat()));
}

void MainWindow::readKextstat()
{
    QString result = QString::fromUtf8(pk->readAll());
    newFile();
    textEdit->append(result);
    textEdit->setModified(false);

    ui->dockWidget_6->setHidden(true);
    ui->actionInfo_win->setChecked(false);
}

void MainWindow::loadLocal()
{
    QTextCodec* codec = QTextCodec::codecForName("System");
    QTextCodec::setCodecForLocale(codec);

    static QTranslator translator; //注意：使translator一直生效
    QLocale locale;
    if (locale.language() == QLocale::English) //获取系统语言环境
    {

        zh_cn = false;

    } else if (locale.language() == QLocale::Chinese) {

        bool tr = false;
        tr = translator.load(":/tr/cn.qm");
        if (tr) {
            qApp->installTranslator(&translator);
            zh_cn = true;
        }

        ui->retranslateUi(this);
    }
}

void MainWindow::on_btnCompile()
{
    miniDlg->close();

    btnCompile_clicked();
}

void MainWindow::on_treeView_doubleClicked(const QModelIndex& index)
{

    fsm_Filepath = model->filePath(index);
    fsm_Index = index;

    if (!model->isDir(index)) {
        loadFile(openFile(fsm_Filepath), -1, -1);
    }
}

void MainWindow::on_btnReturn_clicked()
{

    QString str = model->filePath(fsm_Index.parent());

    ui->treeView->setRootIndex(model->index(str));

    ui->btnReturn->setText(str);

    fsm_Index = fsm_Index.parent();
}

void MainWindow::on_treeView_expanded(const QModelIndex& index)
{
    fsm_Index = index;
    QString str = model->filePath(index);
    set_return_text(str);
}

void MainWindow::on_treeView_collapsed(const QModelIndex& index)
{
    fsm_Index = index;
    QString str = model->filePath(index);
    set_return_text(str);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->treeView) {
        if (event->type() == QEvent::FocusIn) {

        } else if (event->type() == QEvent::FocusOut) {
        }
    }

    if (watched == ui->treeWidget) //判断控件
    {
        if (event->type() == QEvent::FocusIn) //控件获得焦点事件)
        {
            //ui->treeWidget->setStyleSheet( "QTreeWidget::item:hover{background-color:rgb(0,255,0,0)}" "QTreeWidget::item:selected{background-color:rgb(255,0,5)}" );

        } else if (event->type() == QEvent::FocusOut) //控件失去焦点事件
        {
            //ui->treeWidget->setStyleSheet( "QTreeWidget::item:hover{background-color:rgb(0,255,0,0)}" "QTreeWidget::item:selected{background-color:rgb(255,0,0)}" );
        }
    }

    //禁用鼠标滚轮切换标签页
    if (watched == ui->tabWidget_textEdit->tabBar()) {
        if (event->type() == QEvent::Wheel) {
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void MainWindow::on_tabWidget_textEdit_tabBarClicked(int index)
{

    miniDlg->close();

    if (index == -1) //点击标签页之外的区域
        return;

    //取消自动换行
    ui->actionWrapWord->setChecked(false);
    textEdit->setWrapMode(QsciScintilla::WrapNone);
    miniEdit->setWrapMode(QsciScintilla::WrapNone);
    miniDlgEdit->setWrapMode(QsciScintilla::WrapNone);

    QWidget* pWidget = ui->tabWidget_textEdit->widget(index);

    lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout

    if (lblCurrentFile->text() == tr("untitled") + ".dsl") {
        curFile = "";

    } else {

        curFile = lblCurrentFile->text();
    }

    textEdit = getCurrentEditor(index);

    on_btnRefreshTree();

    setWindowTitle(ver + lblCurrentFile->text());

    QFileInfo f(curFile);
    if (f.suffix().toLower() == "dsl" || f.suffix().toLower() == "cpp" || f.suffix().toLower() == "c") {
        ui->actionCompiling->setEnabled(true);

    } else {
        ui->actionCompiling->setEnabled(false);
    }

    //初始化fsm
    ui->treeView->setRootIndex(model->index(f.path()));
    fsm_Index = model->index(f.path());
    set_return_text(f.path());
    ui->treeView->setCurrentIndex(model->index(curFile)); //并设置当前条目为打开的文件

    textEdit->setFocus();

    //刷新打开的文件列表供监控文件的修改使用
    openFileList.clear();
    for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {
        QWidget* pWidget = ui->tabWidget_textEdit->widget(i);

        lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout

        openFileList.push_back(lblCurrentFile->text());
        FileSystemWatcher::addWatchPath(lblCurrentFile->text());
    }

    dragFileName = curFile;
    getCurrentEditor(index)->getCursorPosition(&rowDrag, &colDrag);
    vs = getCurrentEditor(index)->verticalScrollBar()->sliderPosition();
    hs = getCurrentEditor(index)->horizontalScrollBar()->sliderPosition();

    One = false;

    if (!textEdit->isModified()) {

        ui->actionSave->setEnabled(false);
    } else {

        ui->actionSave->setEnabled(true);
    }
}

QsciScintilla* MainWindow::getCurrentEditor(int index)
{
    QWidget* pWidget = ui->tabWidget_textEdit->widget(index);
    QsciScintilla* edit = new QsciScintilla;
    edit = (QsciScintilla*)pWidget->children().at(editNumber);

    return edit;
}

void MainWindow::closeTab(int index)
{

    if (ui->tabWidget_textEdit->tabBar()->count() > 1) {

        ui->tabWidget_textEdit->setCurrentIndex(index);

        textEdit = getCurrentEditor(index);

        QWidget* pWidget = ui->tabWidget_textEdit->widget(index);

        lblCurrentFile = (QLabel*)pWidget->children().at(lblNumber); //2为QLabel,1为textEdit,0为VBoxLayout

        if (lblCurrentFile->text() == tr("untitled") + ".dsl")
            curFile = "";
        else
            curFile = lblCurrentFile->text();

        if (maybeSave(ui->tabWidget_textEdit->tabBar()->tabText(index))) {

            ui->tabWidget_textEdit->removeTab(index);
            if (QFileInfo(lblCurrentFile->text()).exists())
                FileSystemWatcher::removeWatchPath(lblCurrentFile->text()); //移出文件监控
        }

    } else
        ui->tabWidget_textEdit->setTabsClosable(false);

    on_tabWidget_textEdit_tabBarClicked(ui->tabWidget_textEdit->currentIndex());
}

void MainWindow::on_tabWidget_textEdit_currentChanged(int index)
{

    if (index >= 0 && m_searchTextPosList.count() > 0) {

        for (int i = 0; i < ui->tabWidget_textEdit->tabBar()->count(); i++) {

            clearSearchHighlight(getCurrentEditor(i));
        }

        m_searchTextPosList.clear();
    }
}

void MainWindow::view_info()
{
    if (ui->dockWidget_6->isHidden()) {
        ui->dockWidget_6->setHidden(false);
        InfoWinShow = true;
        ui->actionInfo_win->setChecked(true);
    } else if (!ui->dockWidget_6->isHidden()) {
        ui->dockWidget_6->setHidden(true);
        ui->actionInfo_win->setChecked(false);
    }
}

void MainWindow::view_mem_list()
{
    if (ui->dockWidget->isHidden()) {
        ui->dockWidget->setHidden(false);
        ui->actionMembers_win->setChecked(true);
    } else if (!ui->dockWidget->isHidden()) {
        ui->dockWidget->setHidden(true);
        ui->actionMembers_win->setChecked(false);
    }
}

void MainWindow::ds_Decompile()
{

    dlg->setWindowTitle(tr("DSDT + SSDT Decompile"));
    dlg->setModal(true);
    dlg->show();
}

void MainWindow::iaslUsage()
{
    pk = new QProcess;

    QFileInfo appInfo(qApp->applicationDirPath());

#ifdef Q_OS_WIN32
    // win
    pk->start(appInfo.filePath() + "/iasl.exe", QStringList() << "-h");
#endif

#ifdef Q_OS_LINUX
    // linux
    pk->start(appInfo.filePath() + "/iasl", QStringList() << "-h");

#endif

#ifdef Q_OS_MAC
    // mac
    pk->start(appInfo.filePath() + "/iasl", QStringList() << "-h");

#endif

    connect(pk, SIGNAL(finished(int)), this, SLOT(readHelpResult(int)));
}

void MainWindow::readHelpResult(int exitCode)
{
    Q_UNUSED(exitCode);
    QString result;

    result = QString::fromUtf8(pk->readAll());
    newFile();
    textEdit->append(result);
    textEdit->setModified(false);
}

void MainWindow::userGuide()
{
    //QFileInfo appInfo(qApp->applicationDirPath());
    //QString qtManulFile = appInfo.filePath() + "/aslcompiler.pdf";
    //QDesktopServices::openUrl(QUrl::fromLocalFile(qtManulFile));

    QUrl url(QString("https://acpica.org/documentation"));
    QDesktopServices::openUrl(url);
}

void MainWindow::CheckUpdate()
{

    QNetworkRequest quest;
    quest.setUrl(QUrl("https://api.github.com/repos/ic005k/QtiASL/releases/latest"));
    quest.setHeader(QNetworkRequest::UserAgentHeader, "RT-Thread ART");
    manager->get(quest);
}

void MainWindow::replyFinished(QNetworkReply* reply)
{
    QString str = reply->readAll();
    QMessageBox box;
    box.setText(str);
    //box.exec();
    //qDebug() << QSslSocket::supportsSsl() << QSslSocket::sslLibraryBuildVersionString() << QSslSocket::sslLibraryVersionString();

    parse_UpdateJSON(str);

    reply->deleteLater();
}

int MainWindow::parse_UpdateJSON(QString str)
{

    QJsonParseError err_rpt;
    QJsonDocument root_Doc = QJsonDocument::fromJson(str.toUtf8(), &err_rpt);

    if (err_rpt.error != QJsonParseError::NoError) {
        QMessageBox::critical(this, "", tr("Network error!"));
        return -1;
    }
    if (root_Doc.isObject()) {
        QJsonObject root_Obj = root_Doc.object();

        QString macUrl, winUrl, linuxUrl;
        QVariantList list = root_Obj.value("assets").toArray().toVariantList();
        for (int i = 0; i < list.count(); i++) {
            QVariantMap map = list[i].toMap();
            QFileInfo file(map["name"].toString());
            if (file.suffix().toLower() == "zip")
                macUrl = map["browser_download_url"].toString();

            if (file.suffix().toLower() == "7z")
                winUrl = map["browser_download_url"].toString();

            if (file.suffix() == "AppImage")
                linuxUrl = map["browser_download_url"].toString();
        }

        QJsonObject PulseValue = root_Obj.value("assets").toObject();
        QString Verison = root_Obj.value("tag_name").toString();
        QString Url;
        if (mac)
            Url = macUrl;
        if (win)
            Url = winUrl;
        if (linuxOS)
            Url = linuxUrl;

        QString UpdateTime = root_Obj.value("published_at").toString();
        QString ReleaseNote = root_Obj.value("body").toString();

        if (Verison > CurVerison) {
            QString warningStr = tr("New version detected!") + "\n" + tr("Version: ") + "V" + Verison + "\n" + tr("Published at: ") + UpdateTime + "\n" + tr("Release Notes: ") + "\n" + ReleaseNote;
            int ret = QMessageBox::warning(this, "", warningStr, tr("Download"), tr("Cancel"));
            if (ret == 0) {
                QDesktopServices::openUrl(QUrl(Url));
            }
        } else
            QMessageBox::information(this, "", tr("It is currently the latest version!"));
    }
    return 0;
}

void MainWindow::highlighsearchtext(QString searchText)
{
    if (searchText.trimmed() == "")
        return;

    std::string document;

    if (ui->chkCaseSensitive->isChecked()) {
        search_string = searchText;
        document = textEdit->text().toStdString();
    } else {
        search_string = searchText.toLower();
        document = textEdit->text().toLower().toStdString();
    }

    textEdit->SendScintilla(QsciScintillaBase::SCI_INDICSETSTYLE, 0, 8);
    if (red < 55) {
        textEdit->SendScintilla(QsciScintillaBase::SCI_INDICSETOUTLINEALPHA, 0, 255);
        textEdit->SendScintilla(QsciScintillaBase::SCI_INDICSETALPHA, 0, 50);
        textEdit->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE, 0, QColor(Qt::white));
    } else {
        textEdit->SendScintilla(QsciScintillaBase::SCI_INDICSETOUTLINEALPHA, 0, 200);
        textEdit->SendScintilla(QsciScintillaBase::SCI_INDICSETALPHA, 0, 30);
        textEdit->SendScintilla(QsciScintillaBase::SCI_INDICSETFORE, 0, QColor(Qt::red));
    }

    m_searchTextPosList.clear();

    /*int end = document.lastIndexOf(searchText);
    if (!searchText.isEmpty()) {
        int curpos = -1;
        if (end != -1) {
            while (curpos != end) {
                curpos = document.indexOf(search_string, curpos + 1);
                textEdit->SendScintilla(QsciScintilla::SCI_INDICATORFILLRANGE, curpos, search_string.length());
                m_searchTextPosList.append(curpos);
            }
        }
    }*/

    //查找document中flag 出现的所有位置,采用标准字符串来计算，QString会有一些问题
    std::string flag = search_string.toStdString();

    unsigned long long position = 0;

    int i = 1;
    while ((position = document.find(flag, position)) != std::string::npos) {
        //qDebug() << "position  " << i << " : " << position;

        textEdit->SendScintilla(QsciScintilla::SCI_INDICATORFILLRANGE, position, search_string.toStdString().length());

        m_searchTextPosList.append(position);

        position++;
        i++;
    }

    int count = m_searchTextPosList.count();
    lblCount->setText(QString::number(count));
}

void MainWindow::clearSearchHighlight(QsciScintilla* textEdit)
{
    for (int i = 0; i < m_searchTextPosList.count(); i++) {
        textEdit->SendScintilla(QsciScintillaBase::SCI_INDICATORCLEARRANGE, m_searchTextPosList[i], search_string.toStdString().length());
    }
}

void MainWindow::on_editFind_editTextChanged(const QString& arg1)
{

    if (arg1.count() > 0) {

        on_btnFindNext();
    } else {

        clearSearchHighlight(textEdit);
        lblCount->setText("0");
        ui->editFind->lineEdit()->setPlaceholderText(tr("Find") + "  (" + tr("History entries") + ": " + QString::number(ui->editFind->count()) + ")");

        if (red < 55) {

            QPalette palette;
            palette = ui->editFind->palette();
            palette.setColor(QPalette::Base, QColor(50, 50, 50));
            palette.setColor(QPalette::Text, Qt::white); //字色
            ui->editFind->setPalette(palette);

        } else {

            QPalette palette;
            palette = ui->editFind->palette();
            palette.setColor(QPalette::Base, Qt::white);
            palette.setColor(QPalette::Text, Qt::black); //字色
            ui->editFind->setPalette(palette);
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{

    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {

        if (ui->editFind->hasFocus()) {
        }
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (ui->editFind->hasFocus()) {

        if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {

            setEditFindCompleter();
        }
    }
}

void MainWindow::init_findTextList()
{

    QString strBak = ui->editFind->currentText();

    for (int i = 0; i < findTextList.count(); i++) {
        if (ui->editFind->currentText().trimmed() == findTextList.at(i))

            findTextList.removeAt(i);
    }

    findTextList.insert(0, ui->editFind->currentText().trimmed());

    for (int i = 0; i < findTextList.count(); i++) {
        if (findTextList.at(i) == tr("Clear history entries"))

            findTextList.removeAt(i);
    }

    findTextList.append(tr("Clear history entries"));

    ui->editFind->clear();
    ui->editFind->addItems(findTextList);

    ui->editFind->setCurrentText(strBak);
}

void MainWindow::on_editFind_currentIndexChanged(const QString& arg1)
{
    if (arg1 == tr("Clear history entries"))
        ui->editFind->clear();
}

void MainWindow::on_clearFindText()
{
    ui->editFind->clear();
    ui->editFind->lineEdit()->setPlaceholderText(tr("Find") + "  (" + tr("History entries") + ": " + QString::number(ui->editFind->count()) + ")");
}

QString MainWindow::getTabTitle()
{
    int index = ui->tabWidget_textEdit->currentIndex();
    return ui->tabWidget_textEdit->tabBar()->tabText(index);
}

void MainWindow::on_NewWindow()
{

    QFileInfo appInfo(qApp->applicationDirPath());
    QString pathSource = appInfo.filePath() + "/QtiASL";
    QStringList arguments;
    QString fn = "";
    arguments << fn;
    QProcess* process = new QProcess;
    process->setEnvironment(process->environment());
    process->start(pathSource, arguments);
}

void MainWindow::on_miniMap()
{
    if (!ui->actionMinimap->isChecked()) {
        ui->dockWidget_5->setVisible(false);

    }

    else {
        ui->dockWidget_5->setVisible(true);
    }
}

/*void MiniEditor::mousePressEvent(QMouseEvent* event)
{

    this->SendScintilla(QsciScintilla::SCI_SETMOUSEDOWNCAPTURES, 1);
    if (Qt::LeftButton == event->button()) {

        int x, y;
        this->getCursorPosition(&x, &y);
        qDebug() << x << y;
    }
}

void MiniEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
}*/

void MiniEditor::mouseMoveEvent(QMouseEvent* event)
{
    textEditScroll = false;
    miniEditWheel = false;
    mw_one->repaint();
    if (!textEditScroll) {
        showZoomWin(event->x(), event->y());
    }
}

void MiniEditor::wheelEvent(QWheelEvent* event)
{
    int spos = this->verticalScrollBar()->sliderPosition();
    miniEditWheel = true;

    //if (event->delta() > 0) {
    if (event->angleDelta().y() > 0) {

        this->verticalScrollBar()->setSliderPosition(spos - 3);

    } else {
        this->verticalScrollBar()->setSliderPosition(spos + 3);
    }
}

void MiniEditor::showZoomWin(int x, int y)
{

    int totalLines = this->lines();

    if (x < 15) {
        miniDlg->close();
        return;
    }

    curY = y;
    int textHeight = this->textHeight(0);
    int y0 = y / textHeight + this->verticalScrollBar()->sliderPosition();

    QString t1, t2, t3, t4, t5, t6, t7, t8, t9;

    miniDlgEdit->clear();

    if (totalLines < 10) {
        for (int i = 0; i < totalLines; i++) {

            miniDlgEdit->append(QString::number(i + 1) + "  " + this->text(i));

            //清除所有标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERDELETEALL);

            //SCI_MARKERGET 参数用来设置标记，默认为圆形标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERGET, y0);

            //SCI_MARKERSETFORE，SCI_MARKERSETBACK设置标记前景和背景标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERSETFORE, 0, QColor(Qt::blue));
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERSETBACK, 0, QColor(Qt::blue));

            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERADD, y0);
            //下划线
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_STYLESETUNDERLINE, y0 + 1, true);
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 0, QsciScintilla::SC_MARK_UNDERLINE);
        }
    } else {

        if (y0 <= 5) {
            t1 = this->text(0);
            t2 = this->text(1);
            t3 = this->text(2);
            t4 = this->text(3);
            t5 = this->text(4);
            t6 = this->text(5);
            t7 = this->text(6);
            t8 = this->text(7);
            t9 = this->text(8);

            miniDlgEdit->append(QString::number(1) + "  " + t1);
            miniDlgEdit->append(QString::number(2) + "  " + t2);
            miniDlgEdit->append(QString::number(3) + "  " + t3);
            miniDlgEdit->append(QString::number(4) + "  " + t4);
            miniDlgEdit->append(QString::number(5) + "  " + t5);
            miniDlgEdit->append(QString::number(6) + "  " + t6);
            miniDlgEdit->append(QString::number(7) + "  " + t7);
            miniDlgEdit->append(QString::number(8) + "  " + t8);
            miniDlgEdit->append(QString::number(9) + "  " + t9);

            //清除所有标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERDELETEALL);

            //SCI_MARKERGET 参数用来设置标记，默认为圆形标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERGET, y0);

            //SCI_MARKERSETFORE，SCI_MARKERSETBACK设置标记前景和背景标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERSETFORE, 0, QColor(Qt::blue));
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERSETBACK, 0, QColor(Qt::blue));

            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERADD, y0);
            //下划线
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_STYLESETUNDERLINE, y0 + 1, true);
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 0, QsciScintilla::SC_MARK_UNDERLINE);
        }

        if (y0 > 5 && y0 < totalLines - 9) {
            t1 = this->text(y0 - 4);
            t2 = this->text(y0 - 3);
            t3 = this->text(y0 - 2);
            t4 = this->text(y0 - 1);
            t5 = this->text(y0);
            t6 = this->text(y0 + 1);
            t7 = this->text(y0 + 2);
            t8 = this->text(y0 + 3);
            t9 = this->text(y0 + 4);

            miniDlgEdit->append(QString::number(y0 - 3) + "  " + t1);
            miniDlgEdit->append(QString::number(y0 - 2) + "  " + t2);
            miniDlgEdit->append(QString::number(y0 - 1) + "  " + t3);
            miniDlgEdit->append(QString::number(y0 - 0) + "  " + t4);
            miniDlgEdit->append(QString::number(y0 + 1) + "  " + t5);
            miniDlgEdit->append(QString::number(y0 + 2) + "  " + t6);
            miniDlgEdit->append(QString::number(y0 + 3) + "  " + t7);
            miniDlgEdit->append(QString::number(y0 + 4) + "  " + t8);
            miniDlgEdit->append(QString::number(y0 + 5) + "  " + t9);

            //清除所有标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERDELETEALL);

            //SCI_MARKERGET 参数用来设置标记，默认为圆形标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERGET, 4);

            //SCI_MARKERSETFORE，SCI_MARKERSETBACK设置标记前景和背景标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERSETFORE, 0, QColor(Qt::blue));
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERSETBACK, 0, QColor(Qt::blue));

            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERADD, 4);
            //下划线
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_STYLESETUNDERLINE, 5, true);
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 0, QsciScintilla::SC_MARK_UNDERLINE);
        }

        if (y0 >= totalLines - 9 && y0 <= totalLines) {
            t1 = this->text(totalLines - 9);
            t2 = this->text(totalLines - 8);
            t3 = this->text(totalLines - 7);
            t4 = this->text(totalLines - 6);
            t5 = this->text(totalLines - 5);
            t6 = this->text(totalLines - 4);
            t7 = this->text(totalLines - 3);
            t8 = this->text(totalLines - 2);
            t9 = this->text(totalLines - 1);

            miniDlgEdit->append(QString::number(totalLines - 8) + "  " + t1);
            miniDlgEdit->append(QString::number(totalLines - 7) + "  " + t2);
            miniDlgEdit->append(QString::number(totalLines - 6) + "  " + t3);
            miniDlgEdit->append(QString::number(totalLines - 5) + "  " + t4);
            miniDlgEdit->append(QString::number(totalLines - 4) + "  " + t5);
            miniDlgEdit->append(QString::number(totalLines - 3) + "  " + t6);
            miniDlgEdit->append(QString::number(totalLines - 2) + "  " + t7);
            miniDlgEdit->append(QString::number(totalLines - 1) + "  " + t8);
            miniDlgEdit->append(QString::number(totalLines) + "  " + t9);

            //清除所有标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERDELETEALL);

            //SCI_MARKERGET 参数用来设置标记，默认为圆形标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERGET, 9 - (totalLines - y0));

            //SCI_MARKERSETFORE，SCI_MARKERSETBACK设置标记前景和背景标记
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERSETFORE, 0, QColor(Qt::blue));
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERSETBACK, 0, QColor(Qt::blue));

            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERADD, 9 - (totalLines - y0));
            //下划线
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_STYLESETUNDERLINE, 9 - (totalLines - y0) + 1, true);
            miniDlgEdit->SendScintilla(QsciScintilla::SCI_MARKERDEFINE, 0, QsciScintilla::SC_MARK_UNDERLINE);
        }
    }

    if (t1.trimmed() == "" && t5.trimmed() == "" && t2.trimmed() == "" && t3.trimmed() == "" && t4.trimmed() == "") {
        miniDlg->close();
        return;
    }

    int miniEditX = mw_one->getTabWidgetEditX() + mw_one->getTabWidgetEditW();
    int w = 800;
    int h = miniDlgEdit->textHeight(y) * 9;
    int y1 = y;

    if (y >= mw_one->getMainWindowHeight() - h)
        y1 = mw_one->getMainWindowHeight() - h;
    else
        y1 = y;

    miniDlg->setGeometry(mw_one->getDockWidth() + miniEditX - w, y1, w, h);

    if (miniDlg->isHidden()) {
        miniDlg->show();
    }
}

void MiniEditor::miniEdit_cursorPositionChanged()
{
}

void MiniEditor::miniEdit_verticalScrollBarChanged()
{

    if (!textEditScroll) {
        if (curY == 0)
            curY = this->height() / 2;
        showZoomWin(20, curY);
    } else
        miniDlg->close();
}

void MaxEditor::mouseMoveEvent(QMouseEvent* event)
{
    Q_UNUSED(event);

    miniDlg->close();
}

void MainWindow::setValue()
{

    int t = textEdit->verticalScrollBar()->maximum();
    int m = miniEdit->verticalScrollBar()->maximum();
    double b = (double)miniEdit->verticalScrollBar()->sliderPosition() / (double)m;
    int p = b * t;

    textEdit->verticalScrollBar()->setSliderPosition(p);

    textEdit->verticalScrollBar()->setHidden(true);

    //connect(miniEdit->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(miniEdit_verticalScrollBarChanged()));
}

void MainWindow::setValue2()
{

    textEditScroll = true;

    //if (textEdit->geometry().contains(this->mapFromGlobal(QCursor::pos())))
    //{
    int t = textEdit->verticalScrollBar()->maximum();
    int m = miniEdit->verticalScrollBar()->maximum();
    double b = (double)textEdit->verticalScrollBar()->sliderPosition() / (double)t;
    int p = b * m;

    miniEdit->verticalScrollBar()->setSliderPosition(p);

    //textEdit->verticalScrollBar()->setHidden(true);
    //}
}

#ifndef QT_NO_CONTEXTMENU
void MainWindow::contextMenuEvent(QContextMenuEvent* event)
{

    Q_UNUSED(event);
}
#endif

void MainWindow::on_actionOpenDir()
{
    QString dir = "file:" + model->filePath(fsm_Index);
    QDesktopServices::openUrl(QUrl(dir, QUrl::TolerantMode));
}

void MainWindow::on_actionExpandAll()
{
    ui->treeWidget->expandAll();
}

void MainWindow::on_actionCollapseAll()
{
    ui->treeWidget->collapseAll();
}

void MainWindow::mouseMoveEvent(QMouseEvent* e)
{
    e->accept();

    if (enterEdit(e->pos(), miniEdit)) {
        //qDebug() << "miniEdit";
    }

    if (enterEdit(e->pos(), textEdit)) {
        //qDebug() << "textEdit";
    }

    miniDlg->close();
}

bool MainWindow::enterEdit(QPoint pp, QsciScintilla* btn)
{
    int height = btn->height();
    int width = btn->width();
    QPoint btnMinPos = btn->pos();
    QPoint btnMaxPos = btn->pos();
    btnMaxPos.setX(btn->pos().x() + width);
    btnMaxPos.setY(btn->pos().y() + height);
    if (pp.x() >= btnMinPos.x() && pp.y() >= btnMinPos.y()
        && pp.x() <= btnMaxPos.x() && pp.y() <= btnMaxPos.y())
        return true;
    else
        return false;
}

int MainWindow::getDockWidth()
{
    if (ui->dockWidget->x() != 0)
        return 0;

    if (ui->dockWidget->isVisible())
        return ui->dockWidget->width();
    else
        return 0;
}

int MainWindow::getMainWindowHeight()
{
    return this->height();
}

int MainWindow::getMiniDockX()
{
    return ui->dockWidget_5->x();
}

int MainWindow::getTabWidgetEditX()
{
    return ui->tabWidget_textEdit->x();
}

int MainWindow::getTabWidgetEditW()
{
    return ui->tabWidget_textEdit->width(); // + textEdit->verticalScrollBar()->width();
}

void MainWindow::on_PreviousError()
{
    if (QFileInfo(curFile).suffix().toLower() == "dsl")
        on_btnPreviousError();

    if (QFileInfo(curFile).suffix().toLower() == "cpp" || QFileInfo(curFile).suffix().toLower() == "c")
        goCppPreviousError();
}

void MainWindow::on_NextError()
{
    if (QFileInfo(curFile).suffix().toLower() == "dsl")
        on_btnNextError();

    if (QFileInfo(curFile).suffix().toLower() == "cpp" || QFileInfo(curFile).suffix().toLower() == "c")
        goCppNextError();
}

void MainWindow::msg(int value)
{
    QMessageBox box;
    box.setText(QString::number(value));
    box.exec();
}

void MainWindow::msgstr(QString str)
{
    QMessageBox box;
    box.setText(str);
    box.exec();
}

void MainWindow::setEditFindCompleter()
{
    QStringList valueList;
    for (int i = 0; i < ui->editFind->count(); i++) {
        valueList.append(ui->editFind->itemText(i));
    }
    QCompleter* editFindCompleter = new QCompleter(valueList, this);
    editFindCompleter->setCaseSensitivity(Qt::CaseSensitive);
    editFindCompleter->setCompletionMode(QCompleter::InlineCompletion);
    ui->editFind->setCompleter(editFindCompleter);
}

void MainWindow::on_listWidget_itemSelectionChanged()
{
    int index = ui->listWidget->currentRow();
    ui->tabWidget->setCurrentIndex(index);
}

void MainWindow::on_tabWidget_misc_currentChanged(int index)
{

    if (index == 0)
        ui->dockWidget->setWindowTitle(tr("Members"));
    if (index == 1)
        ui->dockWidget->setWindowTitle(tr("Filesystem Browser"));
}

void MainWindow::mousePressEvent(QMouseEvent* e)
{
    Q_UNUSED(e);
}

void MainWindow::loadFindString()
{

    //读取之前的目录

    QFileInfo fi(iniFile);

    if (fi.exists()) {

        QSettings Reg(iniFile, QSettings::IniFormat); //全平台都采用ini格式

        //读取搜索文本
        int count = Reg.value("countFindText").toInt();
        for (int i = 0; i < count; i++) {
            QString item = Reg.value("FindText" + QString::number(i + 1)).toString();

            findTextList.append(item);
        }
        ui->editFind->addItems(findTextList);
        ui->editFind->lineEdit()->setPlaceholderText(tr("Find") + "  (" + tr("History entries") + ": " + QString::number(ui->editFind->count()) + ")");
    }
}
