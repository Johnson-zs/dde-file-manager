// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fulltextsearcher.h"
#include "fulltextsearcher_p.h"
#include "fulltext/chineseanalyzer.h"
#include "utils/searchhelper.h"

#include <dfm-base/base/urlroute.h>
#include <dfm-base/base/device/deviceutils.h>
#include <dfm-base/utils/fileutils.h>
#include <dfm-base/base/configs/dconfig/dconfigmanager.h>
#include <dfm-base/base/schemefactory.h>

// Lucune++ headers
#include <FileUtils.h>
#include <FilterIndexReader.h>
#include <FuzzyQuery.h>
#include <QueryWrapperFilter.h>

#include <QRegExp>
#include <QDebug>
#include <QDateTime>
#include <QMetaEnum>
#include <QDir>
#include <QTime>
#include <QUrl>

#include <dirent.h>
#include <exception>
#include <docparser.h>

static constexpr char kFilterFolders[] = "^/(boot|dev|proc|sys|run|lib|usr).*$";
static constexpr char kSupportFiles[] = "(rtf)|(odt)|(ods)|(odp)|(odg)|(docx)|(xlsx)|(pptx)|(ppsx)|(md)|"
                                        "(xls)|(xlsb)|(doc)|(dot)|(wps)|(ppt)|(pps)|(txt)|(pdf)|(dps)|"
                                        "(sh)|(html)|(htm)|(xml)|(xhtml)|(dhtml)|(shtm)|(shtml)|"
                                        "(json)|(css)|(yaml)|(ini)|(bat)|(js)|(sql)|(uof)|(ofd)";
static int kMaxResultNum = 100000;   // 最大搜索结果数
static int kEmitInterval = 50;   // 推送时间间隔

using namespace Lucene;
DFMBASE_USE_NAMESPACE
DPSEARCH_USE_NAMESPACE

bool FullTextSearcherPrivate::isIndexCreating = false;
FullTextSearcherPrivate::FullTextSearcherPrivate(FullTextSearcher *parent)
    : QObject(parent),
      q(parent)
{
    bindPathTable = DeviceUtils::fstabBindInfo();
}

FullTextSearcherPrivate::~FullTextSearcherPrivate()
{
}

IndexWriterPtr FullTextSearcherPrivate::newIndexWriter(bool create)
{
    return newLucene<IndexWriter>(FSDirectory::open(indexStorePath().toStdWString()),
                                  newLucene<ChineseAnalyzer>(),
                                  create,
                                  IndexWriter::MaxFieldLengthLIMITED);
}

IndexReaderPtr FullTextSearcherPrivate::newIndexReader()
{
    return IndexReader::open(FSDirectory::open(indexStorePath().toStdWString()), true);
}

void FullTextSearcherPrivate::indexDocs(const IndexWriterPtr &writer, const QString &file, IndexType type)
{
    Q_ASSERT(writer);

    try {
        switch (type) {
        case kAddIndex: {
            fmDebug() << "Adding [" << file << "]";
            // 添加
            writer->addDocument(fileDocument(file));
            break;
        }
        case kUpdateIndex: {
            fmDebug() << "Update file: [" << file << "]";
            // 定义一个更新条件
            TermPtr term = newLucene<Term>(L"path", file.toStdWString());
            // 更新
            writer->updateDocument(term, fileDocument(file));
            break;
        }
        case kDeleteIndex: {
            fmDebug() << "Delete file: [" << file << "]";
            // 定义一个删除条件
            TermPtr term = newLucene<Term>(L"path", file.toStdWString());
            // 删除
            writer->deleteDocuments(term);
            break;
        }
        }
    } catch (const LuceneException &e) {
        QMetaEnum enumType = QMetaEnum::fromType<FullTextSearcherPrivate::IndexType>();
        fmWarning() << QString::fromStdWString(e.getError()) << " type: " << enumType.valueToKey(type);
    } catch (const std::exception &e) {
        QMetaEnum enumType = QMetaEnum::fromType<FullTextSearcherPrivate::IndexType>();
        fmWarning() << QString(e.what()) << " type: " << enumType.valueToKey(type);
    } catch (...) {
        fmWarning() << "Index document failed! " << file;
    }
}

bool FullTextSearcherPrivate::checkUpdate(const IndexReaderPtr &reader, const QString &file, IndexType &type)
{
    Q_ASSERT(reader);

    try {
        SearcherPtr searcher = newLucene<IndexSearcher>(reader);
        TermQueryPtr query = newLucene<TermQuery>(newLucene<Term>(L"path", file.toStdWString()));

        // 文件路径为唯一值，所以搜索一个结果就行了
        TopDocsPtr topDocs = searcher->search(query, 1);
        int32_t numTotalHits = topDocs->totalHits;
        if (numTotalHits == 0) {
            type = kAddIndex;
            return true;
        } else {
            DocumentPtr doc = searcher->doc(topDocs->scoreDocs[0]->doc);
            auto info = InfoFactory::create<FileInfo>(QUrl::fromLocalFile(file),
                                                      Global::CreateFileInfoType::kCreateFileInfoSync);
            if (!info)
                return false;

            const QDateTime &modifyTime { info->timeOf(TimeInfoType::kLastModified).toDateTime() };
            const QString &modifyEpoch { QString::number(modifyTime.toSecsSinceEpoch()) };
            const String &storeTime { doc->get(L"modified") };
            if (modifyEpoch.toStdWString() != storeTime) {
                type = kUpdateIndex;
                return true;
            }
        }
    } catch (const LuceneException &e) {
        fmWarning() << QString::fromStdWString(e.getError()) << " file: " << file;
    } catch (const std::exception &e) {
        fmWarning() << QString(e.what()) << " file: " << file;
    } catch (...) {
        fmWarning() << "The file checked failed!" << file;
    }

    return false;
}

void FullTextSearcherPrivate::tryNotify()
{
    int cur = notifyTimer.elapsed();
    if (q->hasItem() && (cur - lastEmit) > kEmitInterval) {
        lastEmit = cur;
        fmDebug() << "unearthed, current spend:" << cur;
        emit q->unearthed(q);
    }
}

DocumentPtr FullTextSearcherPrivate::fileDocument(const QString &file)
{
    DocumentPtr doc = newLucene<Document>();
    // file path
    doc->add(newLucene<Field>(L"path", file.toStdWString(), Field::STORE_YES, Field::INDEX_NOT_ANALYZED));

    // file last modified time
    auto info = InfoFactory::create<FileInfo>(QUrl::fromLocalFile(file),
                                              Global::CreateFileInfoType::kCreateFileInfoSync);
    const QDateTime &modifyTime { info->timeOf(TimeInfoType::kLastModified).toDateTime() };
    const QString &modifyEpoch { QString::number(modifyTime.toSecsSinceEpoch()) };
    doc->add(newLucene<Field>(L"modified", modifyEpoch.toStdWString(), Field::STORE_YES, Field::INDEX_NOT_ANALYZED));

    // file contents
    QString contents = DocParser::convertFile(file.toStdString()).c_str();
    doc->add(newLucene<Field>(L"contents", contents.toStdWString(), Field::STORE_YES, Field::INDEX_ANALYZED));

    return doc;
}

bool FullTextSearcherPrivate::doSearch(const QString &path, const QString &keyword)
{
    fmInfo() << "search path: " << path << " keyword: " << keyword;
    notifyTimer.start();

    bool hasTransform = false;
    QString searchPath = FileUtils::bindPathTransform(path, false);
    if (searchPath != path)
        hasTransform = true;

    try {
        IndexWriterPtr writer = newIndexWriter();
        IndexReaderPtr reader = newIndexReader();
        SearcherPtr searcher = newLucene<IndexSearcher>(reader);
        AnalyzerPtr analyzer = newLucene<ChineseAnalyzer>();
        QueryParserPtr parser = newLucene<QueryParser>(LuceneVersion::LUCENE_CURRENT, L"contents", analyzer);
        //设定第一个* 可以匹配
        parser->setAllowLeadingWildcard(true);
        QueryPtr query = parser->parse(keyword.toStdWString());

        // create query filter
        String filterPath = searchPath.endsWith("/") ? (searchPath + "*").toStdWString() : (searchPath + "/*").toStdWString();
        FilterPtr filter = newLucene<QueryWrapperFilter>(newLucene<WildcardQuery>(newLucene<Term>(L"path", filterPath)));

        // search
        TopDocsPtr topDocs = searcher->search(query, filter, kMaxResultNum);
        Collection<ScoreDocPtr> scoreDocs = topDocs->scoreDocs;

        QHash<QString, QSet<QString>> hiddenFileHash;
        for (auto scoreDoc : scoreDocs) {
            //中断
            if (status.loadAcquire() != AbstractSearcher::kRuning)
                return false;

            DocumentPtr doc = searcher->doc(scoreDoc->doc);
            String resultPath = doc->get(L"path");

            if (!resultPath.empty()) {
                const QUrl &url = QUrl::fromLocalFile(StringUtils::toUTF8(resultPath).c_str());
                auto info = InfoFactory::create<FileInfo>(url,
                                                          Global::CreateFileInfoType::kCreateFileInfoSync);
                // delete invalid index
                if (!info || !info->exists()) {
                    indexDocs(writer, url.path(), kDeleteIndex);
                    continue;
                }

                const QDateTime &modifyTime { info->timeOf(TimeInfoType::kLastModified).toDateTime() };
                const QString &modifyEpoch { QString::number(modifyTime.toSecsSinceEpoch()) };
                const String &storeTime { doc->get(L"modified") };
                if (modifyEpoch.toStdWString() != storeTime) {
                    continue;
                } else {
                    if (!SearchHelper::instance()->isHiddenFile(StringUtils::toUTF8(resultPath).c_str(), hiddenFileHash, searchPath)) {
                        if (hasTransform)
                            resultPath.replace(0, static_cast<unsigned long>(searchPath.length()), path.toStdWString());
                        QMutexLocker lk(&mutex);
                        allResults.append(QUrl::fromLocalFile(StringUtils::toUTF8(resultPath).c_str()));
                    }

                    //推送
                    tryNotify();
                }
            }
        }

        reader->close();
        writer->close();
    } catch (const LuceneException &e) {
        fmWarning() << QString::fromStdWString(e.getError());
    } catch (const std::exception &e) {
        fmWarning() << QString(e.what());
    } catch (...) {
        fmWarning() << "Search failed!";
    }

    return true;
}

QString FullTextSearcherPrivate::dealKeyword(const QString &keyword)
{
    static QRegExp cnReg("^[\u4e00-\u9fa5]");
    static QRegExp enReg("^[A-Za-z]+$");
    static QRegExp numReg("^[0-9]$");

    WordType oldType = kCn, currType = kCn;
    QString newStr;
    for (auto c : keyword) {
        if (cnReg.exactMatch(c)) {
            currType = kCn;
        } else if (enReg.exactMatch(c)) {
            currType = kEn;
        } else if (numReg.exactMatch(c)) {
            currType = kDigit;
        } else {
            // 特殊符号均当作空格处理
            newStr += ' ';
            currType = kSymbol;
            continue;
        }

        newStr += c;
        // 如果上一个字符是空格，则不需要再加空格
        if (oldType == kSymbol) {
            oldType = currType;
            continue;
        }

        if (oldType != currType) {
            oldType = currType;
            newStr.insert(newStr.length() - 1, " ");
        }
    }

    return newStr.trimmed();
}

void FullTextSearcherPrivate::doSearchAndEmit(const QString &path, const QString &key)
{
    doSearch(path, key);
    if (status.testAndSetRelease(AbstractSearcher::kRuning, AbstractSearcher::kCompleted)) {
        if (q->hasItem())
            emit q->unearthed(q);
    }
}

FullTextSearcher::FullTextSearcher(const QUrl &url, const QString &key, QObject *parent)
    : AbstractSearcher(url, key, parent),
      d(new FullTextSearcherPrivate(this))
{
    auto client = TextIndexClient::instance();

    connect(client, &TextIndexClient::taskStarted,
            this, &FullTextSearcher::onIndexTaskStarted);
    connect(client, &TextIndexClient::taskFinished,
            this, &FullTextSearcher::onIndexTaskFinished);
    connect(client, &TextIndexClient::taskFailed,
            this, &FullTextSearcher::onIndexTaskFailed);
}

FullTextSearcher::~FullTextSearcher()
{
}

bool FullTextSearcher::isSupport(const QUrl &url)
{
    if (!url.isValid() || UrlRoute::isVirtual(url))
        return false;

    return DConfigManager::instance()->value(DConfig::kSearchCfgPath,
                                             DConfig::kEnableFullTextSearch,
                                             false)
            .toBool();
}

bool FullTextSearcher::search()
{
    // 准备状态切运行中，否则直接返回
    if (!d->status.testAndSetRelease(kReady, kRuning))
        return false;

    const QString path = UrlRoute::urlToPath(searchUrl);
    const QString key = d->dealKeyword(keyword);
    if (path.isEmpty() || key.isEmpty()) {
        d->status.storeRelease(kCompleted);
        return false;
    }

    auto client = TextIndexClient::instance();

    // 检查服务状态
    auto serviceStatus = client->checkService();
    if (serviceStatus != TextIndexClient::ServiceStatus::Available) {
        // 如果服务不可用，直接执行搜索
        d->doSearchAndEmit(path, key);
        return true;
    }

    // 检查索引是否存在
    auto indexExistsResult = client->indexExists();
    if (!indexExistsResult.has_value()) {
        // 如果无法确定索引状态，直接执行搜索
        d->doSearchAndEmit(path, key);
        return true;
    }

    // 根据索引状态决定是创建还是更新
    if (!indexExistsResult.value()) {
        QString bindPath = FileUtils::bindPathTransform(path, false);
        client->startTask(TextIndexClient::TaskType::Create, bindPath);
    } else {
        client->startTask(TextIndexClient::TaskType::Update, path);
    }

    // 等待任务完成
    if (!waitForTask()) {
        d->status.storeRelease(kCompleted);
        return false;
    }

    // 执行搜索并发送结果
    d->doSearchAndEmit(path, key);
    return true;
}

bool FullTextSearcher::waitForTask()
{
    QMutexLocker locker(&d->taskMutex);
    while (d->taskStatus.loadAcquire() == 0 && d->status.loadAcquire() == kRuning) {
        d->taskCondition.wait(&d->taskMutex);
    }
    return d->taskStatus.loadAcquire() > 0;
}

void FullTextSearcher::onIndexTaskStarted(TextIndexClient::TaskType type, const QString &path)
{
    fmInfo() << "Index task started:" << static_cast<int>(type) << path;
}

void FullTextSearcher::onIndexTaskFinished(TextIndexClient::TaskType type, const QString &path, bool success)
{
    QMutexLocker locker(&d->taskMutex);
    d->taskStatus.storeRelease(success ? 1 : -1);
    d->taskCondition.wakeAll();
}

void FullTextSearcher::onIndexTaskFailed(TextIndexClient::TaskType type, const QString &path, const QString &error)
{
    fmWarning() << "Index task failed:" << static_cast<int>(type) << path << error;
    QMutexLocker locker(&d->taskMutex);
    d->taskStatus.storeRelease(-1);
    d->taskCondition.wakeAll();
}

void FullTextSearcher::stop()
{
    d->status.storeRelease(kTerminated);
    QMutexLocker locker(&d->taskMutex);
    d->taskStatus.storeRelease(-1);
    d->taskCondition.wakeAll();
}

bool FullTextSearcher::hasItem() const
{
    QMutexLocker lk(&d->mutex);
    return !d->allResults.isEmpty();
}

QList<QUrl> FullTextSearcher::takeAll()
{
    QMutexLocker lk(&d->mutex);
    return std::move(d->allResults);
}
