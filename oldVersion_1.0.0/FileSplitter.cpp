/**
 * 文件拆分工具
 * 功能：将大文件按指定大小拆分成多个小文件
 * 作者：AI Assistant
 * 日期：2026-04-23
 * 
 * 使用方法：
 *   FileSplitter.exe <源文件路径> <分块大小MB>
 * 
 * 示例：
 *   FileSplitter.exe "D:\large_file.bin" 100
 *   将 D:\large_file.bin 拆分成多个100MB的小文件
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 缓冲区大小（4MB）
#define BUFFER_SIZE (4 * 1024 * 1024)

/**
 * 获取文件扩展名位置
 * @param filePath 文件路径
 * @return 扩展名位置的指针，如果没有扩展名则返回文件名末尾
 */
const char* getFileExtension(const char* filePath)
{
    const char* pDot = strrchr(filePath, '.');
    const char* pSlash = strrchr(filePath, '\\');
    
    // 如果没有点，或者点在路径分隔符之前，则没有扩展名
    if (pDot == NULL || (pSlash != NULL && pDot < pSlash))
    {
        return filePath + strlen(filePath);
    }
    
    return pDot;
}

/**
 * 拆分文件
 * @param srcFilePath 源文件路径
 * @param splitSizeMB 每个分块的大小（MB）
 * @return 成功返回0，失败返回错误码
 */
int splitFile(const char* srcFilePath, int splitSizeMB)
{
    FILE* srcFp = NULL;
    FILE* partFp = NULL;
    char* buffer = NULL;
    char partFilePath[512];
    int partIndex = 0;
    int ret = 0;
    
    // 参数检查
    if (srcFilePath == NULL || splitSizeMB <= 0)
    {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }
    
    // 计算分块大小（字节）
    long long splitSizeBytes = (long long)splitSizeMB * 1024 * 1024;
    
    // 打开源文件
    srcFp = fopen(srcFilePath, "rb");
    if (srcFp == NULL)
    {
        fprintf(stderr, "Error: Failed to open source file: %s\n", srcFilePath);
        return -2;
    }
    
    // 获取文件大小（使用64位文件操作）
    if (_fseeki64(srcFp, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "Error: Failed to seek to end of file\n");
        ret = -3;
        goto cleanup;
    }
    
    long long fileSize = _ftelli64(srcFp);
    if (fileSize <= 0)
    {
        fprintf(stderr, "Error: Invalid file size: %lld\n", fileSize);
        ret = -4;
        goto cleanup;
    }
    
    // 重置文件指针到开头
    if (_fseeki64(srcFp, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error: Failed to seek to start of file\n");
        ret = -5;
        goto cleanup;
    }
    
    // 分配缓冲区
    buffer = (char*)malloc(BUFFER_SIZE);
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate buffer\n");
        ret = -6;
        goto cleanup;
    }
    
    // 计算分块数量
    int totalParts = (int)((fileSize + splitSizeBytes - 1) / splitSizeBytes);
    
    // 输出文件信息
    printf("Source file: %s\n", srcFilePath);
    printf("File size: %.2f MB (%lld bytes)\n", (double)fileSize / 1024 / 1024, fileSize);
    printf("Split size: %d MB\n", splitSizeMB);
    printf("Total parts: %d\n\n", totalParts);
    
    // 提取文件名和扩展名
    const char* pFileName = strrchr(srcFilePath, '\\');
    if (pFileName == NULL)
    {
        pFileName = srcFilePath;
    }
    else
    {
        pFileName++; // 跳过反斜杠
    }
    
    const char* pExt = getFileExtension(srcFilePath);
    int nameLen = (int)(pExt - pFileName);
    char baseName[256];
    
    // 复制基础文件名（不包含扩展名）
    if (nameLen > 0 && nameLen < 256)
    {
        strncpy(baseName, pFileName, nameLen);
        baseName[nameLen] = '\0';
    }
    else
    {
        strncpy(baseName, pFileName, 255);
        baseName[255] = '\0';
    }
    
    // 开始拆分
    long long remainingBytes = fileSize;
    long long totalWritten = 0;
    
    while (remainingBytes > 0)
    {
        // 生成分块文件路径
        sprintf(partFilePath, "%s.%03d", srcFilePath, partIndex);
        
        // 打开分块文件
        partFp = fopen(partFilePath, "wb");
        if (partFp == NULL)
        {
            fprintf(stderr, "Error: Failed to create part file: %s\n", partFilePath);
            ret = -7;
            goto cleanup;
        }
        
        // 计算当前分块需要写入的字节数
        long long partSize = (remainingBytes > splitSizeBytes) ? splitSizeBytes : remainingBytes;
        long long partRemaining = partSize;
        
        printf("Creating part %03d: %s (%.2f MB)\n", 
               partIndex, partFilePath, (double)partSize / 1024 / 1024);
        
        // 写入当前分块
        while (partRemaining > 0)
        {
            long long bytesToRead = (partRemaining > BUFFER_SIZE) ? BUFFER_SIZE : partRemaining;
            size_t bytesRead = fread(buffer, 1, (size_t)bytesToRead, srcFp);
            
            if (bytesRead <= 0)
            {
                fprintf(stderr, "Error: Failed to read from source file\n");
                ret = -8;
                goto cleanup;
            }
            
            size_t bytesWritten = fwrite(buffer, 1, bytesRead, partFp);
            if (bytesWritten != bytesRead)
            {
                fprintf(stderr, "Error: Failed to write to part file\n");
                ret = -9;
                goto cleanup;
            }
            
            partRemaining -= bytesRead;
            totalWritten += bytesWritten;
        }
        
        // 关闭当前分块文件
        fclose(partFp);
        partFp = NULL;
        
        remainingBytes -= partSize;
        partIndex++;
        
        // 显示进度
        printf("Progress: %.1f%% (%lld / %lld bytes)\n\n", 
               (double)totalWritten * 100 / fileSize, totalWritten, fileSize);
    }
    
    // 输出完成信息
    printf("========================================\n");
    printf("Split completed successfully!\n");
    printf("Total parts created: %d\n", partIndex);
    printf("Total bytes written: %lld (%.2f MB)\n", totalWritten, (double)totalWritten / 1024 / 1024);
    printf("========================================\n");
    
cleanup:
    if (buffer != NULL)
    {
        free(buffer);
    }
    if (partFp != NULL)
    {
        fclose(partFp);
    }
    if (srcFp != NULL)
    {
        fclose(srcFp);
    }
    
    return ret;
}

/**
 * 打印使用帮助
 */
void printUsage()
{
    printf("========================================\n");
    printf("File Splitter Tool\n");
    printf("========================================\n");
    printf("Usage:\n");
    printf("  FileSplitter.exe <source_file> <split_size_MB>\n");
    printf("\n");
    printf("Parameters:\n");
    printf("  source_file    : Path to the source file to split\n");
    printf("  split_size_MB  : Size of each part in MB (must be > 0)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  FileSplitter.exe \"D:\\large_file.bin\" 100\n");
    printf("  FileSplitter.exe \"C:\\data\\video.mp4\" 50\n");
    printf("\n");
    printf("Output:\n");
    printf("  Split files will be named as: source_file.001, source_file.002, ...\n");
    printf("========================================\n");
}

/**
 * 主函数
 */
int main(int argc, char* argv[])
{
    // 参数检查
    if (argc != 3)
    {
        printUsage();
        return -1;
    }
    
    const char* srcFilePath = argv[1];
    int splitSizeMB = atoi(argv[2]);
    
    // 检查分块大小
    if (splitSizeMB <= 0)
    {
        fprintf(stderr, "Error: Split size must be greater than 0\n");
        printUsage();
        return -2;
    }
    
    // 执行拆分
    return splitFile(srcFilePath, splitSizeMB);
}
