#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

static void execute_sql_query(QSqlQuery q);

class Settings : public QObject
{
    Q_OBJECT

private:
    void db_set(QString name, QString value);

    /* QString db_get(QString name); */
    QString get_type(QString name);

public:
    Settings();
    void init();

    Q_INVOKABLE QString get(QString name);
    Q_INVOKABLE QString get_display_name(QString name);
    Q_INVOKABLE void set(QString name, int index);
    Q_INVOKABLE void set(QString name, QString value);

    /*
    Q_INVOKABLE void add_friend_address(const QVariant fid, const QVariant address);
    Q_INVOKABLE QString get_friend_address(const QVariant fid);
    */
};

#endif // SETTINGS_H
