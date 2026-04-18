// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ANALYZERFACTORY_H
#define ANALYZERFACTORY_H

#include <lucene++/LuceneHeaders.h>
#include <QString>
#include <QHash>
#include <functional>

/**
 * @brief Analyzer info structure
 */
struct AnalyzerInfo {
    QString id;           // Unique identifier (e.g., "standard", "chinese")
    QString displayName;  // Display name for UI
    QString description;  // Description of the analyzer
    std::function<Lucene::AnalyzerPtr()> creator;  // Factory function to create analyzer
};

/**
 * @brief Factory class for creating Lucene analyzers
 * This class provides a registry for different analyzers and allows easy extension
 */
class AnalyzerFactory
{
public:
    static AnalyzerFactory* instance();

    // Register a new analyzer type
    void registerAnalyzer(const QString& id,
                          const QString& displayName,
                          const QString& description,
                          std::function<Lucene::AnalyzerPtr()> creator);

    // Get analyzer by ID
    Lucene::AnalyzerPtr createAnalyzer(const QString& id) const;

    // Get analyzer info
    AnalyzerInfo getAnalyzerInfo(const QString& id) const;

    // Get all registered analyzer IDs
    QStringList registeredAnalyzers() const;

    // Check if analyzer exists
    bool hasAnalyzer(const QString& id) const;

private:
    AnalyzerFactory();
    ~AnalyzerFactory() = default;

    void registerBuiltInAnalyzers();

    QHash<QString, AnalyzerInfo> m_analyzers;
};

#endif // ANALYZERFACTORY_H
