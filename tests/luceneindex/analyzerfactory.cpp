// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "analyzerfactory.h"

#include <fulltext/chineseanalyzer.h>
#include "ngramanalyzer.h"

#include <StandardAnalyzer.h>
#include <SimpleAnalyzer.h>
#include <KeywordAnalyzer.h>
#include <WhitespaceAnalyzer.h>

AnalyzerFactory* AnalyzerFactory::instance()
{
    static AnalyzerFactory inst;
    return &inst;
}

AnalyzerFactory::AnalyzerFactory()
{
    registerBuiltInAnalyzers();
}

void AnalyzerFactory::registerAnalyzer(const QString& id,
                                       const QString& displayName,
                                       const QString& description,
                                       std::function<Lucene::AnalyzerPtr()> creator)
{
    AnalyzerInfo info;
    info.id = id;
    info.displayName = displayName;
    info.description = description;
    info.creator = creator;
    m_analyzers[id] = info;
}

Lucene::AnalyzerPtr AnalyzerFactory::createAnalyzer(const QString& id) const
{
    if (!m_analyzers.contains(id))
        return nullptr;

    return m_analyzers[id].creator();
}

AnalyzerInfo AnalyzerFactory::getAnalyzerInfo(const QString& id) const
{
    return m_analyzers.value(id);
}

QStringList AnalyzerFactory::registeredAnalyzers() const
{
    return m_analyzers.keys();
}

bool AnalyzerFactory::hasAnalyzer(const QString& id) const
{
    return m_analyzers.contains(id);
}

void AnalyzerFactory::registerBuiltInAnalyzers()
{
    using namespace Lucene;

    // Standard Analyzer - Basic tokenizer, lowercase filter
    registerAnalyzer(
        "standard",
        "Standard Analyzer",
        "Standard tokenizer with lowercase filter, suitable for English text",
        []() { return newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_CURRENT); }
    );

    // Chinese Analyzer - Optimized for Chinese text
    registerAnalyzer(
        "chinese",
        "Chinese Analyzer",
        "Chinese tokenizer, optimized for Chinese text segmentation",
        []() { return newLucene<ChineseAnalyzer>(); }
    );

    // Simple Analyzer - Letter tokenizer, lowercase
    registerAnalyzer(
        "simple",
        "Simple Analyzer",
        "Letter tokenizer with lowercase filter, splits on non-letters",
        []() { return newLucene<SimpleAnalyzer>(); }
    );

    // Keyword Analyzer - Treats entire content as single token
    registerAnalyzer(
        "keyword",
        "Keyword Analyzer",
        "Treats entire content as a single token, useful for exact matching",
        []() { return newLucene<KeywordAnalyzer>(); }
    );

    // Whitespace Analyzer - Splits on whitespace
    registerAnalyzer(
        "whitespace",
        "Whitespace Analyzer",
        "Splits text on whitespace characters only",
        []() { return newLucene<WhitespaceAnalyzer>(); }
    );

    // NGram Analyzer - Breaks text into n-grams (2-4)
    registerAnalyzer(
        "ngram",
        "NGram Analyzer (2-4)",
        "Generates n-gram tokens of sizes 2 to 4, suitable for partial/fuzzy matching",
        []() { return newLucene<NGramAnalyzer>(2, 2); }
    );
}
