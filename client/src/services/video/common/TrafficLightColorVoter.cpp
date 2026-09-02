#include "TrafficLightColorVoter.h"

TrafficLightColorVoter::TrafficLightColorVoter(int bufferSize)
    : m_maxSize(bufferSize > 0 ? bufferSize : DEFAULT_BUFFER_SIZE)
{
}

QString TrafficLightColorVoter::vote(const QString &currentColor, bool tlDetected)
{
    if (currentColor == "red" || currentColor == "yellow" || currentColor == "green") {
        m_buffer.append(currentColor);
        while (m_buffer.size() > m_maxSize) {
            m_buffer.removeFirst();
        }
    } else if (!tlDetected) {
        // 完全没检出红绿灯 → 缓冲区逐帧衰减
        if (!m_buffer.isEmpty()) {
            m_buffer.removeFirst();
        }
    }
    // 检出但颜色 unknown → 不动缓冲区，让历史投票决定

    if (m_buffer.isEmpty()) return "unknown";

    // 统计多数
    int red = 0, yellow = 0, green = 0;
    for (const QString &c : m_buffer) {
        if (c == "red") red++;
        else if (c == "yellow") yellow++;
        else if (c == "green") green++;
    }
    if (red >= yellow && red >= green && red > 0) return "red";
    if (yellow >= green && yellow > 0) return "yellow";
    if (green > 0) return "green";
    return "unknown";
}

void TrafficLightColorVoter::reset()
{
    m_buffer.clear();
}
