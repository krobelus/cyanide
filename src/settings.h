#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

class Settings : public QObject
{
    Q_OBJECT

private:
    void db_set(QString name, QString value);

    /* QString db_get(QString name); */

public:
    Settings();
    void init();

    Q_INVOKABLE QString get(QString name); // gets the current value
    Q_INVOKABLE QString get_type(QString name);
    Q_INVOKABLE QString get_display_name(QString name);
    Q_INVOKABLE int get_current_index(QString name);
    Q_INVOKABLE QStringList get_names();
    Q_INVOKABLE QStringList get_display_names(QString type);
    Q_INVOKABLE void set(QString name, QString value);
    Q_INVOKABLE void set_current_index(QString name, int i);

    /*
    Q_INVOKABLE void add_friend_address(const QVariant fid, const QVariant address);
    Q_INVOKABLE QString get_friend_address(const QVariant fid);
    */
};

#endif // SETTINGS_H
