#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

typedef struct {
  int index;
  QString display_name;
  QString type;
  QString value;
} settings_entry;

typedef struct {
  QString display_name;
  QString value;
} type_entry;

class Settings : public QObject {
  Q_OBJECT

private:
  static const QString db_version;
  static const QString tables[];
  static std::map<QString, settings_entry> entries;
  static std::map<QString, std::vector<type_entry>> types;

  void update_db_version();
  void create_tables();

  void db_set(QString const &name, QString const &value) const;
  QString db_get(QString const &name) const;

public:
  QString profile_name;
  void open_database(QString &name);
  ~Settings();

  Q_INVOKABLE QString get(QString const &name) const;
  Q_INVOKABLE QString get_type(QString const &name) const;
  Q_INVOKABLE QString get_display_name(QString const &name) const;
  Q_INVOKABLE int get_current_index(QString const &name) const;
  Q_INVOKABLE QStringList get_names() const;
  Q_INVOKABLE QStringList get_display_names(QString const &type) const;
  Q_INVOKABLE void set(QString const &name, QString const &value) const;
  Q_INVOKABLE void set_current_index(QString const &name, int i) const;

  void add_friend(QString const &address) const;
  void add_friend_if_not_exists(QString const &public_key) const;
  Q_INVOKABLE QString get_friend_address(QString const &public_key) const;
  void set_friend_avatar_hash(QString const &public_key,
                              QByteArray const &hash) const;
  QByteArray get_friend_avatar_hash(QString const &public_key) const;
  void remove_friend(QString const &public_key) const;
};

#endif // SETTINGS_H
