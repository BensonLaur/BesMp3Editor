﻿#ifndef MP3_EDITOR_H
#define MP3_EDITOR_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QPixmap>
#include <QTimer>
#include <QMutex>

extern "C" {
    #include "ffmpegDefine.h"
}

class CustomMp3Data
{
public:
    QString imagePath;
    QString title;
    QString artist;
    QString album;

    void clear()
    {
        imagePath = title = artist = album = "";
    }

    bool isEmpty()
    {
        return (imagePath.isEmpty() &&title.isEmpty() &&artist.isEmpty() &&album.isEmpty());
    }
};

//转换线程
class ConvertThread: public QThread
{
    Q_OBJECT
public:
    ConvertThread(QObject* parent = nullptr):QThread(parent) {}

    void SetConvertedData(QString filePath, const CustomMp3Data &customMp3Data);
protected:
    virtual void run();

private:
    void ResetToInitAll();
    void ReleaseAll();

    //初始化选项内容（仿造ffmpeg.exe）
    int ffmpeg_parse_options();

    void init_opts(void);
    void uninit_opts(void);

    static void uninit_options(OptionsContext *o);

    static void init_options(OptionsContext *o);


    void buildOptionContent(OptionParseContext* octx);  //模拟 split_commandline 构建 OptionParseContext

    void init_parse_context(OptionParseContext *octx,
                            const OptionGroupDef *groups, int nb_groups);

    void add_input_streams(OptionsContext *o, AVFormatContext *ic);

    int open_files(OptionGroupList *l, bool isInput);
    int open_input_file(OptionsContext *o, const char *filename);
    int open_output_file(OptionsContext *o, const char *filename);
    int transcode();

private:
    void avformat_get_context_defaults(AVFormatContext *s);

    /*
     * Finish parsing an option group.
     *
     * @param group_idx which group definition should this group belong to
     * @param arg argument of the group delimiting option
     */
    void finish_group(OptionParseContext *octx, int group_idx,
                             const char *arg);


    /*
     * Add an option instance to currently parsed group.
     */
    void add_opt(OptionParseContext *octx, const OptionDef *opt,
                        const char *key, const char *val);


    void print_error(const char *filename, int err);


    void remove_avoptions(AVDictionary **a, AVDictionary *b);

    void assert_avoptions(AVDictionary *m);



private:
    QString inputMp3Path;
    CustomMp3Data customData;

    QByteArray inputMp3Utf8;
    QByteArray inputImageUtf8;
    QByteArray outputMp3Utf8;

    QByteArray mataArtistUtf8;
    QByteArray mataTitleUtf8;
    QByteArray mataAlbumUtf8;

    QString outputMp3Path;
private:
    AVDictionary *sws_dict;
    AVDictionary *swr_opts;
    AVDictionary *format_opts, *codec_opts, *resample_opts;

#define OFFSET(x) offsetof(OptionsContext, x)
    OptionDef optionMap = { "map", HAS_ARG | OPT_EXPERT | OPT_PERFILE |OPT_OUTPUT,opt_map};
    OptionDef optionCodecName = { "c", HAS_ARG | OPT_STRING | OPT_SPEC |
            OPT_INPUT | OPT_OUTPUT, (void*)OFFSET(codec_names)};//{ .off       = OFFSET(codec_names) }};
    OptionDef optionMetadata = { "metadata",
                                 HAS_ARG | OPT_STRING | OPT_SPEC | OPT_OUTPUT, (void*)OFFSET(metadata),//{ .off = OFFSET(metadata) },
            "add metadata", "string=string" };


    FfmpegParamContext paramCtx;
};

//音乐播放器
class Mp3Editor :public QObject
{
    Q_OBJECT

public:
    Mp3Editor(QObject* parent = nullptr);
    ~Mp3Editor() ;

    bool CustomizeMp3(QString filePath,const CustomMp3Data& customData);

signals:
    void sig_getEditResult(bool success, QString path, QString errorTip);

private:
    QString sourceMp3;
    ConvertThread* convertThread;
};

#endif // MP3_EDITOR_H
