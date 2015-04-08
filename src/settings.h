#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

typedef struct {QString display_name; QString type; QString value;} settings_entry;

typedef struct {QString display_name; QString value;} type_entry;

class Settings : public QObject
{
    Q_OBJECT

private:
    QSqlDatabase db;

    static std::map<QString, settings_entry> entries;
    static std::map<QString, std::vector<type_entry>> types;

    void update_db_version();
    void create_tables();

    void db_set(QString name, QString value);
    QString db_get(QString name);

public:
    Settings();
    void create_database(QString name);

    Q_INVOKABLE QString get(QString name); // gets the current value
    Q_INVOKABLE QString get_type(QString name);
    Q_INVOKABLE QString get_display_name(QString name);
    Q_INVOKABLE int get_current_index(QString name);
    Q_INVOKABLE QStringList get_names();
    Q_INVOKABLE QStringList get_display_names(QString type);
    Q_INVOKABLE void set(QString name, QString value);
    Q_INVOKABLE void set_current_index(QString name, int i);

    void add_friend(QString address);
    void add_friend_if_not_exists(QString public_key);
    Q_INVOKABLE QString get_friend_address(QString public_key);
    void set_friend_avatar_hash(QString public_key, QByteArray hash);
    QByteArray get_friend_avatar_hash(QString public_key);
    void remove_friend(QString public_key);
};

#endif // SETTINGS_H
