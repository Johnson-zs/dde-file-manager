#include "newsearchmanager.h"
#include "newanythingsearcher.h"

#include <QFileInfo>
#include <QtConcurrent>
#include <QRegularExpression>
#include <QDebug>
#include <QThread>

NewSearchManager::NewSearchManager(QObject *parent)
    : QObject(parent), m_searcher(nullptr), m_ownsSearcher(false)
{
    auto anything = new NewAnythingSearcher(this);
    setSearcher(anything);

    // 初始化防抖定时器
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout, this, [this]() {
        qDebug() << "===> timer search:" << m_pendingSearchText;
        executeSearch();
    });

    // 记录当前线程ID
    m_timerThreadId = QThread::currentThreadId();

    // 初始化时间戳
    m_lastSearchTime = QDateTime::currentDateTime();
}

NewSearchManager &NewSearchManager::instance()
{
    static NewSearchManager ins;
    return ins;
}

void NewSearchManager::setSearcher(SearcherInterface *searcher)
{
    QMutexLocker locker(&m_searcherMutex);
    if (m_searcher && m_ownsSearcher) {
        delete m_searcher;
    }
    m_searcher = searcher;
    m_ownsSearcher = false;

    if (m_searcher) {
        connect(m_searcher, &SearcherInterface::searchFinished,
                this, &NewSearchManager::onSearchFinished);
        connect(m_searcher, &SearcherInterface::searchFailed,
                this, &NewSearchManager::onSearchFailed);
    }
}

void NewSearchManager::processUserInput(const QString &searchPath, const QString &searchText)
{
    QMutexLocker locker(&m_mutex);

    // 确保定时器在当前线程
    ensureTimerInCurrentThread();

    m_pendingSearchPath = searchPath;
    m_pendingSearchText = searchText;

    // 如果输入为空，直接清空结果
    if (searchText.isEmpty()) {
        emit searchResultsReady(QStringList());
        return;
    }

    // 尝试从缓存获取结果
    QStringList cachedResults;
    if (!m_lastSearchText.isEmpty()) {
        //    QReadLocker readLock(&m_cacheLock);
        if (tryGetFromCache(searchText, cachedResults)) {
            emit searchResultsReady(cachedResults);
            m_lastSearchText = searchText;
            return;
        }
    }

    // 应用防抖：重置定时器
    m_debounceTimer.start(determineDebounceDelay(searchText));
}

void NewSearchManager::executeSearch()
{
    if (m_pendingSearchText.isEmpty()) {
        emit searchResultsReady(QStringList());
        return;
    }

    // 检查缓存
    if (m_resultsCache.contains(m_pendingSearchText)) {
        emit searchResultsReady(m_resultsCache[m_pendingSearchText]);
        m_lastSearchText = m_pendingSearchText;
        return;
    }

    qDebug() << "[excute] About to search: " << m_pendingSearchText;

    // 检查搜索器是否存在
    if (!m_searcher) {
        emit searchError("搜索器未初始化");
        return;
    }

    // 执行实际搜索
    if (!m_searcher->requestSearch(m_pendingSearchPath, m_pendingSearchText)) {
        emit searchError("搜索请求失败");
    }

    // 更新最后搜索时间
    m_lastSearchTime = QDateTime::currentDateTime();
    m_lastSearchText = m_pendingSearchText;
}

void NewSearchManager::onSearchFinished(const QString &query, const QStringList &results)
{
    // 仅处理最新的查询结果
    if (query == m_pendingSearchText) {
        addToCache(query, results);
        emit searchResultsReady(results);
    }
}

void NewSearchManager::onSearchFailed(const QString &query, const QString &errorMessage)
{
    if (query == m_pendingSearchText) {
        emit searchError(errorMessage);
    }
}

NewSearchManager::InputChangeType NewSearchManager::analyzeInputChange(const QString &oldText, const QString &newText)
{
    if (newText.length() > oldText.length()) {
        if (newText.startsWith(oldText)) {
            return InputChangeType::Addition;
        }
    } else if (newText.length() < oldText.length()) {
        if (oldText.startsWith(newText)) {
            return InputChangeType::Deletion;
        }
    }

    return InputChangeType::Replacement;
}

QString NewSearchManager::getFileName(const QString &filePath)
{

    // 缓存未命中，计算文件名
    int lastSeparator = filePath.lastIndexOf('/');
    QString fileName = (lastSeparator == -1) ? filePath : filePath.mid(lastSeparator + 1);

    return fileName;
}

QStringList NewSearchManager::filterLocalResults(const QStringList &sourceResults, const QString &query)
{
    QStringList filteredResults;
    filteredResults.reserve(sourceResults.size());

    const QString queryLower = query.toLower();

    for (const QString &filePath : sourceResults) {
        if (getFileName(filePath).toLower().contains(queryLower)) {
            filteredResults.append(filePath);
        }
    }

    return filteredResults;
}

int NewSearchManager::determineDebounceDelay(const QString &text)
{
    // 基础等待时间
    int delay = 200;   // 毫秒

    // 针对短输入增加延迟
    if (text.length() <= 2) {
        delay += 150;
    }

    // 对于可能返回大量结果的特殊字符增加延迟
    if (text.contains('*') || text.contains('?') || text.startsWith('.')) {
        delay += 200;
    }

    return delay;
}

bool NewSearchManager::shouldDelaySearch(const QString &text)
{
    // 对于过短或者通配符搜索，应该延迟
    return text.length() < 2 || text == "." || text == "*";
}

void NewSearchManager::clearCache()
{
    //   QWriteLocker writeLock(&m_cacheLock);
    m_resultsCache.clear();
    m_cacheUsageOrder.clear();
    m_lastSearchText.clear();
}

bool NewSearchManager::tryGetFromCache(const QString &searchText, QStringList &results)
{
    // 首先检查是否直接有缓存
    if (m_resultsCache.contains(searchText)) {
        qDebug() << "===> search from direct cache: " << searchText;
        results = m_resultsCache[searchText];
        updateCacheUsage(searchText);   // 更新使用情况
        return true;
    }

    // 检查输入变化类型
    InputChangeType changeType = analyzeInputChange(m_lastSearchText, searchText);

    // 根据变化类型处理
    switch (changeType) {
    case InputChangeType::Addition:
        return handleIncrementalSearch(searchText, results);
    case InputChangeType::Deletion:
        return handleDeletionSearch(searchText, results);
    default:
        qDebug() << "==> cache break: replacement or unknown change";
        return false;
    }
}

bool NewSearchManager::handleIncrementalSearch(const QString &searchText, QStringList &results)
{
    // 如果新输入是旧输入的扩展，并且我们有旧缓存
    if (searchText.startsWith(m_lastSearchText) && m_resultsCache.contains(m_lastSearchText)) {
        qDebug() << "===> search from cache(add): " << searchText;
        results = filterLocalResults(m_resultsCache[m_lastSearchText], searchText);
        addToCache(searchText, results);
        updateCacheUsage(m_lastSearchText);   // 更新基础缓存的使用情况
        return true;
    }
    return false;
}

bool NewSearchManager::handleDeletionSearch(const QString &searchText, QStringList &results)
{
    // 查找最佳前缀匹配
    QString bestPrefix;

    for (auto it = m_resultsCache.begin(); it != m_resultsCache.end(); ++it) {
        // 找到所有是当前查询前缀的缓存项
        if (searchText.startsWith(it.key())) {
            // 选择最长的前缀（最接近当前查询的）
            if (bestPrefix.isEmpty() || it.key().length() > bestPrefix.length()) {
                bestPrefix = it.key();
            }
        }
    }

    if (!bestPrefix.isEmpty()) {
        qDebug() << "===> search from prefix cache(del): " << bestPrefix << " for " << searchText;
        results = filterLocalResults(m_resultsCache[bestPrefix], searchText);
        addToCache(searchText, results);
        updateCacheUsage(bestPrefix);   // 更新基础缓存的使用情况
        return true;
    }

    return false;
}

void NewSearchManager::setCacheSize(int size)
{
    if (size > 0) {
        m_maxCacheSize = size;

        // 如果当前缓存超过新的大小限制，清理多余的缓存
        while (m_resultsCache.size() > m_maxCacheSize) {
            // 移除最久未使用的缓存项
            QString oldestKey = m_cacheUsageOrder.takeLast();
            m_resultsCache.remove(oldestKey);
        }
    }
}

void NewSearchManager::addToCache(const QString &key, const QStringList &results)
{
    //   QWriteLocker writeLock(&m_cacheLock);

    // 如果缓存已满，移除最久未使用的项
    if (m_resultsCache.size() >= m_maxCacheSize && !m_resultsCache.contains(key)) {
        QString oldestKey = m_cacheUsageOrder.takeLast();
        m_resultsCache.remove(oldestKey);
    }

    // 添加新缓存项
    m_resultsCache[key] = results;
    updateCacheUsage(key);
}

void NewSearchManager::updateCacheUsage(const QString &key)
{
    // 注意：此方法应在持有m_cacheLock的写锁时调用
    m_cacheUsageOrder.removeAll(key);
    m_cacheUsageOrder.prepend(key);
}

QStringList NewSearchManager::searchSync(const QString &searchPath, const QString &searchText)
{
    QMutexLocker locker(&m_mutex);

    // 如果输入为空，直接返回空结果
    if (searchText.isEmpty()) {
        return QStringList();
    }

    // 尝试从缓存获取结果
    {
        //   QReadLocker readLock(&m_cacheLock);
        QStringList cachedResults;
        if (!m_lastSearchText.isEmpty() && tryGetFromCache(searchText, cachedResults)) {
            qDebug() << "===> searchSync: using cache for " << searchText;
            return cachedResults;
        }
    }

    // 如果没有缓存，直接调用搜索器的同步搜索方法
    QMutexLocker searcherLocker(&m_searcherMutex);
    if (!m_searcher) {
        qWarning() << "Searcher not initialized";
        return QStringList();
    }

    // 执行同步搜索
    QStringList results = m_searcher->searchSync(searchPath, searchText);

    // 缓存结果
    if (!results.isEmpty()) {
        addToCache(searchText, results);
        m_lastSearchText = searchText;
    }

    return results;
}

void NewSearchManager::ensureTimerInCurrentThread()
{
    if (m_timerThreadId != QThread::currentThreadId()) {
        // 如果线程ID不匹配，停止旧定时器并在当前线程创建新定时器
        if (m_debounceTimer.isActive()) {
            m_debounceTimer.stop();
        }

        // 更新线程ID
        m_timerThreadId = QThread::currentThreadId();

        // 重新初始化定时器
        m_debounceTimer.moveToThread(QThread::currentThread());
    }
}
