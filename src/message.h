#ifndef MESSAGE_H
#define MESSAGE_H

#include <QtCore>

typedef struct {
    uint32_t file_number;
    // uint32_t friend_number;

    /* -1 - paused by us
     * -2 - paused by them
     * -3 - paused by both
     *  0 - cancelled
     *  1 - active
     *  2 - finished
     */
    int status;
    // currently not needed because avatars are in
    // friends[fid].avatar_transfer
    // uint32_t kind;

    /*
    QString filename;
    */
    uint8_t *filename;
    size_t filename_length;
    uint64_t file_size;
    uint64_t position;

    FILE *file;
    uint8_t *data;

    uint8_t file_id[TOX_FILE_ID_LENGTH];
} File_Transfer;

#define MSGTYPE_NORMAL      1
#define MSGTYPE_ACTION      2
#define MSGTYPE_IMAGE       3
#define MSGTYPE_FILE        4

typedef struct {
    int type;

    QDateTime timestamp;
    bool author; /* sent by self */
    QString text; /* absolute file path if type =~ MSGTYPE_(IMAGE|FILE) */

    File_Transfer *ft;
} Message;

#endif // MESSAGE_H
