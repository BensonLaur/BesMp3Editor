#include "mp3Editor.h"
#include <QFileInfo>
#include <QDir>

#define RAW_PACKET_BUFFER_SIZE 2500000

void ConvertThread::SetConvertedData(QString filePath, const CustomMp3Data &customMp3Data)
{
    inputMp3Path = filePath;
    customData = customMp3Data;
}

void ConvertThread::run()
{
    ResetToInitAll();           //重置以初始化所有状态

    //初始化选项内容（仿造ffmpeg.exe）
    OptionParseContext octx;
    int ret;

    buildOptionContent(&octx);

    //loglevel
    av_log_set_flags(1);
    av_log_set_level(AV_LOG_TRACE);

    //打开输入的文件（mp3 和 图片）
    OptionGroupList *l = &octx.groups[GROUP_INFILE];
    {
        int i, ret;

        for (i = 0; i < l->nb_groups; i++) {
            OptionGroup *g = &l->groups[i];
            OptionsContext o;

            init_options(&o);
            o.g = g;

            ret = openInputFile(&o);

            uninit_options(&o);

            if(ret < 0)
            {
                goto fail;
            }
        }
    }

    //打开输出文件（mp3）
    ret = openOutputFile();
    if(ret < 0)
    {
        goto fail;
    }

    //转码输出
    ret = transcode();
    if(ret < 0)
    {
        goto fail;
    }


fail:
    ReleaseAll();

}

void ConvertThread::ResetToInitAll()
{
    sws_dict = swr_opts = format_opts = codec_opts = resample_opts = NULL;
}

void ConvertThread::ReleaseAll()
{

}

void ConvertThread::init_opts()
{
    av_dict_set(&sws_dict, "flags", "bicubic", 0);
}

void ConvertThread::uninit_opts()
{
    av_dict_free(&swr_opts);
    av_dict_free(&sws_dict);
    av_dict_free(&format_opts);
    av_dict_free(&codec_opts);
    av_dict_free(&resample_opts);
}

void ConvertThread::uninit_options(OptionsContext *o)
{
    int i;

    //TODO 这里需不需要释放  (uint8_t*)o + po->u.off 位置相关的内存 ？

//    const OptionDef *po = options;

//    /* all OPT_SPEC and OPT_STRING can be freed in generic way */
//    while (po->name) {
//        void *dst = (uint8_t*)o + po->u.off;

//        if (po->flags & OPT_SPEC) {
//            SpecifierOpt **so = dst;
//            int i, *count = (int*)(so + 1);
//            for (i = 0; i < *count; i++) {
//                av_freep(&(*so)[i].specifier);
//                if (po->flags & OPT_STRING)
//                    av_freep(&(*so)[i].u.str);
//            }
//            av_freep(so);
//            *count = 0;
//        } else if (po->flags & OPT_OFFSET && po->flags & OPT_STRING)
//            av_freep(dst);
//        po++;
//    }

    for (i = 0; i < o->nb_stream_maps; i++)
        av_freep(&o->stream_maps[i].linklabel);
    av_freep(&o->stream_maps);
    //av_freep(&o->audio_channel_maps);
    av_freep(&o->streamid_map);
    //av_freep(&o->attachments);
}

void ConvertThread::init_options(OptionsContext *o)
{
    memset(o, 0, sizeof(*o));

    o->stop_time = INT64_MAX;
    o->mux_max_delay  = 0.7;
    o->start_time     = AV_NOPTS_VALUE;
    o->start_time_eof = AV_NOPTS_VALUE;
    o->recording_time = INT64_MAX;
    o->limit_filesize = UINT64_MAX;
    o->chapters_input_file = INT_MAX;
    o->accurate_seek  = 1;
}

void ConvertThread::buildOptionContent(OptionParseContext *octx)
{
    memset(octx, 0, sizeof(*octx));
    init_parse_context(octx, groups, FF_ARRAY_ELEMS(groups));

    //构建相关选项

    //构建输入项选项
    //#0
    finish_group(octx,OptGroup::GROUP_INFILE, inputMp3Path.toUtf8());

    //#1
    if(!customData.imagePath.isEmpty())
        finish_group(octx,OptGroup::GROUP_INFILE, customData.imagePath.toUtf8());

    //OptionDef optionMap = { "map", HAS_ARG | OPT_EXPERT | OPT_PERFILE |OPT_OUTPUT,{.func_arg = opt_map}};
    OptionDef optionMap = { "map", HAS_ARG | OPT_EXPERT | OPT_PERFILE |OPT_OUTPUT,opt_map};

    //构建输出项选项

    if(!customData.imagePath.isEmpty())
    {
        //有图片文件时，分别使用原来的音频流和新图片的视频流
        add_opt(octx,&optionMap,"map","0:0"); //音乐文件#0的音频流(0)
        add_opt(octx,&optionMap,"map","1:0"); //图片文件#1的视频流(0)
    }

#define OFFSET(x) offsetof(OptionsContext, x)

    if( !customData.artist.isEmpty()|| !customData.title.isEmpty()||!customData.album.isEmpty())
    {
        OptionDef optionMetadata = { "metadata",
                                     HAS_ARG | OPT_STRING | OPT_SPEC | OPT_OUTPUT, (void*)OFFSET(metadata),//{ .off = OFFSET(metadata) },
                "add metadata", "string=string" };

         if(!customData.artist.isEmpty()){
             add_opt(octx,&optionMetadata,"metadata",
                     QString("artist="+customData.artist).toUtf8());
         }
         if(!customData.title.isEmpty()){
             add_opt(octx,&optionMetadata,"metadata",
                     QString("title="+customData.title).toUtf8());
         }
         if(!customData.album.isEmpty()){
             add_opt(octx,&optionMetadata,"metadata",
                     QString("album="+customData.album).toUtf8());
         }
    }

     OptionDef optionCodecName = { "c", HAS_ARG | OPT_STRING | OPT_SPEC |
             OPT_INPUT | OPT_OUTPUT, (void*)OFFSET(codec_names)};//{ .off       = OFFSET(codec_names) }};

     //输出使用的codec直接复制使用输入文件的
     add_opt(octx,&optionCodecName,"c","copy");

     //设置 mp3 头格式版本为 3
     av_dict_set(&format_opts, "id3v2_version", "3", 0);

     //构建输出文件的名称
     QFileInfo fileInfo(inputMp3Path);
     outputMp3Path = fileInfo.dir().absolutePath()+"/"+fileInfo.baseName()+"-converted.mp3";

     finish_group(octx,OptGroup::GROUP_OUTFILE, outputMp3Path.toUtf8());
}

void ConvertThread::init_parse_context(OptionParseContext *octx, const OptionGroupDef *groups, int nb_groups)
{
    static const OptionGroupDef global_group = { "global" };
    int i;

    memset(octx, 0, sizeof(*octx));

    octx->nb_groups = nb_groups;
    octx->groups    = reinterpret_cast<OptionGroupList*>(av_mallocz_array(octx->nb_groups, sizeof(*octx->groups)));
    if (!octx->groups)
        exit(1);

    for (i = 0; i < octx->nb_groups; i++)
        octx->groups[i].group_def = &groups[i];

    octx->global_opts.group_def = &global_group;
    octx->global_opts.arg       = "";

    init_opts();
}


int ConvertThread::openInputFile(OptionsContext *o)
{
    InputFile *f;
    AVFormatContext *ic;
    int scan_all_pmts_set = 0;

    //分配 AVFormatContext
    {
        //AVFormatContext *avformat_alloc_context(void)
        AVFormatContext *_ic;
        _ic = reinterpret_cast<AVFormatContext*>(av_malloc(sizeof(AVFormatContext)));
        if (!_ic)
        {
            ic = _ic;
        }
        else
        {
            avformat_get_context_defaults(_ic);

//            _ic->internal = reinterpret_cast<AVFormatInternal*>(av_mallocz(sizeof(*_ic->internal)));
//            if (!_ic->internal) {
//                avformat_free_context(_ic);
//                ic = NULL;
//            }
//            else
//            {
//                _ic->internal->offset = AV_NOPTS_VALUE;
//                _ic->internal->raw_packet_buffer_remaining_size = RAW_PACKET_BUFFER_SIZE;
//                _ic->internal->shortest_end = AV_NOPTS_VALUE;
//                ic = _ic;
//            }
            ic = _ic;
        }

        if(!ic)
            return -1;
    }

    //初始化 AVFormatContext* ic
    ic->video_codec_id     = AV_CODEC_ID_NONE;
    ic->audio_codec_id     = AV_CODEC_ID_NONE;
    ic->subtitle_codec_id  = AV_CODEC_ID_NONE;
    ic->data_codec_id      = AV_CODEC_ID_NONE;

    //ic->interrupt_callback = int_cb;

    if (!av_dict_get(o->g->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
        av_dict_set(&o->g->format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        scan_all_pmts_set = 1;
    }

    //下一步复制：
    //static int open_input_file(OptionsContext *o, const char *filename)
    /* open the input file with generic avformat function */


    /* apply forced codec ids */


    /* update the current parameters so that they match the one of the input stream */


    /* dump the file content */

    /* 构建 InputFile *f ，存储到 paramCtx.input_files */


    /* check if all codec options have been used */

    return -1;
}

int ConvertThread::openOutputFile()
{

    return -1;
}

int ConvertThread::transcode()
{

    return -1;
}

void ConvertThread::avformat_get_context_defaults(AVFormatContext *s)
{
    memset(s, 0, sizeof(AVFormatContext));

    //s->av_class = &av_format_context_class;

    //s->io_open  = io_open_default;
    //s->io_close = io_close_default;

    av_opt_set_defaults(s);
}

void ConvertThread::finish_group(OptionParseContext *octx, int group_idx, const char *arg)
{
    OptionGroupList *l = &octx->groups[group_idx];
    OptionGroup *g;

    //GROW_ARRAY(l->groups, l->nb_groups);
    l->groups = grow_array(l->groups, sizeof(*l->groups), &l->nb_groups, l->nb_groups + 1);

    g = &l->groups[l->nb_groups - 1];

    *g             = octx->cur_group;
    g->arg         = arg;
    g->group_def   = l->group_def;
    g->sws_dict    = sws_dict;
    g->swr_opts    = swr_opts;
    g->codec_opts  = codec_opts;
    g->format_opts = format_opts;
    g->resample_opts = resample_opts;

    codec_opts  = NULL;
    format_opts = NULL;
    resample_opts = NULL;
    sws_dict    = NULL;
    swr_opts    = NULL;
    init_opts();

    memset(&octx->cur_group, 0, sizeof(octx->cur_group));
}

void ConvertThread::add_opt(OptionParseContext *octx, const OptionDef *opt, const char *key, const char *val)
{
    //int global = !(opt->flags & (OPT_PERFILE | OPT_SPEC | OPT_OFFSET));
    //OptionGroup *g = global ? &octx->global_opts : &octx->cur_group;

    //这里模拟的行为中，没有 loglevel,y 等全局参数，构建的选项都是下一个 output 文件的局部参数选项
    OptionGroup *g = &octx->cur_group;

    //GROW_ARRAY(g->opts, g->nb_opts);
    g->opts = grow_array(g->opts, sizeof(*g->opts), &g->nb_opts, g->nb_opts + 1);


    g->opts[g->nb_opts - 1].opt = opt;
    g->opts[g->nb_opts - 1].key = key;
    g->opts[g->nb_opts - 1].val = val;
}



Mp3Editor::Mp3Editor(QObject *parent):QObject(parent)
{
    convertThread = new ConvertThread(this);

}

Mp3Editor::~Mp3Editor()
{

}

bool Mp3Editor::CustomizeMp3(QString filePath, const CustomMp3Data &customData)
{
    if(convertThread->isRunning())
        return false;

    CustomMp3Data cusData = customData;
    cusData.imagePath = "E:/openSourceGit/msvc/bin/x86/test.jpg";
    cusData.artist = "bsArtist";
    cusData.album = "bsAlbum";
    cusData.title = "bsTitle";
    filePath = "E:/openSourceGit/msvc/bin/x86/a.mp3";

    convertThread->SetConvertedData(filePath, cusData);
    convertThread->start(QThread::Priority::HighestPriority);

    emit sig_getEditResult(false, "E:/openSourceGit/msvc/bin/x86/a-converted.mp3", "error when converting");
    return false;
}
