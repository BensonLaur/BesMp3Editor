#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MusicPlayer;
class Mp3Editor;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void initEntity();
    void initConnection();



private slots:
    void durationChanged(qint64);
    void musicPositionChanged(int);
    void onErrorOccurs(int code,QString strErr);
    void onAudioFinished(bool isEndWithForce);

    void changePic(QPixmap pic);
    void onSetMusicTitle(QString title);
    void onSetMusicArtist(QString artist);
    void onSetMusicAlbum(QString ablum);

    void onGetEditResult(bool success, QString path, QString errorTip);

	void onProcessingChange(int percentage);
private:
    void showPosition(int position);

private slots:
    void on_btn_select_music_clicked();

    void on_btn_select_picture_clicked();

    void on_btn_load_music_clicked();


    void on_btn_play_clicked();

    void on_btn_pause_clicked();

    void on_btn_stop_clicked();

    void on_btn_combine_clicked();

    void on_btn_load_output_music_clicked();

private:
    MusicPlayer*      musicPlayer;
    Mp3Editor*        mp3Editor;

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
