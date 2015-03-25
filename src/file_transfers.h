#ifndef FILE_TRANSFERS_H
#define FILE_TRANSFERS_H

typedef struct {
    uint32_t file_number, friend_number;

    uint8_t status;
    uint32_t kind;
    bool incoming;

    uint8_t *filename;
    size_t filename_length;
    uint64_t file_size;
    uint64_t position;

    FILE *file;

    uint8_t file_id[TOX_FILE_ID_LENGTH];
} File_Transfer;

#endif // FILE_TRANSFERS_H
