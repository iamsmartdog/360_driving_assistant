#include "ScreenshotDao.h"
#include "ServerLogger.h"

ScreenshotDao::ScreenshotDao(MySqlDao &db)
    : m_db(db)
{
}

bool ScreenshotDao::createScreenshotRecord(const std::string &username,
                                            const std::string &screenshotName,
                                            int screenshotSize,
                                            const std::string &screenshotPath,
                                            const std::string &recordType,
                                            const std::string &detectionInfo,
                                            const std::string &resolution,
                                            const std::string &cameraSource)
{
    if (!m_db.isConnected()) return false;

    // 字段统一转义后拼接，数值用 to_string，防 SQL 注入
    std::string sql = "INSERT INTO screenshot_records (username, screenshot_name, screenshot_size, "
                      "screenshot_path, record_type, detection_info, resolution, camera_source) VALUES ('"
        + m_db.escapeString(username) + "', '"
        + m_db.escapeString(screenshotName) + "', "
        + std::to_string(screenshotSize) + ", '"
        + m_db.escapeString(screenshotPath) + "', '"
        + m_db.escapeString(recordType) + "', '"
        + m_db.escapeString(detectionInfo) + "', '"
        + m_db.escapeString(resolution) + "', '"
        + m_db.escapeString(cameraSource) + "')";

    if (!m_db.executeUpdate(sql)) {
        LOG_ERROR("创建截图记录失败: %s", m_db.getLastError().c_str());
        return false;
    }

    return true;
}

std::vector<ScreenshotRecordModel> ScreenshotDao::getScreenshotList(const std::string &username,
                                                                      const std::string &recordType,
                                                                      int page, int pageSize)
{
    std::vector<ScreenshotRecordModel> result;

    if (!m_db.isConnected()) return result;

    std::string sql = "SELECT id, username, screenshot_name, screenshot_size, screenshot_path, "
                      "record_type, detection_info, resolution, camera_source, created_at "
                      "FROM screenshot_records";

    auto whereClause = buildWhereClause(username, recordType);
    if (!whereClause.empty()) {
        sql += " WHERE " + whereClause;
    }

    sql += " ORDER BY created_at DESC";

    // 分页
    int offset = (page - 1) * pageSize;
    sql += " LIMIT " + std::to_string(pageSize) + " OFFSET " + std::to_string(offset);

    auto rs = m_db.query(sql);
    while (rs.next()) {
        ScreenshotRecordModel record;
        record.id = rs.getInt(0);
        record.username = rs.getString(1);
        record.screenshot_name = rs.getString(2);
        record.screenshot_size = rs.getInt(3);
        record.screenshot_path = rs.getString(4);
        record.record_type = rs.getString(5);
        record.detection_info = rs.getString(6);
        record.resolution = rs.getString(7);
        record.camera_source = rs.getString(8);
        record.created_at = rs.getString(9);
        result.push_back(record);
    }

    return result;
}

int ScreenshotDao::getScreenshotCount(const std::string &username,
                                       const std::string &recordType)
{
    if (!m_db.isConnected()) return 0;

    std::string sql = "SELECT COUNT(*) FROM screenshot_records";
    auto whereClause = buildWhereClause(username, recordType);
    if (!whereClause.empty()) {
        sql += " WHERE " + whereClause;
    }

    auto rs = m_db.query(sql);
    if (!rs.next()) return 0;
    return rs.getInt(0);
}

std::string ScreenshotDao::buildWhereClause(const std::string &username,
                                              const std::string &recordType)
{
    std::string whereClause;
    if (!username.empty()) {
        whereClause = "username = '" + m_db.escapeString(username) + "'";
    }
    if (!recordType.empty()) {
        if (!whereClause.empty()) whereClause += " AND ";
        whereClause += "record_type = '" + m_db.escapeString(recordType) + "'";
    }
    return whereClause;
}

bool ScreenshotDao::deleteScreenshot(int screenshotId, const std::string &username)
{
    if (!m_db.isConnected()) return false;

    std::string sql = "DELETE FROM screenshot_records WHERE id = "
        + std::to_string(screenshotId) + " AND username = '" + m_db.escapeString(username) + "'";

    if (!m_db.executeUpdate(sql)) {
        LOG_ERROR("删除截图记录失败: %s", m_db.getLastError().c_str());
        return false;
    }

    return m_db.affectedRows() > 0;
}
