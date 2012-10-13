#include "ltwindow.h"
#include "ui_ltwindow.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTableWidgetItem>
#include "logindialog.h"

LTWindow::LTWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LTWindow), eventData(EventData::getInstance())
{    
    ui->setupUi(this);    

    currDriver = -1;

    streamReader = new DataStreamReader(this);
    prefs = new PreferencesDialog(this);
    settings = new QSettings(F1LTCore::iniFile(), QSettings::IniFormat, this);
    loginDialog = new LoginDialog(this);
    ltFilesManagerDialog = new LTFilesManagerDialog(this);
    saw = new SessionAnalysisWidget();
    aboutDialog = new AboutDialog(this);

//    ui->trackStatusWidget->setupItems();

    connect(streamReader, SIGNAL(tryAuthorize()), this, SLOT(tryAuthorize()));
    connect(streamReader, SIGNAL(authorized(QString)), this, SLOT(authorized(QString)));
    connect(streamReader, SIGNAL(eventDataChanged()), this, SLOT(eventDataChanged()));
    connect(streamReader, SIGNAL(driverDataChanged(int)), this, SLOT(driverDataChanged(int)));
    connect(streamReader, SIGNAL(dataChanged()), this, SLOT(dataChanged()));
    connect(streamReader, SIGNAL(sessionStarted()), this, SLOT(sessionStarted()));
    connect(streamReader, SIGNAL(authorizationError()), this, SLOT(authorizationError()));
    connect(streamReader, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(error(QAbstractSocket::SocketError)));
    connect(streamReader, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(error(QNetworkReply::NetworkError)));
    connect(streamReader, SIGNAL(noLiveSession(bool, QString)), this, SLOT(showNoSessionBoard(bool, QString)));

    sessionTimer = new SessionTimer(this);
    eventRecorder = new EventRecorder(this);
    eventPlayer = new EventPlayer(this);

    loadSettings();

    eventPlayerAction = ui->mainToolBar->addWidget(eventPlayer);
    eventPlayerAction->setVisible(false);
    recording = false;
    playing = false;

    connectionProgress = new QProgressDialog(this);

    connect(sessionTimer, SIGNAL(timeout()), this, SLOT(timeout()));
    connect(eventRecorder, SIGNAL(recordingStopped()), this, SLOT(autoStopRecording()));
    connect(eventPlayer, SIGNAL(playClicked(int)), this, SLOT(eventPlayerPlayClicked(int)));
    connect(eventPlayer, SIGNAL(pauseClicked()), this, SLOT(eventPlayerPauseClicked()));
    connect(eventPlayer, SIGNAL(rewindToStartClicked()), this, SLOT(eventPlayerRewindToStartClicked()));
    connect(eventPlayer, SIGNAL(forwardToEndClicked()), this, SLOT(eventPlayerForwardToEndClicked()));
    connect(eventPlayer, SIGNAL(rewindClicked()), this, SLOT(eventPlayerRewindClicked()));
    connect(eventPlayer, SIGNAL(stopClicked()), this, SLOT(eventPlayerStopClicked()));
    connect(eventPlayer, SIGNAL(nextPackets(QList<Packet>)), streamReader, SLOT(parsePackets(QList<Packet>)));

    connect(ui->ltWidget, SIGNAL(driverSelected(int)), ui->driverDataWidget, SLOT(printDriverData(int)));
    connect(ui->ltWidget, SIGNAL(driverDoubleClicked(int)), this, SLOT(ltWidgetDriverSelected(int)));


    ui->messageBoardWidget->setVisible(false);

    QStringList args = qApp->arguments();
    if (args.size() > 1)
    {
        if (eventPlayer->loadFromFile(args.at(1)) == false)
        {
            QMessageBox::critical(this, "Error opening file!", "Could not open specified file, or the file is corrupted.");
            connectToServer();
            return;
        }
        setWindowTitle("FILT - " + args.at(1));
        ui->actionRecord->setVisible(false);
        ui->actionStop_recording->setVisible(false);
        eventPlayerAction->setVisible(true);

        playing = true;

        eventPlayer->startPlaying();
    }
    else
    {
    	if (settings->value("ui/auto_connect").toBool())
    		connectToServer();
    	else
    	{
    		ui->messageBoardWidget->showStartupBoard();
    		showSessionBoard(true);
    	}
    }    

}

LTWindow::~LTWindow()
{
    if (recording)
        eventRecorder->stopRecording();

    saveSettings();

    delete ui;
}

//------------------------- updating the data ----------------------

void LTWindow::eventDataChanged()
{
    if (!playing)
        setWindowTitle("F1LT - " + eventData.getEventInfo().eventName);
    if (!playing && !recording && !ui->actionRecord->isEnabled())
        ui->actionRecord->setEnabled(true);

    if (eventData.getCommentary().size() > ui->textEdit->toPlainText().size())
    {        
        ui->textEdit->setText(eventData.getCommentary());

        QTextCursor c = ui->textEdit->textCursor();
        c.movePosition(QTextCursor::End);
        ui->textEdit->setTextCursor(c);
    }
    if (eventData.getCommentary().size() == 0 && ui->textEdit->toPlainText().size() > 0)
        ui->textEdit->clear();

//    ui->trackStatusWidget->updateTrackStatus(eventData);
    ui->eventStatusWidget->updateEventStatus();
    ui->sessionDataWidget->updateData();

    if (ui->tabWidget->currentIndex() == 2)
    	ui->weatherChartsWidget->updateCharts();

//    if (recording)
//        eventRecorder->updateEventData(eventData);

    ui->ltWidget->updateLT();
}

void LTWindow::driverDataChanged(int carID)
{
    if (!playing && !recording && !ui->actionRecord->isEnabled())
        ui->actionRecord->setEnabled(true);

    if (carID > -1)
    {                
//        eventData.driversData[dd.carID-1] = dd;

        ui->ltWidget->updateLT();

        //if (currDriver == (dd.carID-1))
//        for (int i = 0; i < ui->tableWidget->rowCount(); ++i)
//        {
//            QTableWidgetItem *item = ui->tableWidget->item(i, 1);

//            if (item && currDriver >= 0 && item->text().toInt() == eventData.getDriversData()[currDriver].getNumber())
//            {
//                ui->tableWidget->setCurrentCell(i, 1);
//                break;
//            }
//        }

        ui->sessionDataWidget->updateFastestLaps();
//        ui->tableWidget->updateLT();
        switch(ui->tabWidget->currentIndex())
        {
            case 0:                

//                if (currDriver >= 0 && currDriver == carID-1)
                    ui->driverDataWidget->updateDriverData();//printDriverData(currDriver);

                break;

//            case 1:
//                ui->sessionDataWidget->updateData(dd.carID);
//                break;
        }

//        if (h2hDialog->isVisible())
        for (int i = 0; i < ltcDialog.size(); ++i)
            ltcDialog[i]->driverUpdated(eventData.getDriversData()[carID-1]);

        for (int i = 0; i < h2hDialog.size(); ++i)
            h2hDialog[i]->driverUpdated(eventData.getDriversData()[carID-1]);

        for (int i = 0; i < fadDialog.size(); ++i)
            fadDialog[i]->driverUpdated(eventData.getDriversData()[carID-1]);

        if (saw->isVisible())
            saw->update();
//        if (recording)
//            eventRecorder->updateDriverData(eventData.driversData[carID-1]);
    }
}

void LTWindow::dataChanged()
{
    setWindowTitle("F1LT - " + eventData.getEventInfo().eventName + " (" + eventPlayer->playedFile() + ")");
//    if (eventData.commentary.size() > ui->textEdit->toPlainText().size())
    {        
        ui->textEdit->setText(eventData.getCommentary());

        QTextCursor c = ui->textEdit->textCursor();
        c.movePosition(QTextCursor::End);
        ui->textEdit->setTextCursor(c);
    }

//    ui->trackStatusWidget->updateTrackStatus(eventData);
    ui->eventStatusWidget->updateEventStatus();

    if (!eventPlayer->isPlaying())
        ui->sessionDataWidget->updateEventInfo();

    ui->sessionDataWidget->updateFastestLaps();
    ui->sessionDataWidget->updateSpeedRecords();
    ui->sessionDataWidget->updatePitStops(true);

//    if (currDriver >= 0)
        ui->driverDataWidget->updateDriverData();//printDriverData(currDriver);

    for (int i = 0; i < ltcDialog.size(); ++i)
    {
        ltcDialog[i]->updateData();
        ltcDialog[i]->updateCharts();
    }

    for (int i = 0; i < h2hDialog.size(); ++i)
    {
        h2hDialog[i]->updateData();
        h2hDialog[i]->updateCharts();
    }

    for (int i = 0; i < fadDialog.size(); ++i)
    {
        fadDialog[i]->updateData();
    }

    ui->ltWidget->updateLT();
//    for (int i = 0; i < ui->tableWidget->rowCount(); ++i)
//    {
//        QTableWidgetItem *item = ui->tableWidget->item(i, 1);

//        if (item && currDriver >= 0 && item->text().toInt() == eventData.getDriversData()[currDriver].getNumber())
//        {
//            ui->tableWidget->setCurrentCell(i, 1);
//            break;
//        }
//    }

    if (ui->tabWidget->currentIndex() == 2)
		ui->weatherChartsWidget->updateCharts();

    if (saw->isVisible())
        saw->update();
}

//void LTWindow::on_tableWidget_cellDoubleClicked(int row, int)
//{
//    QList<DriverData> driverList = eventData.getDriversData();
//    qSort(driverList);

//    ui->tabWidget->setCurrentIndex(0);
//    if (row-1 < driverList.size() && (row-1 >= 0))
//    {
//        DriverData dd = driverList[row-1];
//        currDriver = dd.getCarID() - 1;

//        ui->driverDataWidget->printDriverData(currDriver);
//    }
//}

//void LTWindow::on_tableWidget_cellClicked(int row, int)
//{
//    QList<DriverData> driverList = eventData.getDriversData();
//    qSort(driverList);

//    if (row-1 < driverList.size() && (row-1 >= 0))
//    {
//        DriverData dd = driverList[row-1];
//        currDriver = dd.getCarID() - 1;

//        ui->driverDataWidget->printDriverData(currDriver);
//    }
//}

void LTWindow::on_tabWidget_currentChanged(int index)
{
    switch(index)
    {
        case 0:
//            eventData.driversData[dd.carID-1] = dd;
            ui->driverDataWidget->updateView();
            if (currDriver >= 0)// && currDriver == dd.carID-1)
                ui->driverDataWidget->updateDriverData();//printDriverData(currDriver);

            break;

        case 1:
            ui->sessionDataWidget->updateData();
            break;
    }
}

void LTWindow::on_actionLap_time_comparison_triggered()
{
    for (int i = 0; i < ltcDialog.size(); ++i)
    {
        if (!ltcDialog[i]->isVisible())
        {
            ltcDialog[i]->show(ui->ltWidget->getCurrentDriverId());
            return;
        }
    }
    if (ltcDialog.size() < 30)
    {
        LapTimeComparisonDialog *ltc = new LapTimeComparisonDialog(settings->value("ui/reversed_lap_time_comparison").toBool());
        ltc->show(ui->ltWidget->getCurrentDriverId());
        ltc->setFont(prefs->getMainFont());
        ltcDialog.append(ltc);
    }
}

void LTWindow::on_actionHead_to_head_triggered()
{    
    for (int i = 0; i < h2hDialog.size(); ++i)
    {
        if (!h2hDialog[i]->isVisible())
        {
            h2hDialog[i]->show(ui->ltWidget->getCurrentDriverId());
            return;
        }

    }
    if (h2hDialog.size() < 30)
    {
        HeadToHeadDialog *h2h = new HeadToHeadDialog(settings->value("ui/reversed_head_to_head").toBool());
        h2h->show(ui->ltWidget->getCurrentDriverId());
        h2h->setFont(prefs->getMainFont());
        h2hDialog.append(h2h);
    }
//    h2hDialog->raise();
//    h2hDialog->activateWindow();
}

void LTWindow::on_actionFollow_a_driver_triggered()
{
    for (int i = 0; i < fadDialog.size(); ++i)
    {
        if (!fadDialog[i]->isVisible())
        {
            fadDialog[i]->show(ui->ltWidget->getCurrentDriverId());
            return;
        }

    }
    if (fadDialog.size() < eventData.getDriversData().size())
    {
        FollowADriverDialog *fad = new FollowADriverDialog();
        fad->show(ui->ltWidget->getCurrentDriverId());
        fad->setFont(prefs->getMainFont());
        fadDialog.append(fad);
    }
}

void LTWindow::sessionStarted()
{
    if (!sessionTimer->isActive() && (!playing || (playing && eventPlayer->isPlaying() && !eventPlayer->isPaused())))        
        sessionTimer->start(1000);
}

void LTWindow::showNoSessionBoard(bool show, QString msg)
{
	if (show)
	{
        //ui->tableWidget->clear();
        ui->ltWidget->clearData();
		ui->textEdit->clear();
		ui->driverDataWidget->clearData();
		ui->sessionDataWidget->clearData();

		ui->messageBoardWidget->showSessionBoard(msg);
		showSessionBoard(true);
	}
	else
	{
		showSessionBoard(false);
        ui->ltWidget->updateLT();
	}
}

void LTWindow::ltWidgetDriverSelected(int id)
{
    ui->driverDataWidget->printDriverData(id);
    ui->tabWidget->setCurrentIndex(0);
}

void LTWindow::timeout()
{
    if (recording)
        eventRecorder->timeout();

    if (eventPlayer->isPlaying())
        eventPlayer->timeout();

    ui->eventStatusWidget->updateEventStatus();

    //during quali timer is stopped when we have red flag
    if (eventData.isSessionStarted())
    {
        if (!playing && settings->value("ui/auto_record").toBool() && !recording)
            on_actionRecord_triggered();
    }

//        if (!(eventData.getEventType() == LTPackets::RACE_EVENT && eventData.getCompletedLaps() == eventData.getEventInfo().laps) &&
//            !((eventData.getEventType() == LTPackets::QUALI_EVENT || eventData.getEventType() == LTPackets::RACE_EVENT) && eventData.getFlagStatus() == LTPackets::RED_FLAG))
//        {
//            int hours = eventData.getRemainingTime().hour();
//            int mins = eventData.getRemainingTime().minute();
//            int secs = eventData.getRemainingTime().second();
//            --secs;
//            if (secs < 0)
//            {
//                secs = 59;
//                --mins;
//                eventData.saveWeather();

//                if (mins < 0)
//                {
//                    --hours;
//                    mins = 59;

//                    if (hours < 0)
//                    {
//                        secs = mins = hours = 0;
//                        //we don't stop the timer here as it will be needed ie. for session recording, we only change the value of sessionStarted to false
//                        eventData.setSessionStarted(false);
//    //                    eventTimer->stop();
//                    }
//                }
//            }
//            eventData.setRemainingTime(QTime(hours, mins, secs));
////            ui->trackStatusWidget->updateTrackStatus(eventData);
//            ui->eventStatusWidget->updateEventStatus();
//        }
//    }
}

//-------------------- settings ----------------------------

void LTWindow::loadSettings()
{
    QFont mainFont, commentaryFont;
    QString sbuf = settings->value("fonts/main_family").toString();
    mainFont.setFamily(sbuf);

    int ibuf = settings->value("fonts/main_size").toInt();
    mainFont.setPointSize(ibuf);

    ibuf = settings->value("fonts/main_weight").toInt();
    mainFont.setWeight(ibuf);

    bool bbuf = settings->value("fonts/main_italic").toBool();
    mainFont.setItalic(bbuf);

//    prefs->setMainFont(mainFont);

    sbuf = settings->value("fonts/commentary_family").toString();
    commentaryFont.setFamily(sbuf);

    ibuf = settings->value("fonts/commentary_size").toInt();
    commentaryFont.setPointSize(ibuf);

    ibuf = settings->value("fonts/commentary_weight").toInt();
    commentaryFont.setWeight(ibuf);

    bbuf = settings->value("fonts/commentary_italic").toBool();
    commentaryFont.setItalic(bbuf);

//    prefs->setCommentaryFont(commentaryFont);
//    prefs->setSplitterOpaqueResize(settings->value("ui/ltresize").toBool());
//    prefs->setAlternatingRowColors(settings->value("ui/alt_colors").toBool());
//    prefs->setAutoRecord(settings->value("ui/auto_record").toBool());

    ui->splitter->setOpaqueResize(settings->value("ui/ltresize").toBool());
    //ui->tableWidget->setAlternatingRowColors(settings->value("ui/alt_colors").toBool());

    setFonts(mainFont, commentaryFont);

    restoreGeometry(settings->value("ui/window_geometry").toByteArray());

    ui->splitter->restoreState(settings->value("ui/splitter_pos").toByteArray());
    ui->tabWidget->setCurrentIndex(settings->value("ui/current_tab1").toInt());

    if (ui->tabWidget->currentIndex() == 0)
        ui->driverDataWidget->setCurrentIndex(settings->value("ui/current_tab2").toInt());
    else if (ui->tabWidget->currentIndex() == 1)
        ui->sessionDataWidget->setCurrentIndex(settings->value("ui/current_tab2").toInt());

//    prefs->setReverseOrderLapHistory(settings->value("ui/reversed_lap_history").toBool());
//    prefs->setReverseOrderHeadToHead(settings->value("ui/reversed_head_to_head").toBool());
//    prefs->setReverseOrderLapTimeComparison(settings->value("ui/reversed_lap_time_comparison").toBool());

    ui->ltWidget->setDrawCarThumbnails(settings->value("ui/car_thumbnails").toBool());

    eventRecorder->setAutoStopRecord(settings->value("ui/auto_stop_record").toInt());

    saw->loadSettings(*settings);

    ltFilesManagerDialog->loadSettings(settings);
}

void LTWindow::saveSettings()
{
    QString passwd = encodePasswd(settings->value("login/passwd").toString());
    encodePasswd(passwd);
    settings->setValue("ui/window_geometry", saveGeometry());
    settings->setValue("ui/splitter_pos", ui->splitter->saveState());
    settings->setValue("ui/current_tab1", ui->tabWidget->currentIndex());
    if (ui->tabWidget->currentIndex() == 0)
        settings->setValue("ui/current_tab2", ui->driverDataWidget->currentIndex());
    else if (ui->tabWidget->currentIndex() == 1)
        settings->setValue("ui/current_tab2", ui->sessionDataWidget->currentIndex());

    saw->saveSettings(*settings);
    ltFilesManagerDialog->saveSettings(settings);

//    settings->setValue("ui/ltresize", prefs->isSplitterOpaqueResize());
//    settings->setValue("ui/alt_colors", prefs->isAlternatingRowColors());

//    settings->setValue("ui/reversed_lap_history", prefs->isReverseOrderLapHistory());
//    settings->setValue("ui/reversed_head_to_head", prefs->isReverseOrderHeadToHead());
//    settings->setValue("ui/reversed_lap_time_comparison", prefs->isReverseOrderLapTimeComparison());
//    settings->setValue("ui/auto_record", prefs->isAutoRecord());
}

void LTWindow::on_actionPreferences_triggered()
{
    if (prefs->exec(settings) == QDialog::Accepted)
    {
        setFonts(prefs->getMainFont(), prefs->getCommentaryFont());

        ui->splitter->setOpaqueResize(prefs->isSplitterOpaqueResize());
        //ui->tableWidget->setAlternatingRowColors(prefs->isAlternatingRowColors());

        eventRecorder->setAutoStopRecord(settings->value("ui/auto_stop_record").toInt());

//        settings->setValue("fonts/main_family", prefs->getMainFont().family());
//        settings->setValue("fonts/main_size", QString::number(prefs->getMainFont().pointSize()));
//        settings->setValue("fonts/main_weight", QString::number(prefs->getMainFont().weight()));
//        settings->setValue("fonts/main_italic", QString::number(prefs->getMainFont().italic()));

//        settings->setValue("fonts/commentary_family", prefs->getCommentaryFont().family());
//        settings->setValue("fonts/commentary_size", QString::number(prefs->getCommentaryFont().pointSize()));
//        settings->setValue("fonts/commentary_weight", QString::number(prefs->getCommentaryFont().weight()));
//        settings->setValue("fonts/commentary_italic", QString::number(prefs->getCommentaryFont().italic()));

//        settings->setValue("ui/ltresize", prefs->isSplitterOpaqueResize());
//        settings->setValue("ui/alt_colors", prefs->isAlternatingRowColors());

//        settings->setValue("ui/reversed_lap_history", prefs->isReverseOrderLapHistory());
//        settings->setValue("ui/reversed_head_to_head", prefs->isReverseOrderHeadToHead());
//        settings->setValue("ui/reversed_lap_time_comparison", prefs->isReverseOrderLapTimeComparison());
//        settings->setValue("ui/auto_record", prefs->isAutoRecord());

        for (int i = 0; i < h2hDialog.size(); ++i)
        {
            h2hDialog[i]->setReversedOrder(settings->value("ui/reversed_head_to_head").toBool());
            if (h2hDialog[i]->isVisible())
                h2hDialog[i]->updateData();
        }

        for (int i = 0; i < ltcDialog.size(); ++i)
        {
            ltcDialog[i]->setReversedOrder(settings->value("ui/reversed_lap_time_comparison").toBool());
            if (ltcDialog[i]->isVisible())
                ltcDialog[i]->updateData();
        }

//        if (currDriver >= 0)
            ui->driverDataWidget->updateDriverData();//printDriverData(currDriver);

        ui->ltWidget->setDrawCarThumbnails(settings->value("ui/car_thumbnails").toBool());
    }
}

void LTWindow::setFonts(const QFont &mainFont, const QFont &commentaryFont)
{
    ui->ltWidget->setFont(mainFont);
    ui->driverDataWidget->setFont(mainFont, commentaryFont);
    ui->sessionDataWidget->setFont(mainFont);

    for (int i = 0; i < h2hDialog.size(); ++i)
        h2hDialog[i]->setFont(mainFont);

    for (int i = 0; i < ltcDialog.size(); ++i)
        ltcDialog[i]->setFont(mainFont);

    for (int i = 0; i < fadDialog.size(); ++i)
        fadDialog[i]->setFont(mainFont);

    ui->textEdit->setFont(commentaryFont);
//    ui->trackStatusWidget->setFont(mainFont);
    ui->eventStatusWidget->setFont(mainFont);
    prefs->setFonts(mainFont, commentaryFont);
}

bool LTWindow::close()
{
    for (int i = 0; i < h2hDialog.size(); ++i)
        h2hDialog[i]->close();

    for (int i = 0; i < ltcDialog.size(); ++i)
        ltcDialog[i]->close();

    for (int i = 0; i < fadDialog.size(); ++i)
        fadDialog[i]->close();

    if (saw->isVisible())
        saw->close();

    return QMainWindow::close();
}

//-------------------- connection with server ----------------------

void LTWindow::tryAuthorize()
{
    connectionProgress->setWindowModality(Qt::WindowModal);
    connectionProgress->setLabelText("Connecting...");
    connectionProgress->setCancelButton(0);
    connectionProgress->setRange(0,0);
    connectionProgress->setMinimumDuration(0);
    connectionProgress->show();
}

void LTWindow::authorized(QString)
{
    connectionProgress->cancel();
}

void LTWindow::authorizationError()
{
    if (!playing)
    {
        if (connectionProgress->isVisible())
            connectionProgress->cancel();

        QMessageBox::critical(this, tr("Login error!"), tr("Could not login to the server!\nCheck your email and password"));

        on_actionConnect_triggered();
        ui->actionRecord->setEnabled(false);
    }
}

void LTWindow::on_actionConnect_triggered()
{
    QString passwd = encodePasswd(settings->value("login/passwd").toString());
    if (loginDialog->exec(settings->value("login/email").toString(), passwd) == QDialog::Accepted)
    {
        if (playing)
            eventPlayerStopClicked(false);

        streamReader->disconnectFromLTServer();
        QString email = loginDialog->getEmail();
        passwd = loginDialog->getPasswd();
        streamReader->connectToLTServer(email, passwd);

        settings->setValue("login/email", email);

        QString encPasswd = encodePasswd(passwd);
        settings->setValue("login/passwd", encPasswd);

        showSessionBoard(false);
    }
}

void LTWindow::connectToServer()
{
    QString email = settings->value("login/email").toString();
    QString passwd = encodePasswd(settings->value("login/passwd").toString());

    if (email == "" || passwd == "")
    {
        if (loginDialog->exec() == QDialog::Accepted)
        {
            email = loginDialog->getEmail();            
            passwd = loginDialog->getPasswd();
            streamReader->connectToLTServer(email, passwd);

            settings->setValue("login/email", email);
            passwd = encodePasswd(loginDialog->getPasswd());
            settings->setValue("login/passwd", passwd);
        }
    }
    else
        streamReader->connectToLTServer(email, passwd);
}

void LTWindow::error(QAbstractSocket::SocketError er)
{
    if (!playing)
    {
        ui->actionRecord->setEnabled(false);
        if (QMessageBox::critical(this, tr("Connection error!"), tr("There was an error with connection to LT server\n Error code: ") + QString("%1").arg(er), QMessageBox::Retry, QMessageBox::Close)
                == QMessageBox::Retry)
        {
            //try to connect again
            streamReader->disconnectFromLTServer();
            connectToServer();
        }
//        else
//        {
//        	ui->messageBoardWidget->showStartupBoard();
//			showSessionBoard(true);
//        }
    }
}

void LTWindow::error(QNetworkReply::NetworkError er)
{
    if (!playing)
    {
        ui->actionRecord->setEnabled(false);
        if (QMessageBox::critical(this, tr("Connection error!"), tr("There was an error with connection to LT server\n Error code: ") + QString("%1").arg(er), QMessageBox::Retry, QMessageBox::Close)
                == QMessageBox::Retry)
        {
            //try to connect again
            streamReader->disconnectFromLTServer();
            connectToServer();
        }
//        else
//		{
//			ui->messageBoardWidget->showStartupBoard();
//			showSessionBoard(true);
//		}
    }
}

//-------------------- about ------------------------

void LTWindow::on_actionAbout_triggered()
{
    aboutDialog->exec();
    //QMessageBox::about(this, "About F1LT", "<b>F1LT</b> v" + F1LTCore::programVersion()+"<br/>by Mariusz Pilarek, 2012<br/><a href=\"http://f1lt.pl/\">Home site</a>");
}

void LTWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this);
}

//-------------------- event recorder ----------------------------

void LTWindow::on_actionRecord_triggered()
{
    recording = true;
    ui->actionStop_recording->setEnabled(true);
    ui->actionRecord->setEnabled(false);
    ui->actionLT_files_data_base->setEnabled(false);
//        ui->actionRecord->setIcon(QIcon(":/ui_icons/stop.png"));
    ui->actionOpen->setEnabled(false);
    eventRecorder->startRecording();
    connect(streamReader, SIGNAL(packetParsed(Packet)), eventRecorder, SLOT(appendPacket(Packet)));
}

void LTWindow::on_actionStop_recording_triggered()
{
    recording = false;
    ui->actionOpen->setEnabled(true);
    ui->actionRecord->setEnabled(true);
    ui->actionLT_files_data_base->setEnabled(true);
    ui->actionStop_recording->setEnabled(false);
    eventRecorder->stopRecording();
    disconnect(streamReader, SIGNAL(packetParsed(Packet)), eventRecorder, SLOT(appendPacket(Packet)));
}

void LTWindow::autoStopRecording()
{
	recording = false;
	ui->actionOpen->setEnabled(true);
	ui->actionRecord->setEnabled(true);
    ui->actionLT_files_data_base->setEnabled(true);
	ui->actionStop_recording->setEnabled(false);
    disconnect(streamReader, SIGNAL(packetParsed(Packet)), eventRecorder, SLOT(appendPacket(Packet)));
}

//-------------------- event player ----------------------------

void LTWindow::on_actionOpen_triggered()
{
    if (playing)
        eventPlayer->pausePlaying();

    QString ltDir = settings->value("ui/ltdir").toString();
    if (ltDir == "")
        ltDir = F1LTCore::ltDataHomeDir();

    QString fName = QFileDialog::getOpenFileName(this, "Select archive LT event file", ltDir, "*.lt");    

    if (!fName.isNull())
    {
        sessionTimer->stop();
        QFileInfo fi(fName);
        ltDir = fi.absolutePath();

        settings->setValue("ui/ltdir", ltDir);

        if (recording)
            on_actionRecord_triggered();

        eventPlayerOpenFile(fName);
    }
}

void LTWindow::on_actionLT_files_data_base_triggered()
{
    if (playing)
        eventPlayer->pausePlaying();

    QString fName = ltFilesManagerDialog->exec();    

    if (!fName.isNull())
    {
        sessionTimer->stop();
        if (recording)
            on_actionRecord_triggered();

        eventPlayerOpenFile(fName);
    }
}

void LTWindow::eventPlayerOpenFile(QString fName)
{
    if (eventPlayer->loadFromFile(fName) == false)
    {
        QMessageBox::critical(this, "Error opening file!", "Could not open specified file, or the file is corrupted.");
        return;
    }

    QFileInfo fInfo(fName);
    QString name = fInfo.fileName();

    showSessionBoard(false);

    //ui->tableWidget->clear();
    ui->textEdit->clear();
    ui->driverDataWidget->clearData();
    ui->sessionDataWidget->clearData();

    ui->ltWidget->loadCarImages();
    for (int i = 0; i < h2hDialog.size(); ++i)
        h2hDialog[i]->loadCarImages();

    for (int i = 0; i < ltcDialog.size(); ++i)
        ltcDialog[i]->loadCarImages();

    for (int i = 0; i < fadDialog.size(); ++i)
        fadDialog[i]->loadCarImages();

    ui->actionRecord->setVisible(false);
    ui->actionStop_recording->setVisible(false);
    eventPlayerAction->setVisible(true);

    streamReader->disconnectFromLTServer();
    streamReader->clearData();

    playing = true;

    eventPlayer->startPlaying();
    saw->resetView();
}

void LTWindow::eventPlayerPlayClicked(int interval)
{
    sessionTimer->start(interval);
}

void LTWindow::eventPlayerPauseClicked()
{
    sessionTimer->stop();
}

void LTWindow::eventPlayerRewindToStartClicked()
{
    sessionTimer->stop();
    streamReader->clearData();
    ui->sessionDataWidget->clearData();

    eventPlayer->startPlaying();
}

void LTWindow::eventPlayerForwardToEndClicked()
{
    sessionTimer->stop();
}

void LTWindow::eventPlayerRewindClicked()
{
    //clear all data
    ui->sessionDataWidget->clearFastestLaps();
    ui->ltWidget->clearModelsData();
    streamReader->clearData();
}

void LTWindow::eventPlayerStopClicked(bool connect)
{
    streamReader->clearData();
    eventPlayerAction->setVisible(false);
    ui->actionRecord->setVisible(true);
    ui->actionStop_recording->setVisible(true);
    //ui->tableWidget->clear();
    ui->ltWidget->clearData();
    ui->driverDataWidget->clearData();
    ui->sessionDataWidget->clearData();
    ui->textEdit->clear();
    saw->resetView();

    playing = false;
    SeasonData::getInstance().loadSeasonFile();

    ui->ltWidget->loadCarImages();
    for (int i = 0; i < h2hDialog.size(); ++i)
        h2hDialog[i]->loadCarImages();

    for (int i = 0; i < ltcDialog.size(); ++i)
        ltcDialog[i]->loadCarImages();

    for (int i = 0; i < fadDialog.size(); ++i)
        fadDialog[i]->loadCarImages();

    if (connect)
    {
    	if (settings->value("ui/auto_connect").toBool())
    		connectToServer();
    	else
    	{
    		ui->messageBoardWidget->showStartupBoard();
			showSessionBoard(true);
    	}
    }
}

void LTWindow::showSessionBoard(bool show)
{
    if (show)
    {
        ui->splitter->setVisible(!show);
        ui->eventStatusWidget->setVisible(!show);
        ui->messageBoardWidget->setVisible(show);
    }
    else
    {
        ui->messageBoardWidget->setVisible(show);
        ui->eventStatusWidget->setVisible(!show);
        ui->splitter->setVisible(!show);
    }
	ui->actionRecord->setEnabled(!show);
    ui->actionHead_to_head->setEnabled(!show);
    ui->actionLap_time_comparison->setEnabled(!show);
    ui->actionFollow_a_driver->setEnabled(!show);
    ui->actionSession_analysis->setEnabled(!show);
}

void LTWindow::on_actionSession_analysis_triggered()
{
    saw->exec();
}


