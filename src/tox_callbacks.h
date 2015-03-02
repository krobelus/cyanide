#ifndef TOX_CALLBACKS_H
#define TOX_CALLBACKS_H

#include <tox/tox.h>
#include "unused.h"

void callback_friend_request(Tox *UNUSED(tox), const uint8_t *id, const uint8_t *msg, uint16_t length, void *UNUSED(userdata));

void callback_friend_message(Tox *tox, int fid, const uint8_t *message, uint16_t length, void *UNUSED(userdata));

void callback_friend_action(Tox *tox, int fid, const uint8_t *action, uint16_t length, void *UNUSED(userdata));

void callback_name_change(Tox *UNUSED(tox), int fid, const uint8_t *newname, uint16_t length, void *UNUSED(userdata));

void callback_status_message(Tox *UNUSED(tox), int fid, const uint8_t *newstatus, uint16_t length, void *UNUSED(userdata));

void callback_user_status(Tox *UNUSED(tox), int fid, uint8_t status, void *UNUSED(userdata));

void callback_typing_change(Tox *UNUSED(tox), int fid, uint8_t is_typing, void *UNUSED(userdata));

void callback_read_receipt(Tox *UNUSED(tox), int fid, uint32_t receipt, void *UNUSED(userdata));

void callback_connection_status(Tox *tox, int fid, uint8_t status, void *UNUSED(userdata));

void callback_avatar_info(Tox *tox, int fid, uint8_t format, uint8_t *hash, void *UNUSED(userdata));

void callback_avatar_data(Tox *tox, int fid, uint8_t format, uint8_t *hash, uint8_t *data, uint32_t datalen, void *UNUSED(userdata));

void callback_group_invite(Tox *tox, int fid, uint8_t type, const uint8_t *data, uint16_t length, void *UNUSED(userdata));

void callback_group_message(Tox *tox, int gid, int pid, const uint8_t *message, uint16_t length, void *UNUSED(userdata));

void callback_group_action(Tox *tox, int gid, int pid, const uint8_t *action, uint16_t length, void *UNUSED(userdata));

void callback_group_namelist_change(Tox *tox, int gid, int pid, uint8_t change, void *UNUSED(userdata));

void callback_group_title(Tox *tox, int gid, int pid, const uint8_t *title, uint8_t length, void *UNUSED(userdata));

void callback_file_send_request(Tox *tox, int32_t fid, uint8_t filenumber, uint64_t filesize, const uint8_t *filename,
                                       uint16_t filename_length, void *UNUSED(userdata));
void callback_file_control(Tox *tox, int32_t fid, uint8_t receive_send, uint8_t filenumber, uint8_t control,
                                  const uint8_t *data, uint16_t length, void *UNUSED(userdata));
void callback_file_data(Tox *UNUSED(tox), int32_t fid, uint8_t filenumber, const uint8_t *data, uint16_t length,
                               void *UNUSED(userdata));

#endif // TOX_CALLBACKS_H
