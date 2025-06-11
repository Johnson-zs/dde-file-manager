#include "testutils.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QDebug>
#include <QThread>

// 静态成员初始化
QCoreApplication *TestUtils::s_app = nullptr;
QTemporaryDir *TestUtils::s_tempDir = nullptr;

bool TestUtils::initTestEnvironment(int argc, char *argv[])
{
    // 检查测试模式
    if (!isTestMode()) {
        qWarning() << "Warning: DFM_TEST_MODE not set. Please set DFM_TEST_MODE=1 for safe testing.";
    }

    // 初始化Qt应用程序
    if (!QCoreApplication::instance()) {
        s_app = new QCoreApplication(argc, argv);
        s_app->setApplicationName("DFM-Tests2");
        s_app->setApplicationVersion("1.0.0");
    }

    // 设置测试数据路径
    QStandardPaths::setTestModeEnabled(true);

    // 创建全局临时目录
    s_tempDir = new QTemporaryDir("dfm-test-XXXXXX");
    if (!s_tempDir->isValid()) {
        qCritical() << "Failed to create temporary directory for tests";
        return false;
    }

    qDebug() << "Test environment initialized";
    qDebug() << "Temporary directory:" << s_tempDir->path();

    return true;
}

void TestUtils::cleanupTestEnvironment()
{
    if (s_tempDir) {
        delete s_tempDir;
        s_tempDir = nullptr;
    }

    if (s_app) {
        delete s_app;
        s_app = nullptr;
    }

    qDebug() << "Test environment cleaned up";
}

QString TestUtils::createTempDir(const QString &prefix)
{
    if (!s_tempDir || !s_tempDir->isValid()) {
        qWarning() << "Global temp directory not available, creating local one";
        QTemporaryDir localTempDir(prefix + "-XXXXXX");
        if (localTempDir.isValid()) {
            localTempDir.setAutoRemove(false);
            return localTempDir.path();
        }
        return QString();
    }

    QString dirPath = s_tempDir->path() + "/" + prefix + "-" + QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QDir dir;
    if (dir.mkpath(dirPath)) {
        return dirPath;
    }

    return QString();
}

QString TestUtils::createTempFile(const QString &dir, const QString &fileName, const QString &content)
{
    QString filePath = dir + "/" + fileName;
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create temp file:" << filePath;
        return QString();
    }

    if (!content.isEmpty()) {
        QTextStream out(&file);
        out << content;
    }

    file.close();
    return filePath;
}

bool TestUtils::isTestMode()
{
    QString testMode = qgetenv("DFM_TEST_MODE");
    return testMode == "1" || testMode.toLower() == "true";
}

void TestUtils::waitForEvents(int timeout)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeout);
    loop.exec();
}

bool TestUtils::compareFiles(const QString &file1, const QString &file2)
{
    QFile f1(file1);
    QFile f2(file2);

    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data1 = f1.readAll();
    QByteArray data2 = f2.readAll();

    return data1 == data2;
}

// BaseTest 实现
void BaseTest::SetUp()
{
    m_tempDir = TestUtils::createTempDir("base-test");
    ASSERT_FALSE(m_tempDir.isEmpty()) << "Failed to create temporary directory";
}

void BaseTest::TearDown()
{
    if (!m_tempDir.isEmpty()) {
        QDir dir(m_tempDir);
        dir.removeRecursively();
    }
}

QString BaseTest::createTestFile(const QString &fileName, const QString &content)
{
    return TestUtils::createTempFile(m_tempDir, fileName, content);
}

// QtObjectTest 实现
void QtObjectTest::SetUp()
{
    BaseTest::SetUp();
    
    if (!QCoreApplication::instance()) {
        static int argc = 1;
        static char appName[] = "test";
        static char *argv[] = { appName, nullptr };
        m_app = new QCoreApplication(argc, argv);
    }
}

void QtObjectTest::TearDown()
{
    if (m_app && m_app != QCoreApplication::instance()) {
        delete m_app;
        m_app = nullptr;
    }
    
    BaseTest::TearDown();
}

bool QtObjectTest::waitForSignal(QObject *sender, const char *signal, int timeout)
{
    if (!sender) {
        return false;
    }

    QSignalSpy spy(sender, signal);
    if (!spy.isValid()) {
        return false;
    }

    return spy.wait(timeout);
} 