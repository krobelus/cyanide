#include <tox/tox.h>

#include "cyanide.h"
#include "util.h"

#include "settings.h"

const QString Settings::db_version = "0002";

const QString Settings::tables[] = {
    "CREATE TABLE IF NOT EXISTS settings (name TEXT PRIMARY KEY, value TEXT)",
    "CREATE TABLE IF NOT EXISTS friends (public_key TEXT PRIMARY KEY, address "
    "TEXT, avatar_hash BLOB)"};

std::map<QString, settings_entry> Settings::entries = {
    {"wifi-only", {0, tr("Wi-Fi only"), "bool", "true"}},
    {"send-typing-notifications",
     {1, tr("Send typing notifications?"), "bool", "true"}},
    {"keep-history", {2, tr("Keep chat history"), "bool", "true"}},
    {"udp-enabled", {3, tr("Enable UDP"), "bool", "true"}},
    {"notification-when", {4, tr("Notify me when…"), "none", ""}},
    {"notification-message-received",
     {5, tr("I receive a message"), "bool", "true"}},
    {"notification-friend-request-received",
     {6, tr("I receive a friend request"), "bool", "true"}}};

std::map<QString, std::vector<type_entry>> Settings::types = {
    {"none", {}}, {"bool", {{"", "true"}, {"", "false"}}}
    /*
   ,{ "sound",
        { { tr("No sound"), "" }
        , { "jolla-alarm",
   "/usr/share/sounds/jolla-ringtones/stereo/jolla-alarm.wav" }
        , { "jolla-calendar-alarm",
   "/usr/share/sounds/jolla-ringtones/stereo/jolla-calendar-alarm.wav" }
        , { "jolla-emailtone",
   "/usr/share/sounds/jolla-ringtones/stereo/jolla-emailtone.wav" }
        , { "jolla-imtone",
   "/usr/share/sounds/jolla-ringtones/stereo/jolla-imtone.wav" }
        , { "jolla-messagetone",
   "/usr/share/sounds/jolla-ringtones/stereo/jolla-messagetone.wav" }
        , { "jolla-ringtone",
   "/usr/share/sounds/jolla-ringtones/stereo/jolla-ringtone.wav" }
        }
    }
    */
};

#define QUERY(q) QSqlQuery q(QSqlDatabase::database("s" + profile_name))
void execute_sql_query(QSqlQuery &q) {
  if (!q.exec())
    qFatal("Failed to execute SQL query: %s", qPrintable(q.lastError().text()));
}

void Settings::open_database(QString &name) {
  QSqlDatabase::database("s" + profile_name).close();
  QSqlDatabase::removeDatabase("s" + profile_name);

  profile_name = name;

  if (QSqlDatabase::contains("s" + profile_name))
    return;

  QSqlDatabase db;

  if (db.isOpen()) {
    //        db.close();
  } else {
    db = QSqlDatabase::addDatabase("QSQLITE", "s" + profile_name);
  }

  QString dbdir = CYANIDE_DATA_DIR;
  if (!QDir(dbdir).exists())
    QDir().mkpath(dbdir);

  /* old database location */
  QFile file(TOX_DATA_DIR + "cyanide.sqlite");
  if (file.exists()) {
    file.rename(CYANIDE_DATA_DIR + "tox_save.sqlite");
  }

  db.setDatabaseName(dbdir + name.replace('/', '_') + ".sqlite");
  db.open();

  update_db_version();

  QString str("INSERT OR IGNORE INTO settings (name, value) VALUES (?, ?)");
  for (size_t i = 1; i < entries.size(); i++) {
    str.append(",(?,?)");
  }

  QUERY(q);
  q.prepare(str);
  for (auto &entry : entries) {
    q.addBindValue(entry.first);
    q.addBindValue(entry.second.value);
  }

  q.setForwardOnly(true);
  q.prepare("SELECT name, value FROM settings");
  execute_sql_query(q);
  while (q.next()) {
    QString name = q.value("name").toString();
    QString value = q.value("value").toString();
    if (name == "db_version")
      continue;
    auto it = entries.find(name);
    if (it != entries.end())
      it->second.value = value;
  }
}

Settings::~Settings() { qDebug() << "todo…"; }

QString Settings::get(QString const &name) const {
  return entries.at(name).value;
}

QString Settings::get_type(QString const &name) const {
  return entries.at(name).type;
}

QString Settings::get_display_name(QString const &name) const {
  return entries.at(name).display_name;
}

int Settings::get_current_index(QString const &name) const {
  QString value = get(name);
  QString type = get_type(name);
  std::vector<type_entry> ts = types.at(type);
  for (size_t i = 0; i < ts.size(); i++)
    if (ts.at(i).value == value)
      return i;
  return 0;
}

QStringList Settings::get_names() const {
  QStringList names;
  for (auto const &entry : entries) {
    (void)entry;
    names.append(QString());
  }
  for (auto const &entry : entries)
    names[entry.second.index] = entry.first;
  return names;
}

QStringList Settings::get_display_names(QString const &type) const {
  QStringList sl;
  std::vector<type_entry> ts = types.at(type);
  for (type_entry const &t : ts)
    sl << t.display_name;
  return sl;
}

void Settings::set(QString const &name, QString const &value) const {
  entries.find(name)->second.value = value;
  db_set(name, value);
}

void Settings::set_current_index(QString const &name, int i) const {
  QString type = get_type(name);
  QString value = types.find(type)->second[i].value;
  set(name, value);
}

void Settings::db_set(QString const &name, QString const &value) const {
  QUERY(q);
  q.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
  q.addBindValue(name);
  q.addBindValue(value);
  execute_sql_query(q);
}

QString Settings::db_get(QString const &name) const {
  QUERY(q);
  q.prepare("SELECT value FROM settings WHERE name = ?");
  q.addBindValue(name);
  execute_sql_query(q);
  if (q.first()) {
    return q.value("value").toString();
  } else {
    return "";
  }
}

void Settings::add_friend(QString const &address) const {
  QUERY(q);
  q.prepare(
      "INSERT OR REPLACE INTO friends (public_key, address) VALUES (?, ?)");
  q.addBindValue(address.left(2 * TOX_PUBLIC_KEY_SIZE));
  q.addBindValue(address);
  execute_sql_query(q);
}

void Settings::add_friend_if_not_exists(QString const &public_key) const {
  QUERY(q);
  q.prepare("INSERT OR IGNORE INTO FRIENDS (public_key) VALUES (?)");
  q.addBindValue(public_key);
  execute_sql_query(q);
}

void Settings::remove_friend(QString const &public_key) const {
  QUERY(q);
  q.prepare("DELETE FROM friends WHERE public_key = ?");
  q.addBindValue(public_key);
  execute_sql_query(q);
}

QString Settings::get_friend_address(QString const &public_key) const {
  QUERY(q);
  q.prepare("SELECT address FROM friends WHERE public_key = ?");
  q.addBindValue(public_key);
  execute_sql_query(q);
  if (q.first()) {
    return q.value("address").toString();
  } else {
    return "";
  }
}

void Settings::set_friend_avatar_hash(QString const &public_key,
                                      QByteArray const &hash) const {
  QUERY(q);
  q.prepare("UPDATE friends SET avatar_hash = ? WHERE public_key = ?");
  q.addBindValue(hash);
  q.addBindValue(public_key);
  execute_sql_query(q);
}

QByteArray Settings::get_friend_avatar_hash(QString const &public_key) const {
  QUERY(q);
  q.prepare("SELECT avatar_hash FROM friends WHERE public_key = ?");
  q.addBindValue(public_key);
  execute_sql_query(q);
  if (q.first()) {
    return q.value("avatar_hash").toByteArray();
  } else {
    return QByteArray(TOX_HASH_LENGTH, '\0');
  }
}

void Settings::update_db_version() {
  create_tables();
  QString current_version = db_get("db_version");

  if (current_version == db_version) {
    ;
  } else if (current_version == "0001") {
    QUERY(q);
    QUERY(i);
    q.prepare("CREATE TABLE old_friends (fid INT PRIMARY KEY, address TEXT)");
    execute_sql_query(q);
    q.prepare("INSERT INTO old_friends SELECT * FROM friends");
    execute_sql_query(q);
    q.prepare("DROP TABLE friends");
    execute_sql_query(q);
    create_tables();
    q.setForwardOnly(true);
    q.prepare("SELECT * FROM old_friends");
    execute_sql_query(q);
    while (q.next()) {
      QString address = q.value("address").toString();
      if (address.length() == 2 * TOX_ADDRESS_SIZE) {
        QString public_key = address.left(2 * TOX_PUBLIC_KEY_SIZE);
        i.prepare("INSERT INTO friends (public_key, address) VALUES (?, ?)");
        i.addBindValue(public_key);
        i.addBindValue(address);
        execute_sql_query(i);
      }
    }
    q.prepare("DROP TABLE old_friends");
    execute_sql_query(q);
  } else {
    ;
  }
  db_set("db_version", db_version);
}

void Settings::create_tables() {
  QUERY(q);
  for (size_t i = 0; i < countof(tables); i++) {
    q.prepare(tables[i]);
    execute_sql_query(q);
  }
}
