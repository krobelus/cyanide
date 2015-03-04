#ifndef UTIL_H
#define UTIL_H

#define countof(x) (sizeof(x)/sizeof(*(x)))

/*todo: sprint_bytes */

/* read a whole file from a path,
 *  on success: returns pointer to data (must be free()'d later), writes size of data to *size if size is not NULL
 *  on failure: returns NULL
 */
void* file_raw(char *path, uint32_t *size);

//add null terminator to data
void* file_text(char *path);

void to_hex(char *a, char *p, int size);

void id_to_string(char *dest, char *src);

void cid_to_string(char *dest, char *src);

void hash_to_string(char *dest, char *src);

bool string_to_id(char *w, char *a);

QString to_QString(const void *ptr, int length);

const uint8_t *to_tox_string(QString str);
int to_tox_string(QString str, uint8_t *dest);
//TODO rename
int tox_string_length(QString str);

const char *to_const_char(QString str);
int to_const_char(QString str, char *dest);

uint64_t get_time();
int get_time_ms();

#endif // UTIL_H
