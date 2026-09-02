#ifndef VIDEODAO_H
#define VIDEODAO_H

#include "MySqlDao.h"
#include <string>
#include <vector>

// 视频记录数据模型
struct VideoRecordModel
{
    int id = 0;
    std::string username;
    std::string video_name;
    int video_size = 0;         // MB
    std::string video_path;
    std::string record_type;    // 行车记录/倒车记录/鸟瞰记录/特征记录
    // 视频详情
    int duration_sec = 0;       // 时长（秒）
    std::string resolution;     // 分辨率 如 "1920x1080"
    int fps = 0;                // 帧率
    std::string camera_source;  // 摄像头来源
    // 播放记录
    int last_play_sec = 0;      // 上次播放到的秒数
    int play_count = 0;         // 播放次数
    std::string created_at;
};

/**
 * @brief 视频记录数据访问对象
 * 封装视频相关的数据库操作
 */
class VideoDao
{
public:
    explicit VideoDao(MySqlDao &db);

    // 上传视频记录（含视频详情）
    bool createVideoRecord(const std::string &username, const std::string &videoName,
                           int videoSize, const std::string &videoPath = "",
                           const std::string &recordType = "",
                           int durationSec = 0, const std::string &resolution = "",
                           int fps = 0, const std::string &cameraSource = "");

    // 获取视频列表
    std::vector<VideoRecordModel> getVideoList(const std::string &username = "",
                                                int page = 1, int pageSize = 20);

    // 获取视频记录总数（用于分页 total_count，与 getVideoList 同一过滤条件）
    int getVideoCount(const std::string &username = "");

    // 删除视频记录
    bool deleteVideo(int videoId, const std::string &username);

    // 获取用户视频总大小（MB）
    int64_t getTotalVideoSize(const std::string &username);

    // 删除最旧的视频（自动清理）
    bool deleteOldestVideo(const std::string &username);

    // 更新播放记录
    bool updatePlayRecord(int videoId, const std::string &username, int lastPlaySec);

private:
    MySqlDao &m_db;

    // 构建与 getVideoList 一致的 WHERE 子句（不含 "WHERE" 关键字；无过滤条件时返回空串）
    // 抽取为公共方法，确保 getVideoList 与 getVideoCount 使用同一过滤条件，避免漂移
    std::string buildWhereClause(const std::string &username);
};

#endif // VIDEODAO_H
