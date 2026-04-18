// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include <DTextEncoding>

#include <docparser.h>

#include <lucene++/LuceneHeaders.h>
#include <lucene++/LuceneException.h>

QString IndexManager::s_baseIndexDir;

IndexManager::IndexManager(const QString& analyzerId, QObject* parent)
    : QObject(parent)
    , m_analyzerId(analyzerId)
{
    if (s_baseIndexDir.isEmpty()) {
        s_baseIndexDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                         + "/test-index";
    }

    m_analyzer = AnalyzerFactory::instance()->createAnalyzer(analyzerId);
}

IndexManager::~IndexManager()
{
}

QString IndexManager::indexDirectory() const
{
    return s_baseIndexDir + "/" + m_analyzerId;
}

void IndexManager::setSourceDirectory(const QString& path)
{
    m_sourceDirectory = path;
}

QString IndexManager::sourceDirectory() const
{
    return m_sourceDirectory;
}

bool IndexManager::createIndex()
{
    if (!m_analyzer) {
        emit indexingFinished(false, "Invalid analyzer");
        return false;
    }

    if (m_sourceDirectory.isEmpty()) {
        emit indexingFinished(false, "Source directory not set");
        return false;
    }

    QDir sourceDir(m_sourceDirectory);
    if (!sourceDir.exists()) {
        emit indexingFinished(false, "Source directory does not exist");
        return false;
    }

    // Collect all files first
    QStringList files;
    traverseDirectory(m_sourceDirectory, files);
    m_totalCount = files.size();
    m_processedCount = 0;

    // Create index directory
    QString indexPath = indexDirectory();
    QDir().mkpath(indexPath);

    try {
        using namespace Lucene;

        IndexWriterPtr writer = newLucene<IndexWriter>(
            FSDirectory::open(indexPath.toStdWString()),
            m_analyzer,
            true,  // Create new index
            IndexWriter::MaxFieldLengthUNLIMITED
        );

        QElapsedTimer timer;
        timer.start();

        for (const QString& file : files) {
            try {
                DocumentPtr doc = createDocument(file);
                if (doc) {
                    writer->addDocument(doc);
                }
            } catch (...) {
                qWarning() << "Failed to index file:" << file;
            }

            m_processedCount++;
            if (m_processedCount % 100 == 0 || m_processedCount == m_totalCount) {
                emit progressChanged(m_processedCount, m_totalCount);
            }
        }

        writer->commit();
        writer->optimize();
        writer->close();

        QString msg = QString("Indexed %1 files in %2ms")
                      .arg(m_processedCount)
                      .arg(timer.elapsed());
        emit indexingFinished(true, msg);
        return true;

    } catch (const Lucene::LuceneException& e) {
        QString err = QString("Lucene error: %1").arg(QString::fromStdWString(e.getError()));
        emit indexingFinished(false, err);
        return false;
    } catch (const std::exception& e) {
        emit indexingFinished(false, QString("Error: %1").arg(e.what()));
        return false;
    }

    return false;
}

bool IndexManager::indexExists() const
{
    QDir dir(indexDirectory());
    return dir.exists() && !dir.entryList(QDir::Files).isEmpty();
}

bool IndexManager::deleteIndex()
{
    QString indexPath = indexDirectory();
    QDir dir(indexPath);

    if (!dir.exists())
        return true;

    bool success = dir.removeRecursively();
    return success;
}

QList<SearchResult> IndexManager::search(const QString& query, int maxResults)
{
    QList<SearchResult> results;

    if (!m_analyzer || !indexExists()) {
        return results;
    }

    try {
        using namespace Lucene;

        QString indexPath = indexDirectory();
        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(indexPath.toStdWString()), true);
        IndexSearcherPtr searcher = newLucene<IndexSearcher>(reader);

        // Create query parser for contents field
        QueryParserPtr parser = newLucene<QueryParser>(LuceneVersion::LUCENE_CURRENT, L"contents", m_analyzer);
        QueryPtr queryObj = parser->parse(query.toStdWString());

        QElapsedTimer timer;
        timer.start();

        TopDocsPtr topDocs = searcher->search(queryObj, maxResults);

        m_lastSearchTimeMs = timer.elapsed();

        for (int32_t i = 0; i < topDocs->totalHits && i < maxResults; ++i) {
            DocumentPtr doc = searcher->doc(topDocs->scoreDocs[i]->doc);

            SearchResult result;
            result.filePath = QString::fromStdWString(doc->get(L"path"));
            result.filename = QString::fromStdWString(doc->get(L"filename"));
            result.modifiedTime = QString::fromStdWString(doc->get(L"modified"));
            result.score = topDocs->scoreDocs[i]->score;
            result.isHidden = (QString::fromStdWString(doc->get(L"is_hidden")) == "Y");

            results.append(result);
        }

        reader->close();

    } catch (const Lucene::LuceneException& e) {
        qWarning() << "Search error:" << QString::fromStdWString(e.getError());
    } catch (const std::exception& e) {
        qWarning() << "Search error:" << e.what();
    }

    return results;
}

qint64 IndexManager::lastSearchTimeMs() const
{
    return m_lastSearchTimeMs;
}

IndexStats IndexManager::getIndexStats() const
{
    IndexStats stats;
    QString indexPath = indexDirectory();
    QDir dir(indexPath);

    stats.exists = indexExists();
    if (!stats.exists)
        return stats;

    // Calculate index size
    qint64 totalSize = 0;
    for (const QFileInfo& file : dir.entryInfoList(QDir::Files)) {
        totalSize += file.size();
    }
    stats.indexSizeBytes = totalSize;

    // Get document count
    try {
        using namespace Lucene;
        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(indexPath.toStdWString()), true);
        stats.totalDocuments = reader->numDocs();
        reader->close();
    } catch (...) {
        stats.totalDocuments = 0;
    }

    return stats;
}

void IndexManager::traverseDirectory(const QString& path, QStringList& files)
{
    QDir dir(path);
    QDir::Filters filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks;

    for (const QFileInfo& info : dir.entryInfoList(filters)) {
        if (info.isDir()) {
            traverseDirectory(info.absoluteFilePath(), files);
        } else if (info.isFile()) {
            // Check file extension
            QString suffix = info.suffix().toLower();
            static const QStringList supportedExts = {
                "txt", "md", "cpp", "h", "c", "hpp", "cc", "cxx",
                "py", "java", "js", "ts", "json", "xml", "html", "htm",
                "css", "sh", "bash", "conf", "cfg", "ini", "yaml", "yml",
                "doc", "docx", "pdf", "xls", "xlsx", "ppt", "pptx",
                "odt", "ods", "odp", "rtf"
            };

            if (supportedExts.contains(suffix)) {
                files.append(info.absoluteFilePath());
            }
        }
    }
}

Lucene::DocumentPtr IndexManager::createDocument(const QString& filePath)
{
    using namespace Lucene;

    DocumentPtr doc = newLucene<Document>();

    QFileInfo fileInfo(filePath);

    // Path field
    doc->add(newLucene<Field>(L"path", filePath.toStdWString(),
                              Field::STORE_YES, Field::INDEX_NOT_ANALYZED));

    // Filename field
    doc->add(newLucene<Field>(L"filename", fileInfo.fileName().toStdWString(),
                              Field::STORE_YES, Field::INDEX_ANALYZED));

    // Modified time
    QDateTime modifyTime = fileInfo.lastModified();
    QString modifyEpoch = QString::number(modifyTime.toSecsSinceEpoch());
    doc->add(newLucene<Field>(L"modified", modifyEpoch.toStdWString(),
                              Field::STORE_YES, Field::INDEX_NOT_ANALYZED));

    // Hidden flag
    QString hiddenTag = fileInfo.isHidden() ? "Y" : "N";
    doc->add(newLucene<Field>(L"is_hidden", hiddenTag.toStdWString(),
                              Field::STORE_YES, Field::INDEX_NOT_ANALYZED));

    // Content
    QString content = extractFileContent(filePath);
    if (!content.isEmpty()) {
        doc->add(newLucene<Field>(L"contents", content.toStdWString(),
                                  Field::STORE_NO, Field::INDEX_ANALYZED));
    }

    return doc;
}

QString IndexManager::extractFileContent(const QString& filePath)
{
    // Try using DocParser for document formats
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    static const QStringList docFormats = { "doc", "docx", "pdf", "xls", "xlsx",
                                            "ppt", "pptx", "odt", "ods", "odp", "rtf" };

    if (docFormats.contains(suffix)) {
        try {
            std::string content = DocParser::convertFile(filePath.toStdString());
            return QString::fromStdString(content);
        } catch (...) {
            // Fall through to plain text handling
        }
    }

    // Plain text files
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QByteArray data = file.read(10 * 1024 * 1024);  // Max 10MB
    file.close();

    // Detect encoding
    QByteArray encoding = Dtk::Core::DTextEncoding::detectFileEncoding(filePath);

    QString content;
    if (encoding.toLower() == "utf-8" || encoding.isEmpty()) {
        content = QString::fromUtf8(data);
    } else {
        // Try to convert from detected encoding
        QByteArray converted;
        if (Dtk::Core::DTextEncoding::convertTextEncodingEx(data, converted, "utf-8", encoding)) {
            content = QString::fromUtf8(converted);
        } else {
            content = QString::fromLocal8Bit(data);
        }
    }

    return content;
}

QString IndexManager::configFilePath() const
{
    return s_baseIndexDir + "/" + m_analyzerId + "/config.json";
}

bool IndexManager::saveConfig() const
{
    QString configPath = configFilePath();
    QDir().mkpath(QFileInfo(configPath).absolutePath());

    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save config:" << configPath;
        return false;
    }

    QJsonObject obj;
    obj["sourceDirectory"] = m_sourceDirectory;

    QJsonDocument doc(obj);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool IndexManager::loadConfig()
{
    QString configPath = configFilePath();
    QFile file(configPath);

    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse config:" << error.errorString();
        return false;
    }

    QJsonObject obj = doc.object();
    m_sourceDirectory = obj["sourceDirectory"].toString();

    return !m_sourceDirectory.isEmpty();
}
