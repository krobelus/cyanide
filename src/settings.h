#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

class settings
{
public:
    settings();
};

void execute_sql_query(QSqlQuery q);

void init_db();

void settings_add_friend_address(int fid, QString address);

void settings_get_friend_address(int fid);

#endif // SETTINGS_H
