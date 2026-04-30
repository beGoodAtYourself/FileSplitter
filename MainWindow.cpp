/**
 * @file MainWindow.cpp
 * @brief 主窗口类实现
 *
 * 实现 FileSplitter GUI 的主界面，包括界面构建、事件处理、
 * 工作线程管理、进度更新和日志显示等功能。
 */

#include "MainWindow.h"
#include "SplitWorker.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QLabel>
#include <QFileInfo>

// ============================================================================
// 构造 / 析构
// ============================================================================

/**
 * @brief 构造函数
 *
 * 初始化 UI 控件、连接信号槽、更新初始预估信息。
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_pWorker(NULL)
{
    createUI();
    createConnections();
    updateEstimation();
}

/**
 * @brief 析构函数
 *
 * 确保工作线程安全退出：先设置取消标志，再等待线程结束，最后释放对象。
 */
MainWindow::~MainWindow()
{
    if (m_pWorker != NULL)
    {
        m_pWorker->cancel();   // 请求取消
        m_pWorker->wait();     // 等待线程结束
        delete m_pWorker;
        m_pWorker = NULL;
    }
}

// ============================================================================
// UI 构建
// ============================================================================

/**
 * @brief 创建所有 UI 控件和布局
 *
 * 界面布局（从上到下）：
 *   ┌─ Source File ──────────────────────────────┐
 *   │ [源文件路径输入框                    ] [Browse] │
 *   └────────────────────────────────────────────┘
 *   ┌─ Split Settings ───────────────────────────┐
 *   │ Split Size: [100] [MB▼]                    │
 *   │ Estimation: 500.00 MB -> 5 parts           │
 *   └────────────────────────────────────────────┘
 *   ┌─ Output Directory ─────────────────────────┐
 *   │ [输出目录输入框                      ] [Browse] │
 *   └────────────────────────────────────────────┘
 *   ──────────────────────────────── [Start] [Cancel]
 *   [===============进度条========================]
 *   ┌─ Log ──────────────────────────────────────┐
 *   │ 日志信息...                                  │
 *   └────────────────────────────────────────────┘
 */
void MainWindow::createUI()
{
    setWindowTitle(tr("FileSplitter v2.0.0"));
    setMinimumWidth(560);
    setMinimumHeight(480);

    // 中央容器
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主垂直布局
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // ---- 源文件选择区域 ----
    QGroupBox* sourceGroup = new QGroupBox(tr("Source File"), centralWidget);
    QHBoxLayout* sourceLayout = new QHBoxLayout(sourceGroup);

    m_sourceFileEdit = new QLineEdit(sourceGroup);
    m_sourceFileEdit->setPlaceholderText(tr("Select the file to split..."));
    m_browseSourceBtn = new QPushButton(tr("Browse..."), sourceGroup);
    m_browseSourceBtn->setFixedWidth(90);

    sourceLayout->addWidget(m_sourceFileEdit);
    sourceLayout->addWidget(m_browseSourceBtn);

    // ---- 拆分设置区域 ----
    QGroupBox* splitGroup = new QGroupBox(tr("Split Settings"), centralWidget);
    QGridLayout* splitLayout = new QGridLayout(splitGroup);

    // 拆分大小输入框
    splitLayout->addWidget(new QLabel(tr("Split Size:"), splitGroup), 0, 0);
    m_splitSizeSpin = new QSpinBox(splitGroup);
    m_splitSizeSpin->setMinimum(1);
    m_splitSizeSpin->setMaximum(999999);
    m_splitSizeSpin->setValue(100);
    splitLayout->addWidget(m_splitSizeSpin, 0, 1);

    // 单位下拉框：KB / MB / GB，默认 MB
    m_unitCombo = new QComboBox(splitGroup);
    m_unitCombo->addItem(tr("KB"), (int)UNIT_KB);
    m_unitCombo->addItem(tr("MB"), (int)UNIT_MB);
    m_unitCombo->setCurrentIndex(1);  // 默认选中 MB
    splitLayout->addWidget(m_unitCombo, 0, 2);

    // 预估信息标签
    m_estimationLabel = new QLabel(splitGroup);
    splitLayout->addWidget(new QLabel(tr("Estimation:"), splitGroup), 1, 0);
    splitLayout->addWidget(m_estimationLabel, 1, 1, 1, 2);

    // ---- 输出目录选择区域 ----
    QGroupBox* outputGroup = new QGroupBox(tr("Output Directory"), centralWidget);
    QHBoxLayout* outputLayout = new QHBoxLayout(outputGroup);

    m_outputDirEdit = new QLineEdit(outputGroup);
    m_outputDirEdit->setPlaceholderText(tr("Leave empty to output to source file directory"));
    m_browseOutputBtn = new QPushButton(tr("Browse..."), outputGroup);
    m_browseOutputBtn->setFixedWidth(90);

    outputLayout->addWidget(m_outputDirEdit);
    outputLayout->addWidget(m_browseOutputBtn);

    // ---- 操作按钮区域 ----
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton(tr("Start"), centralWidget);
    m_startBtn->setFixedHeight(32);
    m_cancelBtn = new QPushButton(tr("Cancel"), centralWidget);
    m_cancelBtn->setFixedHeight(32);
    m_cancelBtn->setEnabled(false);  // 初始状态下禁用取消按钮

    btnLayout->addStretch();         // 按钮靠右对齐
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_cancelBtn);

    // ---- 进度条 ----
    m_progressBar = new QProgressBar(centralWidget);
    m_progressBar->setTextVisible(true);  // 显示百分比文字

    // ---- 日志区域 ----
    QGroupBox* logGroup = new QGroupBox(tr("Log"), centralWidget);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);

    m_logEdit = new QTextEdit(logGroup);
    m_logEdit->setReadOnly(true);           // 只读，用户不可编辑
    m_logEdit->setFont(QFont("Consolas", 9));  // 等宽字体，适合日志显示

    logLayout->addWidget(m_logEdit);

    // ---- 组装主布局 ----
    mainLayout->addWidget(sourceGroup);
    mainLayout->addWidget(splitGroup);
    mainLayout->addWidget(outputGroup);
    mainLayout->addLayout(btnLayout);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(logGroup);
}

// ============================================================================
// 信号槽连接
// ============================================================================

/**
 * @brief 连接所有 UI 控件的信号与槽
 *
 * 包括：浏览按钮点击、开始/取消按钮点击、输入参数变化触发预估更新。
 */
void MainWindow::createConnections()
{
    // 浏览按钮
    connect(m_browseSourceBtn, SIGNAL(clicked()), this, SLOT(onBrowseSourceFile()));
    connect(m_browseOutputBtn, SIGNAL(clicked()), this, SLOT(onBrowseOutputDir()));

    // 操作按钮
    connect(m_startBtn, SIGNAL(clicked()), this, SLOT(onStartSplit()));
    connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelSplit()));

    // 输入变化时更新预估信息
    connect(m_sourceFileEdit, SIGNAL(textChanged(QString)), this, SLOT(onEstimationChanged()));
    connect(m_splitSizeSpin, SIGNAL(valueChanged(int)), this, SLOT(onEstimationChanged()));
    connect(m_unitCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onEstimationChanged()));
}

// ============================================================================
// 槽函数 —— 文件/目录浏览
// ============================================================================

/**
 * @brief 浏览选择源文件
 *
 * 弹出文件选择对话框，用户选择文件后：
 *   1. 填充源文件路径输入框
 *   2. 若输出目录为空，自动填充为源文件所在目录
 */
void MainWindow::onBrowseSourceFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select Source File"), QString(), tr("All Files (*.*)"));

    if (!filePath.isEmpty())
    {
        m_sourceFileEdit->setText(filePath);

        // 自动填充输出目录为源文件所在目录（仅当输出目录为空时）
        if (m_outputDirEdit->text().isEmpty())
        {
            QFileInfo fi(filePath);
            m_outputDirEdit->setText(fi.absolutePath());
        }
    }
}

/**
 * @brief 浏览选择输出目录
 *
 * 弹出文件夹选择对话框，用户选择后填充输出目录输入框。
 */
void MainWindow::onBrowseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Output Directory"), QString());

    if (!dir.isEmpty())
    {
        m_outputDirEdit->setText(dir);
    }
}

// ============================================================================
// 槽函数 —— 拆分操作
// ============================================================================

/**
 * @brief 开始拆分操作
 *
 * 流程：
 *   1. 校验源文件路径（非空、存在、是文件）
 *   2. 校验输出目录（若非空则必须存在且是目录）
 *   3. 创建 SplitWorker 并配置参数
 *   4. 连接工作线程信号到主窗口槽
 *   5. 重置进度条和日志
 *   6. 禁用输入控件
 *   7. 启动工作线程
 */
void MainWindow::onStartSplit()
{
    // ---- 校验源文件 ----
    QString srcPath = m_sourceFileEdit->text().trimmed();
    if (srcPath.isEmpty())
    {
        QMessageBox::warning(this, tr("Warning"), tr("Please select a source file."));
        return;
    }

    QFileInfo srcInfo(srcPath);
    if (!srcInfo.exists() || !srcInfo.isFile())
    {
        QMessageBox::warning(this, tr("Warning"), tr("Source file does not exist."));
        return;
    }

    // ---- 校验输出目录 ----
    QString outDir = m_outputDirEdit->text().trimmed();
    if (!outDir.isEmpty())
    {
        QFileInfo outInfo(outDir);
        if (!outInfo.exists() || !outInfo.isDir())
        {
            QMessageBox::warning(this, tr("Warning"), tr("Output directory does not exist."));
            return;
        }
    }

    // ---- 获取拆分参数 ----
    int splitSize = m_splitSizeSpin->value();
    SplitUnit unit = (SplitUnit)m_unitCombo->currentData().toInt();

    // ---- 创建并配置工作线程 ----
    m_pWorker = new SplitWorker();
    m_pWorker->setSourceFilePath(srcPath);
    m_pWorker->setSplitSize(splitSize, unit);

    if (!outDir.isEmpty())
    {
        m_pWorker->setOutputDir(outDir);
    }

    // ---- 连接工作线程信号 ----
    connect(m_pWorker, SIGNAL(progressChanged(int, long long, long long, int)),
            this, SLOT(onProgressChanged(int, long long, long long, int)));
    connect(m_pWorker, SIGNAL(logMessage(QString)),
            this, SLOT(onLogMessage(QString)));
    connect(m_pWorker, SIGNAL(splitFinished(int)),
            this, SLOT(onSplitFinished(int)));

    // ---- 重置 UI 状态 ----
    m_progressBar->setValue(0);
    m_logEdit->clear();
    appendLog("Starting split operation...", "blue");

    // ---- 禁用输入控件，防止拆分过程中修改参数 ----
    setControlsEnabled(false);

    // ---- 启动工作线程 ----
    m_pWorker->start();
}

/**
 * @brief 取消拆分操作
 *
 * 设置工作线程的取消标志，并禁用取消按钮防止重复点击。
 * 实际取消是异步的——工作线程在下次检查标志时才会中止，
 * 最终通过 onSplitFinished(-100) 通知完成。
 */
void MainWindow::onCancelSplit()
{
    if (m_pWorker != NULL && m_pWorker->isRunning())
    {
        m_pWorker->cancel();
        appendLog("Cancelling...", "orange");
        m_cancelBtn->setEnabled(false);  // 防止重复点击
    }
}

// ============================================================================
// 槽函数 —— 进度和日志更新
// ============================================================================

/**
 * @brief 更新进度条
 *
 * 根据已写入字节数和总字节数计算百分比。
 * 进度回调频率较高，此函数需保持轻量。
 */
void MainWindow::onProgressChanged(int partIndex, long long writtenBytes, long long totalBytes, int totalParts)
{
    if (totalBytes > 0)
    {
        int percent = (int)((double)writtenBytes * 100 / totalBytes);
        m_progressBar->setValue(percent);
    }
}

/**
 * @brief 处理日志消息
 *
 * 根据消息内容中的标记自动选择颜色：
 *   - [ERROR] -> 红色
 *   - [INFO]  -> 蓝色
 *   - 其他    -> 黑色
 */
void MainWindow::onLogMessage(const QString& message)
{
    if (message.contains("[ERROR]"))
    {
        appendLog(message, "red");
    }
    else if (message.contains("[INFO]"))
    {
        appendLog(message, "blue");
    }
    else
    {
        appendLog(message, "black");
    }
}

/**
 * @brief 拆分完成处理
 *
 * 根据返回码显示不同的完成信息：
 *   - 0    : 成功（绿色）
 *   - -100 : 用户取消（橙色）
 *   - 其他 : 失败（红色）
 *
 * 恢复 UI 控件状态，清理工作线程对象。
 */
void MainWindow::onSplitFinished(int retCode)
{
    // 恢复所有输入控件
    setControlsEnabled(true);

    if (retCode == 0)
    {
        m_progressBar->setValue(100);
        appendLog("Split completed successfully!", "green");
    }
    else if (retCode == -100)
    {
        appendLog("Split cancelled.", "orange");
    }
    else
    {
        appendLog(QString("Split failed with error code: %1").arg(retCode), "red");
    }

    // 清理工作线程（使用 deleteLater 避免在槽函数中直接删除发送者）
    if (m_pWorker != NULL)
    {
        m_pWorker->deleteLater();
        m_pWorker = NULL;
    }
}

/**
 * @brief 输入参数变化时更新预估信息
 *
 * 由源文件路径、拆分大小、单位三者的变化触发。
 */
void MainWindow::onEstimationChanged()
{
    updateEstimation();
}

// ============================================================================
// 辅助方法
// ============================================================================

/**
 * @brief 批量启用/禁用输入控件
 *
 * 拆分进行中禁用所有输入控件和开始按钮，启用取消按钮；
 * 拆分结束后恢复。防止用户在拆分过程中修改参数。
 *
 * @param enabled true=启用输入控件+禁用取消按钮, false=禁用输入控件+启用取消按钮
 */
void MainWindow::setControlsEnabled(bool enabled)
{
    m_sourceFileEdit->setEnabled(enabled);
    m_browseSourceBtn->setEnabled(enabled);
    m_splitSizeSpin->setEnabled(enabled);
    m_unitCombo->setEnabled(enabled);
    m_outputDirEdit->setEnabled(enabled);
    m_browseOutputBtn->setEnabled(enabled);
    m_startBtn->setEnabled(enabled);
    m_cancelBtn->setEnabled(!enabled);  // 取消按钮与输入控件互斥
}

/**
 * @brief 更新预估分块信息
 *
 * 根据当前源文件大小和拆分设置，实时计算并显示预计分块数。
 * 显示格式："<文件大小> MB -> <分块数> parts (<拆分大小> <单位> each)"
 *
 * 若源文件为空或不存在，显示相应提示。
 */
void MainWindow::updateEstimation()
{
    QString srcPath = m_sourceFileEdit->text().trimmed();

    // 源文件路径为空
    if (srcPath.isEmpty())
    {
        m_estimationLabel->setText(tr("No source file selected"));
        return;
    }

    // 源文件不存在
    QFileInfo fi(srcPath);
    if (!fi.exists() || !fi.isFile())
    {
        m_estimationLabel->setText(tr("File not found"));
        return;
    }

    // 计算文件大小（MB）
    double fileSizeMB = (double)fi.size() / 1024 / 1024;

    // 获取拆分参数
    int splitSize = m_splitSizeSpin->value();
    SplitUnit unit = (SplitUnit)m_unitCombo->currentData().toInt();

    // 将拆分大小转换为字节数
    long long splitBytes = splitSize;
    if (unit == UNIT_KB) splitBytes *= 1024;
    else if (unit == UNIT_MB) splitBytes *= 1024 * 1024;
    else if (unit == UNIT_GB) splitBytes *= 1024 * 1024 * 1024;

    // 拆分大小无效
    if (splitBytes <= 0)
    {
        m_estimationLabel->setText(tr("Invalid split size"));
        return;
    }

    // 计算预计分块数（向上取整）
    long long fileSize = fi.size();
    int parts = (int)((fileSize + splitBytes - 1) / splitBytes);

    // 格式化显示
    QString unitStr = (unit == UNIT_KB) ? "KB" : (unit == UNIT_MB) ? "MB" : "GB";
    m_estimationLabel->setText(tr("%1 MB -> %2 parts (%3 %4 each)")
        .arg(fileSizeMB, 0, 'f', 2)
        .arg(parts)
        .arg(splitSize)
        .arg(unitStr));
}

/**
 * @brief 向日志区域追加带颜色标注的文本
 *
 * 使用 HTML <span> 标签实现彩色文本，QTextEdit 自动渲染。
 * toHtmlEscaped() 转义特殊字符，防止日志内容被当作 HTML 解析。
 *
 * @param text  日志文本内容
 * @param color HTML 颜色值（如 "red", "green", "#FF6600"）
 */
void MainWindow::appendLog(const QString& text, const QString& color)
{
    m_logEdit->append(QString("<span style=\"color:%1;\">%2</span>").arg(color, text.toHtmlEscaped()));
}
