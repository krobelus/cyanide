#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

class Settings : public QObject
{
    Q_OBJECT

public:
    Settings();
    void init();
    enum Parameters {
        send_typing_notifications = 1,
        notify_on_friend_message = 2
    };

    void add_friend_address(const QVariant fid, const QVariant address);
    QString get_friend_address(const QVariant fid);

};

static void execute_sql_query(QSqlQuery q);

#endif // SETTINGS_H
