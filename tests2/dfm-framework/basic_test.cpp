#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDebug>

#include "testutils.h"

// 包含dfm-framework头文件进行基础测试
#include "dfm-framework/event/eventchannel.h"
#include "dfm-framework/event/event.h"
#include "dfm-framework/lifecycle/lifecycle.h"

using namespace dpf;

/**
 * @brief dfm-framework基础测试类
 */
class DfmFrameworkBasicTest : public QtObjectTest
{
protected:
    void SetUp() override
    {
        QtObjectTest::SetUp();
        qDebug() << "DfmFrameworkBasicTest SetUp";
    }

    void TearDown() override
    {
        qDebug() << "DfmFrameworkBasicTest TearDown";
        QtObjectTest::TearDown();
    }
};

/**
 * @brief 测试dfm-framework基础功能是否可用
 */
TEST_F(DfmFrameworkBasicTest, BasicFunctionality)
{
    // 测试EventChannelManager是否可以创建
    EXPECT_NO_THROW({
        auto channel = Event::instance()->channel();
        EXPECT_NE(channel, nullptr);
    });

    qDebug() << "Basic functionality test passed";
}

/**
 * @brief 测试生命周期管理器是否可用
 */
TEST_F(DfmFrameworkBasicTest, LifecycleManager)
{
    // 测试LifeCycle命名空间的函数是否可用
    EXPECT_NO_THROW({
        auto iids = LifeCycle::pluginIIDs();
        // 这个测试只是验证函数可以调用，不验证返回值
        qDebug() << "LifeCycle::pluginIIDs() returned" << iids.size() << "items";
    });

    qDebug() << "Lifecycle manager test passed";
}

/**
 * @brief 测试测试工具是否正常工作
 */
TEST_F(DfmFrameworkBasicTest, TestUtilities)
{
    // 测试临时目录创建
    QString tempDir = TestUtils::createTempDir("basic-test");
    EXPECT_FALSE(tempDir.isEmpty());
    EXPECT_DIR_EXISTS(tempDir);

    // 测试临时文件创建
    QString testFile = TestUtils::createTempFile(tempDir, "test.txt", "Hello, World!");
    EXPECT_FALSE(testFile.isEmpty());
    EXPECT_FILE_EXISTS(testFile);

    qDebug() << "Test utilities test passed";
}

/**
 * @brief 测试环境变量检查
 */
TEST(EnvironmentTest, TestModeCheck)
{
    // 检查测试模式环境变量
    bool isTestMode = TestUtils::isTestMode();
    if (isTestMode) {
        qDebug() << "Running in test mode";
    } else {
        qWarning() << "Not running in test mode - please set DFM_TEST_MODE=1";
    }

    // 这个测试总是通过，只是用来检查环境
    EXPECT_TRUE(true);
}

/**
 * @brief 主函数
 */
int main(int argc, char *argv[])
{
    // 初始化测试环境
    if (!TestUtils::initTestEnvironment(argc, argv)) {
        qCritical() << "Failed to initialize test environment";
        return -1;
    }

    // 初始化GTest
    ::testing::InitGoogleTest(&argc, argv);

    // 运行测试
    int result = RUN_ALL_TESTS();

    // 清理测试环境
    TestUtils::cleanupTestEnvironment();

    return result;
} 