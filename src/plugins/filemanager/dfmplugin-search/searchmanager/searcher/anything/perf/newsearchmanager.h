#ifndef NEWSEARCHMANAGER_H
#define NEWSEARCHMANAGER_H

#include "searcherinterface.h"

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QReadWriteLock>
#include <QThreadPool>

class NewSearchManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(NewSearchManager)
public:
    static NewSearchManager &instance();
    // 设置搜索器
    void setSearcher(SearcherInterface *searcher);

    // 异步处理用户输入
    void processUserInput(const QString &searchPath, const QString &searchText);

    // 新增: 同步搜索方法
    QStringList searchSync(const QString &searchPath, const QString &searchText);

    // 清除缓存
    void clearCache();

    // 设置缓存大小
    void setCacheSize(int size);

signals:
    // 搜索结果信号
    void searchResultsReady(const QStringList &results);
    void searchError(const QString &errorMessage);

private slots:
    void executeSearch();
    void onSearchFinished(const QString &query, const QStringList &results);
    void onSearchFailed(const QString &query, const QString &errorMessage);

private:
    explicit NewSearchManager(QObject *parent = nullptr);

    // 确保定时器在当前线程正常工作
    void ensureTimerInCurrentThread();

    // 输入变化类型
    enum class InputChangeType { Addition,
                                 Deletion,
                                 Replacement,
                                 Unknown };

    // 分析输入变化
    InputChangeType analyzeInputChange(const QString &oldText, const QString &newText);

    // 在本地过滤结果
    QStringList filterLocalResults(const QStringList &sourceResults, const QString &query);

    // 确定防抖延迟
    int determineDebounceDelay(const QString &text);

    // 判断是否需要延迟搜索
    bool shouldDelaySearch(const QString &text);

    QString getFileName(const QString &filePath);

    // 从缓存中查找结果
    bool tryGetFromCache(const QString &searchText, QStringList &results);

    // 处理增量搜索（当输入是添加字符时）
    bool handleIncrementalSearch(const QString &searchText, QStringList &results);

    // 处理删除操作搜索
    bool handleDeletionSearch(const QString &searchText, QStringList &results);

    // 添加到缓存
    void addToCache(const QString &key, const QStringList &results);

    // 更新缓存使用情况
    void updateCacheUsage(const QString &key);

    SearcherInterface *m_searcher;
    bool m_ownsSearcher;
    QTimer m_debounceTimer;
    QString m_pendingSearchPath;
    QString m_pendingSearchText;
    QString m_lastSearchText;
    QDateTime m_lastSearchTime;

    // 标识定时器所属线程ID
    Qt::HANDLE m_timerThreadId;

    // 缓存结构
    QMap<QString, QStringList> m_resultsCache;
    QList<QString> m_cacheUsageOrder;   // 用于LRU，使用QList替代QLinkedList
    int m_maxCacheSize = 50;   // 默认缓存大小
    int m_throttleInterval = 300;   // 毫秒

    // 当前搜索工作目录
    QString m_currentSearchPath;

    // 互斥锁保护成员变量
    mutable QMutex m_mutex;   // 保护一般成员变量
    mutable QReadWriteLock m_cacheLock;   // 保护缓存相关操作
    mutable QMutex m_searcherMutex;   // 保护搜索器
};

#endif   // NEWSEARCHMANAGER_H
