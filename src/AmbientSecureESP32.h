#ifndef AMBIENT_SECURE_ESP32_H
#define AMBIENT_SECURE_ESP32_H

#include <Arduino.h>
#include <cstdint>

#define AMBIENT_SECURE_WRITEKEY_SIZE 18
#define AMBIENT_SECURE_READKEY_SIZE 18
#define AMBIENT_SECURE_MAX_RETRY 5
#define AMBIENT_SECURE_DATA_SIZE 24
#define AMBIENT_SECURE_NUM_PARAMS 11
#define AMBIENT_SECURE_CMNT_SIZE 64
#define AMBIENT_SECURE_TIMEOUT 30000UL

#define AMBIENT_SECURE_ESP_LOG_TAG F("AmbientSecureESP32")

//! @brief ambidata.io用証明書。ISRG Root X1
// 有効期限 Mon, 04 Jun 2035 11:04:38 GMT
//
// @see https://letsencrypt.org/certs/isrgrootx1.pem.txt
#define AMBIENT_SECURE_CA_CERT                                                 \
  ("-----BEGIN CERTIFICATE-----\n"                                             \
   "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"        \
   "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"        \
   "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"        \
   "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"        \
   "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"        \
   "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"        \
   "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"        \
   "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"        \
   "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"        \
   "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"        \
   "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"        \
   "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"        \
   "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"        \
   "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"        \
   "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"        \
   "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"        \
   "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"        \
   "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"        \
   "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"        \
   "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"        \
   "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"        \
   "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"        \
   "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"        \
   "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"        \
   "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"        \
   "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"        \
   "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"        \
   "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"        \
   "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"        \
   "-----END CERTIFICATE-----\n")

//! @brief Ambientデータ
typedef struct AmbientSecureData_T {
  bool set;                            //!< データ有効フラグ
  char item[AMBIENT_SECURE_DATA_SIZE]; //!< データ値文字列
} AmbientSecureData;

//! @brief Ambientコメント
typedef struct AmbientSecureCmnt_T {
  bool set;                            //!< コメント有効フラグ
  char item[AMBIENT_SECURE_CMNT_SIZE]; //!< コメント文字列
} AmbientSecureCmnt;

//! @brief ESP32向けHTTPS対応番Ambientデータ送受信ライブラリ
class AmbientSecure {
public:
  AmbientSecure(){};
  bool begin(uint32_t channelId, const char *writeKey,
             const char *readKey = nullptr,
             const char *CAcert = AMBIENT_SECURE_CA_CERT);
  bool set(uint8_t field, const char *data);
  bool set(uint8_t field, double data) {
    return set(field, String(data).c_str());
  };
  bool set(uint8_t field, int data) {
    return set(field, String(data).c_str());
  };
  bool setcmnt(const char *cmnt);
  bool clear(uint8_t field);
  bool clear_all();
  bool send(uint32_t tmout = AMBIENT_SECURE_TIMEOUT);
  int32_t bulk_send(const char *buf, uint32_t tmout = AMBIENT_SECURE_TIMEOUT);
  bool read(char *buf, size_t len, size_t n = 1,
            uint32_t tmout = AMBIENT_SECURE_TIMEOUT);
  String read(size_t n = 1, uint32_t tmout = AMBIENT_SECURE_TIMEOUT);
  bool delete_data(const char *userKey,
                   uint32_t tmout = AMBIENT_SECURE_TIMEOUT);
  bool getchannel(const char *userKey, const char *devKey, uint32_t &channelId,
                  char *writeKey, char *readKey = nullptr,
                  uint32_t tmout = AMBIENT_SECURE_TIMEOUT);

  int status; //!< HTTPステータスコード

private:
  const char *CAcert; //!< 証明書
  uint32_t channelId; //!< チャネルID
  String writeKey;    //!< ライトキー
  String readKey;     //!< リードキー

  AmbientSecureData data[AMBIENT_SECURE_NUM_PARAMS]; //!< データ
  AmbientSecureCmnt cmnt;                            //!< コメント

  bool https_send_request(const char *url, const char *method,
                          const char *json_payload, size_t json_payload_size,
                          Stream *stream, uint32_t tmout);
  String https_send_request(const char *url, const char *method,
                            uint32_t tmout);

  size_t get_data_as_json(char *buf, size_t output_size);
};

#endif

#pragma once
