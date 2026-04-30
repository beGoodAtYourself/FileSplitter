/**
 * @file SplitWorker.cpp
 * @brief 文件拆分工作线程实现
 *
 * 实现 QThread 子类，在独立线程中运行 FileSplitterCore。
 * 关键技术点：通过静态指针 s_pCurrentWorker 将 C 回调桥接为 Qt 信号。
 */

#include "SplitWorker.h"

// 初始化静态指针，当前无活动的工作线程
SplitWorker* SplitWorker::s_pCurrentWorker = NULL;

// ============================================================================
// 构造 / 析构
// ============================================================================

SplitWorker::SplitWorker(QObject *parent)
    : QThread(parent)
    , m_splitSize(100)
    , m_splitUnit(UNIT_MB)
    , m_cancelFlag(false)
{
}

SplitWorker::~SplitWorker()
{
}

// ============================================================================
// 参数设置
// ============================================================================

void SplitWorker::setSourceFilePath(const QString& path)
{
    m_sourceFilePath = path;
}

void SplitWorker::setSplitSize(int size, SplitUnit unit)
{
    m_splitSize = size;
    m_splitUnit = unit;
}

void SplitWorker::setOutputDir(const QString& dir)
{
    m_outputDir = dir;
}

/**
 * @brief 请求取消拆分
 *
 * 设置 m_cancelFlag 为 true。FileSplitterCore 在每次读写周期中
 * 都会检查此标志，发现为 true 后立即中止并返回 -100。
 */
void SplitWorker::cancel()
{
    m_cancelFlag = true;
}

// ============================================================================
// 线程主函数
// ============================================================================

/**
 * @brief 线程入口，执行文件拆分
 *
 * 流程：
 *   1. 重置取消标志
 *   2. 创建 FileSplitterCore 并配置参数
 *   3. 设置静态指针，注册回调，绑定取消标志
 *   4. 执行拆分
 *   5. 清除静态指针，发射完成信号
 *
 * 注意：toLocal8Bit() 用于将 QString 转换为本地编码的 QByteArray，
 * 确保包含中文的文件路径能正确传递给 C 标准库函数。
 */
void SplitWorker::run()
{
    // 重置取消标志（支持同一 Worker 对象的重复使用）
    m_cancelFlag = false;

    // 创建核心拆分对象并配置参数
    FileSplitterCore core;
    core.setSourceFilePath(m_sourceFilePath.toLocal8Bit().constData());
    core.setSplitSize(m_splitSize, m_splitUnit);

    if (!m_outputDir.isEmpty())
    {
        core.setOutputDir(m_outputDir.toLocal8Bit().constData());
    }

    // 设置静态指针，使静态回调函数能够访问当前 Worker 对象
    s_pCurrentWorker = this;

    // 注册回调和取消标志
    core.setProgressCallback(progressCallback);
    core.setLogCallback(logCallback);
    core.setCancelFlag(&m_cancelFlag);

    // 执行拆分（阻塞直到完成或取消）
    int ret = core.split();

    // 清除静态指针，防止悬空引用
    s_pCurrentWorker = NULL;

    // 通知 UI 拆分已完成
    emit splitFinished(ret);
}

// ============================================================================
// 静态回调函数 —— C 回调到 Qt 信号的桥接
// ============================================================================

/**
 * @brief 进度回调桥接
 *
 * FileSplitterCore 通过此 C 函数报告进度，
 * 函数内部通过 s_pCurrentWorker 将进度信息转发为 Qt 信号。
 *
 * 此函数在工作线程中执行，emit 的信号会被 Qt 自动
 * 通过队列连接（Queued Connection）传递到主线程的槽函数。
 */
void SplitWorker::progressCallback(int partIndex, long long writtenBytes, long long totalBytes, int totalParts)
{
    if (s_pCurrentWorker != NULL)
    {
        emit s_pCurrentWorker->progressChanged(partIndex, writtenBytes, totalBytes, totalParts);
    }
}

/**
 * @brief 日志回调桥接
 *
 * 将 C 字符串日志转换为 QString 后通过信号发送。
 * fromLocal8Bit() 确保中文日志信息正确解码。
 */
void SplitWorker::logCallback(const char* message)
{
    if (s_pCurrentWorker != NULL && message != NULL)
    {
        emit s_pCurrentWorker->logMessage(QString::fromLocal8Bit(message));
    }
}
