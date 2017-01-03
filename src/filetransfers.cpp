#include <unistd.h>

#include "cyanide.h"

#include "settings.h"
#include "unused.h"
#include "util.h"

void callback_file_recv(Tox *tox, uint32_t fid, uint32_t file_number,
                        uint32_t kind, uint64_t file_size,
                        const uint8_t *filename, size_t filename_length,
                        void *that) {
  Cyanide *cyanide = (Cyanide *)that;
  if (kind == TOX_FILE_KIND_AVATAR)
    return cyanide->incoming_avatar(fid, file_number, file_size, filename,
                                    filename_length);

  qDebug() << "Received file transfer request: friend" << fid << "file"
           << file_number << "size" << file_size;

  Message m;

  QString basename = utf8_to_qstr(filename, filename_length);
  m.type =
      looks_like_an_image(basename) ? Message_Type::Image : Message_Type::File;

  m.timestamp = QDateTime::currentDateTime();
  m.author = false;

  File_Transfer *ft = m.ft = (File_Transfer *)malloc(sizeof(File_Transfer));
  if (ft == nullptr) {
    qDebug() << "Failed to allocate memory for the file transfer";
    return;
  }

  if (file_size == 0) {
    free(m.ft);
    m.ft = nullptr;
    qDebug() << "ignoring transfer request of size 0";
    return;
  }

  ft->file_number = file_number;
  ft->file_size = file_size;
  ft->position = 0;
  ft->filename_length = filename_length;
  ft->status = File_State::Paused_Us;
  ft->filename = (uint8_t *)malloc(filename_length);
  memcpy(ft->filename, filename, filename_length);

  QString path = m.text =
      DOWNLOAD_DIR + utf8_to_qstr(filename, filename_length);
  int i = 1;
  while (QFile::exists(path)) {
    path = m.text + "." + QString::number(i);
    i++;
  }
  m.text = path;

  if ((ft->file = fopen(m.text.toUtf8().constData(), "wb")) == nullptr)
    qDebug() << "Failed to open file " << m.text;

  cyanide->add_message(fid, m);
}

void Cyanide::incoming_avatar(uint32_t fid, uint32_t file_number,
                              uint64_t file_size, const uint8_t *filename,
                              size_t filename_length) {
  qDebug() << "Received avatar transfer request: friend" << fid << "file"
           << file_number << "size" << file_size;

  friends[fid].files[file_number] = -2;

  File_Transfer *ft = &friends[fid].avatar_transfer;
  ft->file_number = file_number;
  ft->file_size = file_size;
  ft->position = 0;
  ft->file = nullptr;
  ft->data = nullptr;
  ft->status = File_State::Paused_Them;

  /* check if the hash is the same as the one of the cached avatar */
  get_file_id(fid, ft);
  if (0 == memcmp(friends[fid].avatar_hash, ft->file_id, TOX_HASH_LENGTH)) {
    qDebug() << "avatar already present";
    goto cancel;
  }

  ft->filename_length = filename_length;
  ft->filename = (uint8_t *)malloc(filename_length);
  memcpy(ft->filename, filename, filename_length);
  char p[TOX_AVATAR_DIR.size() + 2 * TOX_PUBLIC_KEY_SIZE + 4];
  char hex_pubkey[2 * TOX_PUBLIC_KEY_SIZE + 1];
  public_key_to_string(hex_pubkey, (char *)friends[fid].public_key);
  hex_pubkey[2 * TOX_PUBLIC_KEY_SIZE] = '\0';
  sprintf(p, "%s%s.png", TOX_AVATAR_DIR.toUtf8().constData(), hex_pubkey);

  if (file_size == 0) {
    unlink(p);
    memset(friends[fid].avatar_hash, 0, TOX_HASH_LENGTH);
    QByteArray hash((const char *)friends[fid].avatar_hash, TOX_HASH_LENGTH);
    settings.set_friend_avatar_hash(get_friend_public_key(fid), hash);
    emit signal_avatar_change(fid);
    goto cancel;
  } else if (file_size > MAX_AVATAR_SIZE) {
    qDebug() << "avatar too large, rejecting";
    goto cancel;
  } else if ((ft->file = fopen(p, "wb")) == nullptr) {
    qDebug() << "Failed to open file " << p;
    goto cancel;
  } else if ((ft->data = (uint8_t *)malloc(ft->file_size)) == nullptr) {
    qDebug() << "Failed to allocate memory for the avatar";
    goto cancel;
  } else {
    QString errmsg = send_file_control(fid, -2, TOX_FILE_CONTROL_RESUME);
    if (errmsg == "")
      return;
    else
      qDebug() << "failed to resume avatar transfer: " << errmsg;
  }
cancel:
  QString errmsg = send_file_control(fid, -2, TOX_FILE_CONTROL_CANCEL);
  if (errmsg != "")
    qDebug() << "failed to cancel avatar transfer: " << errmsg;
  free(ft->data);
  if (ft->file != nullptr)
    fclose(ft->file);
}

void Cyanide::incoming_avatar_chunk(uint32_t fid, uint64_t position,
                                    const uint8_t *data, size_t length) {
  bool success;
  size_t n;

  Friend *f = &friends[fid];
  File_Transfer ft = f->avatar_transfer;

  if (length == 0) {
    qDebug() << "avatar transfer finished: friend" << fid << "file"
             << ft.file_number;
    success = tox_hash(f->avatar_hash, ft.data, ft.file_size);
    QByteArray hash((const char *)f->avatar_hash, TOX_HASH_LENGTH);
    settings.set_friend_avatar_hash(get_friend_public_key(fid), hash);
    Q_ASSERT(success);
    n = fwrite(ft.data, 1, ft.file_size, ft.file);
    Q_ASSERT(n == ft.file_size);
    fclose(ft.file);
    free(ft.data);
    emit signal_avatar_change(fid);
  } else {
    memcpy(ft.data + position, data, length);
  }
  (void)success;
  (void)n;
}

void callback_file_recv_chunk(Tox *tox, uint32_t fid, uint32_t file_number,
                              uint64_t position, const uint8_t *data,
                              size_t length, void *that) {
  Cyanide *cyanide = (Cyanide *)that;
  qDebug() << "was called";

  Friend *f = &cyanide->friends[fid];
  int mid = f->files[file_number];

  if (mid == -1) {
    Q_ASSERT(false);
  } else if (mid == -2) {
    return cyanide->incoming_avatar_chunk(fid, position, data, length);
  }

  File_Transfer *ft = f->messages[mid].ft;
  FILE *file = ft->file;

  if (length == 0) {
    qDebug() << "file transfer finished";
    fclose(ft->file);
    ft->status = File_State::Finished;
    emit cyanide->signal_file_status(fid, mid, ft->status);
    emit cyanide->signal_file_progress(fid, mid, 100);
  } else if (fwrite(data, 1, length, file) != length) {
    qDebug() << "fwrite failed";
  } else {
    if (ft->file_size == 0) {
      qDebug() << "receiving file with size 0";
      qDebug() << "lol";
    } else {
      ft->position += length;
      emit cyanide->signal_file_progress(fid, mid,
                                         cyanide->get_file_progress(fid, mid));
    }
  }
}

void callback_file_recv_control(Tox *UNUSED(tox), uint32_t fid,
                                uint32_t file_number, TOX_FILE_CONTROL action,
                                void *that) {
  Cyanide *cyanide = (Cyanide *)that;
  qDebug() << "was called";

  Friend *f = &cyanide->friends[fid];
  int mid = f->files[file_number];
  qDebug() << "message id is" << mid << "file number is" << file_number;

  Message *m = nullptr;
  File_Transfer *ft;

  if (mid == -1) {
    qDebug() << "outgoing avatar transfer";
    ft = &cyanide->self.avatar_transfer;
  } else if (mid == -2) {
    qDebug() << "incoming avatar transfer";
    ft = &f->avatar_transfer;
  } else {
    qDebug() << "normal transfer";
    if (mid > f->messages.size() - 1) {
      qWarning() << "received callback for deleted file transfer";
      return;
    }
    m = &f->messages[mid];
    ft = m->ft;
  }

  switch (action) {
  case TOX_FILE_CONTROL_RESUME:
    qDebug() << "transfer was resumed by peer, status:" << ft->status;
    if (ft->status == File_State::Paused_Them) {
      ft->status = File_State::Active;
    } else if (ft->status == File_State::Paused_Both) {
      ft->status = File_State::Paused_Us;
    } else {
      qDebug() << "unexpected status ^";
    }
    break;
  case TOX_FILE_CONTROL_PAUSE:
    qDebug() << "transfer was paused by peer, status:" << ft->status;
    if (ft->status == File_State::Paused_Us) {
      ft->status = File_State::Paused_Both;
    } else if (ft->status == File_State::Active) {
      ft->status = File_State::Paused_Them;
    } else {
      qDebug() << "unexpected status ^";
    }
    break;
  case TOX_FILE_CONTROL_CANCEL:
    qDebug() << "transfer was cancelled by peer, status:" << ft->status;
    ft->status = File_State::Cancelled;
    cyanide->history.update_file(*ft);
    /* if it's an incoming file, delete the file */
    if (m != nullptr && !m->author) {
      if (!QFile::remove(m->text))
        qDebug() << "Failed to remove file" << m->text;
    }
    break;
  default:
    Q_ASSERT(false);
  }
  emit cyanide->signal_file_status(fid, mid, ft->status);
}

void callback_file_chunk_request(Tox *tox, uint32_t fid, uint32_t file_number,
                                 uint64_t position, size_t length, void *that) {
  Cyanide *cyanide = (Cyanide *)that;
  qDebug() << "was called";

  bool success = false;
  TOX_ERR_FILE_SEND_CHUNK error;
  uint8_t *chunk = nullptr;

  Friend *f = &cyanide->friends[fid];
  int mid = f->files[file_number];

  qDebug() << "mid" << mid << "fid" << fid << "file_number" << file_number;

  File_Transfer *ft;

  if (mid == -1) {
    ft = &cyanide->self.avatar_transfer;
  } else {
    Q_ASSERT(mid != -2);
    ft = f->messages[mid].ft;
  }

  FILE *file = ft->file;

  if (length == 0) {
    fclose(file);
    qDebug() << "file sending done";
    ft->status = File_State::Finished;
    emit cyanide->signal_file_status(fid, mid, ft->status);
    success = true;
  } else {
    chunk = (uint8_t *)malloc(length);
    if (fread(chunk, 1, length, file) != length) {
      qDebug() << "fread failed";
      return;
    }
    success = tox_file_send_chunk(tox, fid, file_number, position,
                                  (const uint8_t *)chunk, length, &error);
  }

  if (success) {
    ft->position += length;
    if (mid >= 0)
      emit cyanide->signal_file_progress(fid, mid,
                                         cyanide->get_file_progress(fid, mid));
  } else {
    qDebug() << "Failed to send file chunk";
    switch (error) {
    case TOX_ERR_FILE_SEND_CHUNK_OK:
      break;
    case TOX_ERR_FILE_SEND_CHUNK_NULL:
      qDebug() << "chunk is null";
      break;
    case TOX_ERR_FILE_SEND_CHUNK_FRIEND_NOT_FOUND:
      qDebug() << "friend not found";
      break;
    case TOX_ERR_FILE_SEND_CHUNK_FRIEND_NOT_CONNECTED:
      qDebug() << "friend not connected";
      break;
    case TOX_ERR_FILE_SEND_CHUNK_NOT_FOUND:
      qDebug() << "file transfer not found";
      break;
    case TOX_ERR_FILE_SEND_CHUNK_NOT_TRANSFERRING:
      qDebug() << "file transfer inactive";
      break;
    case TOX_ERR_FILE_SEND_CHUNK_INVALID_LENGTH:
      qDebug() << "trying to send invalid length";
      break;
    case TOX_ERR_FILE_SEND_CHUNK_SENDQ:
      qDebug() << "packet queue is full";
      break;
    case TOX_ERR_FILE_SEND_CHUNK_WRONG_POSITION:
      qDebug() << "wrong position";
      break;
    default:
      Q_ASSERT(false);
    }
  }

  free(chunk);
}

QString Cyanide::resume_transfer(int fid, int mid) {
  return send_file_control(fid, mid, TOX_FILE_CONTROL_RESUME);
}

QString Cyanide::pause_transfer(int fid, int mid) {
  return send_file_control(fid, mid, TOX_FILE_CONTROL_PAUSE);
}

QString Cyanide::cancel_transfer(int fid, int mid) {
  return send_file_control(fid, mid, TOX_FILE_CONTROL_CANCEL);
}

QString Cyanide::send_file_control(int fid, int mid, TOX_FILE_CONTROL action) {
  bool success;
  TOX_ERR_FILE_CONTROL error;

  Message *m = nullptr;
  File_Transfer *ft;

  if (mid == -1) {
    ft = &self.avatar_transfer;
  } else if (mid == -2) {
    ft = &friends[fid].avatar_transfer;
  } else {
    m = &friends[fid].messages[mid];
    ft = m->ft;
  }

  success = tox_file_control(tox, fid, ft->file_number, action, &error);

  if (success) {
    switch (action) {
    case TOX_FILE_CONTROL_RESUME:
      qDebug() << "resuming transfer, status:" << ft->status;
      if (ft->status == File_State::Paused_Us) {
        ft->status = File_State::Active;
      } else if (ft->status == File_State::Paused_Both) {
        ft->status = File_State::Paused_Them;
      } else {
        qDebug() << "unexpected status ^";
      }
      break;
    case TOX_FILE_CONTROL_PAUSE:
      qDebug() << "pausing transfer, status:" << ft->status;
      if (ft->status == File_State::Active) {
        ft->status = File_State::Paused_Us;
      } else if (ft->status == File_State::Paused_Them) {
        ft->status = File_State::Paused_Both;
      } else {
        qDebug() << "unexpected status ^";
      }
      break;
    case TOX_FILE_CONTROL_CANCEL:
      qDebug() << "cancelling transfer, status:" << ft->status;
      ft->status = File_State::Cancelled;
      history.update_file(*ft);
      /* if it's an incoming file, delete the file */
      if (m != nullptr && !m->author) {
        if (!QFile::remove(m->text))
          qDebug() << "Failed to remove file" << m->text;
      }
      break;
    }
    qDebug() << "sent file control, new status:" << ft->status;
    if (mid >= 0)
      emit signal_file_status(fid, mid, ft->status);
    return "";
  } else {
    qDebug() << "File control failed";
    switch (error) {
    case TOX_ERR_FILE_CONTROL_OK:
      return "";
    case TOX_ERR_FILE_CONTROL_FRIEND_NOT_FOUND:
      return tr("Error: Friend not found");
    case TOX_ERR_FILE_CONTROL_FRIEND_NOT_CONNECTED:
      return tr("Error: Friend not connected");
    case TOX_ERR_FILE_CONTROL_NOT_FOUND:
      return tr("Error: File transfer not found");
    case TOX_ERR_FILE_CONTROL_NOT_PAUSED:
      return "Bug (please report): Not paused";
    case TOX_ERR_FILE_CONTROL_DENIED:
      return "Bug (please report): Transfer is paused by peer";
    case TOX_ERR_FILE_CONTROL_ALREADY_PAUSED:
      return tr("Error: Already paused");
    case TOX_ERR_FILE_CONTROL_SENDQ:
      return tr("Error: Packet queue is full");
    default:
      Q_ASSERT(false);
      return "Bug (please report): Unknown";
    }
  }
}

bool Cyanide::get_file_id(uint32_t fid, File_Transfer *ft) {
  TOX_ERR_FILE_GET error;

  if (!tox_file_get_file_id(tox, fid, ft->file_number, ft->file_id, &error)) {
    qDebug() << "Failed to get file id";
    switch (error) {
    case TOX_ERR_FILE_GET_OK:
      break;
    case TOX_ERR_FILE_GET_NULL:
      break;
    case TOX_ERR_FILE_GET_FRIEND_NOT_FOUND:
      qDebug() << "Error: File not found";
      break;
    case TOX_ERR_FILE_GET_NOT_FOUND:
      qDebug() << "Error: File transfer not found";
      break;
    }
    return false;
  }
  return true;
}

QString Cyanide::send_file(int fid, QString const path) {
  return send_file(TOX_FILE_KIND_DATA, fid, path, nullptr);
}

QString Cyanide::send_avatar(int fid) {
  QString avatar = TOX_AVATAR_DIR + get_friend_public_key(SELF_FRIEND_NUMBER) +
                   QString(".png");

  return send_file(TOX_FILE_KIND_AVATAR, fid, avatar, self.avatar_hash);
}

QString Cyanide::send_file(TOX_FILE_KIND kind, int fid, QString const &path,
                           uint8_t *file_id) {
  int error;

  Message m;
  File_Transfer *ft;

  if (kind == TOX_FILE_KIND_AVATAR) {
    ft = &self.avatar_transfer;
  } else {
    ft = (File_Transfer *)malloc(sizeof(File_Transfer));
    if (ft == nullptr) {
      qDebug() << "Failed to allocate memory for the file transfer";
      return "no memory";
    }
    m.type =
        looks_like_an_image(path) ? Message_Type::Image : Message_Type::File;
    m.timestamp = QDateTime::currentDateTime();
    m.author = true;
    m.text = path;
    m.ft = ft;
  }

  ft->position = 0;
  ft->status = File_State::Paused_Them;

  QString basename = path.right(path.size() - 1 - path.lastIndexOf("/"));
  ft->filename_length = qstrlen(basename);
  ft->filename = (uint8_t *)malloc(ft->filename_length + 1);
  qstr_to_utf8(ft->filename, basename);

  ft->file = fopen(path.toUtf8().constData(), "rb");
  if (ft->file == nullptr) {
    if (kind == TOX_FILE_KIND_AVATAR) {
      ft->file_size = 0;
    } else {
      return tr("Error: Failed to open file: ") + path;
    }
  } else {
    fseek(ft->file, 0L, SEEK_END);
    ft->file_size = ftell(ft->file);
    rewind(ft->file);
  }

  ft->file_number =
      tox_file_send(tox, fid, kind, ft->file_size, file_id, ft->filename,
                    ft->filename_length, (TOX_ERR_FILE_SEND *)&error);
  friends[fid].files[ft->file_number] = -1;
  qDebug() << "sending file, friend number" << fid << "file number"
           << ft->file_number;

  get_file_id(fid, ft);

  switch (error) {
  case TOX_ERR_FILE_SEND_OK:
    break;
  case TOX_ERR_FILE_SEND_FRIEND_NOT_FOUND:
    return tr("Error: Friend not found");
  case TOX_ERR_FILE_SEND_FRIEND_NOT_CONNECTED:
    return tr("Error: Friend not connected");
  case TOX_ERR_FILE_SEND_NAME_TOO_LONG:
    return tr("Error: Filename too long");
  case TOX_ERR_FILE_SEND_TOO_MANY:
    return tr("Error: Too many ongoing transfers");
  default:
    Q_ASSERT(false);
  }

  if (kind != TOX_FILE_KIND_AVATAR)
    add_message(fid, m);

  return "";
}
