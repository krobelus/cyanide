#include <AL/al.h>
#include <AL/alc.h>
#include <thread>
#include <unistd.h>

#include "cyanide.h"

/*
 * toxav_audio_bit_rate_set()
 * toxav_video_bit_rate_set()
 */

const int AUDIO_FRAME_DURATION = 20;
const int AUDIO_SAMPLE_RATE = 48000;
const int AUDIO_BIT_RATE = 32; //TODO dew the math
const int VIDEO_BIT_RATE = 3000;

bool Cyanide::call(int fid, bool video)
{
    Q_ASSERT(!in_call);

    set_audio_bit_rate(AUDIO_BIT_RATE);
    set_video_bit_rate(video ? VIDEO_BIT_RATE : 0);

    TOXAV_ERR_CALL error;
    bool success = toxav_call(toxav, fid, audio_bit_rate, video_bit_rate, &error);

    if(success) {
        set_in_call(true);
        set_call_friend_number(fid);
        set_call_state(Call_State::Ringing | Call_State::Outgoing);
    } else {
        qDebug() << "toxav_call() error:" << error;
        switch(error) {
        case TOXAV_ERR_CALL_OK:
            break;
        case TOXAV_ERR_CALL_MALLOC:
            qDebug() << "malloc";
            break;
        case TOXAV_ERR_CALL_FRIEND_NOT_FOUND:
            qDebug() << "friend not found";
            break;
        case TOXAV_ERR_CALL_FRIEND_NOT_CONNECTED:
            qDebug() << "friend not connected";
            break;
        case TOXAV_ERR_CALL_FRIEND_ALREADY_IN_CALL:
            qDebug() << "friend already in call";
            break;
        case TOXAV_ERR_CALL_INVALID_BIT_RATE:
            qDebug() << "invalid bit rate";
            break;
        case TOXAV_ERR_CALL_SYNC:
            qDebug() << "toxav error call sync";
            break;
        }
    }
    return success;
}

bool Cyanide::answer()
{
    Q_ASSERT(in_call);
    Q_ASSERT(call_state == (Call_State::Ringing | Call_State::Incoming));

    TOXAV_ERR_ANSWER error;

    set_call_state(0);

    bool success = toxav_answer(toxav, call_friend_number, audio_bit_rate, video_bit_rate, &error);

    if(success) {
    } else {
        qDebug() << "toxav_answer() error:" << error;
        switch(error) {
        case TOXAV_ERR_ANSWER_OK:
            break;
        case TOXAV_ERR_ANSWER_CODEC_INITIALIZATION:
            qDebug() << "codec initialization";
            break;
        case TOXAV_ERR_ANSWER_FRIEND_NOT_FOUND:
            qDebug() << "friend not found";
            break;
        case TOXAV_ERR_ANSWER_FRIEND_NOT_CALLING:
            qDebug() << "friend not calling";
            break;
        case TOXAV_ERR_ANSWER_INVALID_BIT_RATE:
            qDebug() << "invalid bit rate";
            break;
        case TOXAV_ERR_ANSWER_SYNC:
            qDebug() << "toxav error answer sync";
            break;
        }
    }
    return success;
}

bool Cyanide::hang_up()
{
    bool success = call_control(call_friend_number, Call_Control::Cancel);
    set_in_call(false);
    qDebug() << "call state:" << call_state;
    /*
    if(call_state & Call_State::Active) {
        my_audio_thread->join();
        delete my_audio_thread;
    }
    */
    return success;
}

bool Cyanide::call_control(int fid, int control)
{
    Q_ASSERT(call_friend_number == fid);

    TOXAV_ERR_CALL_CONTROL error;
    qDebug() << "sending toxav_call_control" << control;
    bool success = toxav_call_control(toxav, fid, (TOXAV_CALL_CONTROL)control, &error);
    if(!success) {
        qDebug() << "toxav_call_control() error:" << error;
        switch(error) {
        case TOXAV_ERR_CALL_CONTROL_OK:
            break;
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_FOUND:
            qDebug() << "friend not found";
            break;
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_IN_CALL:
            qDebug() << "friend not in call";
            break;
        case TOXAV_ERR_CALL_CONTROL_INVALID_TRANSITION:
            qDebug() << "invalid transition";
            break;
        case TOXAV_ERR_CALL_CONTROL_SYNC:
            qDebug() << "toxav error call control sync";
            break;
        }
    }
    return success;
}

void callback_call(ToxAV *av, uint32_t fid, bool audio_enabled, bool video_enabled, void *that)
{
    qDebug();
    Cyanide *cyanide = (Cyanide*)that;
    if(cyanide->in_call) {
        qDebug() << "ignoring av invite since we are already in a call";
        qDebug() << "TODO - notify";
        return;
    }
    cyanide->set_in_call(true);
    cyanide->set_call_friend_number(fid);
    cyanide->set_call_state(Call_State::Ringing | Call_State::Incoming);
    cyanide->set_audio_bit_rate(audio_enabled ? AUDIO_BIT_RATE : 0);
    cyanide->set_video_bit_rate(audio_enabled ? VIDEO_BIT_RATE : 0);
    emit cyanide->signal_call(fid);
}

QStringList Call_State::display(int s)
{
    return
    QStringList() << (s & Error ? "Error" : "")
                  << (s & Finished ? "Finished" : "")
                  << (s & Sending_A ? "Sending_A" : "")
                  << (s & Sending_V ? "Sending_V" : "")
                  << (s & Accepting_A ? "Accepting_A" : "")
                  << (s & Accepting_V ? "Accepting_V" : "")
                  << (s & Ringing ? "Ringing" : "");
}

void callback_call_state(ToxAV *av, uint32_t fid, uint32_t state, void *that)
{
    Cyanide *cyanide = (Cyanide*)that;
    cyanide->_callback_call_state(fid, state);
}

void Cyanide::_callback_call_state(int fid, int state)
{
    Q_ASSERT(in_call);
    Q_ASSERT(call_friend_number == fid);

    qDebug() << "call with friend" << fid << "changes state from" << Call_State::display(call_state) << "to" << Call_State::display(state);

    if(call_state & Call_State::Ringing && state & Call_State::Active) {
        qDebug() << "call was answered";
        my_audio_thread = new std::thread([this] () { this->audio_thread(); });
    } else if(state & Call_State::Finished) {
        qDebug() << "call was finished";
        set_in_call(false);
        my_audio_thread->join();
        delete my_audio_thread;
    } else if(state & Call_State::Error) {
        qDebug() << "error in call, terminating";
        set_in_call(false);
        my_audio_thread->join();
        delete my_audio_thread;
    } else {
        qDebug() << "unexpected state transition";
    }
    set_call_state(state);
}

void callback_audio_bit_rate_status(ToxAV *av, uint32_t fid, bool stable, uint32_t bit_rate, void *that)
{
    qDebug();
}

void callback_video_bit_rate_status(ToxAV *av, uint32_t fid, bool stable, uint32_t bit_rate, void *that)
{
    qDebug();
}

void callback_audio_receive_frame(ToxAV *av, uint32_t fid, const int16_t *pcm, size_t sample_count, uint8_t channels, uint32_t sampling_rate, void *that)
{
    //Cyanide *cyanide = (Cyanide*)that;
    static bool init = false;

    static ALCdevice *output_device;
    static ALCcontext *context;
    static ALuint source, buffer;
    static ALint state;

    if(init)
        goto play;

    init = true;
    output_device = alcOpenDevice(NULL);
    context = alcCreateContext(output_device, NULL);
    alcMakeContextCurrent(context);

    alGenSources(1, &source);
    alGenBuffers(1, &buffer);

play:
    ALCenum error;
#define alck(i) do { \
    error = alGetError(); \
    if(error != AL_NO_ERROR) \
        qDebug() << i << error; \
    } while(0)

    ALint processed = 0, queued = 16;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
    alck(1);
    alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
    alck(2);
    alSourcei(source, AL_LOOPING, AL_FALSE);
    alck(3);

    qDebug() << "processed:" << processed << "queued" << queued;
    if(processed) {
        ALuint buffers[processed];
        alSourceUnqueueBuffers(source, processed, buffers);
        alDeleteBuffers(processed - 1, buffers + 1);
        buffer = buffers[0];
    } else if(queued < 16) {
        qDebug() << "dropped audio frame";
    }
    alBufferData(buffer, AL_FORMAT_MONO16, pcm, sample_count * 2, sampling_rate);
    alck(4);
    alSourceQueueBuffers(source, 1, &buffer);
    alck(5);

    alGetSourcei(source, AL_SOURCE_STATE, &state);
    if(state != AL_PLAYING) {
        qDebug() << "starting source";
        alSourcePlay(source);
    }
}

void callback_video_receive_frame(ToxAV *av, uint32_t fid, uint16_t width, uint16_t height,
        const uint8_t *y, const uint8_t *u, const uint8_t *v, int32_t ystride, int32_t ustride, int32_t vstride, void *that)
{
    qDebug();
}

void Cyanide::audio_thread()
{
    qDebug() << "starting audio transmission for friend" << call_friend_number;

    ALCdevice *input_device;
    int samples_per_frame = (AUDIO_FRAME_DURATION * AUDIO_SAMPLE_RATE) / 1000;
    int16_t buffer[samples_per_frame];

    input_device = alcCaptureOpenDevice(NULL, AUDIO_SAMPLE_RATE, AL_FORMAT_MONO16, sizeof(buffer));
    alcCaptureStart(input_device);

    ALint samples;
    TOXAV_ERR_SEND_FRAME error;

    while(in_call) {
        alcGetIntegerv(input_device, ALC_CAPTURE_SAMPLES, sizeof(samples), &samples);
        if(samples >= samples_per_frame) {
            alcCaptureSamples(input_device, buffer, samples_per_frame);
            toxav_audio_send_frame(toxav, call_friend_number, buffer, samples_per_frame, 1, AUDIO_SAMPLE_RATE, &error);
            if(error != TOXAV_ERR_SEND_FRAME_OK)
                qDebug() << "send frame error:" << error;
        }
        callback_audio_receive_frame(NULL, call_friend_number, buffer, samples_per_frame, 1, AUDIO_SAMPLE_RATE, this);
        if(samples < samples_per_frame * 2)
            usleep(AUDIO_FRAME_DURATION * 1000);
    }
    alcCaptureStop(input_device);
    alcCaptureCloseDevice(input_device);
}

void Cyanide::set_in_call(bool in_call)
{
    this->in_call = in_call;
    emit in_call_changed();
}

void Cyanide::set_call_friend_number(int call_friend_number)
{
    if(this->call_friend_number == call_friend_number)
        return;
    this->call_friend_number = call_friend_number;
    emit call_friend_number_changed();
}

void Cyanide::set_call_state(int call_state)
{
    if(this->call_state == call_state)
        return;
    this->call_state = call_state;
    emit call_state_changed();
}

void Cyanide::set_audio_bit_rate(int audio_bit_rate)
{
    if(this->audio_bit_rate == audio_bit_rate)
        return;
    this->audio_bit_rate = audio_bit_rate;
    emit audio_bit_rate_changed();
}

void Cyanide::set_video_bit_rate(int video_bit_rate)
{
    if(this->video_bit_rate == video_bit_rate)
        return;
    this->video_bit_rate = video_bit_rate;
    emit video_bit_rate_changed();
}
