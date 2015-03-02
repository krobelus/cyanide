/*
#include <QGuiApplication>
#include <QStandardPaths>
#include <QSqlError>
#include <QDebug>
#include <QSqlDatabase>

void init_db()
{
    QSqlDatabase db;
    QString dbdir = QStandardPaths::writableLocation((QStandardPaths::DataLocation));
    if (!QDir(dbdir).exists())
        QDir().mkpath(dbdir);
    db = QSqlDatabase::addDatabase("QSQLITE");
    Q_ASSERT(db.isValid());
    db.setDatabaseName(dbdir + QDir::separator() + kApplicationDBFileName);
    qDebug() << "Application database path: " <<
                dbdir + QDir::separator() + kApplicationDBFileName;

*/

#include <time.h>

#include "message.h"
#include "util.h"

Message::Message(const uint8_t *message, uint16_t length, bool from_me)
{
    author = from_me;
    text = to_QString(message, length);
    timestamp = QDateTime::currentDateTime();
}

Message::Message(QString msg, bool from_me)
{
    author = from_me;
    text = msg;
    timestamp = QDateTime::currentDateTime();
}
