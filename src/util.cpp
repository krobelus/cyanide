#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <tox/tox.h>

#include <QDebug>

void* file_raw(char *path, uint32_t *size)
{
    FILE *file;
    char *data;
    int len;

    file = fopen(path, "rb");
    if(!file) {
        qDebug() << "File not found (" << path << ")";
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    len = ftell(file);
    data = (char*)malloc(len);
    if(!data) {
        fclose(file);
        return NULL;
    }

    fseek(file, 0, SEEK_SET);

    if(fread(data, len, 1, file) != 1) {
        qDebug() << "Read error (" << path << ")";
        fclose(file);
        free(data);
        return NULL;
    }

    fclose(file);

    qDebug() << "Read " << len << "bytes (" << path << ")";

    if(size) {
        *size = len;
    }
    return data;
}

void* file_text(char *path)
{
    FILE *file;
    char *data;
    int len;

    file = fopen(path, "rb");
    if(!file) {
        qDebug() << "File not found (" << path << ")";
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    len = ftell(file);
    data = (char*)malloc(len + 1);
    if(!data) {
        fclose(file);
        return NULL;
    }

    fseek(file, 0, SEEK_SET);

    if(fread(data, len, 1, file) != 1) {
        qDebug() << "Read error (" << path << ")";
        fclose(file);
        free(data);
        return NULL;
    }

    fclose(file);

    qDebug() << "Read " << len << " bytes (" << path << ")";

    data[len] = 0;
    return data;
}

void to_hex(char *a, char *p, int size)
{
    char b, c, *end = p + size;

    while(p != end) {
        b = *p++;

        c = (b & 0xF);
        b = (b >> 4);

        if(b < 10) {
            *a++ = b + '0';
        } else {
            *a++ = b - 10 + 'A';
        }

        if(c < 10) {
            *a++ = c + '0';
        } else {
            *a++ = c  - 10 + 'A';
        }
    }
}

void id_to_string(char *dest, char *src)
{
    to_hex(dest, src, TOX_FRIEND_ADDRESS_SIZE);
}

void cid_to_string(char *dest, char *src)
{
    to_hex(dest, src, TOX_PUBLIC_KEY_SIZE);
}

void hash_to_string(char *dest, char *src)
{
    to_hex(dest, src, TOX_HASH_LENGTH);
}

bool string_to_id(char *w, char *a)
{
    char *end = w + TOX_FRIEND_ADDRESS_SIZE;
    while(w != end) {
        char c, v;

        c = *a++;
        if(c >= '0' && c <= '9') {
            v = (c - '0') << 4;
        } else if(c >= 'A' && c <= 'F') {
            v = (c - 'A' + 10) << 4;
        } else if(c >= 'a' && c <= 'f') {
            v = (c - 'a' + 10) << 4;
        } else {
            return 0;
        }

        c = *a++;
        if(c >= '0' && c <= '9') {
            v |= (c - '0');
        } else if(c >= 'A' && c <= 'F') {
            v |= (c - 'A' + 10);
        } else if(c >= 'a' && c <= 'f') {
            v |= (c - 'a' + 10);
        } else {
            return 0;
        }

        *w++ = v;
    }

    return 1;
}

QString to_QString(const void *ptr, int length)
{
    return QString(QByteArray((const char*)ptr, length));
}


const char *to_const_char(QString str)
{
    return str.toUtf8().constData();
}


int to_const_char(QString str, void *dest)
{
    return -1;
}

const uint8_t *to_tox_string(QString str)
{
    return (const uint8_t*)str.toUtf8().constData();
}

int to_tox_string(QString str, uint8_t *dest)
{
    QByteArray bytes = str.toUtf8();
    const char *charp = bytes.constData();
    memcpy(dest, charp, bytes.size());
    return bytes.size();
}

int tox_string_length(QString str)
{
    return str.toUtf8().length();
}

uint64_t get_time()
{
    struct timespec ts;
    #ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    #else
    clock_gettime(CLOCK_MONOTONIC, &ts);
    #endif

    return ((uint64_t)ts.tv_sec * (1000 * 1000 * 1000)) + (uint64_t)ts.tv_nsec;
}

