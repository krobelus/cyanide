#ifndef HISTORY_H
#define HISTORY_H

#include "message.h"

class History
{
 private:
    QString profile_name;
    int get_chat_id(QString& public_key);
    static QString tables[];
 public:
    History();
    void open_database(QString name);
    void close_databases();
    void add_message(QString public_key, Message *m);
    void load_messages(QString public_key, QList<Message> *messages, int limit = 25);
    void clear_messages(QString public_key);
};

#endif // HISTORY_H
