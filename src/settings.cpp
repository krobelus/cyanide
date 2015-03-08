#include "settings.h"

#include "config.h"

settings::settings()
{
}

void execute_sql_query(QSqlQuery q)
{
    if (!q.exec())
        qFatal("Failed to execute SQL query: %s", qPrintable(q.lastError().text()));
}

void init_db()
{
    QSqlDatabase db;
    QString dbdir = CONFIG_PATH;
    if (!QDir(dbdir).exists())
        QDir().mkpath(dbdir);
    db = QSqlDatabase::addDatabase("QSQLITE");
    Q_ASSERT(db.isValid());
    db.setDatabaseName(dbdir + QDir::separator() + "cyanide.sqlite");

    QSqlQuery q;
    q.prepare("CREATE TABLE IF NOT EXISTS friends (fid INT PRIMARY KEY, address CHAR(72))");
    execute_sql_query(q);
}

void settings_add_friend_address(const QVariant &fid, const QVariant &address)
{
    QSqlQuery q;
    q.prepare("INSERT INTO friends (fid, address) VALUES (?, ?)");
    q.addBindValue(fid);
    q.addBindValue(address);
    execute_sql_query(q);
}

QString settings_get_friend_address(const QVariant &fid)
{
    QSqlQuery q;
    q.prepare("SELECT address FROM friends WHERE fid = ?");
    q.addBindValue(fid);
    execute_sql_query(q);
    if(q.first()) {
        return q.value("address").toString();
    } else {
        return "";
    }
}
