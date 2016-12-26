#ifndef TOX_CALLBACKS_H
#define TOX_CALLBACKS_H

#include "cyanide.h"
#include "message.h"
#include "unused.h"
#include "util.h"

void callback_friend_request(Tox *UNUSED(tox), const uint8_t *id,
                             const uint8_t *msg, size_t length, void *that) {
  qDebug() << "was called";
  Cyanide *cyanide = (Cyanide *)that;
  int name_length = 2 * TOX_ADDRESS_SIZE * sizeof(char);
  void *name = malloc(name_length);
  address_to_string((char *)name, (char *)id);
  Friend *f = new Friend(id, utf8_to_qstr(name, name_length),
                         utf8_to_qstr(msg, length));
  f->accepted = false;
  uint32_t friend_number = cyanide->add_friend(f);
  emit cyanide->signal_friend_request(friend_number);
}

void callback_friend_message(Tox *UNUSED(tox), uint32_t fid,
                             TOX_MESSAGE_TYPE type, const uint8_t *msg,
                             size_t length, void *that) {
  qDebug() << "was called";
  Cyanide *cyanide = (Cyanide *)that;
  Message m;
  m.type = type == TOX_MESSAGE_TYPE_ACTION ? Message_Type::Action
                                           : Message_Type::Normal;
  m.author = false;
  m.text = utf8_to_qstr(msg, length);
  m.timestamp = QDateTime::currentDateTime();
  m.ft = NULL;
  cyanide->add_message(fid, m);
}

void callback_friend_name(Tox *UNUSED(tox), uint32_t fid,
                          const uint8_t *newname, size_t length, void *that) {
  qDebug() << "was called";
  Cyanide *cyanide = (Cyanide *)that;
  Friend *f = &cyanide->friends[fid];
  QString previous_name = f->name;
  f->name = utf8_to_qstr(newname, length);
  emit cyanide->signal_friend_name(fid, previous_name);
  cyanide->save_needed = true;
}

void callback_friend_status_message(Tox *UNUSED(tox), uint32_t fid,
                                    const uint8_t *newstatus, size_t length,
                                    void *that) {
  qDebug() << "was called";
  Cyanide *cyanide = (Cyanide *)that;
  cyanide->friends[fid].status_message = utf8_to_qstr(newstatus, length);
  emit cyanide->signal_friend_status_message(fid);
  cyanide->save_needed = true;
}

void callback_friend_status(Tox *UNUSED(tox), uint32_t fid,
                            TOX_USER_STATUS status, void *that) {
  qDebug() << "was called";
  Cyanide *cyanide = (Cyanide *)that;
  cyanide->friends[fid].user_status = status;
  emit cyanide->signal_friend_status(fid);
}

void callback_friend_typing(Tox *UNUSED(tox), uint32_t fid, bool is_typing,
                            void *that) {
  Cyanide *cyanide = (Cyanide *)that;
  emit cyanide->signal_friend_typing(fid, is_typing);
}

void callback_friend_read_receipt(Tox *UNUSED(tox), uint32_t fid,
                                  uint32_t receipt, void *that) {
  qDebug() << "was called";
  // Cyanide *cyanide = (Cyanide*)that;
}

void callback_friend_connection_status(Tox *tox, uint32_t fid,
                                       TOX_CONNECTION status, void *that) {
  Cyanide *cyanide = (Cyanide *)that;
  Friend *f = &cyanide->friends[fid];
  f->connection_status = status;
  if (status != TOX_CONNECTION_NONE && f->needs_avatar) {
    qDebug() << "Sending avatar to friend" << fid;
    QString errmsg = cyanide->send_avatar(fid);
    if (errmsg != "")
      qDebug() << "Failed to send avatar. " << errmsg;
    else
      f->needs_avatar = false;
  }
  emit cyanide->signal_friend_connection_status(fid,
                                                status != TOX_CONNECTION_NONE);
}

void callback_file_recv(Tox *tox, uint32_t fid, uint32_t file_number,
                        uint32_t kind, uint64_t file_size,
                        const uint8_t *filename, size_t filename_length,
                        void *that);

void callback_file_recv_chunk(Tox *tox, uint32_t fid, uint32_t file_number,
                              uint64_t position, const uint8_t *data,
                              size_t length, void *that);

void callback_file_recv_control(Tox *UNUSED(tox), uint32_t friend_number,
                                uint32_t file_number, TOX_FILE_CONTROL control,
                                void *that);

void callback_file_chunk_request(Tox *tox, uint32_t friend_number,
                                 uint32_t file_number, uint64_t position,
                                 size_t length, void *that);

void callback_call(ToxAV *av, uint32_t fid, bool audio_enabled,
                   bool video_enabled, void *that);
void callback_call_state(ToxAV *av, uint32_t fid, uint32_t state, void *that);

void callback_video_bit_rate_status(ToxAV *av, uint32_t fid, bool stable,
                                    uint32_t bit_rate, void *that);
void callback_audio_bit_rate_status(ToxAV *av, uint32_t fid, bool stable,
                                    uint32_t bit_rate, void *that);

void callback_audio_receive_frame(ToxAV *av, uint32_t fid, const int16_t *pcm,
                                  size_t sample_count, uint8_t channels,
                                  uint32_t sampling_rate, void *that);
void callback_video_receive_frame(ToxAV *av, uint32_t fid, uint16_t width,
                                  uint16_t height, const uint8_t *y,
                                  const uint8_t *u, const uint8_t *v,
                                  int32_t ystride, int32_t ustride,
                                  int32_t vstride, void *that);

#endif // TOX_CALLBACKS_H
