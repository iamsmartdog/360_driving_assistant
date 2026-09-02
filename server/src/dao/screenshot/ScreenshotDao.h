#ifndef SCREENSHOTDAO_H
#define SCREENSHOTDAO_H

#include "MySqlDao.h"
#include <string>
#include <vector>

// 截图记录数据模型
struct ScreenshotRecordModel
{
    int id = 0;
    std::string username;
    std::string screenshot_name;
    int screenshot_size = 0;      // KB
    std::string screenshot_path;
    std::string record_type;      // 行车截图/播放截图/倒车截图
    std::string detection_info;   // 检测结果JSON
    std::string resolution;       // 分辨率
    std::string camera_source;    // 摄像头来源
    std::string created_at;
};

/**
 * @brief 截图记录数据访问对象
 * 封装截图相关的数据库操作
 */
class ScreenshotDao
{
public:
    explicit ScreenshotDao(MySqlDao &db);

    // 创建截图记录
    bool createScreenshotRecord(const std::string &username,
                                const std::string &screenshotName,
                                int screenshotSize,
                                const std::string &screenshotPath = "",
                                const std::string &recordType = "",
                                const std::string &detectionInfo = "",
                                const std::string &resolution = "",
                                const std::string &cameraSource = "");

    // 获取截图列表
    std::vector<ScreenshotRecordModel> getScreenshotList(const std::string &username = "",
                                                          const std::string &recordType = "",
                                                          int page = 1, int pageSize = 20);

    // 获取截图记录总数（用于分页 total_count，与 getScreenshotList 同一过滤条件）
    int getScreenshotCount(const std::string &username = "",
                           const std::string &recordType = "");

    // 删除截图记录
    bool deleteScreenshot(int screenshotId, const std::string &username);

private:
    MySqlDao &m_db;

    // 构建与 getScreenshotList 一致的 WHERE 子句（不含 "WHERE" 关键字；无过滤条件时返回空串）
    // 抽取为公共方法，确保 getScreenshotList 与 getScreenshotCount 使用同一过滤条件，避免漂移
    std::string buildWhereClause(const std::string &username, const std::string &recordType);
};

#endif // SCREENSHOTDAO_H
