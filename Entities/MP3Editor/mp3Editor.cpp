#include "mp3Editor.h"
#include <QFileInfo>
#include <QDir>
#include <assert.h>

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

    /* parse options and open all input/output files */
    int ret = ffmpeg_parse_options();

    //转码输出
    ret = transcode();
    if(ret < 0){
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

int ConvertThread::ffmpeg_parse_options()
{
    OptionParseContext octx;
    char error[128];
    int ret;

    buildOptionContent(&octx);

    //loglevel
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    av_log_set_level(AV_LOG_TRACE);

    //打开输入的文件（mp3 和 图片）
    ret = open_files(&octx.groups[GROUP_INFILE], true);

    if(ret < 0){
        goto fail;
    }

    //打开输出文件（mp3）
    ret = open_files(&octx.groups[GROUP_OUTFILE], false);

    if(ret < 0){
        goto fail;
    }

fail:
    //uninit_parse_context(&octx);
    if (ret < 0) {
        av_strerror(ret, error, sizeof(error));
        av_log(NULL, AV_LOG_FATAL, "%s\n", error);
    }
    return ret;

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
//            SpecifierOpt **so = (SpecifierOpt **)dst;
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
    o->mux_max_delay  = (float)0.7;
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
    inputMp3Utf8 = inputMp3Path.toUtf8();//保证变量一直存在
    finish_group(octx,OptGroup::GROUP_INFILE, inputMp3Utf8);

    //#1
    if(!customData.imagePath.isEmpty())
    {
        inputImageUtf8 = customData.imagePath.toUtf8();//保证变量一直存在
        finish_group(octx,OptGroup::GROUP_INFILE, inputImageUtf8);
    }

    //构建输出项选项

    if(!customData.imagePath.isEmpty())
    {
        //有图片文件时，分别使用原来的音频流和新图片的视频流
        add_opt(octx,&optionMap,"map","0:0"); //音乐文件#0的音频流(0)
        add_opt(octx,&optionMap,"map","1:0"); //图片文件#1的视频流(0)
    }

    if( !customData.artist.isEmpty()|| !customData.title.isEmpty()||!customData.album.isEmpty())
    {
         if(!customData.artist.isEmpty()){
             mataArtistUtf8 = QString("artist="+customData.artist).toUtf8();
             add_opt(octx,&optionMetadata,"metadata",mataArtistUtf8);
         }
         if(!customData.title.isEmpty()){
             mataTitleUtf8 = QString("title="+customData.title).toUtf8();
             add_opt(octx,&optionMetadata,"metadata",mataTitleUtf8);
         }
         if(!customData.album.isEmpty()){
             mataAlbumUtf8 = QString("album="+customData.album).toUtf8();
             add_opt(octx,&optionMetadata,"metadata",mataAlbumUtf8);
         }
    }

     //输出使用的codec直接复制使用输入文件的
     add_opt(octx,&optionCodecName,"c","copy");

     //设置 mp3 头格式版本为 3
     av_dict_set(&format_opts, "id3v2_version", "3", 0);

     //构建输出文件的名称
     QFileInfo fileInfo(inputMp3Path);
     outputMp3Path = fileInfo.dir().absolutePath()+"/"+fileInfo.baseName()+"-converted.mp3";
     outputMp3Utf8 = outputMp3Path.toUtf8();//保证变量一直存在
     finish_group(octx,OptGroup::GROUP_OUTFILE, outputMp3Utf8);
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

void ConvertThread::add_input_streams(OptionsContext *o, AVFormatContext *ic)
{
    int ret;

    for (unsigned int i = 0; i < ic->nb_streams; i++)
    {
        AVStream *st = ic->streams[i];
        AVCodecParameters *par = st->codecpar;
        InputStream *ist = (InputStream *)av_mallocz(sizeof(*ist));
        char *framerate = NULL, *hwaccel_device = NULL;
        const char *hwaccel = NULL;
        char *hwaccel_output_format = NULL;
        char *codec_tag = NULL;
        char *next;
        char *discard_str = NULL;
        const AVClass *cc = avcodec_get_class();
        const AVOption *discard_opt = av_opt_find(&cc, "skip_frame", NULL, 0, 0);

        if (!ist)
            exit(1);

        GROW_ARRAY_2(paramCtx.input_streams, paramCtx.nb_input_streams,InputStream *);

        paramCtx.input_streams[ paramCtx.nb_input_streams - 1] = ist;

        ist->st = st;
        ist->file_index = paramCtx.nb_input_files;
        ist->discard = 1;
        st->discard  = AVDISCARD_ALL;
        ist->nb_samples = 0;
        ist->min_pts = INT64_MAX;
        ist->max_pts = INT64_MIN;

        ist->ts_scale = 1.0;
        MATCH_PER_STREAM_OPT(ts_scale, dbl, ist->ts_scale, ic, st);

        ist->autorotate = 1;
        MATCH_PER_STREAM_OPT(autorotate, i, ist->autorotate, ic, st);

        MATCH_PER_STREAM_OPT_2(codec_tags, str, codec_tag, ic, st);
        if (codec_tag) {
            uint32_t tag = strtol(codec_tag, &next, 0);
            if (*next)
                tag = AV_RL32(codec_tag);
            st->codecpar->codec_tag = tag;
        }

        ist->dec = choose_decoder(o, ic, st);
        ist->decoder_opts = filter_codec_opts(o->g->codec_opts, ist->st->codecpar->codec_id, ic, st, ist->dec);

        ist->reinit_filters = -1;
        MATCH_PER_STREAM_OPT(reinit_filters, i, ist->reinit_filters, ic, st);

        MATCH_PER_STREAM_OPT_2(discard, str, discard_str, ic, st);
        ist->user_set_discard = AVDISCARD_NONE;

        if ((o->video_disable && ist->st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) ||
            (o->audio_disable && ist->st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) ||
            (o->subtitle_disable && ist->st->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) ||
            (o->data_disable && ist->st->codecpar->codec_type == AVMEDIA_TYPE_DATA))
                ist->user_set_discard = AVDISCARD_ALL;

        if (discard_str && av_opt_eval_int(&cc, discard_opt, discard_str, &ist->user_set_discard) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error parsing discard %s.\n",
                    discard_str);
            exit(1);
        }

        ist->filter_in_rescale_delta_last = AV_NOPTS_VALUE;

        ist->dec_ctx = avcodec_alloc_context3(ist->dec);
        if (!ist->dec_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Error allocating the decoder context.\n");
            exit(1);
        }

        ret = avcodec_parameters_to_context(ist->dec_ctx, par);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error initializing the decoder context.\n");
            exit(1);
        }

        if (o->bitexact)
            ist->dec_ctx->flags |= AV_CODEC_FLAG_BITEXACT;

        switch (par->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            if(!ist->dec)
                ist->dec = avcodec_find_decoder(par->codec_id);
#if FF_API_LOWRES
            if (st->codec->lowres) {
                ist->dec_ctx->lowres = st->codec->lowres;
                ist->dec_ctx->width  = st->codec->width;
                ist->dec_ctx->height = st->codec->height;
                ist->dec_ctx->coded_width  = st->codec->coded_width;
                ist->dec_ctx->coded_height = st->codec->coded_height;
            }
#endif

            // avformat_find_stream_info() doesn't set this for us anymore.
            ist->dec_ctx->framerate = st->avg_frame_rate;

            MATCH_PER_STREAM_OPT_2(frame_rates, str, framerate, ic, st);
            if (framerate && av_parse_video_rate(&ist->framerate,
                                                 framerate) < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error parsing framerate %s.\n",
                       framerate);
                exit(1);
            }

            ist->top_field_first = -1;
            MATCH_PER_STREAM_OPT(top_field_first, i, ist->top_field_first, ic, st);

            MATCH_PER_STREAM_OPT_2(hwaccels, str, hwaccel, ic, st);
            if (hwaccel) {
                assert(false);
                //未搬运逻辑
                exit(1);
            }

            MATCH_PER_STREAM_OPT_2(hwaccel_devices, str, hwaccel_device, ic, st);
            if (hwaccel_device) {
                ist->hwaccel_device = av_strdup(hwaccel_device);
                if (!ist->hwaccel_device)
                    exit(1);
            }

            MATCH_PER_STREAM_OPT_2(hwaccel_output_formats, str,
                                 hwaccel_output_format, ic, st);
            if (hwaccel_output_format) {
                ist->hwaccel_output_format = av_get_pix_fmt(hwaccel_output_format);
                if (ist->hwaccel_output_format == AV_PIX_FMT_NONE) {
                    av_log(NULL, AV_LOG_FATAL, "Unrecognised hwaccel output "
                           "format: %s", hwaccel_output_format);
                }
            } else {
                ist->hwaccel_output_format = AV_PIX_FMT_NONE;
            }

            ist->hwaccel_pix_fmt = AV_PIX_FMT_NONE;

            break;
        case AVMEDIA_TYPE_AUDIO:
            ist->guess_layout_max = INT_MAX;
            MATCH_PER_STREAM_OPT(guess_layout_max, i, ist->guess_layout_max, ic, st);
            guess_input_channel_layout(ist);
            break;
        case AVMEDIA_TYPE_DATA:
        case AVMEDIA_TYPE_SUBTITLE: {
            break;
        }
        case AVMEDIA_TYPE_ATTACHMENT:
        case AVMEDIA_TYPE_UNKNOWN:
            break;
        default:
            abort();
        }

        ret = avcodec_parameters_from_context(par, ist->dec_ctx);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error initializing the decoder context.\n");
            exit(1);
        }
    }

}

int ConvertThread::open_files(OptionGroupList *l,bool isInput)
{
    int i, ret;
    const char * inout = isInput? "input":"output";

    for (i = 0; i < l->nb_groups; i++) {
        OptionGroup *g = &l->groups[i];
        OptionsContext o;

        init_options(&o);
        o.g = g;

        //TODO 详细了解 ffmpeg 模块中（write_option 的 opt_map实际做了什么）
        //TODO 解决后，搬运 open_output_file
        ret = parse_optgroup(&o, g, (void*)&paramCtx);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error parsing options for %s file "
                                       "%s.\n", inout, g->arg);
            return ret;
        }

        av_log(NULL, AV_LOG_DEBUG, "Opening an %s file: %s.\n", inout, g->arg);

        if(isInput)
            ret = open_input_file(&o, g->arg);
        else
            ret = open_output_file(&o, g->arg);

        uninit_options(&o);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error opening %s file %s.\n",
                   inout, g->arg);
            return ret;
        }
        av_log(NULL, AV_LOG_DEBUG, "Successfully opened the file.\n");
    }

    return 0;
}

int ConvertThread::open_input_file(OptionsContext *o, const char *filename)
{
    static int find_stream_info = 1;

    InputFile *f;
    AVInputFormat *file_iformat = NULL;
    int err, ret;
    unsigned int i;
    int64_t timestamp = 0;
    AVDictionary *unused_opts = NULL;
    AVDictionaryEntry *e = NULL;
    int scan_all_pmts_set = 0;

    //由于自定义AVFormatContext 创建内容时，需要用到 ->av_class = &av_format_context_class
    // 而 av_format_context_class 在 FFmpeg\libavformat\options.c 中定义，不打算搬运过来
    //所以 由后面 avformat_open_input 支持的方式自动分配

    AVFormatContext *ic = NULL;

    if (!av_dict_get(o->g->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
        av_dict_set(&o->g->format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        scan_all_pmts_set = 1;
    }

    /* open the input file with generic avformat function */
    err = avformat_open_input(&ic, filename, file_iformat, &o->g->format_opts);
    if (err < 0) {
        print_error(filename, err);
        if (err == AVERROR_PROTOCOL_NOT_FOUND)
            av_log(NULL, AV_LOG_ERROR, "Did you mean file:%s?\n", filename);
       exit(1);
    }
    if (scan_all_pmts_set)
        av_dict_set(&o->g->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);
    remove_avoptions(&o->g->format_opts, o->g->codec_opts);
    assert_avoptions(o->g->format_opts);

    /* apply forced codec ids */
    for (i = 0; i < ic->nb_streams; i++)
        choose_decoder(o, ic, ic->streams[i]);

    if (find_stream_info) {
        AVDictionary **opts = setup_find_stream_info_opts(ic, o->g->codec_opts);
        unsigned int orig_nb_streams = ic->nb_streams;

        /* If not enough info to get the stream parameters, we decode the
           first frames to get it. (used in mpeg case for example) */
        ret = avformat_find_stream_info(ic, opts);

        for (i = 0; i < orig_nb_streams; i++)
            av_dict_free(&opts[i]);
        av_freep(&opts);

        if (ret < 0) {
            av_log(NULL, AV_LOG_FATAL, "%s: could not find codec parameters\n", filename);
            if (ic->nb_streams == 0) {
                avformat_close_input(&ic);
                exit(1);
            }
        }
    }

    /* update the current parameters so that they match the one of the input stream */
    add_input_streams(o, ic);

    /* dump the file content */
    av_dump_format(ic, paramCtx.nb_input_files, filename, 0);

    /* 构建 InputFile *f ，存储到 paramCtx.input_files */

    GROW_ARRAY_2(paramCtx.input_files, paramCtx.nb_input_files,InputFile*);
    f = (InputFile *)av_mallocz(sizeof(*f));
    if (!f)
       exit(1);

    paramCtx.input_files[paramCtx.nb_input_files - 1] = f;

    f->ctx        = ic;
    f->ist_index  = paramCtx.nb_input_streams - ic->nb_streams;
    f->start_time = o->start_time;
    f->recording_time = o->recording_time;
    f->input_ts_offset = o->input_ts_offset;
    f->ts_offset  = o->input_ts_offset - (paramCtx.copy_ts ? (paramCtx.start_at_zero && ic->start_time != AV_NOPTS_VALUE ? ic->start_time : 0) : timestamp);
    f->nb_streams = ic->nb_streams;
    f->rate_emu   = o->rate_emu;
    f->accurate_seek = o->accurate_seek;
    f->loop = o->loop;
    f->duration = 0;
    f->time_base = { 1, 1};//(AVRational){ 1, 1}
#if HAVE_THREADS
    f->thread_queue_size = o->thread_queue_size > 0 ? o->thread_queue_size : 8;
#endif

    /* check if all codec options have been used */
    unused_opts = strip_specifiers(o->g->codec_opts);
    for (int i = f->ist_index; i < paramCtx.nb_input_streams; i++) {
        e = NULL;
        while ((e = av_dict_get(paramCtx.input_streams[i]->decoder_opts, "", e,
                                AV_DICT_IGNORE_SUFFIX)))
            av_dict_set(&unused_opts, e->key, NULL, 0);
    }

    e = NULL;
    while ((e = av_dict_get(unused_opts, "", e, AV_DICT_IGNORE_SUFFIX))) {
       assert(false);
       //未搬运
       exit(1);
    }
    av_dict_free(&unused_opts);

    paramCtx.input_stream_potentially_available = 1;

    return 0;
}

int ConvertThread::open_output_file(OptionsContext *o, const char *filename)
{
    //TODO open_output_file

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

    GROW_ARRAY_2(l->groups, l->nb_groups, OptionGroup);
    //l->groups = (OptionGroup *)grow_array(l->groups, sizeof(*l->groups), &l->nb_groups, l->nb_groups + 1);

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

    GROW_ARRAY_2(g->opts, g->nb_opts, Option);
    //g->opts = (Option*)grow_array(g->opts, sizeof(*g->opts), &g->nb_opts, g->nb_opts + 1);


    g->opts[g->nb_opts - 1].opt = opt;
    g->opts[g->nb_opts - 1].key = key;
    g->opts[g->nb_opts - 1].val = val;
}

void ConvertThread::print_error(const char *filename, int err)
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", filename, errbuf_ptr);
}

void ConvertThread::remove_avoptions(AVDictionary **a, AVDictionary *b)
{
    AVDictionaryEntry *t = NULL;

    while ((t = av_dict_get(b, "", t, AV_DICT_IGNORE_SUFFIX))) {
        av_dict_set(a, t->key, NULL, AV_DICT_MATCH_CASE);
    }
}

void ConvertThread::assert_avoptions(AVDictionary *m)
{
    AVDictionaryEntry *t;
    if ((t = av_dict_get(m, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_FATAL, "Option %s not found.\n", t->key);
        exit(1);
    }
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
