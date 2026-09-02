#ifndef TRAFFICLIGHTCOLORVOTER_H
#define TRAFFICLIGHTCOLORVOTER_H

#include <QString>
#include <QStringList>

// ============================================================================
// TrafficLightColorVoter - 红绿灯颜色时序投票器
//
// 维护最近 N 帧的颜色投票缓冲区，输出多数结果，避免单帧误判导致状态闪烁。
//
// 投票策略：
//   - 确认颜色(red/yellow/green) → 推入缓冲区，超出容量弹出最旧
//   - 检到但颜色 unknown        → 不投票也不衰减，让历史投票决定（保持状态稳定）
//   - 完全没检到               → 缓冲区逐帧衰减（弹出最旧），N 帧后回到 unknown
//
// 将投票状态从 DetectionEngine 中外提，使每个检测上下文（录制/回放）可拥有
// 独立投票器，且便于单测。
// ============================================================================
class TrafficLightColorVoter
{
public:
    /// @param bufferSize 投票窗口大小（帧数），<=0 时取默认值
    explicit TrafficLightColorVoter(int bufferSize = DEFAULT_BUFFER_SIZE);

    /**
     * @brief 推入本帧候选颜色并返回投票后的稳定状态
     * @param currentColor 本帧候选颜色 ("red"/"yellow"/"green"/"unknown"/"detected")
     * @param tlDetected   本帧是否检出红绿灯（无论颜色是否明确）
     * @return 投票后的稳定状态 ("red"/"yellow"/"green"/"unknown")
     */
    QString vote(const QString &currentColor, bool tlDetected);

    /// 当前缓冲区投票数（调试/诊断用）
    int currentSize() const { return m_buffer.size(); }

    /// 重置投票状态（切换视频/停止检测时调用）
    void reset();

    /// 默认投票窗口（约 0.23 秒 @30fps）
    static constexpr int DEFAULT_BUFFER_SIZE = 7;

private:
    int m_maxSize;
    QStringList m_buffer;
};

#endif // TRAFFICLIGHTCOLORVOTER_H
