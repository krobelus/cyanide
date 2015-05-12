#ifndef UTIL_H
#define UTIL_H

#include <ctype.h>
#include <QtCore>

#define countof(x) (sizeof(x)/sizeof(*(x)))

#define MAX(a,b) a > b ? a : b
#define MIN(a,b) a > b ? b : a

/*todo: sprint_bytes */

/* read a whole file from a path,
 *  on success: returns pointer to data (must be free()'d later), writes size of data to *size if size is not NULL
 *  on failure: returns NULL
 */
void* file_raw(char *path, uint32_t *size);

//add null terminator to data
void* file_text(char *path);

void to_hex(char *a, char *p, int size);

void address_to_string(char *dest, char *src);

void public_key_to_string(char *dest, char *src);

void hash_to_string(char *dest, char *src);

bool string_to_address(char *w, char *a);

QString utf8_to_qstr(const void *src, size_t length);
size_t qstrlen(QString str);
void qstr_to_utf8(uint8_t *dest, QString src);

uint64_t get_time();
int get_time_ms();

bool looks_like_an_image(QString &path);

#endif // UTIL_H
