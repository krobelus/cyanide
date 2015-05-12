#ifndef HISTORY_H
#define HISTORY_H

#include "message.h"

class History
{
 private:
    QString profile_name;
    int get_chat_id(QString &public_key);
    int get_file_id(QByteArray &tox_file_id);
    static QString tables[];
    static QString db_version;
 public:
    void open_database(QString name);
    void close_databases();
    void add_message(QString public_key, QByteArray &tox_file_id, Message *m);
    void add_file(File_Transfer *ft, QByteArray &tox_file_id);
    void load_messages(QString public_key, QList<Message> *messages, int limit = 25); // TODO make sense
    void load_file_transfer(int file_id, File_Transfer *ft);
    void clear_messages(QString public_key);
};

#endif // HISTORY_H
