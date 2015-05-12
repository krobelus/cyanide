#include "history.h"

#include "cyanide.h"
#include "util.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#define QUERY(q) QSqlQuery q(QSqlDatabase::database("h"+profile_name))

void execute_sql_query(QSqlQuery q);

QString History::db_version = "0001";

QString History::tables[] = {
    "CREATE TABLE IF NOT EXISTS history (version TEXT PRIMARY KEY NOT NULL)",
    "CREATE TABLE IF NOT EXISTS chats (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL)",
    "CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL, chat_id INTEGER NOT NULL, author INTEGER NOT NULL, message TEXT NOT NULL, file_id INTEGER)",
    "CREATE TABLE IF NOT EXISTS files (id INTEGER PRIMARY KEY AUTOINCREMENT, tox_file_id BLOB UNIQUE NOT NULL, status INTEGER NOT NULL, filename TEXT NOT NULL, file_size INTEGER NOT NULL, position INTEGER NOT NULL)"
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

    q.prepare("INSERT OR REPLACE INTO history (version) VALUES (?)");
    q.addBindValue(db_version);
    execute_sql_query(q);
}

void History::close_databases()
{
}

int History::get_chat_id(QString &name)
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

int History::get_file_id(QByteArray &tox_file_id)
{
    QUERY(q);
    q.prepare("SELECT id FROM files WHERE tox_file_id = ?");
    q.addBindValue(tox_file_id);
    execute_sql_query(q);
    if(q.first())
        return q.value("id").toInt();
    else
        return 0;
}

void History::add_message(QString public_key, QByteArray &tox_file_id, Message *m)
{
    QUERY(q);

    if(m->ft != NULL) {
        add_file(m->ft, tox_file_id);

        q.prepare("INSERT INTO messages (file_id, timestamp, chat_id, author, message) VALUES (?, ?, ?, ?, ?)");
        q.addBindValue(get_file_id(tox_file_id));
    } else {
        q.prepare("INSERT INTO messages (timestamp, chat_id, author, message) VALUES (?, ?, ?, ?)");
    }

    q.addBindValue(m->timestamp);
    q.addBindValue(get_chat_id(public_key));
    q.addBindValue((int)m->author);
    q.addBindValue(m->text);
    execute_sql_query(q);
}

void History::add_file(File_Transfer *ft, QByteArray &tox_file_id)
{
    QUERY(q);

    q.prepare("INSERT INTO files (tox_file_id, status, filename, file_size, position)"
              "VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(tox_file_id);
    q.addBindValue(ft->status);
    q.addBindValue(utf8_to_qstr(ft->filename, ft->filename_length));
    q.addBindValue(ft->file_size);
    q.addBindValue(ft->position);
    execute_sql_query(q);
}

void History::load_messages(QString public_key, QList<Message> *messages, int limit)
{
    QUERY(q);
    q.prepare("SELECT timestamp, author, message, file_id FROM messages WHERE chat_id = ? LIMIT ?");
    q.addBindValue(get_chat_id(public_key));
    q.addBindValue(limit);
    execute_sql_query(q);
    while(q.next()) {
        Message m;
        m.author = q.value("author").toBool();
        m.text = q.value("message").toString();
        m.timestamp = q.value("timestamp").toDateTime();
        if(q.value("file_id").isNull()) {
            m.type = Message_Type::Normal; // maybe store the type too
            m.ft = NULL;
        } else {
            m.type = looks_like_an_image(m.text) ? Message_Type::Image : Message_Type::File;
            m.ft = (File_Transfer*)malloc(sizeof(File_Transfer));
            load_file_transfer(q.value("file_id").toInt(), m.ft);
        }
        messages->append(m);
    }
}

void History::load_file_transfer(int file_id, File_Transfer *ft)
{
    QUERY(q);
    q.prepare("SELECT tox_file_id, status, filename, file_size, position FROM files WHERE id = ?");
    q.addBindValue(file_id);
    execute_sql_query(q);
    if(!q.first()) {
        qWarning() << "file transfer not found";
        return;
    }

    memcpy(ft->file_id, q.value("tox_file_id").toByteArray().data(), TOX_FILE_ID_LENGTH);
    ft->status = q.value("status").toInt();
    ft->filename = (uint8_t*)q.value("filename").toString().toUtf8().data();
    ft->filename_length = q.value("filename").toString().toUtf8().size();
    ft->file_size = q.value("file_size").toInt();
    ft->position = q.value("position").toInt();

    /* uninitialized fields:
     * file_number - retrieve from tox_file_send()
     * file
     * data
     */
}

void History::clear_messages(QString public_key)
{
    QUERY(q);
    QUERY(d);
    int chat_id = get_chat_id(public_key);
    q.prepare("SELECT file_id FROM messages WHERE chat_id = ?");
    q.addBindValue(chat_id);
    execute_sql_query(q);

    while(q.next()) {
        d.prepare("DELETE FROM files WHERE id = ?");
        d.addBindValue(q.value("file_id"));
        execute_sql_query(d);
    }

    q.prepare("DELETE FROM messages WHERE chat_id = ?");
    q.addBindValue(chat_id);
    execute_sql_query(q);
}
