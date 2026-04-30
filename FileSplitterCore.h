/**
 * @file FileSplitterCore.h
 * @brief 文件拆分核心逻辑声明
 *
 * FileSplitterCore 封装了文件拆分的核心算法，与 UI 完全解耦。
 * 通过回调机制（进度回调、日志回调）将内部状态传递给外部，
 * 支持取消操作（通过外部 bool 标志），适用于多线程场景。
 *
 * 核心特性：
 *   - 64位文件操作（_fseeki64/_ftelli64），支持 >2GB 文件
 *   - 4MB 缓冲区读写，平衡内存占用与 I/O 效率
 *   - 支持 KB/MB/GB 三种拆分单位
 *   - 输出文件命名格式：001_filename.ext
 *   - 支持自定义输出目录
 */

#ifndef FILESPLITTERCORE_H
#define FILESPLITTERCORE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 缓冲区大小（4MB）——兼顾内存占用与磁盘 I/O 吞吐量
#define FS_BUFFER_SIZE (4 * 1024 * 1024)

/**
 * @brief 拆分大小单位枚举
 *
 * 用于指定拆分粒度，对应不同的字节数倍率。
 */
enum SplitUnit
{
    UNIT_KB = 0,    ///< 千字节 (x 1024)
    UNIT_MB = 1,    ///< 兆字节 (x 1024^2)
    UNIT_GB = 2     ///< 吉字节 (x 1024^3)
};

/**
 * @brief 进度回调函数类型
 *
 * 在拆分过程中周期性调用，通知外部当前进度。
 * 回调发生在拆分线程中，外部需注意线程安全。
 *
 * @param partIndex    当前分块序号（0-based）
 * @param writtenBytes 截至当前已写入的总字节数
 * @param totalBytes   源文件总字节数
 * @param totalParts   预计总分块数
 */
typedef void (*ProgressCallback)(int partIndex, long long writtenBytes, long long totalBytes, int totalParts);

/**
 * @brief 日志回调函数类型
 *
 * 用于将拆分过程中的日志信息传递给外部，支持错误、信息等不同级别。
 *
 * @param message 日志文本，以 [ERROR] 或 [INFO] 前缀标识级别
 */
typedef void (*LogCallback)(const char* message);

/**
 * @brief 文件拆分核心类
 *
 * 纯逻辑层，不依赖任何 UI 框架。通过 setter 方法配置参数，
 * 通过回调接口输出进度和日志，适合在 QThread 或其他线程中运行。
 *
 * 典型使用流程：
 *   1. setSourceFilePath()  设置源文件路径
 *   2. setSplitSize()       设置拆分大小和单位
 *   3. setOutputDir()       设置输出目录（可选）
 *   4. setProgressCallback() / setLogCallback()  注册回调（可选）
 *   5. setCancelFlag()      绑定取消标志（可选）
 *   6. split()              执行拆分
 */
class FileSplitterCore
{
public:
    FileSplitterCore();
    ~FileSplitterCore();

    /**
     * @brief 设置源文件路径
     * @param path 源文件的完整路径
     */
    void setSourceFilePath(const char* path);

    /**
     * @brief 设置拆分大小和单位
     * @param size 拆分大小数值（必须 > 0）
     * @param unit 拆分单位（KB/MB/GB）
     */
    void setSplitSize(int size, SplitUnit unit);

    /**
     * @brief 设置输出目录
     * @param dir 输出目录路径，为空时输出到当前工作目录
     */
    void setOutputDir(const char* dir);

    /**
     * @brief 注册进度回调
     * @param cb 进度回调函数指针，NULL 则不回调
     */
    void setProgressCallback(ProgressCallback cb);

    /**
     * @brief 注册日志回调
     * @param cb 日志回调函数指针，NULL 则不回调
     */
    void setLogCallback(LogCallback cb);

    /**
     * @brief 绑定取消标志
     *
     * 拆分过程中会周期性检查该标志，若为 true 则中止操作。
     * 标志的生命周期必须覆盖整个 split() 调用。
     *
     * @param pCancel 指向外部的 bool 标志，NULL 则不检查取消
     */
    void setCancelFlag(const bool* pCancel);

    /**
     * @brief 获取源文件大小
     * @return 文件字节数；负数表示错误（-1:路径空, -2:打开失败, -3:定位失败）
     */
    long long getSourceFileSize() const;

    /**
     * @brief 获取预计分块数
     * @return 预计分块数；0 表示参数无效或文件不可访问
     */
    int getEstimatedParts() const;

    /**
     * @brief 执行文件拆分
     *
     * 按设定的拆分大小将源文件切割为多个小文件。
     * 输出文件命名格式：001_basename.ext, 002_basename.ext, ...
     * 可通过进度回调和日志回调获取实时状态。
     *
     * @return 0 成功；负数为错误码：
     *         -1   参数无效
     *         -2   源文件打开失败
     *         -3   文件定位失败（SEEK_END）
     *         -4   文件大小无效
     *         -5   文件定位失败（SEEK_SET）
     *         -6   缓冲区分配失败
     *         -7   分块文件创建失败
     *         -8   源文件读取失败
     *         -9   分块文件写入失败
     *         -100 用户取消
     */
    int split();

private:
    char   m_sourceFilePath[512];   ///< 源文件完整路径
    char   m_outputDir[512];        ///< 输出目录路径，空则使用当前目录
    int    m_splitSize;             ///< 拆分大小数值
    SplitUnit m_splitUnit;          ///< 拆分单位
    ProgressCallback m_progressCb;  ///< 进度回调函数指针
    LogCallback     m_logCb;        ///< 日志回调函数指针
    const bool*     m_pCancel;      ///< 取消标志指针（外部管理生命周期）

    /**
     * @brief 计算拆分大小（字节）
     * @return 根据 m_splitSize 和 m_splitUnit 换算后的字节数
     */
    long long calcSplitSizeBytes() const;

    /**
     * @brief 输出日志信息
     * @param msg 日志文本
     */
    void log(const char* msg) const;
};

#endif // FILESPLITTERCORE_H
