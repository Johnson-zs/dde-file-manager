#include "newsearchmanager.h"
#include "newanythingsearcher.h"

#include <QFileInfo>
#include <QtConcurrent>
#include <QRegularExpression>
#include <QDebug>
#include <QThread>

#include <lucene++/LuceneHeaders.h>

using namespace Lucene;
namespace {

// 获取索引目录路径：/run/user/[当前用户id]/deepin-anything-server
static QString getIndexDirectory()
{
    QString indexDir = QString("/run/user/%1/deepin-anything-server").arg(getuid());
    return indexDir;
}

static QString getHomeDirectory()
{
    QString homeDir;

    if (QFileInfo::exists("/data/home")) {
        homeDir = "/data";
    } else if (QFileInfo::exists("/persistent/home")) {
        homeDir = "/persistent";
    }

    homeDir.append(QDir::homePath());

    return homeDir;
}

static QStringList search2(const QString &orginPath, const QString &key, bool nrt)
{
    QString keywords = key;
    QString path = orginPath;
    if (path.startsWith(QDir::homePath()))
        path.replace(0, QDir::homePath().length(), getHomeDirectory());

    if (keywords.isEmpty()) {
        return {};
    }

    // 原始词条
    String query_terms = StringUtils::toUnicode(keywords.toStdString());

    // 给普通 parser 用
    if (keywords.at(0) == QChar('*') || keywords.at(0) == QChar('?')) {
        keywords = keywords.mid(1);
    }

    try {
        int32_t max_results;

        // 获取索引目录
        QString indexDir = getIndexDirectory();
        qDebug() << "搜索索引目录:" << indexDir;

        // 打开索引目录
        FSDirectoryPtr directory = FSDirectory::open(StringUtils::toUnicode(indexDir.toStdString()));

        // 检查索引是否存在
        if (!IndexReader::indexExists(directory)) {
            qWarning() << "索引不存在:" << indexDir;
            return QStringList();
        }

        // 打开索引读取器
        IndexReaderPtr reader = IndexReader::open(directory, true);
        if (reader->numDocs() == 0) {
            qWarning() << "索引为空，没有文档";
            return QStringList();
        }

        // 创建搜索器
        SearcherPtr searcher = newLucene<IndexSearcher>(reader);

        if (reader->numDocs() == 0) {
            qWarning() << "索引为空，没有文档";
            return QStringList();
        }

        max_results = reader->numDocs();

        String queryString = L"*" + StringUtils::toLower(StringUtils::toUnicode(keywords.toStdString())) + L"*";
        TermPtr term = newLucene<Term>(L"file_name", queryString);
        QueryPtr query = newLucene<WildcardQuery>(term);

        auto search_results = searcher->search(query, max_results);

        QStringList results;
        results.reserve(search_results->scoreDocs.size());
        QStringList dirs, files;
        dirs.reserve(search_results->scoreDocs.size() / 2);   // 预估目录数量
        files.reserve(search_results->scoreDocs.size() / 2);   // 预估文件数量

        for (const auto &score_doc : search_results->scoreDocs) {
            DocumentPtr doc = searcher->doc(score_doc->doc);
            auto result = QString::fromStdWString(doc->get(L"full_path"));

            // 先进行路径过滤，避免不必要的类型判断
            if (!result.startsWith(path)) continue;

            // 延迟获取类型，只有通过路径过滤的才需要判断
            auto type = QString::fromStdWString(doc->get(L"file_type"));

            // 使用移动语义 + 条件分类
            if (type == "dir") {
                dirs.append(std::move(result));   // 目录列表
            } else {
                files.append(std::move(result));   // 文件列表
            }
        }

        // 合并结果（O(1) 时间复杂度操作）
        results = std::move(dirs) + std::move(files);

        return results;
    } catch (const LuceneException &e) {

        return {};
    }
}

}   // namespace

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
    // 如果输入为空，直接返回空结果
    if (searchText.isEmpty()) {
        return QStringList();
    }

    // 尝试从缓存获取结果
    {
        QMutexLocker guard(&m_mutex);
        QStringList cachedResults;
        if (!m_lastSearchText.isEmpty() && tryGetFromCache(searchText, cachedResults)) {
            qDebug() << "===> searchSync: using cache for " << searchText;
            return cachedResults;
        }
    }

    QStringList results = ::search2(searchPath, searchText, true);

    // AnythingSearcher searcher;
    // // 执行同步搜索
    // QStringList results = searcher.searchSync(searchPath, searchText);

    // 缓存结果

    if (!results.isEmpty()) {
        QMutexLocker guard(&m_mutex);
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
