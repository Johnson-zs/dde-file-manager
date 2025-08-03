// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filedatamanager.h"
#include "managers/directorymanager.h"
#include "events/workspaceeventcaller.h"

#include <dfm-base/utils/protocolutils.h>
#include <dfm-base/utils/universalutils.h>
#include <dfm-base/utils/watchercache.h>
#include <dfm-base/base/schemefactory.h>
#include <dfm-base/base/device/deviceproxymanager.h>
#include <dfm-base/dfm_log_defines.h>

#include <QApplication>

using namespace dfmbase;
using namespace dfmplugin_workspace;

FileDataManager *FileDataManager::instance()
{
    static FileDataManager ins;
    return &ins;
}

DirectoryManager *FileDataManager::fetchDirectoryManager(const QUrl &url)
{
    if (directoryManagerMap.contains(url))
        return directoryManagerMap.value(url);

    fmDebug() << "Creating new DirectoryManager for URL:" << url.toString();
    return createDirectoryManager(url);
}

DirectoryManager *FileDataManager::createDirectoryManager(const QUrl &url)
{
    fmDebug() << "Creating DirectoryManager for:" << url.toString();
    
    bool needCache = checkNeedCache(url);
    DirectoryManager *manager = new DirectoryManager(url, needCache, this);
    
    directoryManagerMap.insert(url, manager);
    
    fmInfo() << "DirectoryManager created successfully for:" << url.toString() << "cache enabled:" << needCache;
    return manager;
}

void FileDataManager::cleanDirectoryManager(const QUrl &url)
{
    if (!directoryManagerMap.contains(url)) {
        fmDebug() << "No DirectoryManager found for URL:" << url.toString();
        return;
    }
    
    fmInfo() << "Cleaning DirectoryManager for:" << url.toString();
    
    DirectoryManager *manager = directoryManagerMap.take(url);
    if (manager) {
        manager->deleteLater();
    }
}

void FileDataManager::setFileActive(const QUrl &rootUrl, const QUrl &childUrl, bool active)
{
    DirectoryManager *manager = directoryManagerMap.value(rootUrl);
    if (manager) {
        manager->enableWatching(childUrl, active);
        fmDebug() << "Set file active for:" << childUrl.toString() << "active:" << active;
    } else {
        fmWarning() << "No DirectoryManager found for root URL:" << rootUrl.toString();
    }
}

void FileDataManager::onAppAttributeChanged(DFMBASE_NAMESPACE::Application::ApplicationAttribute aa, const QVariant &value)
{
    if (aa == DFMBASE_NAMESPACE::Application::kFileAndDirMixedSort) {
        isMixFileAndFolder = value.toBool();
        fmInfo() << "Mixed file and folder sort changed to:" << isMixFileAndFolder;
    }
}

void FileDataManager::onHandleFileDeleted(const QUrl url)
{
    fmInfo() << "Handling file deletion for:" << url.toString();
    
    // Clean up DirectoryManager if it's for the deleted directory
    if (directoryManagerMap.contains(url)) {
        cleanDirectoryManager(url);
    }
    
    // Notify all managers about the deletion
    for (auto manager : directoryManagerMap.values()) {
        // The manager will handle the deletion through its file watching system
        Q_UNUSED(manager)
    }
}

FileDataManager::FileDataManager(QObject *parent)
    : QObject(parent)
{
    fmDebug() << "FileDataManager initialized";
    
    // Initialize cache schemes
    cacheDataSchemes << "file" << "recent" << "trash" << "computer" << "bookmark";
    
    // Connect to application attribute changes
    connect(Application::instance(), &Application::appAttributeChanged,
            this, &FileDataManager::onAppAttributeChanged);
}

FileDataManager::~FileDataManager()
{
    fmDebug() << "FileDataManager destroying, cleaning up DirectoryManagers";
    
    // Clean up all DirectoryManagers
    for (auto it = directoryManagerMap.begin(); it != directoryManagerMap.end(); ++it) {
        DirectoryManager *manager = it.value();
        if (manager) {
            manager->deleteLater();
        }
    }
    directoryManagerMap.clear();
}

bool FileDataManager::checkNeedCache(const QUrl &url)
{
    const QString &scheme = url.scheme();
    bool needCache = cacheDataSchemes.contains(scheme);
    
    // Special handling for certain schemes
    if (scheme == "smb" || scheme == "ftp" || scheme == "sftp") {
        needCache = true; // Network locations benefit from caching
    }
    
    fmDebug() << "Cache check for URL:" << url.toString() << "scheme:" << scheme << "needs cache:" << needCache;
    return needCache;
}

void FileDataManager::initMntedDevsCache()
{
    fmDebug() << "Initializing mounted devices cache";
    
    // Initialize device cache - this functionality remains the same
    // as it's not directly related to RootInfo
    auto deviceManager = DeviceProxyManager::instance();
    if (deviceManager) {
        // Trigger device enumeration
        deviceManager->getAllBlockIds();
    }
}
