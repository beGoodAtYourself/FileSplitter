/**
 * @file MainWindow.h
 * @brief 主窗口类声明
 *
 * MainWindow �?FileSplitter GUI 的主界面，提供以下功能：
 *   - 源文件选择（文件浏览对话框�?
 *   - 拆分大小和单位设置（数字输入 + KB/MB/GB 下拉框）
 *   - 输出目录选择（文件夹浏览对话框）
 *   - 实时预览预估分块�?
 *   - 开�?取消拆分操作
 *   - 进度条显�?
 *   - 彩色日志输出（红色错误、蓝色信息、绿色完成、橙色取消）
 *
 * 架构说明�?
 *   MainWindow 持有 SplitWorker 指针，通过信号槽与工作线程通信�?
 *   拆分期间禁用输入控件，防止参数被修改；拆分完成后恢复�?
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QLabel>
#include "FileSplitterCore.h"

// 前置声明，避免头文件循环依赖
class SplitWorker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    /** @brief 点击源文件浏览按钮，弹出文件选择对话�?*/
    void onBrowseSourceFile();

    /** @brief 点击输出目录浏览按钮，弹出文件夹选择对话�?*/
    void onBrowseOutputDir();

    /** @brief 点击开始按钮，校验参数并启动拆分工作线�?*/
    void onStartSplit();

    /** @brief 点击取消按钮，设置取消标志中止拆�?*/
    void onCancelSplit();

    /**
     * @brief 响应进度变化信号，更新进度条
     * @param partIndex    当前分块序号
     * @param writtenBytes 已写入字节数
     * @param totalBytes   总字节数
     * @param totalParts   总分块数
     */
    void onProgressChanged(int partIndex, long long writtenBytes, long long totalBytes, int totalParts);

    /**
     * @brief 响应日志消息信号，追加彩色日�?
     * @param message 日志文本
     */
    void onLogMessage(const QString& message);

    /**
     * @brief 响应拆分完成信号，恢�?UI 状�?
     * @param retCode 返回码，0 成功�?100 取消，其他负数失�?
     */
    void onSplitFinished(int retCode);

    /** @brief 输入参数变化时更新预估信�?*/
    void onEstimationChanged();

private:
    /** @brief 创建所�?UI 控件和布局 */
    void createUI();

    /** @brief 连接信号与槽 */
    void createConnections();

    /**
     * @brief 批量启用/禁用输入控件
     * @param enabled true 启用输入控件并禁用取消按钮，false 反之
     */
    void setControlsEnabled(bool enabled);

    /** @brief 根据当前输入计算并显示预估分块数 */
    void updateEstimation();

    /**
     * @brief 向日志区域追加带颜色的文�?
     * @param text  日志内容
     * @param color HTML 颜色值
     */
    void appendLog(const QString& text, const QString& color = "black");

    // ---- UI 控件 ----
    QLineEdit*    m_sourceFileEdit;   ///< 源文件路径输入框
    QPushButton*  m_browseSourceBtn;  ///< 源文件浏览按�?
    QSpinBox*     m_splitSizeSpin;    ///< 拆分大小数值输�?
    QComboBox*    m_unitCombo;        ///< 拆分单位下拉框（KB/MB/GB�?
    QLineEdit*    m_outputDirEdit;    ///< 输出目录路径输入�?
    QPushButton*  m_browseOutputBtn;  ///< 输出目录浏览按钮
    QPushButton*  m_startBtn;         ///< 开始拆分按�?
    QPushButton*  m_cancelBtn;        ///< 取消拆分按钮
    QProgressBar* m_progressBar;      ///< 进度�?
    QLabel*       m_estimationLabel;  ///< 预估信息标签
    QTextEdit*    m_logEdit;          ///< 日志显示区域

    // ---- 工作线程 ----
    SplitWorker*  m_pWorker;          ///< 拆分工作线程指针，NULL 表示空闲
};

#endif // MAINWINDOW_H
