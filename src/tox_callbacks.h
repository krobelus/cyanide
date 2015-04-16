#ifndef TOX_CALLBACKS_H
#define TOX_CALLBACKS_H

#include <tox/tox.h>
#include "unused.h"

void callback_friend_request(Tox *UNUSED(tox), const uint8_t *id, const uint8_t *msg, size_t length, void *UNUSED(userdata));

void callback_friend_message(Tox *tox, uint32_t fid, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *UNUSED(userdata));

void callback_friend_name(Tox *UNUSED(tox), uint32_t fid, const uint8_t *newname, size_t length, void *UNUSED(userdata));

void callback_friend_status_message(Tox *UNUSED(tox), uint32_t fid, const uint8_t *newstatus, size_t length, void *UNUSED(userdata));

void callback_friend_status(Tox *UNUSED(tox), uint32_t fid, TOX_USER_STATUS status, void *UNUSED(userdata));

void callback_friend_typing(Tox *UNUSED(tox), uint32_t fid, bool is_typing, void *UNUSED(userdata));

void callback_friend_read_receipt(Tox *UNUSED(tox), uint32_t fid, uint32_t receipt, void *UNUSED(userdata));

void callback_friend_connection_status(Tox *tox, uint32_t fid, TOX_CONNECTION status, void *UNUSED(userdata));

void callback_file_recv(Tox *tox, uint32_t fid, uint32_t file_number, uint32_t kind,
                        uint64_t file_size, const uint8_t *filename, size_t filename_length, void *UNUSED(user_data));

void callback_file_recv_chunk(Tox *tox, uint32_t fid, uint32_t file_number, uint64_t position,
                              const uint8_t *data, size_t length, void *UNUSED(user_data));

void callback_file_recv_control(Tox *UNUSED(tox), uint32_t friend_number, uint32_t file_number, TOX_FILE_CONTROL control,
                                void *UNUSED(userdata));


void callback_file_chunk_request(Tox *tox, uint32_t friend_number, uint32_t file_number, uint64_t position, size_t length, void *UNUSED(user_data));


void callback_av_invite(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));
void callback_av_start(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));
void callback_av_cancel(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));
void callback_av_reject(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));
void callback_av_end(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));

void callback_av_ringing(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));

void callback_av_requesttimeout(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));
void callback_av_peertimeout(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));
void callback_av_selfmediachange(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));
void callback_av_peermediachange(ToxAv *arg, int32_t call_index, void *UNUSED(userdata));

void callback_av_audio(ToxAv *av, int32_t call_index, const int16_t *data, uint16_t samples, void *UNUSED(userdata));
void callback_av_video(ToxAv *av, int32_t call_index, const vpx_image_t *img, void *UNUSED(userdata));

#endif // TOX_CALLBACKS_H
