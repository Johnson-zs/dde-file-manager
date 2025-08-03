// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEDATAMANAGER_H
#define FILEDATAMANAGER_H

#include "dfmplugin_workspace_global.h"

#include <dfm-base/interfaces/fileinfo.h>
#include <dfm-base/base/application/application.h>

#include <QObject>
#include <QMap>

namespace dfmplugin_workspace {

class DirectoryManager;

class FileDataManager : public QObject
{
    Q_OBJECT
public:
    static FileDataManager *instance();

    void initMntedDevsCache();
    
    /**
     * @brief Fetch or create DirectoryManager for the given URL
     * 
     * This method provides DirectoryManager instances for directory management,
     * replacing the old RootInfo-based system with a clean, thread-safe architecture.
     * 
     * @param url Directory URL to get manager for
     * @return DirectoryManager* Manager instance for the URL
     */
    DirectoryManager *fetchDirectoryManager(const QUrl &url);

    /**
     * @brief Create DirectoryManager for the given URL
     * 
     * Creates a new DirectoryManager instance and registers it in the internal map.
     * 
     * @param url Directory URL to create manager for
     * @return DirectoryManager* New manager instance
     */
    DirectoryManager *createDirectoryManager(const QUrl &url);

    /**
     * @brief Clean up DirectoryManager for specific URL
     * 
     * @param url Directory URL to clean up
     */
    void cleanDirectoryManager(const QUrl &url);

    /**
     * @brief Set file watcher active state
     * 
     * @param rootUrl Directory root URL
     * @param childUrl Child file URL
     * @param active Whether to activate watching
     */
    void setFileActive(const QUrl &rootUrl, const QUrl &childUrl, bool active);

public Q_SLOTS:
    void onAppAttributeChanged(DFMBASE_NAMESPACE::Application::ApplicationAttribute aa, const QVariant &value);
    void onHandleFileDeleted(const QUrl url);

private:
    explicit FileDataManager(QObject *parent = nullptr);
    ~FileDataManager();

    bool checkNeedCache(const QUrl &url);

    // New architecture: DirectoryManager mapping
    QMap<QUrl, DirectoryManager *> directoryManagerMap {};

    bool isMixFileAndFolder { false };

    // scheme in cacheDataSchemes will have cache
    QList<QString> cacheDataSchemes {};
};

}

#endif   // FILEDATAMANAGER_H
