// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INDEXMANAGER_H
#define INDEXMANAGER_H

#include "analyzerfactory.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QDateTime>

#include <lucene++/LuceneHeaders.h>

/**
 * @brief Search result structure
 */
struct SearchResult {
    QString filePath;
    QString filename;
    QString modifiedTime;
    double score;
    bool isHidden;
};

/**
 * @brief Index statistics
 */
struct IndexStats {
    qint64 totalDocuments { 0 };
    qint64 indexSizeBytes { 0 };
    QDateTime lastUpdateTime;
    bool exists { false };
};

/**
 * @brief Index manager for creating and searching Lucene indexes
 * Each analyzer type has its own index directory
 */
class IndexManager : public QObject
{
    Q_OBJECT

public:
    explicit IndexManager(const QString& analyzerId, QObject* parent = nullptr);
    ~IndexManager() override;

    // Index directory management
    QString indexDirectory() const;
    void setSourceDirectory(const QString& path);
    QString sourceDirectory() const;

    // Persist source directory
    bool saveConfig() const;
    bool loadConfig();

    // Index operations
    bool createIndex();
    bool indexExists() const;
    bool deleteIndex();

    // Search operations
    QList<SearchResult> search(const QString& query, int maxResults = 100);
    qint64 lastSearchTimeMs() const;

    // Statistics
    IndexStats getIndexStats() const;

    // Progress
    qint64 processedCount() const { return m_processedCount; }
    qint64 totalCount() const { return m_totalCount; }

signals:
    void progressChanged(qint64 processed, qint64 total);
    void indexingFinished(bool success, const QString& message);

private:
    void traverseDirectory(const QString& path, QStringList& files);
    Lucene::DocumentPtr createDocument(const QString& filePath);
    QString extractFileContent(const QString& filePath);
    QString configFilePath() const;

    QString m_analyzerId;
    QString m_sourceDirectory;
    Lucene::AnalyzerPtr m_analyzer;

    qint64 m_processedCount { 0 };
    qint64 m_totalCount { 0 };
    qint64 m_lastSearchTimeMs { 0 };

    static QString s_baseIndexDir;
};

#endif // INDEXMANAGER_H
