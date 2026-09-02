#ifndef PASSWORDHASHER_H
#define PASSWORDHASHER_H

#include <string>

/**
 * @brief 密码哈希工具（服务端专用）
 *
 * 使用 PBKDF2-HMAC-SHA256 加盐哈希，取代原先的裸 MD5。
 *
 * 存储格式：pbkdf2$<iterations>$<salt_hex>$<hash_hex>
 *   - salt：16 字节随机盐
 *   - iterations：PBKDF2 迭代次数（默认 100000）
 *   - hash：32 字节（256 bit）派生密钥
 *
 * 向后兼容：verify() 自动识别旧版 MD5 哈希（32 位十六进制，无 $ 分隔符），
 *           验证通过后调用 needsUpgrade() 提示调用方升级存储格式。
 */
class PasswordHasher
{
public:
    /// 对明文密码进行加盐哈希，返回 "pbkdf2$iter$salt$hash" 格式字符串
    static std::string hash(const std::string &password);

    /**
     * @brief 验证明文密码是否与存储的哈希匹配
     * @param password 用户输入的明文密码
     * @param stored   数据库中存储的哈希（PBKDF2 格式或旧版 MD5）
     * @return 匹配返回 true
     *
     * 若 stored 为旧版 MD5（32 位十六进制），会对 password 求 MD5 后比较，
     * 调用方可随后调用 needsUpgrade() 判断是否需要升级存储格式。
     */
    static bool verify(const std::string &password, const std::string &stored);

    /// 判断存储的哈希是否为旧版格式（需升级）
    static bool needsUpgrade(const std::string &stored);

private:
    static constexpr int SALT_LEN = 16;
    static constexpr int KEY_LEN = 32;        // 256 bit
    static constexpr int ITERATIONS = 100000;

    static std::string toHex(const unsigned char *data, int len);
    static std::string md5Hex(const std::string &input);  // 旧版兼容
};

#endif // PASSWORDHASHER_H
