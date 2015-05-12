#include "history.h"

#include "cyanide.h"
#include "util.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#define QUERY(q) QSqlQuery q(QSqlDatabase::database("h"+profile_name))

void execute_sql_query(QSqlQuery q);

History::History()
{
}

QString History::tables[] = {
    "CREATE TABLE IF NOT EXISTS chats (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL)",
    "CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL, chat_id INTEGER NOT NULL, author INTEGER NOT NULL, message TEXT NOT NULL)"
};
    
void History::open_database(QString name)
{
    QSqlDatabase::database("h"+profile_name).close();
    QSqlDatabase::removeDatabase("h"+profile_name);

    profile_name = name;

    if(QSqlDatabase::contains("h"+profile_name))
        return;

    QSqlDatabase db;

    if(db.isOpen()) {
    //  db.close();
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", "h"+profile_name);
    }

    db.setDatabaseName(CYANIDE_DATA_DIR + profile_name.replace('/', '_') + ".history");
    db.open();
    Q_ASSERT(db.isOpen());
    Q_ASSERT(db.isValid());

    QUERY(q);
    for(size_t i=0; i<countof(tables); i++) {
        q.prepare(tables[i]);
        execute_sql_query(q);
    }
}

void History::close_databases()
{
}

void History::add_message(QString public_key, Message *m)
{
    QUERY(q);
    int chat_id = get_chat_id(public_key);
    
    q.prepare("INSERT INTO messages (timestamp, chat_id, author, message) VALUES (?, ?, ?, ?)");
    q.addBindValue(m->timestamp);
    q.addBindValue(chat_id);
    q.addBindValue((int)m->author);
    q.addBindValue(m->text);
    qDebug() << "";
    execute_sql_query(q);
}

int History::get_chat_id(QString& name)
{
    QUERY(q);
    QUERY(s);
    q.prepare("SELECT id FROM chats WHERE name = ?");
    q.addBindValue(name);
    execute_sql_query(q);
    if(q.first()) {
        return q.value("id").toInt();
    } else {
        s.prepare("INSERT INTO chats (name) VALUES (?)");
        s.addBindValue(name);
        execute_sql_query(s);
        execute_sql_query(q);
        return q.value("id").toInt();
    }
}

void History::load_messages(QString public_key, QList<Message> *messages, int limit)
{
    QUERY(q);
    q.prepare("SELECT timestamp, author, message FROM messages WHERE chat_id = ? LIMIT ?");
    int chat_id = get_chat_id(public_key);
    q.addBindValue(chat_id);
    q.addBindValue(limit);
    execute_sql_query(q);
    while(q.next()) {
        Message m;
        m.type = Message_Type::Normal;
        m.author = q.value("author").toBool();
        m.text = q.value("message").toString();
        m.timestamp = q.value("timestamp").toDateTime();
        m.ft = NULL;
        messages->append(m);
    }
}

void History::clear_messages(QString public_key)
{
    QUERY(q);
    q.prepare("DELETE FROM messages WHERE chat_id = ?");
    q.addBindValue(get_chat_id(public_key));
    execute_sql_query(q);
}
