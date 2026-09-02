#include "VideoDao.h"
#include "ServerLogger.h"

VideoDao::VideoDao(MySqlDao &db)
    : m_db(db)
{
}

bool VideoDao::createVideoRecord(const std::string &username, const std::string &videoName,
                                  int videoSize, const std::string &videoPath,
                                  const std::string &recordType,
                                  int durationSec, const std::string &resolution,
                                  int fps, const std::string &cameraSource)
{
    if (!m_db.isConnected()) return false;

    // 字段统一转义后拼接，数值用 to_string，防 SQL 注入
    std::string sql = "INSERT INTO video_records (username, video_name, video_size, video_path, record_type, "
                      "duration_sec, resolution, fps, camera_source) VALUES ('"
        + m_db.escapeString(username) + "', '"
        + m_db.escapeString(videoName) + "', "
        + std::to_string(videoSize) + ", '"
        + m_db.escapeString(videoPath) + "', '"
        + m_db.escapeString(recordType) + "', "
        + std::to_string(durationSec) + ", '"
        + m_db.escapeString(resolution) + "', "
        + std::to_string(fps) + ", '"
        + m_db.escapeString(cameraSource) + "')";

    if (!m_db.executeUpdate(sql)) {
        LOG_ERROR("创建视频记录失败: %s", m_db.getLastError().c_str());
        return false;
    }

    return true;
}

std::vector<VideoRecordModel> VideoDao::getVideoList(const std::string &username,
                                                      int page, int pageSize)
{
    std::vector<VideoRecordModel> result;

    if (!m_db.isConnected()) return result;

    std::string sql = "SELECT id, username, video_name, video_size, video_path, record_type, "
                      "duration_sec, resolution, fps, camera_source, "
                      "last_play_sec, play_count, created_at FROM video_records";

    auto where = buildWhereClause(username);
    if (!where.empty()) {
        sql += " WHERE " + where;
    }

    sql += " ORDER BY created_at DESC";

    // 分页
    int offset = (page - 1) * pageSize;
    sql += " LIMIT " + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);

    auto rs = m_db.query(sql);
    while (rs.next()) {
        VideoRecordModel record;
        record.id = rs.getInt(0);
        record.username = rs.getString(1);
        record.video_name = rs.getString(2);
        record.video_size = rs.getInt(3);
        record.video_path = rs.getString(4);
        record.record_type = rs.getString(5);
        record.duration_sec = rs.getInt(6);
        record.resolution = rs.getString(7);
        record.fps = rs.getInt(8);
        record.camera_source = rs.getString(9);
        record.last_play_sec = rs.getInt(10);
        record.play_count = rs.getInt(11);
        record.created_at = rs.getString(12);
        result.push_back(record);
    }

    return result;
}

int VideoDao::getVideoCount(const std::string &username)
{
    if (!m_db.isConnected()) return 0;

    std::string sql = "SELECT COUNT(*) FROM video_records";
    auto where = buildWhereClause(username);
    if (!where.empty()) {
        sql += " WHERE " + where;
    }

    auto rs = m_db.query(sql);
    if (!rs.next()) return 0;
    return rs.getInt(0);
}

std::string VideoDao::buildWhereClause(const std::string &username)
{
    if (username.empty()) return "";
    return "username = '" + m_db.escapeString(username) + "'";
}

bool VideoDao::deleteVideo(int videoId, const std::string &username)
{
    if (!m_db.isConnected()) return false;

    std::string sql = "DELETE FROM video_records WHERE id = "
        + std::to_string(videoId) + " AND username = '" + m_db.escapeString(username) + "'";

    if (!m_db.executeUpdate(sql)) {
        LOG_ERROR("删除视频记录失败: %s", m_db.getLastError().c_str());
        return false;
    }

    return m_db.affectedRows() > 0;
}

int64_t VideoDao::getTotalVideoSize(const std::string &username)
{
    if (!m_db.isConnected()) return 0;

    std::string sql = "SELECT COALESCE(SUM(video_size), 0) FROM video_records WHERE username = '"
        + m_db.escapeString(username) + "'";

    auto rs = m_db.query(sql);
    if (!rs.next()) return 0;

    return rs.getInt64(0);
}

bool VideoDao::deleteOldestVideo(const std::string &username)
{
    if (!m_db.isConnected()) return false;

    auto escapedUsername = m_db.escapeString(username);
    std::string sql = "DELETE FROM video_records WHERE id = "
        "(SELECT id FROM (SELECT id FROM video_records WHERE username = '"
        + escapedUsername + "' ORDER BY created_at ASC LIMIT 1) AS tmp)";

    if (!m_db.executeUpdate(sql)) {
        LOG_ERROR("删除最旧视频失败: %s", m_db.getLastError().c_str());
        return false;
    }

    return m_db.affectedRows() > 0;
}

bool VideoDao::updatePlayRecord(int videoId, const std::string &username, int lastPlaySec)
{
    if (!m_db.isConnected()) return false;

    std::string sql = "UPDATE video_records SET last_play_sec = "
        + std::to_string(lastPlaySec) + ", play_count = play_count + 1 "
        + "WHERE id = " + std::to_string(videoId)
        + " AND username = '" + m_db.escapeString(username) + "'";

    if (!m_db.executeUpdate(sql)) {
        LOG_ERROR("更新播放记录失败: %s", m_db.getLastError().c_str());
        return false;
    }

    return m_db.affectedRows() > 0;
}
