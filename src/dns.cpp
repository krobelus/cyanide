#include <tox/toxdns.h>
#include <QDnsLookup>
#include <QDebug>

#include "util.h"

struct toxdns3_server {
    const char* name;
    uint8_t public_key[32];
} servers[] = {
    { "toxme.se",
      {0x5D, 0x72, 0xC5, 0x17, 0xDF, 0x6A, 0xEC, 0x54, 0xF1, 0xE9, 0x77, 0xA6, 0xB6, 0xF2, 0x59, 0x14,
                    0xEA, 0x4C, 0xF7, 0x27, 0x7A, 0x85, 0x02, 0x7C, 0xD9, 0xF5, 0x19, 0x6D, 0xF1, 0x7E, 0x0B, 0x13}},
    { "utox.org",
      {0xD3, 0x15, 0x4F, 0x65, 0xD2, 0x8A, 0x5B, 0x41, 0xA0, 0x5D, 0x4A, 0xC7, 0xE4, 0xB3, 0x9C, 0x6B,
                    0x1C, 0x23, 0x3C, 0xC8, 0x57, 0xFB, 0x36, 0x5C, 0x56, 0xE8, 0x39, 0x27, 0x37, 0x46, 0x2A, 0x12}}
};

QByteArray fetch_last_text_record(QString record)
{
    QByteArray result;

    QDnsLookup dns;
    dns.setType(QDnsLookup::TXT);
    dns.setName(record);
    dns.lookup();

    int timeout = 0;
    while(!dns.isFinished()) {
        qApp->processEvents();
        usleep(100 * 1000);
        if(++timeout == 30) {
            qDebug() << "dns request timed out";
            dns.abort();
            return result;
        }
    }

    if(dns.error() == QDnsLookup::NotFoundError) {
        // TODO show this to the user
        qDebug() << "address not found";
        return result;
    } else if(dns.error() != QDnsLookup::NoError) {
        qDebug() << "dns lookup error";
        return result;
    }

    const QList<QDnsTextRecord> text_records = dns.textRecords();
    if(text_records.isEmpty()) {
        qDebug() << "no text record found";
        return result;
    }

    const QList<QByteArray> text_record_values = text_records.last().values();
    if(text_record_values.length() != 1) {
        qDebug() << "unexpected number of dns text record values" << text_record_values.length();
        return result;
    }

    result = text_record_values.first();
    return result;
}

bool query_toxdns3(uint8_t *dest, QString name, struct toxdns3_server server)
{
    QByteArray user;
    QByteArray record, id;
    QString entry;
    int dns_string_length, ret;
    const int dns_string_max_length = 128;

    int i = name.indexOf('@');
    if(i == -1)
        user = name.toUtf8();
    else
        user = name.left(i).toUtf8();

    void *dns3 = tox_dns3_new(server.public_key);
    if(dns3 == NULL) {
        qDebug() << "failed to create toxdns3 object";
    }
    uint8_t dns_string[dns_string_max_length];
    uint32_t request_id;
    dns_string_length = tox_generate_dns3_string(dns3, dns_string, dns_string_max_length,
            &request_id, (uint8_t*)user.data(), user.size());

    if(dns_string_length < 0) {
        return false;
    }

    record = '_'+QByteArray((char*)dns_string, dns_string_length)+"._tox."+server.name;

    qDebug() << record;

    entry = fetch_last_text_record(record);

    qDebug() << entry;

    if(entry == "") {
        return false;
    }

    if(!entry.startsWith("v=tox3")) {
        qDebug() << "server returned bad toxdns version";
        return false;
    }

    id = entry.mid(sizeof("v=tox3;id=")-1).toUtf8();

    ret = tox_decrypt_dns3_TXT(dns3, dest, (uint8_t*)id.data(),
                                          id.size(), request_id);
    if(ret < 0) {
        qDebug() << "failed to decrypt dns data";
        return false;
    }

    tox_dns3_kill(dns3);

    return true;
}

bool dns_request(uint8_t *dest, QString name)
{
    QString server_name = name.mid(name.indexOf('@')+1);
    qDebug() << "server name" << server_name;
    for(size_t i = 0; i < countof(servers); i++) {
        qDebug() << "server name:" << servers[i].name;
        if(servers[i].name == server_name) {
            return query_toxdns3(dest, name, servers[i]);
        }
    }

    /* try toxme.se if there is no server given */
    return query_toxdns3(dest, name, servers[0]);
}
