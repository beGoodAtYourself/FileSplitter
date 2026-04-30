/**
 * @file FileSplitterCore.cpp
 * @brief 文件拆分核心逻辑实现
 *
 * 实现文件拆分的完整算法：打开源文件 -> 计算分块 -> 循环读写 -> 关闭文件。
 * 使用 4MB 缓冲区逐块读写，支持 64 位文件操作处理大文件。
 * 通过回调机制与外部通信，通过取消标志支持中断操作。
 */

#include "FileSplitterCore.h"

// ============================================================================
// 构造 / 析构
// ============================================================================

FileSplitterCore::FileSplitterCore()
    : m_splitSize(100)          // 默认拆分大小 100
    , m_splitUnit(UNIT_MB)      // 默认单位 MB
    , m_progressCb(NULL)
    , m_logCb(NULL)
    , m_pCancel(NULL)
{
    memset(m_sourceFilePath, 0, sizeof(m_sourceFilePath));
    memset(m_outputDir, 0, sizeof(m_outputDir));
}

FileSplitterCore::~FileSplitterCore()
{
}

// ============================================================================
// Setter 方法
// ============================================================================

void FileSplitterCore::setSourceFilePath(const char* path)
{
    if (path != NULL)
    {
        // 安全拷贝，确保以 '\0' 结尾
        strncpy(m_sourceFilePath, path, sizeof(m_sourceFilePath) - 1);
        m_sourceFilePath[sizeof(m_sourceFilePath) - 1] = '\0';
    }
}

void FileSplitterCore::setSplitSize(int size, SplitUnit unit)
{
    m_splitSize = size;
    m_splitUnit = unit;
}

void FileSplitterCore::setOutputDir(const char* dir)
{
    if (dir != NULL)
    {
        strncpy(m_outputDir, dir, sizeof(m_outputDir) - 1);
        m_outputDir[sizeof(m_outputDir) - 1] = '\0';
    }
}

void FileSplitterCore::setProgressCallback(ProgressCallback cb)
{
    m_progressCb = cb;
}

void FileSplitterCore::setLogCallback(LogCallback cb)
{
    m_logCb = cb;
}

void FileSplitterCore::setCancelFlag(const bool* pCancel)
{
    m_pCancel = pCancel;
}

// ============================================================================
// 查询方法
// ============================================================================

/**
 * @brief 获取源文件大小
 *
 * 独立打开文件读取大小，不依赖 split() 中已打开的文件句柄。
 * 用于在拆分前预览文件信息和预估分块数。
 */
long long FileSplitterCore::getSourceFileSize() const
{
    // 源文件路径未设置
    if (m_sourceFilePath[0] == '\0')
    {
        return -1;
    }

    // 以二进制只读方式打开
    FILE* fp = fopen(m_sourceFilePath, "rb");
    if (fp == NULL)
    {
        return -2;
    }

    // 使用 64 位定位获取文件末尾位置
    if (_fseeki64(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return -3;
    }

    long long fileSize = _ftelli64(fp);
    fclose(fp);

    return fileSize;
}

/**
 * @brief 获取预计分块数
 *
 * 根据源文件大小和拆分大小计算，向上取整。
 * 用于 UI 预览，不保证与实际分块数完全一致（文件可能被修改）。
 */
int FileSplitterCore::getEstimatedParts() const
{
    long long fileSize = getSourceFileSize();
    if (fileSize <= 0)
    {
        return 0;
    }

    long long splitBytes = calcSplitSizeBytes();
    if (splitBytes <= 0)
    {
        return 0;
    }

    // 向上取整：(fileSize + splitBytes - 1) / splitBytes
    return (int)((fileSize + splitBytes - 1) / splitBytes);
}

// ============================================================================
// 私有辅助方法
// ============================================================================

/**
 * @brief 将拆分大小换算为字节数
 *
 * 根据 SplitUnit 枚举值，将 m_splitSize 乘以对应的倍率。
 * 默认按 MB 计算。
 */
long long FileSplitterCore::calcSplitSizeBytes() const
{
    long long base = m_splitSize;
    switch (m_splitUnit)
    {
    case UNIT_KB:
        return base * 1024;
    case UNIT_MB:
        return base * 1024 * 1024;
    case UNIT_GB:
        return base * 1024 * 1024 * 1024;
    default:
        return base * 1024 * 1024;  // 未知单位默认按 MB
    }
}

/**
 * @brief 通过日志回调输出日志
 * @param msg 日志文本
 */
void FileSplitterCore::log(const char* msg) const
{
    if (m_logCb != NULL && msg != NULL)
    {
        m_logCb(msg);
    }
}

// ============================================================================
// 核心拆分方法
// ============================================================================

/**
 * @brief 执行文件拆分
 *
 * 算法流程：
 *   1. 参数校验
 *   2. 打开源文件，获取文件大小
 *   3. 分配 4MB I/O 缓冲区
 *   4. 循环处理每个分块：
 *      a. 检查取消标志
 *      b. 生成输出文件路径（001_basename.ext 格式）
 *      c. 创建分块文件
 *      d. 逐缓冲区读写数据
 *      e. 触发进度回调
 *   5. 释放资源
 *
 * 资源管理采用 goto cleanup 模式，确保在任何错误路径上
 * 都能正确释放已分配的缓冲区和已打开的文件句柄。
 */
int FileSplitterCore::split()
{
    FILE* srcFp = NULL;     // 源文件句柄
    FILE* partFp = NULL;    // 当前分块文件句柄
    char* buffer = NULL;    // I/O 缓冲区
    char partFilePath[512]; // 分块文件路径
    int partIndex = 0;      // 当前分块序号（0-based）
    int ret = 0;            // 返回值，0 表示成功

    // ---- 参数检查 ----
    if (m_sourceFilePath[0] == '\0' || m_splitSize <= 0)
    {
        log("[ERROR] Invalid parameters");
        return -1;
    }

    // 计算拆分大小（字节）
    long long splitSizeBytes = calcSplitSizeBytes();
    if (splitSizeBytes <= 0)
    {
        log("[ERROR] Invalid split size");
        return -1;
    }

    // ---- 打开源文件 ----
    srcFp = fopen(m_sourceFilePath, "rb");
    if (srcFp == NULL)
    {
        log("[ERROR] Failed to open source file");
        return -2;
    }

    // ---- 获取文件大小（64位操作） ----
    if (_fseeki64(srcFp, 0, SEEK_END) != 0)
    {
        log("[ERROR] Failed to seek to end of file");
        ret = -3;
        goto cleanup;
    }

    long long fileSize = _ftelli64(srcFp);
    if (fileSize <= 0)
    {
        log("[ERROR] Invalid file size");
        ret = -4;
        goto cleanup;
    }

    // ---- 重置文件指针到开头，准备读取 ----
    if (_fseeki64(srcFp, 0, SEEK_SET) != 0)
    {
        log("[ERROR] Failed to seek to start of file");
        ret = -5;
        goto cleanup;
    }

    // ---- 分配 I/O 缓冲区（4MB） ----
    buffer = (char*)malloc(FS_BUFFER_SIZE);
    if (buffer == NULL)
    {
        log("[ERROR] Failed to allocate buffer");
        ret = -6;
        goto cleanup;
    }

    // ---- 计算总分块数（向上取整） ----
    int totalParts = (int)((fileSize + splitSizeBytes - 1) / splitSizeBytes);

    // ---- 输出文件概要信息 ----
    {
        char infoMsg[512];
        const char* unitStr = (m_splitUnit == UNIT_KB) ? "KB" : (m_splitUnit == UNIT_MB) ? "MB" : "GB";
        sprintf(infoMsg, "Source file: %s", m_sourceFilePath);
        log(infoMsg);
        sprintf(infoMsg, "File size: %.2f MB (%lld bytes)", (double)fileSize / 1024 / 1024, fileSize);
        log(infoMsg);
        sprintf(infoMsg, "Split size: %d %s", m_splitSize, unitStr);
        log(infoMsg);
        sprintf(infoMsg, "Total parts: %d", totalParts);
        log(infoMsg);
    }

    // ---- 提取文件名（去除路径前缀） ----
    // 同时支持 Windows 反斜杠和 Unix 正斜杠路径分隔符
    const char* pFileName = strrchr(m_sourceFilePath, '\\');
    if (pFileName == NULL)
    {
        pFileName = strrchr(m_sourceFilePath, '/');
    }
    if (pFileName == NULL)
    {
        pFileName = m_sourceFilePath;  // 无路径前缀，整体即为文件名
    }
    else
    {
        pFileName++;  // 跳过路径分隔符
    }

    // ---- 提取扩展名 ----
    // 在文件名部分查找最后一个点号，分离基本名和扩展名
    const char* pExt = strrchr(pFileName, '.');
    char baseName[256];  // 不含扩展名的基本名
    char extName[64];    // 包含点号的扩展名（如 ".bin"）

    if (pExt != NULL)
    {
        // 有扩展名：分离基本名和扩展名
        int nameLen = (int)(pExt - pFileName);
        if (nameLen > 0 && nameLen < 256)
        {
            strncpy(baseName, pFileName, nameLen);
            baseName[nameLen] = '\0';
        }
        else
        {
            // 基本名过长，截断处理
            strncpy(baseName, pFileName, 255);
            baseName[255] = '\0';
        }
        strncpy(extName, pExt, sizeof(extName) - 1);
        extName[sizeof(extName) - 1] = '\0';
    }
    else
    {
        // 无扩展名
        strncpy(baseName, pFileName, sizeof(baseName) - 1);
        baseName[sizeof(baseName) - 1] = '\0';
        extName[0] = '\0';
    }

    // ---- 确定输出目录 ----
    // 如果未设置输出目录，则使用当前目录 "."
    const char* outputDir = m_outputDir[0] != '\0' ? m_outputDir : NULL;

    // ---- 主拆分循环 ----
    long long remainingBytes = fileSize;  // 剩余未处理的字节数
    long long totalWritten = 0;           // 已写入的总字节数

    while (remainingBytes > 0)
    {
        // 检查取消标志（在每个分块开始前检查）
        if (m_pCancel != NULL && *m_pCancel)
        {
            log("[INFO] Split cancelled by user");
            ret = -100;
            goto cleanup;
        }

        // ---- 生成分块文件路径 ----
        // 格式：<输出目录>\<序号>_<基本名><扩展名>
        // 例如：D:\Output\001_data.bin, D:\Output\002_data.bin
        if (outputDir != NULL)
        {
            if (extName[0] != '\0')
            {
                sprintf(partFilePath, "%s\\%03d_%s%s", outputDir, partIndex + 1, baseName, extName);
            }
            else
            {
                sprintf(partFilePath, "%s\\%03d_%s", outputDir, partIndex + 1, baseName);
            }
        }
        else
        {
            // 未指定输出目录时使用当前目录
            if (extName[0] != '\0')
            {
                sprintf(partFilePath, "%s\\%03d_%s%s", ".", partIndex + 1, baseName, extName);
            }
            else
            {
                sprintf(partFilePath, "%s\\%03d_%s", ".", partIndex + 1, baseName);
            }
        }

        // ---- 打开分块文件 ----
        partFp = fopen(partFilePath, "wb");
        if (partFp == NULL)
        {
            char errMsg[512];
            sprintf(errMsg, "[ERROR] Failed to create part file: %s", partFilePath);
            log(errMsg);
            ret = -7;
            goto cleanup;
        }

        // 计算当前分块的字节数（最后一个分块可能不足一个完整大小）
        long long partSize = (remainingBytes > splitSizeBytes) ? splitSizeBytes : remainingBytes;
        long long partRemaining = partSize;  // 当前分块剩余待写入字节数

        // 输出当前分块信息
        {
            char partMsg[512];
            sprintf(partMsg, "Creating part %03d: %s (%.2f MB)", partIndex + 1, partFilePath, (double)partSize / 1024 / 1024);
            log(partMsg);
        }

        // ---- 内层循环：逐缓冲区写入当前分块 ----
        while (partRemaining > 0)
        {
            // 检查取消标志（每个缓冲区读写周期检查一次）
            if (m_pCancel != NULL && *m_pCancel)
            {
                log("[INFO] Split cancelled by user");
                ret = -100;
                goto cleanup;
            }

            // 计算本次读取的字节数（不超过缓冲区大小）
            long long bytesToRead = (partRemaining > FS_BUFFER_SIZE) ? FS_BUFFER_SIZE : partRemaining;

            // 从源文件读取数据
            size_t bytesRead = fread(buffer, 1, (size_t)bytesToRead, srcFp);
            if (bytesRead <= 0)
            {
                log("[ERROR] Failed to read from source file");
                ret = -8;
                goto cleanup;
            }

            // 将数据写入分块文件
            size_t bytesWritten = fwrite(buffer, 1, bytesRead, partFp);
            if (bytesWritten != bytesRead)
            {
                log("[ERROR] Failed to write to part file");
                ret = -9;
                goto cleanup;
            }

            // 更新计数器
            partRemaining -= bytesRead;
            totalWritten += bytesWritten;

            // 触发进度回调，通知外部更新进度
            if (m_progressCb != NULL)
            {
                m_progressCb(partIndex, totalWritten, fileSize, totalParts);
            }
        }

        // ---- 关闭当前分块文件 ----
        fclose(partFp);
        partFp = NULL;

        // 更新剩余字节数和分块序号
        remainingBytes -= partSize;
        partIndex++;
    }

    // ---- 输出完成摘要 ----
    {
        char doneMsg[256];
        sprintf(doneMsg, "Split completed! Total parts: %d, Total bytes: %lld (%.2f MB)",
                partIndex, totalWritten, (double)totalWritten / 1024 / 1024);
        log(doneMsg);
    }

    // ---- 资源清理 ----
cleanup:
    // 释放 I/O 缓冲区
    if (buffer != NULL)
    {
        free(buffer);
    }
    // 关闭未完成的分块文件（异常路径下可能仍打开）
    if (partFp != NULL)
    {
        fclose(partFp);
    }
    // 关闭源文件
    if (srcFp != NULL)
    {
        fclose(srcFp);
    }

    return ret;
}
