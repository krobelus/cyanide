#ifndef HISTORY_H
#define HISTORY_H

#include "message.h"

class History {
private:
  QString profile_name;
  int get_chat_id(QString const &public_key) const;
  int get_file_id(QByteArray const &tox_file_id) const;
  static const QString tables[];
  static const QString db_version;

public:
  void open_database(QString const &name);
  ~History();
  void add_message(QString const &public_key, Message const &m) const;
  void add_file(File_Transfer const &ft) const;
  void update_file(File_Transfer const &ft) const;
  void load_messages(QString const &public_key, QList<Message> &messages,
                     QDateTime const &from = QDateTime(),
                     QDateTime const &to = QDateTime()) const;
  void load_file_transfer(int file_id, File_Transfer &ft) const;
  void clear_messages(QString const &public_key) const;
};

#endif // HISTORY_H
