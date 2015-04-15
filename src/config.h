#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_NAME "Tox User"
#define DEFAULT_STATUS "Toxing on Cyanide"
#define DEFAULT_FRIEND_REQUEST_MESSAGE "Tox me maybe?"

#define MAX_AVATAR_SIZE 64 * 1024
#define MAX_CALLS 16

#include <QString>
#include <QDir>

const QString TOX_DATA_DIR = QDir::homePath() + "/.config/tox/";
const QString TOX_AVATAR_DIR   = TOX_DATA_DIR + "avatars/";
const QString CYANIDE_DATA_DIR = TOX_DATA_DIR + "cyanide/";

const QString DEFAULT_PROFILE_NAME = "tox_save";
const QString DEFAULT_PROFILE_FILE  = CYANIDE_DATA_DIR + "default_profile";

const QString DOWNLOAD_DIR = QDir::homePath() + "Downloads/";

const uint32_t MAX_ITERATION_TIME = 20;

enum LOOP_STATE
             { LOOP_RUN = 0
             , LOOP_FINISH
             , LOOP_RELOAD
             , LOOP_RELOAD_OTHER
             , LOOP_SUSPEND
             , LOOP_STOP
};

#endif // CONFIG_H
