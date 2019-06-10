#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include "global.h"
#include <QMessageBox>
#include <MusicPlayer/musicPlayer.h>
#include <MP3Editor/mp3Editor.h>

QString defaultMusicPath = "";
QString defaultPicturePath = "";

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    initEntity();
    initConnection();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initEntity()
{
    ui->slider_current_music_position->setRange(0,1000);

    musicPlayer = new MusicPlayer(this);
    musicPlayer->setNotifyInterval(33);

    mp3Editor = new Mp3Editor(this);
}

void MainWindow::initConnection()
{
    connect(musicPlayer, &MusicPlayer::durationChanged, this, &MainWindow::durationChanged);
    connect(musicPlayer, &MusicPlayer::errorOccur, this, &MainWindow::onErrorOccurs);
    connect(musicPlayer, &MusicPlayer::positionChanged, this, &MainWindow::musicPositionChanged);

    connect(musicPlayer, &MusicPlayer::titleFound, this, &MainWindow::onSetMusicTitle);
    connect(musicPlayer, &MusicPlayer::artistFound, this, &MainWindow::onSetMusicArtist);
    connect(musicPlayer, &MusicPlayer::albumFound, this, &MainWindow::onSetMusicAlbum);
    connect(musicPlayer, &MusicPlayer::pictureFound, this, &MainWindow::changePic);

    connect(mp3Editor, &Mp3Editor::sig_getEditResult, this, &MainWindow::onGetEditResult);
}

void MainWindow::durationChanged(qint64 duration)
{
    int ms = duration % 1000;
    duration = duration/1000;
    int s = duration % 60;
    int m = duration/60;

    QString timeLabel;
    timeLabel.sprintf("%.2d:%.2d.%.3d",m, s, ms);

    ui->label_current_music_length->setText(timeLabel);
}

void MainWindow::musicPositionChanged(int position)
{
    //audioOriginalPos = position; //持续更新 audioOriginalPos，这样在拖动时则会保留拖动时刻的位置
    //qDebug()<<"BottomWidget::positionChanged => audioOriginalPos="<<audioOriginalPos<<" sliderSong->value()="<<sliderSong->value();

    int pecentOfThousand = musicPlayer->duration() == 0? 0: int(1.0 * position / musicPlayer->duration() * 1000);
    ui->slider_current_music_position->setValue(pecentOfThousand);

    showPosition(position);
}

void MainWindow::onErrorOccurs(int code, QString strErr)
{
    Q_UNUSED(code)
    QMessageBox::information(this,tr("提示")
                             ,tr("播放音频时发生错误，请尝试使用别的音频文件")+ "\n\n" + tr("出错细节:")+ strErr
                             ,QMessageBox::StandardButton::Ok);
}

void MainWindow::onAudioFinished(bool isEndWithForce)
{
    if(!isEndWithForce)
        musicPlayer->play();
}

void MainWindow::changePic(QPixmap pic)
{
    QRect rect = ui->label_current_image->geometry();
    ui->label_current_image->setPixmap(pic.scaled(QSize(rect.width(),rect.height()),Qt::KeepAspectRatio));
}

void MainWindow::onSetMusicTitle(QString title)
{
    ui->label_current_title->setText(title);
}

void MainWindow::onSetMusicArtist(QString artist)
{
    ui->label_current_artist->setText(artist);
}

void MainWindow::onSetMusicAlbum(QString ablum)
{
    ui->label_current_album->setText(ablum);
}

void MainWindow::onGetEditResult(bool success, QString path, QString errorTip)
{
    if(!success)
    {
        QMessageBox::information(this,tr("提示"),tr("组合出错:")+errorTip,QMessageBox::StandardButton::Ok);
    }

    ui->label_output_music_path->setText(path);
}

void MainWindow::showPosition(int position)
{
    int ms = position % 1000;
    position = position/1000;
    int s = position % 60;
    int m = position/60;

    QString timeLabel;
    timeLabel.sprintf("%.2d:%.2d.%.3d",m, s, ms);

    ui->label_current_music_pos->setText(timeLabel);
}

void MainWindow::on_btn_select_music_clicked()
{
    defaultMusicPath = QFileDialog::getOpenFileName(this, tr("打开音频文件"),
                                                      defaultMusicPath,//SettingManager::GetInstance().data().defaultMusicPath,
                                                      tr("音频 (*.mp3 *.wav *.ncm);;视频 (*.mp4)"));


    ui->edit_music->setText(defaultMusicPath);
}

void MainWindow::on_btn_select_picture_clicked()
{

    defaultPicturePath = QFileDialog::getOpenFileName(this, tr("打开图片"),
                                                      defaultPicturePath,//SettingManager::GetInstance().data().defaultMusicPath,
                                                      tr("图片 (*.jpg *.png)"));


    ui->edit_picture->setText(defaultPicturePath);
}

void MainWindow::on_btn_load_output_music_clicked()
{
    QString path = ui->label_output_music_path->text();
    if(path.isEmpty())
    {
        QMessageBox::information(this,tr("提示"),tr("请先组合转换成新的音乐输出"),QMessageBox::StandardButton::Ok);
        return;
    }

    ui->label_current_image->setPixmap(QPixmap());
    ui->label_current_title->setText("");
    ui->label_current_artist->setText("");
    ui->label_current_album->setText("");

    musicPlayer->setMusicPath(path);
    musicPlayer->reload();
}

void MainWindow::on_btn_load_music_clicked()
{
    if(defaultMusicPath.isEmpty())
    {
        QMessageBox::information(this,tr("提示"),tr("请先选择音乐文件路径"),QMessageBox::StandardButton::Ok);
        return;
    }

    ui->label_current_image->setPixmap(QPixmap());
    ui->label_current_title->setText("");
    ui->label_current_artist->setText("");
    ui->label_current_album->setText("");

    musicPlayer->setMusicPath(defaultMusicPath);
    musicPlayer->reload();
}

void MainWindow::on_btn_play_clicked()
{
    musicPlayer->play();
}

void MainWindow::on_btn_pause_clicked()
{
    musicPlayer->pause();
}

void MainWindow::on_btn_stop_clicked()
{
    musicPlayer->stop();
}

void MainWindow::on_btn_combine_clicked()
{
    if(defaultMusicPath.isEmpty())
    {
        QMessageBox::information(this,tr("提示"),tr("请先选择音乐文件路径"),QMessageBox::StandardButton::Ok);
        return;
    }

    CustomMp3Data data;
    data.imagePath = ui->edit_picture->text();
    data.title = ui->edit_title->text();
    data.artist= ui->edit_artist->text();
    data.album = ui->edit_album->text();

    if(data.isEmpty())
    {
        QMessageBox::information(this,tr("提示"),tr("请先填写至少一项要组合的信息"),QMessageBox::StandardButton::Ok);
        return;
    }

    mp3Editor->CustomizeMp3(defaultMusicPath,data);
}

