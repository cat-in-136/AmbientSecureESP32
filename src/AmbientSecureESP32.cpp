#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_log.h>

#include <AmbientSecureESP32.h>

#define LOGE(...) ESP_LOGE(AMBIENT_SECURE_ESP_LOG_TAG, __VA_ARGS__)
#define LOGD(...) ESP_LOGD(AMBIENT_SECURE_ESP_LOG_TAG, __VA_ARGS__)

static const char *ambient_keys[AMBIENT_SECURE_NUM_PARAMS] = {
    "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "lat", "lng", "created"};

class ArrayBufferStream : public Stream {
public:
  ArrayBufferStream(uint8_t *buf, size_t max_len) {
    this->buf = this->p = buf;
    this->max_len = max_len;
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    if (size && buffer && (p + size < buf + max_len - 1)) {
      memcpy(p, buffer, size);
      p += size;
      return size;
    }
    return 0;
  }
  size_t write(uint8_t data) override {
    if (p + 1 < buf + max_len - 1) {
      *p = data;
      p += 1;
      return 1;
    }
    return 0;
  }

  int available() override { return (int)(p - buf); }
  int read() override {
    LOGE("Unspported: ArrayBufferStream::read()");
    return -1;
  }
  int peek() override {
    LOGE("Unspported: ArrayBufferStream::peek()");
    return -1;
  }
  void flush() override { /* do nothing */
  }

  uint8_t *buf;
  uint8_t *p;
  size_t max_len;
};

//! @brief Ambientの初期化
//!
//! @param[in] channelID    データを送受信するチャネルのチャネルID
//! @param[in] writeKey     ライトキー。読み込みのみの場合は nullptr 指定
//! @param[in] readKey      リードキー。書き込みのみの場合は nullptr 指定
//! @param[in] CAcert       証明書キー
//! @retval true  成功
//! @retval false 失敗
//! @attention Ambient本家ライブラリと引数非互換
bool AmbientSecure::begin(uint32_t channelId, const char *writeKey,
                          const char *readKey, const char *CAcert) {
  this->channelId = channelId;

  if (writeKey) {
    this->writeKey = writeKey;
  }
  if (readKey) {
    this->readKey = readKey;
  }
  this->CAcert = CAcert;

  this->clear_all();

  return true;
}

//! @brief 送信データの設定
//!
//! @param[in] field        データフィールド番号。1〜8の数値を指定
//! @param[in] data         送信するデータ
//! @retval true  成功
//! @retval false 失敗
bool AmbientSecure::set(uint8_t field, const char *data) {
  if (field < 1 || AMBIENT_SECURE_NUM_PARAMS < field) {
    LOGE("Out of bound of field number: %d", field);
    return false;
  }
  if (strnlen(data, AMBIENT_SECURE_DATA_SIZE + 1) > AMBIENT_SECURE_DATA_SIZE) {
    LOGE("Too long data for field number: %d", field);
    return false;
  }

  const auto field_num = field - 1;
  this->data[field_num].set = true;
  strncpy(this->data[field_num].item, data, AMBIENT_SECURE_DATA_SIZE);

  return true;
}

//! @brief コメント文字列の設定
//!
//! @param[in] cmnt         コメント文字列
//! @retval true  成功
//! @retval false 失敗
bool AmbientSecure::setcmnt(const char *cmnt) {
  if (strnlen(cmnt, AMBIENT_SECURE_CMNT_SIZE + 1) > AMBIENT_SECURE_DATA_SIZE) {
    LOGE("Too long comment");
    return false;
  }

  this->cmnt.set = true;
  strncpy(this->cmnt.item, cmnt, AMBIENT_SECURE_CMNT_SIZE);

  return true;
}

//! @brief 送信データの消去
//!
//! @param[in] field        データフィールド番号。1〜8の数値を指定
//! @retval true  成功
//! @retval false 失敗
bool AmbientSecure::clear(uint8_t field) {
  if (field < 1 || AMBIENT_SECURE_NUM_PARAMS < field) {
    LOGE("Out of bound of field number: %d", field);
    return false;
  }

  const auto field_num = field - 1;
  this->data[field_num].set = false;
  this->cmnt.set = false;

  return true;
}

//! @brief 送信データを全クリア
//!
//! @retval true  成功
bool AmbientSecure::clear_all() {
  for (auto i = 0; i < sizeof(this->data) / sizeof(this->data[0]); i++) {
    this->data[i].set = false;
  }
  this->cmnt.set = false;

  return true;
}

//! @brief begin()で指定したチャネルにset()で設定したデータを送信
//!
//! 送信後、set()で設定したデータは消去され、AmbientサーバーからのHTTPステータスコードがAmbient::statusにセットされる。
//!
//! @param[in]  tmout   サーバ接続のタイムアウト値(ms)
//! @retval true  成功
//! @retval false 失敗
bool AmbientSecure::send(uint32_t tmout) {
  char json_data[512];
  char url[128];

  const size_t json_data_size =
      this->get_data_as_json(json_data, sizeof(json_data));

  snprintf(url, sizeof(url), "https://ambidata.io/api/v2/channels/%d/data",
           this->channelId);

  return this->https_send_request(url, "POST", json_data, json_data_size,
                                  nullptr, tmout);
}

//! @brief begin()で指定したチャネルに複数件のデータをまとめて送信
//!
//! 送信後、set()で設定したデーターは消去され、AmbientサーバーからのHTTPステータスコードがAmbient::statusにセットされる。
//!
//! @param[in]  buf     送信データのポインタ。JSON文字列。
//! @param[in]  tmout   サーバ接続のタイムアウト値(ms)
//! @retval true  成功
//! @retval false 失敗
int32_t AmbientSecure::bulk_send(const char *buf, uint32_t tmout) {
  char url[128];
  snprintf(url, sizeof(url), "https://ambidata.io/api/v2/channels/%d/dataarray",
           this->channelId);

  return this->https_send_request(url, "POST", buf, strlen(buf), nullptr,
                                  tmout);
}

//! @brief begin()で指定したチャネルからデータを受信
//!
//! 受信後、AmbientサーバーからのHTTPステータスコードがAmbient::statusにセットされる。
//!
//! @param[out] buf     データが返されるバッファ
//! @param[in]  len     バッファのサイズ
//! @param[in]  n       受信する件数
//! @param[in]  tmout   サーバ接続のタイムアウト値(ms)
//! @retval true  成功
//! @retval false 失敗
bool AmbientSecure::read(char *buf, size_t len, size_t n, uint32_t tmout) {
  char url[128];

  snprintf(url, sizeof(url),
           "https://ambidata.io/api/v2/channels/%u/data?readKey=%s&n=%u",
           this->channelId, this->readKey.c_str(), n);

  ArrayBufferStream stream(reinterpret_cast<uint8_t *>(buf), len);
  return this->https_send_request(url, "GET", nullptr, 0, &stream, tmout);
}

//! @brief begin()で指定したチャネルからデータを受信
//!
//! 受信後、AmbientサーバーからのHTTPステータスコードがAmbient::statusにセットされる。
//!
//! @param[in]  n       受信する件数
//! @param[in]  tmout   サーバ接続のタイムアウト値(ms)
//! @return JSONメッセージ。エラー時は空文字列
//! @attention Ambient本家ライブラリと引数非互換
String AmbientSecure::read(size_t n, uint32_t tmout) {
  char url[128];

  snprintf(url, sizeof(url),
           "https://ambidata.io/api/v2/channels/%u/data?readKey=%s&n=%u",
           this->channelId, this->readKey.c_str(), n);

  return this->https_send_request(url, "GET", tmout);
}

//! @brief begin()で指定したチャネルに保存された全てのデーターを削除
//!
//! @param[in] userKey  ユーザキー
//! @param[in] tmout    サーバ接続のタイムアウト値(ms)
//! @retval true  成功
//! @retval false 失敗
//! @attention 削除したデーターは復元できない
bool AmbientSecure::delete_data(const char *userKey, uint32_t tmout) {
  char url[128];

  snprintf(url, sizeof(url),
           "https://ambidata.io/api/v2/channels/%u/data?userKey=%s",
           this->channelId, userKey);

  return this->https_send_request(url, "DELETE", nullptr, 0, nullptr, tmout);
}

//! @brief
//! 端末ごとの固有のキー（デバイスキー）を送り、対応するチャネルIDとライトキーを取得
//!
//! @param[in]  userKey   ユーザキー
//! @param[in]  devKey    デバイスキー
//! @param[out] channelID チャネルIDのアドレス
//! @param[out] writeKey  ライトキー
//! @param[out] readKey   リードキー
//! @param[in]  tmout     サーバ接続のタイムアウト値(ms)
//! @retval true  成功
//! @retval false 失敗
//! @attention Ambient本家ライブラリと引数非互換
bool AmbientSecure::getchannel(const char *userKey, const char *devKey,
                               uint32_t &channelId, char *writeKey,
                               char *readKey, uint32_t tmout) {
  char url[128];

  snprintf(url, sizeof(url),
           "https://ambidata.io/api/v2/channels/?userKey=%s&devKey=%s", userKey,
           devKey);

  const String response = this->https_send_request(url, "GET", tmout);
  if (response.isEmpty()) {
    return false;
  }

  StaticJsonDocument<48> filter;
  filter["ch"] = true;
  filter["readKey"] = true;
  filter["writeKey"] = true;
  StaticJsonDocument<96> doc;
  const auto error = deserializeJson(doc, response.c_str(), response.length(),
                                     DeserializationOption::Filter(filter));
  if (error) {
    LOGE("deserializeJson failed: %s", error.c_str());
    return false;
  }

  channelId = atoi(doc["ch"]);
  if (!writeKey) {
    strcpy(writeKey, doc["writeKey"]);
  }
  if (!readKey) {
    strcpy(readKey, doc["readKey"]);
  }

  return true;
}

bool AmbientSecure::https_send_request(const char *url, const char *method,
                                       const char *json_payload,
                                       size_t json_payload_size, Stream *stream,
                                       uint32_t tmout) {
  HTTPClient https;

  https.setTimeout(tmout);
  LOGD("Connect to %s %s", method, url);
  if (https.begin(url, this->CAcert)) {
    if (json_payload_size > 0) {
      https.addHeader(F("Content-Type"), "application/json");
      LOGD("Sending: %d bytes: %s", json_payload_size, json_payload);
    }
    const auto httpCode = https.sendRequest(
        method,
        const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(json_payload)),
        json_payload_size);
    this->status = httpCode;

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        LOGD("HTTPS response: %d", httpCode);
        return (stream && (https.writeToStream(stream) > 0)) ? true : false;
      }
    }
    LOGE("HTTPS response error: %d %s", httpCode,
         https.errorToString(httpCode).c_str());
  } else {
    LOGE("Connection failed to %s", url);
  }

  return false;
}

String AmbientSecure::https_send_request(const char *url, const char *method,
                                         uint32_t tmout) {
  HTTPClient https;

  https.setTimeout(tmout);
  LOGD("Connect to %s %s", method, url);
  if (https.begin(url, this->CAcert)) {
    const auto httpCode = https.sendRequest(method);
    this->status = httpCode;

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        const String response = https.getString();
        LOGD("HTTPS response: %d: %s", httpCode, response);
        return response;
      }
    }
    LOGE("HTTPS response error: %d %s", httpCode,
         https.errorToString(httpCode).c_str());
  } else {
    LOGE("Connection failed to %s", url);
  }

  return String("");
}

size_t AmbientSecure::get_data_as_json(char *buf, size_t output_size) {
  StaticJsonDocument<192> doc;
  doc["writeKey"] = this->writeKey;
  for (int i = 0; i < AMBIENT_SECURE_NUM_PARAMS; i++) {
    if (this->data[i].set) {
      doc[ambient_keys[i]] = this->data[i].item;
    }
  }
  if (this->cmnt.set) {
    doc["cmnt"] = this->cmnt.item;
  }
  return serializeJson(doc, buf, output_size);
}
