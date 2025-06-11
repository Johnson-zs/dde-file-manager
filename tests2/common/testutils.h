#ifndef TESTUTILS_H
#define TESTUTILS_H

#include <gtest/gtest.h>
#include <QObject>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QDebug>

/**
 * @brief 测试工具类，提供通用的测试辅助功能
 */
class TestUtils
{
public:
    /**
     * @brief 初始化测试环境
     * @param argc 命令行参数个数
     * @param argv 命令行参数数组
     * @return 是否初始化成功
     */
    static bool initTestEnvironment(int argc, char *argv[]);

    /**
     * @brief 清理测试环境
     */
    static void cleanupTestEnvironment();

    /**
     * @brief 创建临时测试目录
     * @param prefix 目录前缀
     * @return 临时目录路径
     */
    static QString createTempDir(const QString &prefix = "dfm-test");

    /**
     * @brief 创建临时测试文件
     * @param dir 目录路径
     * @param fileName 文件名
     * @param content 文件内容
     * @return 文件完整路径
     */
    static QString createTempFile(const QString &dir, const QString &fileName, const QString &content = "");

    /**
     * @brief 检查测试模式环境变量
     * @return 是否处于测试模式
     */
    static bool isTestMode();

    /**
     * @brief 等待事件循环处理
     * @param timeout 超时时间（毫秒）
     */
    static void waitForEvents(int timeout = 100);

    /**
     * @brief 比较两个文件内容是否相同
     * @param file1 文件1路径
     * @param file2 文件2路径
     * @return 是否相同
     */
    static bool compareFiles(const QString &file1, const QString &file2);

private:
    static QCoreApplication *s_app;
    static QTemporaryDir *s_tempDir;
};

/**
 * @brief 测试基类，提供通用的测试设置和清理
 */
class BaseTest : public ::testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    /**
     * @brief 获取测试临时目录
     * @return 临时目录路径
     */
    QString tempDir() const { return m_tempDir; }

    /**
     * @brief 创建测试文件
     * @param fileName 文件名
     * @param content 文件内容
     * @return 文件完整路径
     */
    QString createTestFile(const QString &fileName, const QString &content = "");

private:
    QString m_tempDir;
};

/**
 * @brief Qt对象测试基类，提供Qt对象的测试环境
 */
class QtObjectTest : public BaseTest
{
protected:
    void SetUp() override;
    void TearDown() override;

    /**
     * @brief 等待信号发出
     * @param sender 信号发送者
     * @param signal 信号
     * @param timeout 超时时间（毫秒）
     * @return 是否收到信号
     */
    bool waitForSignal(QObject *sender, const char *signal, int timeout = 5000);

private:
    QCoreApplication *m_app = nullptr;
};

// 测试宏定义
#define ASSERT_FILE_EXISTS(path) \
    ASSERT_TRUE(QFile::exists(path)) << "File does not exist: " << path.toStdString()

#define ASSERT_DIR_EXISTS(path) \
    ASSERT_TRUE(QDir(path).exists()) << "Directory does not exist: " << path.toStdString()

#define EXPECT_FILE_EXISTS(path) \
    EXPECT_TRUE(QFile::exists(path)) << "File does not exist: " << path.toStdString()

#define EXPECT_DIR_EXISTS(path) \
    EXPECT_TRUE(QDir(path).exists()) << "Directory does not exist: " << path.toStdString()

#endif // TESTUTILS_H 