#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_NAME "Tox User"
#define DEFAULT_STATUS "Toxing on Cyanide"
#define DEFAULT_FRIEND_REQUEST_MESSAGE "Tox me maybe?"
#define CONFIG_PATH "/home/nemo/.config/tox/"
#define AVATAR_PATH "/home/nemo/.config/tox/avatars/"
#define DOWNLOAD_PATH "/home/nemo/Downloads/"

#define MAX_AVATAR_SIZE 64 * 1024
#define MAX_CALLS 16

enum { LOOP_RUN = 0
     , LOOP_FINISH = 1
     , LOOP_RELOAD = 2
     , LOOP_SUSPEND = 3
};

#endif // CONFIG_H
