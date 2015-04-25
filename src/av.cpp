#include <AL/al.h>
#include <AL/alc.h>

#include "cyanide.h"

#include "unused.h"
#include "util.h"

extern Cyanide cyanide;

static ALCdevice *device_out, *device_in;
static ALCcontext *context;
static ALuint source[MAX_CALLS];

void callback_av_invite(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";

    int fid = toxav_get_peer_id(av, call_index, 0);
    Friend *f = &cyanide.friends[fid];

    ToxAvCSettings peer_settings ;
    toxav_get_peer_csettings(av, call_index, 0, &peer_settings);
    bool video = peer_settings.call_type == av_TypeVideo;

    f->call_index = call_index;
    f->callstate = -2;
    emit cyanide.signal_friend_callstate(fid, f->callstate);
    emit cyanide.signal_av_invite(fid);
}

void callback_av_start(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
    ToxAvCSettings peer_settings;
    int fid = toxav_get_peer_id(av, call_index, 0);
    toxav_get_peer_csettings(av, call_index, 0, &peer_settings);
    bool video = peer_settings.call_type == av_TypeVideo;
    if(toxav_prepare_transmission(av, call_index, 1) == 0) {
        // call started
    } else {
        qDebug() << "toxav_prepare_transmission() failed";
        return;
    }
}

void callback_av_cancel(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_reject(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_end(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_ringing(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_requesttimeout(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_peertimeout(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_selfmediachange(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void callback_av_peermediachange(ToxAv *av, int32_t call_index, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void audio_play(int i, const int16_t *data, int samples, uint8_t channels, unsigned int sample_rate)
{
    if(!channels || channels > 2) {
        return;
    }

    ALuint bufid;
    ALint processed = 0, queued = 16;
    alGetSourcei(source[i], AL_BUFFERS_PROCESSED, &processed);
    alGetSourcei(source[i], AL_BUFFERS_QUEUED, &queued);
    alSourcei(source[i], AL_LOOPING, AL_FALSE);

    if(processed) {
        ALuint bufids[processed];
        alSourceUnqueueBuffers(source[i], processed, bufids);
        alDeleteBuffers(processed - 1, bufids + 1);
        bufid = bufids[0];
    } else if(queued < 16) {
        alGenBuffers(1, &bufid);
    } else {
        qDebug() << "dropped audio frame";
        return;
    }

    alBufferData(bufid, (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, data, samples * 2 * channels, sample_rate);
    alSourceQueueBuffers(source[i], 1, &bufid);

    ALint state;
    alGetSourcei(source[i], AL_SOURCE_STATE, &state);
    if(state != AL_PLAYING) {
        alSourcePlay(source[i]);
        qDebug() << "Starting source" << i;
    }
}

void Cyanide::audio_thread()
{
    const char *device_list, *output_device = NULL;
    //void *audio_device = NULL;

    bool call[MAX_CALLS] = {0}, preview = 0;
    bool audio_filtering_enabled;
    // bool groups_audio[MAX_NUM_GROUPS] = {0};

    int perframe = (av_DefaultSettings.audio_frame_duration * av_DefaultSettings.audio_sample_rate) / 1000;
    uint8_t buf[perframe * 2 * av_DefaultSettings.audio_channels], dest[perframe * 2 * av_DefaultSettings.audio_channels];
    memset(buf, 0, sizeof(buf));

    uint8_t audio_count = 0;
    bool record_on = 0;
    #ifdef AUDIO_FILTERING
    qDebug() << "Audio Filtering enabled";
    #ifdef ALC_LOOPBACK_CAPTURE_SAMPLES
    qDebug() << "Echo cancellation enabled";
    #endif
    #endif

    qDebug() << "frame size:", perframe;

    device_list = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
    if(device_list) {
        output_device = device_list;
        qDebug() << "Output Device List:";
        while(*device_list) {
            qDebug() << device_list;
            //postmessage(NEW_AUDIO_OUT_DEVICE, 0, 0, (void*)device_list);
            device_list += strlen(device_list) + 1;
        }
    }

    device_out = alcOpenDevice(output_device);
    if(!device_out) {
        qDebug() << "alcOpenDevice() failed";
        return;
    }

    int attrlist[] = {  ALC_FREQUENCY, av_DefaultSettings.audio_sample_rate,
                        ALC_INVALID };

    context = alcCreateContext(device_out, attrlist);
    if(!alcMakeContextCurrent(context)) {
        qDebug() << "alcMakeContextCurrent() failed";
        alcCloseDevice(device_out);
        return;
    }

    alGenSources(countof(source), source);

    static ALuint ringSrc[MAX_CALLS];
    alGenSources(MAX_CALLS, ringSrc);

    /* Create buffer to store samples */
    ALuint RingBuffer;
    alGenBuffers(1, &RingBuffer);

    {
        float frequency1 = 441.f;
        float frequency2 = 882.f;
        int seconds = 4;
        unsigned sample_rate = 22050;
        size_t buf_size = seconds * sample_rate * 2; //16 bit (2 bytes per sample)
        int16_t *samples = (int16_t*)malloc(buf_size * sizeof(int16_t));
        if (!samples)
            return;

        /*Generate an electronic ringer sound that quickly alternates between two frequencies*/
        int index = 0;
        for(index = 0; index < buf_size; ++index) {
            if ((index / (sample_rate)) % 4 < 2 ) {//4 second ring cycle, first 2 secondsring, the rest(2 seconds) is silence
                if((index / 1000) % 2 == 1) {
                    samples[index] = 5000 * sin((2.0 * 3.1415926 * frequency1) / sample_rate * index); //5000=amplitude(volume level). It can be from zero to 32700
                } else {
                    samples[index] = 5000 * sin((2.0 * 3.1415926 * frequency2) / sample_rate * index);
                }
            } else {
                samples[index] = 0;
            }
        }

        alBufferData(RingBuffer, AL_FORMAT_MONO16, samples, buf_size, sample_rate);
        free(samples);
    }

    {
        unsigned int i;
        for (i = 0; i < MAX_CALLS; ++i) {
            alSourcei(ringSrc[i], AL_LOOPING, AL_TRUE);
            alSourcei(ringSrc[i], AL_BUFFER, RingBuffer);
        }
    }
    #ifdef AUDIO_FILTERING
    Filter_Audio *f_a = NULL;
    #endif

    while(loop == LOOP_RUN || loop == LOOP_SUSPEND) {
        #ifdef AUDIO_FILTERING
        if (!f_a && audio_filtering_enabled) {
            f_a = new_filter_audio(av_DefaultSettings.audio_sample_rate);
            if (!f_a) {
                audio_filtering_enabled = 0;
                qDebug() << "filter audio failed");
            } else {
                qDebug() << "filter audio on");
            }
        } else if (f_a && !audio_filtering_enabled) {
            kill_filter_audio(f_a);
            f_a = NULL;
            qDebug() << "filter audio off");
        }
        #else
        if (audio_filtering_enabled)
            audio_filtering_enabled = 0;
        #endif

        bool sleep = 1;

        if(record_on) {
            ALint samples;
            alcGetIntegerv(device_in, ALC_CAPTURE_SAMPLES, sizeof(samples), &samples);
            if(samples >= perframe) {
                alcCaptureSamples(device_in, buf, perframe);
                if (samples >= perframe * 2) {
                    sleep = 0;
                }
            }
        }

        #ifdef AUDIO_FILTERING
        #ifdef ALC_LOOPBACK_CAPTURE_SAMPLES
        if (f_a && audio_filtering_enabled) {
            ALint samples;
            alcGetIntegerv(device_out, ALC_LOOPBACK_CAPTURE_SAMPLES, sizeof(samples), &samples);
            if(samples >= perframe) {
                int16_t buffer[perframe];
                alcCaptureSamplesLoopback(device_out, buffer, perframe);
                pass_audio_output(f_a, buffer, perframe);
                set_echo_delay_ms(f_a, 5);
                    if (samples >= perframe * 2) {
                    sleep = 0;
                }
            }
        }
        #endif
        #endif

        #ifdef AUDIO_FILTERING
        if (f_a && filter_audio(f_a, (int16_t*)buf, perframe) == -1) {
            qDebug() << "filter audio error");
        }
        #endif
        if(preview) {
            audio_play(0, (int16_t*)buf, perframe, av_DefaultSettings.audio_channels, av_DefaultSettings.audio_sample_rate);
        }

        int i;
        for(i = 0; i < MAX_CALLS; i++) {
            if(call[i]) {
                int r;
                if((r = toxav_prepare_audio_frame(toxav, i, dest, sizeof(dest), (const int16_t*)buf, perframe)) < 0) {
                    qDebug() << "toxav_prepare_audio_frame error" << r;
                    continue;
                }

                if((r = toxav_send_audio(toxav, i, dest, r)) < 0) {
                    qDebug() << "toxav_send_audio error" << r;
                }
            }
        }

        if (sleep) {
            usleep(5000);
        }
    }

    #ifdef AUDIO_FILTERING
    kill_filter_audio(f_a);
    #endif

    //missing some cleanup ?
    alDeleteSources(MAX_CALLS, ringSrc);
    alDeleteSources(countof(source), source);
    alDeleteBuffers(1, &RingBuffer);

    if(device_in) {
        if(record_on) {
            alcCaptureStop(device_in);
        }
        alcCaptureCloseDevice(device_in);
    }

    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device_out);
}

void callback_av_audio(ToxAv *av, int32_t call_index, const int16_t *data, uint16_t samples, void *UNUSED(userdata))
{
    //qDebug() << "was called";
    ToxAvCSettings dest;
    if(toxav_get_peer_csettings(av, call_index, 0, &dest) == 0) {
        audio_play(call_index, data, samples, dest.audio_channels, dest.audio_sample_rate);
    }
}

void callback_av_video(ToxAv *av, int32_t call_index, const vpx_image_t *img, void *UNUSED(userdata))
{
    qDebug() << "was called";
}

void Cyanide::av_invite_accept(int fid)
{
    Friend *f = &friends[fid];
    ToxAvCSettings csettings = av_DefaultSettings;
    toxav_answer(toxav, f->call_index, &csettings);
    emit signal_friend_callstate(fid, (f->callstate = 2));
}

void Cyanide::av_invite_reject(int fid)
{
    Friend *f = &friends[fid];
    toxav_reject(toxav, f->call_index, ""); //TODO add reason
    emit signal_friend_callstate(fid, (f->callstate = 0));
}

void Cyanide::av_call(int fid)
{
    Friend *f = &friends[fid];
    if(f->callstate != 0)
        notify_error("already in a call", "");

    ToxAvCSettings csettings = av_DefaultSettings;

    toxav_call(toxav, &f->call_index, fid, &csettings, 15);
    emit signal_friend_callstate(fid, (f->callstate = -1));
}

void Cyanide::av_call_cancel(int fid)
{
    qDebug() << "cancelling call";
    Friend *f = &friends[fid];
    Q_ASSERT(f->callstate == -1);
    toxav_cancel(toxav, f->call_index, fid, "Call cancelled by friend");
    emit signal_friend_callstate(fid, (f->callstate = 0));
}

void Cyanide::av_hangup(int fid)
{
    qDebug() << "hanging up";
    Friend *f = &friends[fid];
    toxav_hangup(toxav, f->call_index);
    emit signal_friend_callstate(fid, (f->callstate = 0));
}

