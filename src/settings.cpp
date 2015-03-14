#include "config.h"
#include "util.h"

#include "settings.h"

QString db_version = "0001";

QString create_tables[] = {
        "CREATE TABLE IF NOT EXISTS settings (name TEXT PRIMARY KEY, value TEXT)",
        "CREATE TABLE IF NOT EXISTS friends (fid INT PRIMARY KEY, address TEXT)"};

typedef struct {QString display_name; QString type; QString value;} settings_entry;

std::map<QString, settings_entry> entries = {
        //% "Enable sounds"
        { "enable-sounds", { qtTrId("setting-enable-sounds"), "bool", "true" } }
        //% "Sound for incoming messages"
      , { "sound-message-received", { qtTrId("setting-sound-message-received")
            , "sound", "/usr/share/sounds/jolla-ringtones/stereo/jolla-imtone.wav" } }
        //% "Sound for incoming friend requests"
      , { "sound-friend-request-received", { qtTrId("setting-sound-friend-request-received")
            , "sound", "/usr/share/sounds/jolla-ringtones/stereo/jolla-emailtone.wav" } }
        //% "Sound when a friend comes online"
      , { "sound-friend-connected", { qtTrId("setting-sound-friend-connected")
            , "sound", "/usr/share/sounds/jolla-ringtones/stereo/jolla-imtone.wav" } }
        //% "Notify when a message is received"
      , { "notification-message-received", { qtTrId("setting-notification-message-received")
            , "bool", "true" } }
        //% "Notify when a friend request is received"
      , { "notification-friend-request-received", { qtTrId("setting-notification-friend-request-received")
            , "bool", "true" } }
        //% "Notify when a friend comes online"
      , { "notification-friend-connected", { qtTrId("setting-notification-friend-connected")
            , "bool", "true" } }
        //% "Notify when a friend changes his name"
      , { "notification-friend-name-change", { qtTrId("setting-notification-friend-name-change")
            , "bool", "true" } }
    };

typedef struct {QString display_name; QString value;} type_entry;
std::map<QString, std::vector<type_entry>> types = {
    { "bool",
        { { qtTrId("setting-yes"), "true" }
        , { qtTrId("setting-no"), "false" }
        }
    },
    { "sound",
      //% "None"
        { { qtTrId("setting-sound-none"), "none" }
        , { "jolla-alarm", "/usr/share/sounds/jolla-ringtones/stereo/jolla-alarm.wav" }
        , { "jolla-calendar-alarm", "/usr/share/sounds/jolla-ringtones/stereo/jolla-calendar-alarm.wav" }
        , { "jolla-emailtone", "/usr/share/sounds/jolla-ringtones/stereo/jolla-emailtone.wav" }
        , { "jolla-imtone", "/usr/share/sounds/jolla-ringtones/stereo/jolla-imtone.wav" }
        , { "jolla-messagetone", "/usr/share/sounds/jolla-ringtones/stereo/jolla-messagetone.wav" }
        , { "jolla-ringtone", "/usr/share/sounds/jolla-ringtones/stereo/jolla-ringtone.wav" }
        }
    }
};

Settings::Settings()
{
}

void execute_sql_query(QSqlQuery q)
{
    if (!q.exec())
        qFatal("Failed to execute SQL query: %s", qPrintable(q.lastError().text()));
}

void Settings::init()
{
    QSqlDatabase db;
    QString dbdir = CONFIG_PATH;
    if (!QDir(dbdir).exists())
        QDir().mkpath(dbdir);

    db = QSqlDatabase::addDatabase("QSQLITE");
    Q_ASSERT(db.isValid());
    db.setDatabaseName(dbdir + QDir::separator() + "cyanide.sqlite");
    Q_ASSERT(db.open());

    QSqlQuery q;
    for(size_t i=0; i<countof(create_tables); i++) {
        q.prepare(create_tables[i]);
        execute_sql_query(q);
    }

    db_set("db_version", db_version);

    QString qstr("INSERT OR IGNORE INTO settings (name, value) VALUES (?, ?)");
    for(size_t i=1; i<entries.size(); i++) {
        qstr.append(",(?,?)");
    }

    q.prepare(qstr);
    for(auto i=entries.begin(); i!=entries.end(); i++) {
        q.addBindValue(i->first);
        q.addBindValue(i->second.value);
    }
    execute_sql_query(q);

    q.setForwardOnly(true);
    q.prepare("SELECT name, value FROM settings");
    execute_sql_query(q);
    while(q.next()) {
        QString name = q.value("name").toString();
        QString value = q.value("value").toString();
        if(name != "db_version")
            entries.at(name).value = value;
    }
}

QString Settings::get(QString name)
{
    return entries.at(name).value;
}

QString Settings::get_type(QString name)
{
    return entries.at(name).type;
}

QString Settings::get_display_name(QString name)
{
    return entries.at(name).display_name;
}

int Settings::get_current_index(QString name)
{
    QString value = get(name);
    QString type = get_type(name);
    std::vector<type_entry> ts = types.at(type);
    for(size_t i=0; i<ts.size(); i++) {
        type_entry t = ts[i];
        if(t.value == value)
            return i;
    }
    return 0;
}

QStringList Settings::get_names()
{
    QStringList sl;
    for(auto i=entries.begin(); i!=entries.end(); i++) {
        sl << i->first;
    }
    return sl;
}

QStringList Settings::get_display_names(QString type)
{
    QStringList sl;
    std::vector<type_entry> ts = types.at(type);
    for(size_t i=0; i<ts.size(); i++)
        sl << ts[i].display_name;
    return sl;
}

void Settings::set(QString name, QString value)
{
    entries.find(name)->second.value = value;
    db_set(name, value);
}

void Settings::set_current_index(QString name, int i)
{
    QString type = get_type(name);
    QString value = types.find(type)->second[i].value;
    set(name, value);
}

void Settings::db_set(QString name, QString value)
{
    QSqlQuery q;
    q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    q.addBindValue(name);
    q.addBindValue(value);
    execute_sql_query(q);
}

/*
QString Settings::db_get(QString name)
{
    QSqlQuery q;
    q.prepare("SELECT value FROM settings WHERE name = ?");
    q.addBindValue(name);
    execute_sql_query(q);
    if(q.first()) {
        return q.value("value").toString();
    } else {
        return "";
    }
}
*/

/*
void Settings::add_friend_address(const QVariant fid, const QVariant address)
{
    QSqlQuery q;
    q.prepare("INSERT INTO friends (fid, address) VALUES (?, ?)");
    q.addBindValue(fid);
    q.addBindValue(address);
    execute_sql_query(q);
}

QString Settings::get_friend_address(const QVariant fid)
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
*/
