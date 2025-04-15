// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "traversaldirthreadmanager.h"
#include <dfm-base/base/schemefactory.h>
#include <dfm-base/file/local/localdiriterator.h>
#include <dfm-base/utils/fileutils.h>

#include <QElapsedTimer>
#include <QDebug>

typedef QList<QSharedPointer<DFMBASE_NAMESPACE::SortFileInfo>>& SortInfoList;

using namespace dfmbase;
using namespace dfmplugin_workspace;
USING_IO_NAMESPACE

TraversalDirThreadManager::TraversalDirThreadManager(const QUrl &url,
                                                     const QStringList &nameFilters,
                                                     QDir::Filters filters,
                                                     QDirIterator::IteratorFlags flags,
                                                     QObject *parent)
    : TraversalDirThread(url, nameFilters, filters, flags, parent)
{
    qRegisterMetaType<QList<FileInfoPointer>>();
    qRegisterMetaType<FileInfoPointer>();
    qRegisterMetaType<QList<SortInfoPointer>>();
    qRegisterMetaType<SortInfoPointer>();
    traversalToken = QString::number(quintptr(this), 16);
}

TraversalDirThreadManager::~TraversalDirThreadManager()
{
    quit();
    wait();
    if (future) {
        future->deleteLater();
        future = nullptr;
    }
}

void TraversalDirThreadManager::setSortAgruments(const Qt::SortOrder order, const Global::ItemRoles sortRole, const bool isMixDirAndFile)
{
    sortOrder = order;
    this->isMixDirAndFile = isMixDirAndFile;
    switch (sortRole) {
    case Global::ItemRoles::kItemFileDisplayNameRole:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileName;
        break;
    case Global::ItemRoles::kItemFileSizeRole:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileSize;
        break;
    case Global::ItemRoles::kItemFileLastReadRole:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileLastRead;
        break;
    case Global::ItemRoles::kItemFileLastModifiedRole:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareFileLastModified;
        break;
    default:
        this->sortRole = dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareDefault;
    }
}

void TraversalDirThreadManager::setTraversalToken(const QString &token)
{
    traversalToken = token;
}

void TraversalDirThreadManager::start()
{
    running = true;
    if (this->sortRole != dfmio::DEnumerator::SortRoleCompareFlag::kSortRoleCompareDefault
            && dirIterator->oneByOne())
        dirIterator->setProperty("QueryAttributes","standard::name,standard::type,standard::size,\
                                  standard::size,standard::is-symlink,standard::symlink-target,access::*,time::*");
    auto local = dirIterator.dynamicCast<LocalDirIterator>();
    if (local && local->oneByOne()) {
        future = local->asyncIterator();
        if (future) {
            connect(future, &DEnumeratorFuture::asyncIteratorOver, this, &TraversalDirThreadManager::onAsyncIteratorOver);
            future->startAsyncIterator();
            return;
        }
    }

    TraversalDirThread::start();
}

bool TraversalDirThreadManager::isRunning() const
{
    return running;
}

void TraversalDirThreadManager::onAsyncIteratorOver()
{
    Q_EMIT iteratorInitFinished();
    TraversalDirThread::start();
}

void TraversalDirThreadManager::run()
{
    if (dirIterator.isNull()) {
        emit traversalFinished(traversalToken);
        running = false;
        return;
    }

    QElapsedTimer timer;
    timer.start();
    fmInfo() << "dir query start, url: " << dirUrl;

    int count = 0;
    if (!dirIterator->oneByOne()) {
        const QList<SortInfoPointer> &fileList = iteratorAll();
        count = fileList.count();
        fmInfo() << "local dir query end, file count: " << count << " url: " << dirUrl << " elapsed: " << timer.elapsed();
        createFileInfo(fileList);
    } else {
        count = iteratorOneByOne(timer);
        fmInfo() << "dir query end, file count: " << count << " url: " << dirUrl << " elapsed: " << timer.elapsed();
    }
    running = false;
}

int TraversalDirThreadManager::iteratorOneByOne(const QElapsedTimer &timere)
{
    dirIterator->cacheBlockIOAttribute();
    fmInfo() << "cacheBlockIOAttribute finished, url: " << dirUrl << " elapsed: " << timere.elapsed();
    if (stopFlag) {
        emit traversalFinished(traversalToken);
        return 0;
    }

    if (!dirIterator->initIterator()) {
        fmWarning() << "dir iterator init failed !! url : " << dirUrl;
        emit traversalFinished(traversalToken);
        return 0;
    }

    if (!future)
        Q_EMIT iteratorInitFinished();

    if (!timer)
        timer = new QElapsedTimer();

    timer->restart();

    QList<FileInfoPointer> childrenList;   // 当前遍历出来的所有文件
    QList<SortInfoPointer> updateList;     // 当前需要更新的文件 - 修改为SortInfoPointer
    QSet<QUrl> urls;
    int filecount = 0;
    bool noCache = dirIterator->property("FileInfoNoCache").toBool();
    while (dirIterator->hasNext()) {
        if (stopFlag)
            break;

        if (timer->elapsed() > timeCeiling || childrenList.count() > countCeiling || updateList.count() > countCeiling) {
            emit updateChildrenManager(childrenList, traversalToken);
            if (!updateList.isEmpty())
                emit updateChildrenInfo(updateList, traversalToken);
            timer->restart();
            childrenList.clear();
            updateList.clear();
        }

        // 调用一次fileinfo进行文件缓存
        const auto &fileUrl = dirIterator->next();
        if (!fileUrl.isValid())
            continue;

        auto fileInfo = dirIterator->fileInfo();
        if (fileUrl.isValid() && !fileInfo) {
            fileInfo = InfoFactory::create<FileInfo>(fileUrl,
                                                     noCache ? Global::CreateFileInfoType::kCreateFileInfoAutoNoCache
                                                             : Global::CreateFileInfoType::kCreateFileInfoAuto);
        } else if (!fileInfo.isNull() && !noCache) {
            InfoFactory::cacheFileInfo(fileInfo);
        }

        if (!fileInfo)
            continue;

        if (urls.contains(fileUrl)) {
            // 创建SortInfoPointer而不是使用FileInfoPointer
            auto sortInfo = QSharedPointer<SortFileInfo>(new SortFileInfo());
            sortInfo->setUrl(fileUrl);
            
            // 如果有其他需要从fileInfo复制到sortInfo的属性,可以在这里添加
            
            updateList.append(sortInfo);
            continue;
        }

        urls.insert(fileUrl);

        childrenList.append(fileInfo);
        filecount++;
    }

    if (childrenList.length() > 0)
        emit updateChildrenManager(childrenList, traversalToken);
    
    if (updateList.length() > 0)
        emit updateChildrenInfo(updateList, traversalToken);

    if (!dirIterator->property(IteratorProperty::kKeepOrder).toBool())
        emit traversalRequestSort(traversalToken);

    emit traversalFinished(traversalToken);

    return filecount;
}

QList<SortInfoPointer> TraversalDirThreadManager::iteratorAll()
{
    QVariantMap args;
    args.insert("sortRole",
                QVariant::fromValue(sortRole));
    args.insert("mixFileAndDir", isMixDirAndFile);
    args.insert("sortOrder", sortOrder);
    dirIterator->setArguments(args);
    if (!dirIterator->initIterator()) {
        fmWarning() << "dir iterator init failed !! url : " << dirUrl;
        emit traversalFinished(traversalToken);
        return {};
    }
    Q_EMIT iteratorInitFinished();
    
    // Get the initial list of files
    auto fileList = dirIterator->sortFileInfoList();
    
    // Emit the initial file list
    emit updateLocalChildren(fileList, sortRole, sortOrder, isMixDirAndFile, traversalToken);
    
    // Check if the iterator is waiting for more updates (search still in progress, etc.)
    while (dirIterator->isWaitingForUpdates()) {
        fileList = dirIterator->sortFileInfoList();
        if (!fileList.isEmpty())
            emit updateChildrenInfo(fileList, traversalToken);
    }
    
    // Iterator is not waiting for updates, so signal that we're done
    emit traversalFinished(traversalToken);
    qWarning() << "====================== send traversalFinished";

    return fileList;
}

void TraversalDirThreadManager::createFileInfo(const QList<SortInfoPointer> &list)
{
    for (const SortInfoPointer &sortInfo : list) {
        if (stopFlag)
            return;
        const QUrl &url = sortInfo->fileUrl();
        InfoFactory::create<FileInfo>(url);
    }
}
