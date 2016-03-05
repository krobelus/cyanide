#ifndef MESSAGE_H
#define MESSAGE_H

#include <tox/tox.h>
#include <QtCore>

class Message_Type : public QObject
{
    Q_OBJECT
    Q_ENUMS(_)
public:
    enum _ { Normal = 0x0001
           , Action = 0x0003
           , File   = 0x0010
           , Image  = 0x0030
           };
};

class File_State : public QObject
{
    Q_OBJECT
    Q_ENUMS(_)
public:
    enum _ { Cancelled   = 0
           , Paused      = 1
           , Paused_Us   = 1 | 2
           , Paused_Them = 1 | 4
           , Paused_Both = Paused_Us | Paused_Them
           , Active      = 8
           , Finished    = 16
           };
};

typedef struct {
    uint32_t file_number;

    int status; // File_Transfer_Status

    uint8_t *filename;
    size_t filename_length;
    uint64_t file_size;
    uint64_t position;

    FILE *file;
    uint8_t *data;

    uint8_t file_id[TOX_FILE_ID_LENGTH];
} File_Transfer;

typedef struct {
    int type; // Message_Type

    QDateTime timestamp;
    bool author; /* sent by self */
    QString text; /* absolute file path if Image or File */

    File_Transfer *ft;
} Message;

#endif // MESSAGE_H
