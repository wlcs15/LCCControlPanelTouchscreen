// Unwrap the house Wi-Fi PSK with AES-256-GCM.
// Key = HKDF-SHA256(IKM = flash_uid || MAC,
//                   info = OwlThree 05.01.01.01.A5 || MAC).
// Node ID is not in the wrap key so SD nodeid.txt can change without re-wrap.
// Host encrypts (utils/wifi_wrap.py); this file only decrypts live IDs.
// This is obfuscation bound to this module, not Flash Encryption.

#include "wifi_cred.h"

#include <string.h>

#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "mbedtls/gcm.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"

#ifndef WIFI_WRAP_BLOB_PRESENT
#define WIFI_WRAP_BLOB_PRESENT 0
#endif

#if WIFI_WRAP_BLOB_PRESENT
#include "wifi_psk_wrap.inc"
#endif

static const char *TAG = "wifi_cred";

static const char *kNvsNs = "owl3wifi";
static const char *kNvsBlob = "psk_gcm";
static const char *kNvsSsid = "ssid";

// OwlThree assigned prefix (not a 6-byte node ID by itself).
static const uint8_t kOwlThreePrefix[] = {0x05, 0x01, 0x01, 0x01, 0xA5};

// Distinct from the D1 R32 salt so a wrap from that board cannot be reused.
static const char *kHkdfSalt = "owlthree-ws43b-wifi-wrap-v2";

static uint64_t s_node_id;

struct wrap_blob
{
    uint8_t ver;
    uint8_t nonce[12];
    uint8_t tag[16];
    uint8_t clen;
    uint8_t cipher[64];
} __attribute__((packed));

void wifi_cred_set_node_id(uint64_t node_id)
{
    s_node_id = node_id;
}

uint64_t wifi_node_id(void)
{
    return s_node_id;
}

static void get_mac(uint8_t mac[6])
{
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK)
    {
        memset(mac, 0, 6);
    }
}

static void get_flash_uid(uint8_t uid[8], bool *ok)
{
    uint64_t id = 0;
    if (esp_flash_read_unique_chip_id(NULL, &id) != ESP_OK || id == 0)
    {
        memset(uid, 0, 8);
        if (ok)
        {
            *ok = false;
        }
        return;
    }
    for (int i = 7; i >= 0; --i)
    {
        uid[i] = (uint8_t)(id & 0xFF);
        id >>= 8;
    }
    if (ok)
    {
        *ok = true;
    }
}

void wifi_hw_ids_read(uint8_t mac[6], uint8_t flash_uid[8], bool *uid_ok)
{
    get_mac(mac);
    get_flash_uid(flash_uid, uid_ok);
}

// IKM = flash_uid || MAC (not the OpenLCB node ID)
static esp_err_t derive_wrap_key(uint8_t key[32])
{
    uint8_t mac[6];
    uint8_t uid[8];
    uint8_t ikm[14];
    uint8_t info[11];
    bool uid_ok = false;

    get_mac(mac);
    get_flash_uid(uid, &uid_ok);
    if (!uid_ok)
    {
        ESP_LOGW(TAG, "flash unique id unavailable; wrap key uses MAC only");
    }
    memcpy(ikm, uid, 8);
    memcpy(ikm + 8, mac, 6);
    memcpy(info, kOwlThreePrefix, 5);
    memcpy(info + 5, mac, 6);

    ESP_LOGI(TAG, "wrap bind MAC %02X:%02X:%02X:%02X:%02X:%02X (chip only, not node ID)",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    const int rc = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                                reinterpret_cast<const unsigned char *>(kHkdfSalt),
                                strlen(kHkdfSalt),
                                ikm, sizeof(ikm),
                                info, sizeof(info),
                                key, 32);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "HKDF failed %d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t gcm_decrypt(const uint8_t key[32], const wrap_blob *in, char *psk, size_t psk_len)
{
    if (in->ver != 1 || in->clen == 0 || in->clen >= psk_len || in->clen > sizeof(in->cipher))
    {
        return ESP_ERR_INVALID_SIZE;
    }
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (rc == 0)
    {
        rc = mbedtls_gcm_auth_decrypt(&gcm, in->clen,
                                      in->nonce, sizeof(in->nonce),
                                      nullptr, 0,
                                      in->tag, sizeof(in->tag),
                                      in->cipher,
                                      reinterpret_cast<unsigned char *>(psk));
    }
    mbedtls_gcm_free(&gcm);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "GCM unwrap failed (wrong chip or corrupt wrap)");
        return ESP_FAIL;
    }
    psk[in->clen] = '\0';
    return ESP_OK;
}

#if WIFI_WRAP_BLOB_PRESENT
static esp_err_t nvs_save(const char *ssid, const wrap_blob *blob)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(kNvsNs, NVS_READWRITE, &h);
    if (err != ESP_OK)
    {
        return err;
    }
    err = nvs_set_str(h, kNvsSsid, ssid);
    if (err == ESP_OK)
    {
        err = nvs_set_blob(h, kNvsBlob, blob, sizeof(*blob));
    }
    if (err == ESP_OK)
    {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}
#endif

static esp_err_t nvs_load(char *ssid, size_t ssid_len, wrap_blob *blob)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(kNvsNs, NVS_READONLY, &h);
    if (err != ESP_OK)
    {
        return err;
    }
    size_t slen = ssid_len;
    err = nvs_get_str(h, kNvsSsid, ssid, &slen);
    size_t blen = sizeof(*blob);
    if (err == ESP_OK)
    {
        err = nvs_get_blob(h, kNvsBlob, blob, &blen);
    }
    nvs_close(h);
    if (err == ESP_OK && blen != sizeof(*blob))
    {
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    return err;
}

esp_err_t wifi_cred_load(char *ssid, size_t ssid_len, char *psk, size_t psk_len)
{
    if (ssid == nullptr || psk == nullptr || ssid_len < 2 || psk_len < 2)
    {
        return ESP_ERR_INVALID_ARG;
    }
    ssid[0] = '\0';
    psk[0] = '\0';

    uint8_t key[32];
    esp_err_t err = derive_wrap_key(key);
    if (err != ESP_OK)
    {
        return err;
    }

    wrap_blob blob;

#if WIFI_WRAP_BLOB_PRESENT
    // Baked wrap wins over NVS so a new provision is not stuck behind
    // an old NVS blob (dummy Password! or v1 key). Wrap-free rebuilds
    // omit the blob and keep NVS.
    static_assert(sizeof(blob) == sizeof(kWifiWrapBlob), "wrap blob size mismatch");
    if (strlen(kWifiWrapSsid) >= ssid_len)
    {
        memset(key, 0, sizeof(key));
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(&blob, kWifiWrapBlob, sizeof(blob));
    strncpy(ssid, kWifiWrapSsid, ssid_len - 1);
    ssid[ssid_len - 1] = '\0';
    err = gcm_decrypt(key, &blob, psk, psk_len);
    if (err == ESP_OK)
    {
        wrap_blob nvs_blob;
        char nvs_ssid[33] = {0};
        const bool nvs_ok = (nvs_load(nvs_ssid, sizeof(nvs_ssid), &nvs_blob) == ESP_OK);
        if (!nvs_ok || memcmp(&nvs_blob, &blob, sizeof(blob)) != 0)
        {
            (void)nvs_save(ssid, &blob);
            ESP_LOGI(TAG, "PSK unwrapped from baked ciphertext and stored in NVS");
        }
        else
        {
            ESP_LOGI(TAG, "PSK unwrapped from baked ciphertext (NVS already matches)");
        }
        memset(key, 0, sizeof(key));
        return ESP_OK;
    }
    ESP_LOGW(TAG, "baked wrap unwrap failed; trying NVS");
#endif

    err = nvs_load(ssid, ssid_len, &blob);
    if (err == ESP_OK)
    {
        err = gcm_decrypt(key, &blob, psk, psk_len);
        memset(key, 0, sizeof(key));
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "PSK unwrapped from NVS (SSID present, password not logged)");
        }
        return err;
    }

    memset(key, 0, sizeof(key));
    ESP_LOGW(TAG, "No usable wrap. Collect IDs, then run utils/provision_wifi_build.sh in your terminal.");
    return ESP_ERR_NOT_FOUND;
}
