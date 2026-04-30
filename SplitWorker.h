/**
 * @file SplitWorker.h
 * @brief 文件拆分工作线程声明
 *
 * SplitWorker 继承 QThread，在独立线程中执行文件拆分操作，
 * 避免阻塞 UI 主线程。
 *
 * 核心设计——回调到信号的桥接模式：
 *   FileSplitterCore 使用 C 风格函数指针回调（ProgressCallback/LogCallback），
 *   而 Qt UI 需要通过信号槽通信。SplitWorker 通过静态指针 s_pCurrentWorker
 *   将 C 回调转发为 Qt 信号，实现核心逻辑与 UI 的解耦。
 *
 * 线程安全说明：
 *   - s_pCurrentWorker 仅在 run() 执行期间有效，run() 结束后置 NULL
 *   - m_cancelFlag 为简单 bool 类型，单写（UI线程）单读（工作线程）无需加锁
 *   - 信号 emit 在工作线程中触发，Qt 自动队列化到主线程执行槽函数
 */

#ifndef SPLITWORKER_H
#define SPLITWORKER_H

#include <QThread>
#include "FileSplitterCore.h"

class SplitWorker : public QThread
{
    Q_OBJECT

public:
    explicit SplitWorker(QObject *parent = 0);
    ~SplitWorker();

    /**
     * @brief 设置源文件路径
     * @param path 源文件完整路径
     */
    void setSourceFilePath(const QString& path);

    /**
     * @brief 设置拆分大小和单位
     * @param size 拆分大小数值
     * @param unit 拆分单位（KB/MB/GB）
     */
    void setSplitSize(int size, SplitUnit unit);

    /**
     * @brief 设置输出目录
     * @param dir 输出目录路径
     */
    void setOutputDir(const QString& dir);

    /**
     * @brief 请求取消拆分操作
     *
     * 设置内部取消标志为 true，拆分循环将在下次检查时中止。
     * 调用后仍需等待线程自然结束（通过 wait() 或 splitFinished 信号）。
     */
    void cancel();

signals:
    /**
     * @brief 进度变化信号
     * @param partIndex    当前分块序号（0-based）
     * @param writtenBytes 已写入总字节数
     * @param totalBytes   文件总字节数
     * @param totalParts   预计总分块数
     */
    void progressChanged(int partIndex, long long writtenBytes, long long totalBytes, int totalParts);

    /**
     * @brief 日志消息信号
     * @param message 日志文本
     */
    void logMessage(const QString& message);

    /**
     * @brief 拆分完成信号
     * @param retCode 返回码，0 成功，负数失败，-100 用户取消
     */
    void splitFinished(int retCode);

protected:
    /**
     * @brief 线程入口函数
     *
     * 创建 FileSplitterCore 实例，配置参数，注册回调，执行拆分。
     * 拆分完成后发射 splitFinished 信号。
     */
    void run();

private:
    QString   m_sourceFilePath;  ///< 源文件路径
    int       m_splitSize;       ///< 拆分大小数值
    SplitUnit m_splitUnit;       ///< 拆分单位
    QString   m_outputDir;       ///< 输出目录路径
    bool      m_cancelFlag;      ///< 取消标志（UI 线程写，工作线程读）

    /**
     * @brief 当前工作线程实例的静态指针
     *
     * 用于在 C 风格的静态回调函数中访问 Qt 对象，
     * 将回调转发为 Qt 信号。仅在 run() 执行期间非 NULL。
     */
    static SplitWorker* s_pCurrentWorker;

    /**
     * @brief 进度回调（C 风格静态函数）
     *
     * 由 FileSplitterCore 在拆分过程中调用，
     * 通过 s_pCurrentWorker 将进度信息转发为 progressChanged 信号。
     */
    static void progressCallback(int partIndex, long long writtenBytes, long long totalBytes, int totalParts);

    /**
     * @brief 日志回调（C 风格静态函数）
     *
     * 由 FileSplitterCore 调用，通过 s_pCurrentWorker
     * 将日志信息转发为 logMessage 信号。
     */
    static void logCallback(const char* message);
};

#endif // SPLITWORKER_H
