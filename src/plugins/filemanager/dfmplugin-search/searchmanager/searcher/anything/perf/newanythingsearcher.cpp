#include "newanythingsearcher.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusPendingCallWatcher>
#include <QDebug>

namespace {
QString extract(const QString &pathWithMetadata)
{
    const qsizetype pos = pathWithMetadata.indexOf("<\\>");
    return (pos != -1) ? pathWithMetadata.left(pos) : pathWithMetadata;
}

QStringList batchExtract(const QStringList &paths)
{
    QStringList result;
    result.reserve(paths.size());

    std::transform(paths.cbegin(), paths.cend(),
                   std::back_inserter(result),
                   [](const QString &path) { return extract(path); });

    return result;
}
}   // namespace

NewAnythingSearcher::NewAnythingSearcher(QObject *parent)
    : SearcherInterface { parent }, currentRequest(nullptr)
{
    anythingInterface = new QDBusInterface("com.deepin.anything",
                                           "/com/deepin/anything",
                                           "com.deepin.anything",
                                           QDBusConnection::systemBus(),
                                           this);
    anythingInterface->setTimeout(1000);
}

bool NewAnythingSearcher::requestSearch(const QString &path, const QString &text)
{
    if (!anythingInterface->isValid())
        return false;
    if (path.isEmpty() || text.isEmpty())
        return false;

    // 取消先前的请求（如果有）
    cancelSearch();

    // 保存当前查询
    m_currentQuery = text;

    // 使用异步调用
    QDBusPendingCall pendingCall = anythingInterface->asyncCallWithArgumentList("search", { path, text });
    currentRequest = new QDBusPendingCallWatcher(pendingCall, this);

    connect(currentRequest, &QDBusPendingCallWatcher::finished,
            this, &NewAnythingSearcher::onRequestFinished);

    return true;
}

void NewAnythingSearcher::cancelSearch()
{
    if (currentRequest) {
        disconnect(currentRequest, nullptr, this, nullptr);
        delete currentRequest;
        currentRequest = nullptr;
        m_currentQuery.clear();   // 清除当前查询
    }
}

void NewAnythingSearcher::onRequestFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QStringList> reply = *watcher;

    if (reply.isError()) {
        emit searchFailed(m_currentQuery,   // 使用保存的查询
                          QString("搜索失败: %1").arg(reply.error().message()));
    } else {
        QStringList results = batchExtract(reply.value());
        emit searchFinished(m_currentQuery, results);   // 使用保存的查询
    }

    // 清理
    watcher->deleteLater();
    currentRequest = nullptr;
}

QStringList NewAnythingSearcher::searchSync(const QString &path, const QString &text)
{
    if (!anythingInterface->isValid() || path.isEmpty() || text.isEmpty())
        return QStringList();

    // 直接使用同步调用
    QDBusReply<QStringList> reply = anythingInterface->call("search", path, text);

    if (!reply.isValid()) {
        qWarning() << "同步搜索失败:" << reply.error().message();
        return QStringList();
    }

    // 处理结果
    return batchExtract(reply.value());
}
