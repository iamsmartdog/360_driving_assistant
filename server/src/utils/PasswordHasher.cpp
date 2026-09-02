#include "PasswordHasher.h"
#include "ServerLogger.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/md5.h>
#include <cstdio>
#include <cstring>

// ============================================================================
// 工具函数
// ============================================================================

std::string PasswordHasher::toHex(const unsigned char *data, int len)
{
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (int i = 0; i < len; ++i) {
        out.push_back(hex[data[i] >> 4]);
        out.push_back(hex[data[i] & 0x0f]);
    }
    return out;
}

std::string PasswordHasher::md5Hex(const std::string &input)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char *>(input.data()),
        input.size(), digest);
    return toHex(digest, MD5_DIGEST_LENGTH);
}

// ============================================================================
// 公开接口
// ============================================================================

std::string PasswordHasher::hash(const std::string &password)
{
    unsigned char salt[SALT_LEN];
    if (RAND_bytes(salt, SALT_LEN) != 1) {
        // RAND_bytes 失败极少见，回退到 /dev/urandom 不现实（OpenSSL 已封装）
        LOG_ERROR("PasswordHasher: RAND_bytes 失败");
        return "";
    }

    unsigned char key[KEY_LEN];
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                          salt, SALT_LEN, ITERATIONS,
                          EVP_sha256(), KEY_LEN, key) != 1) {
        LOG_ERROR("PasswordHasher: PKCS5_PBKDF2_HMAC 失败");
        return "";
    }

    // pbkdf2$<iterations>$<salt_hex>$<hash_hex>
    return "pbkdf2$" + std::to_string(ITERATIONS) + "$"
         + toHex(salt, SALT_LEN) + "$"
         + toHex(key, KEY_LEN);
}

bool PasswordHasher::verify(const std::string &password, const std::string &stored)
{
    // ---- 旧版 MD5 兼容（32 位十六进制，无 $ 分隔符）----
    if (stored.size() == 32 && stored.find('$') == std::string::npos) {
        return md5Hex(password) == stored;
    }

    // ---- PBKDF2 格式：pbkdf2$<iter>$<salt_hex>$<hash_hex> ----
    if (stored.substr(0, 7) != "pbkdf2$") {
        return false;  // 未知格式
    }

    // 解析各段
    size_t pos = 7;
    size_t next;

    // iterations
    next = stored.find('$', pos);
    if (next == std::string::npos) return false;
    int iterations = std::atoi(stored.substr(pos, next - pos).c_str());
    pos = next + 1;

    // salt_hex
    next = stored.find('$', pos);
    if (next == std::string::npos) return false;
    std::string saltHex = stored.substr(pos, next - pos);
    pos = next + 1;

    // hash_hex（剩余部分）
    std::string hashHex = stored.substr(pos);
    if (hashHex.empty() || saltHex.empty()) return false;

    // 十六进制 → 二进制
    auto fromHex = [](const std::string &hex) -> std::string {
        std::string out;
        out.reserve(hex.size() / 2);
        for (size_t i = 0; i + 1 < hex.size(); i += 2) {
            unsigned int byte;
            sscanf(hex.c_str() + i, "%02x", &byte);
            out.push_back(static_cast<char>(byte));
        }
        return out;
    };

    std::string salt = fromHex(saltHex);
    std::string expectedHash = fromHex(hashHex);

    // 用相同参数重新派生
    unsigned char key[KEY_LEN];
    if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                          reinterpret_cast<const unsigned char *>(salt.data()),
                          static_cast<int>(salt.size()), iterations,
                          EVP_sha256(), KEY_LEN, key) != 1) {
        return false;
    }

    // 常量时间比较，防时序攻击
    unsigned char diff = 0;
    for (int i = 0; i < KEY_LEN; ++i) {
        diff |= key[i] ^ static_cast<unsigned char>(expectedHash[i]);
    }
    return diff == 0;
}

bool PasswordHasher::needsUpgrade(const std::string &stored)
{
    // 非 "pbkdf2$" 开头即为旧格式
    return stored.substr(0, 7) != "pbkdf2$";
}
