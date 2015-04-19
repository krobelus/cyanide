#include "cyanide.h"

#include "unused.h"

extern Cyanide cyanide;

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
        cyanide.notify_error("asdf call started", "");
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

void callback_av_audio(ToxAv *av, int32_t call_index, const int16_t *data, uint16_t samples, void *UNUSED(userdata))
{
    qDebug() << "was called";
    ToxAvCSettings dest;
    if(toxav_get_peer_csettings(av, call_index, 0, &dest) == 0) {
        //audio_play(call_index, data, samples, dest.audio_channels);
    }
}

/*
static void audio_play(int i, const int16_t *data, int samples, uint8_t channels, unsigned int sample_rate)
{
    if(!channels || channels > 2) {
        return;
    }
    // TODO
}
*/

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
    Friend *f = &friends[fid];
    Q_ASSERT(f->callstate == -1);
    toxav_cancel(toxav, f->call_index, fid, "Call cancelled by friend");
    emit signal_friend_callstate(fid, (f->callstate = 0));
}

void Cyanide::av_hangup(int fid)
{
    Friend *f = &friends[fid];
    toxav_hangup(toxav, f->call_index);
    emit signal_friend_callstate(fid, (f->callstate = 0));
}

