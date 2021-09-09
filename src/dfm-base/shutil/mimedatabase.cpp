/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *               2016 ~ 2018 dragondjf
 *
 * Author:     dragondjf<dingjiangfeng@deepin.com>
 *
 * Maintainer: dragondjf<dingjiangfeng@deepin.com>
 *             zccrs<zhangjide@deepin.com>
 *             Tangtong<tangtong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "shutil/mimedatabase.h"
#include "shutil/fileutils.h"
#include "base/standardpaths.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QMimeDatabase>
#include <QString>
#include <QHash>
#include <QtConcurrent>

DFMBASE_BEGIN_NAMESPACE
Q_GLOBAL_STATIC(QMimeDatabase, mimedb)

//global mimetypes define list cache
Q_GLOBAL_STATIC(QStringList, mimeArchiveTypes);
Q_GLOBAL_STATIC(QStringList, mimeTextTypes)
Q_GLOBAL_STATIC(QStringList, mimeVideoTypes)
Q_GLOBAL_STATIC(QStringList, mimeAudioTypes)
Q_GLOBAL_STATIC(QStringList, mimeImageTypes)
Q_GLOBAL_STATIC(QStringList, mimeExecutableTypes)
Q_GLOBAL_STATIC(QStringList, mimeBackupTypes)

typedef QHash<int,QString> kiVsHash;
Q_GLOBAL_STATIC(kiVsHash, mimeDisplayNames)
Q_GLOBAL_STATIC(kiVsHash, mimeStdIconNames)
/*!
 * \class MimeDatabase 文件类型数据获取类
 *
 * \brief 通过Qt和gio两种方式获取文件的类型
 */
/*!
 * \brief readlines 读取文件的所有行数据
 *
 * \param path 文件路径
 *
 * \return QStringList 文件所有内容
 */
QStringList readlines(const QString &path)
{
    QStringList result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }
    QTextStream in(&file);
    while (!in.atEnd()) {
        // Read new line
        QString line = in.readLine();
        // Skip empty line or line with invalid format
        if (line.trimmed().isEmpty()) {
            continue;
        }
        result.append(line.trimmed());
    }
    file.close();
    return result;
}
/*!
 * \brief loadSupportMimeTypes 加载支持的Mimetype
 *
 * \return bool 是否加载成功
 */
bool loadSupportMimeTypes()
{
    QString textPath = QString("%1/%2").arg(StandardPaths::location(StandardPaths::MimeTypePath), "text.mimetype");
    QString archivePath = QString("%1/%2").arg(StandardPaths::location(StandardPaths::MimeTypePath), "archive.mimetype");
    QString videoPath = QString("%1/%2").arg(StandardPaths::location(StandardPaths::MimeTypePath), "video.mimetype");
    QString audioPath = QString("%1/%2").arg(StandardPaths::location(StandardPaths::MimeTypePath), "audio.mimetype");
    QString imagePath = QString("%1/%2").arg(StandardPaths::location(StandardPaths::MimeTypePath), "image.mimetype");
    QString executablePath = QString("%1/%2").arg(StandardPaths::location(StandardPaths::MimeTypePath), "executable.mimetype");
    QString backupPath = QString("%1/%2").arg(StandardPaths::location(StandardPaths::MimeTypePath), "backup.mimetype");
    *mimeTextTypes = readlines(textPath);
    *mimeArchiveTypes = readlines(archivePath);
    *mimeVideoTypes = readlines(videoPath);
    *mimeAudioTypes = readlines(audioPath);
    *mimeImageTypes = readlines(imagePath);
    *mimeExecutableTypes = readlines(executablePath);
    *mimeBackupTypes = readlines(backupPath);
    return true;
}
/*!
 * \brief loadFileTypeName 加载文件的基本类型格式
 *
 * \return bool 是否加载成功
 */
bool loadFileTypeName()
{
    (*mimeDisplayNames)[MimeDatabase::FileType::Directory] = QObject::tr("Directory");
    (*mimeDisplayNames)[MimeDatabase::FileType::DesktopApplication] = QObject::tr("Application");
    (*mimeDisplayNames)[MimeDatabase::FileType::Videos] = QObject::tr("Video");
    (*mimeDisplayNames)[MimeDatabase::FileType::Audios] = QObject::tr("Audio");
    (*mimeDisplayNames)[MimeDatabase::FileType::Images] = QObject::tr("Image");
    (*mimeDisplayNames)[MimeDatabase::FileType::Archives] = QObject::tr("Archive");
    (*mimeDisplayNames)[MimeDatabase::FileType::Documents] = QObject::tr("Text");
    (*mimeDisplayNames)[MimeDatabase::FileType::Executable] = QObject::tr("Executable");
    (*mimeDisplayNames)[MimeDatabase::FileType::Backups] = QObject::tr("Backup file");
    (*mimeDisplayNames)[MimeDatabase::FileType::Unknown] = QObject::tr("Unknown");
    return true;
}
/*!
 * \brief loadMimeStdIcon 加载标准的文件格式对应的icon
 *
 * \return bool 是否加载成功
 */
bool loadMimeStdIcon()
{
    (*mimeStdIconNames)[MimeDatabase::FileType::Directory] = "folder";
    (*mimeStdIconNames)[MimeDatabase::FileType::DesktopApplication] = "application-default-icon";
    (*mimeStdIconNames)[MimeDatabase::FileType::Videos] = "video";
    (*mimeStdIconNames)[MimeDatabase::FileType::Audios] = "music";
    (*mimeStdIconNames)[MimeDatabase::FileType::Images] = "image";
    (*mimeStdIconNames)[MimeDatabase::FileType::Archives] = "application-x-archive";
    (*mimeStdIconNames)[MimeDatabase::FileType::Documents] = "text-plain";
    (*mimeStdIconNames)[MimeDatabase::FileType::Executable] = "application-x-executable";
    (*mimeStdIconNames)[MimeDatabase::FileType::Backups] = "application-x-archive"; // generic backup file icon?
    (*mimeStdIconNames)[MimeDatabase::FileType::Unknown] = "application-default-icon";

    return true;
}

//Q_GLOBAL_STATIC_WITH_ARGS(QFuture<void>, _threadLoadMimeTypes, {QtConcurrent::run(loadSupportMimeTypes)});
//Q_GLOBAL_STATIC_WITH_ARGS(QFuture<void>, _threadLoadStdIcons, {QtConcurrent::run(loadMimeStdIcon)});
//Q_GLOBAL_STATIC_WITH_ARGS(QFuture<void>, _threadLoadMimeNames, {QtConcurrent::run(loadFileTypeName)});

static auto _threadLoadMimeTypes = QtConcurrent::run(loadSupportMimeTypes);
static auto _threadLoadStdIcons = QtConcurrent::run(loadMimeStdIcon);
static auto _threadLoadMimeNames = QtConcurrent::run(loadFileTypeName);

MimeDatabase::MimeDatabase()
{
    Q_UNUSED(_threadLoadMimeTypes);
    Q_UNUSED(_threadLoadStdIcons);
    Q_UNUSED(_threadLoadMimeNames);
}

MimeDatabase::~MimeDatabase()
{

}
/*!
 * \brief MimeDatabase::mimeFileType 获取文件的mimetype
 *
 * \param mimeType
 *
 * \return QString 返回文件的mimetype
 */
QString MimeDatabase::mimeFileType(const QString &mimeType)
{
    Q_UNUSED(mimeType)
//todo:
//#ifdef QT_DEBUG
//    return mimeDisplayNames->value(mimeFileTypeNameToEnum(mimeType)) + " (" + mimeType + ")";
//#else // Q_DEBUG
//    return mimeDisplayNames->value(mimeFileTypeToEnum(mimeType));
//#endif
    return "";
}
/*!
 * \brief MimeDatabase::mimeFileTypeNameToEnum 获取文件的filetype
 *
 * \param mimeFileTypeName 文件的mimetype
 *
 * \return FileType 文件filetype
 */
MimeDatabase::FileType MimeDatabase::mimeFileTypeNameToEnum(const QString &mimeFileTypeName)
{
    if (mimeFileTypeName == "application/x-desktop") {
        return FileType::DesktopApplication;
    } else if (mimeFileTypeName == "inode/directory") {
        return FileType::Directory;
    } else if (mimeFileTypeName == "application/x-executable"
               || mimeExecutableTypes->contains(mimeFileTypeName)) {
        return FileType::Executable;
    } else if (mimeFileTypeName.startsWith("video/")
               || mimeVideoTypes->contains(mimeFileTypeName)) {
        return FileType::Videos;
    } else if (mimeFileTypeName.startsWith("audio/")
               || mimeAudioTypes->contains(mimeFileTypeName)) {
        return FileType::Audios;
    } else if (mimeFileTypeName.startsWith("image/")
               || mimeImageTypes->contains(mimeFileTypeName)) {
        return FileType::Images;
    } else if (mimeFileTypeName.startsWith("text/")
               || mimeTextTypes->contains(mimeFileTypeName)) {
        return FileType::Documents;
    } else if (mimeArchiveTypes->contains(mimeFileTypeName)) {
        return FileType::Archives;
    } else if (mimeBackupTypes->contains(mimeFileTypeName)) {
        return FileType::Backups;
    } else {
        return FileType::Unknown;
    }
}
/*!
 * \brief MimeDatabase::supportMimeFileType 根据文件的filetype获取文件的mimetype
 *
 * \param mimeFileType 文件的filetype
 *
 * \return QStringList 文件的mimetype
 */
QStringList MimeDatabase::supportMimeFileType(MimeDatabase::FileType mimeFileType)
{
    QStringList list;

    switch (mimeFileType) {
    case Documents:
        return list; //empty
    case Images:
        return *mimeImageTypes;
    case Videos:
        return *mimeVideoTypes;
    case Audios:
        return *mimeAudioTypes;
    case Archives:
        return *mimeArchiveTypes;
    case DesktopApplication:
        return list; //empty
    case Executable:
        return *mimeExecutableTypes;
    case Backups:
        return *mimeBackupTypes;
    default:
        return list; //empty
    }
}
/*!
 * \brief MimeDatabase::mimeStdIcon 根据文件的mimetype获取文件的标准icon
 *
 * \param mimeType 文件的mimetype
 *
 * \return QString 标注icon的名称
 */
QString MimeDatabase::mimeStdIcon(const QString &mimeType)
{
    return mimeStdIconNames->value(mimeFileTypeNameToEnum(mimeType));
}
/*!
 * \brief MimeDatabase::mimeTypeForFile 获取一个文件的mimetype，调用的是qt的mimeTypeForFile
 *
 * \param fileName 文件名称
 *
 * \param mode 匹配模式（MatchDefault、MatchExtension、MatchContent）其中MatchExtension获取文件的mimetype最快
 *
 * \return QMimeType 文件的mimetype
 */
QMimeType MimeDatabase::mimeTypeForFile(const QString &fileName, QMimeDatabase::MatchMode mode)
{
    return mimedb->mimeTypeForFile(fileName,mode);
}
/*!
 * \brief MimeDatabase::mimeTypeForFile 获取一个文件的mimetype
 *
 * \param fileInfo 文件的QFileInfo
 *
 * \param mode 匹配模式（MatchDefault、MatchExtension、MatchContent）其中MatchExtension获取文件的mimetype最快
 *
 * \return QMimeType 文件的mimetype
 */
QMimeType MimeDatabase::mimeTypeForFile(const QFileInfo &fileInfo, QMimeDatabase::MatchMode mode)
{
    return mimedb->mimeTypeForFile(fileInfo,mode);
}
/*!
 * \brief MimeDatabase::mimeTypesForFileName 获取一个文件的mimetype，调用qt的接口mimeTypesForFileName
 *
 * \param fileName 文件的名称
 *
 * \return QList<QMimeType> 文件的mimetype的列表
 */
QList<QMimeType> MimeDatabase::mimeTypesForFileName(const QString &fileName)
{
    return mimedb->mimeTypesForFileName(fileName);
}
/*!
 * \brief MimeDatabase::mimeTypeForData 根据data获取文件的mimetype
 *
 *  A valid MIME type is always returned. If data doesn't match any
 *
 *  known MIME type data, the default MIME type (application/octet-stream) is returned
 *
 * \param data QByteArray类型的数据
 *
 * \return QMimeType 文件的mimetype
 */
QMimeType MimeDatabase::mimeTypeForData(const QByteArray &data)
{
    return mimedb->mimeTypeForData(data);
}
/*!
 * \brief MimeDatabase::mimeTypeForData 根据data获取文件的mimetype
 *
 *  A valid MIME type is always returned. If data doesn't match any
 *
 *  known MIME type data, the default MIME type (application/octet-stream) is returned
 *
 * \param device QIODevice类型的数据，可以是QFile
 *
 * \return QMimeType 文件的mimetype
 */
QMimeType MimeDatabase::mimeTypeForData(QIODevice *device)
{
    return mimedb->mimeTypeForData(device);
}
/*!
 * \brief MimeDatabase::mimeTypeForUrl 根据文件的url获取文件的mimetype
 *
 * \param url 文件的url
 *
 * \return QMimeType 文件的mimetype
 */
QMimeType MimeDatabase::mimeTypeForUrl(const QUrl &url)
{
    if(url.isLocalFile())
        return mimedb->mimeTypeForUrl(url);
    else
        return mimedb->mimeTypeForFile(UrlRoute::urlToPath(url));
}
/*!
 * \brief MimeDatabase::mimeTypeForFileNameAndData Returns a MIME type for the given fileName and device data.
 *
 *  This overload can be useful when the file is remote, and we started to download some of its data in a device.
 *
 *  This allows to do full MIME type matching for remote files as well.
 *
 *  If the device is not open, it will be opened by this function, and closed after the MIME type detection is completed.
 *
 *  A valid MIME type is always returned. If device data doesn't match any known MIME type data, the default MIME
 *
 *  type (application/octet-stream) is returned.
 *
 *  This method looks at both the file name and the file contents, if necessary.
 *
 *  The file extension has priority over the contents, but the contents will be used if the file extension is unknown, or matches multiple MIME types.
 *
 * \param fileName 文件名称
 *
 * \param device 文件的QIODevice对象
 *
 * \return QMimeType 文件的mimetype
 */
QMimeType MimeDatabase::mimeTypeForFileNameAndData(const QString &fileName, QIODevice *device)
{
    return mimedb->mimeTypeForFileNameAndData(fileName, device);
}
/*!
 * \brief MimeDatabase::mimeTypeForFileNameAndData Returns a MIME type for the given fileName and device data.
 *
 *  This overload can be useful when the file is remote, and we started to download some of its data.
 *
 *  This allows to do full MIME type matching for remote files as well.
 *
 *  A valid MIME type is always returned. If data doesn't match any known MIME type data,
 *
 *  the default MIME type (application/octet-stream) is returned.
 *
 *  This method looks at both the file name and the file contents, if necessary.
 *
 *  The file extension has priority over the contents, but the contents will be used if the
 *
 *  file extension is unknown, or matches multiple MIME types.
 *
 * \param fileName 文件名称
 *
 * \param data 文件内容QByteArray的data
 *
 * \return QMimeType 文件的mimetype
 */
QMimeType MimeDatabase::mimeTypeForFileNameAndData(const QString &fileName, const QByteArray &data)
{
    return mimedb->mimeTypeForFileNameAndData(fileName, data);
}
/*!
 * \brief MimeDatabase::suffixForFileName Returns the suffix for the file fileName,
 *
 *  as known by the MIME database.
 *
 *  This allows to pre-select "tar.bz2" for foo.tar.bz2, but still only "txt" for my.file.with.dots.txt.
 *
 * \param fileName 文件的名称
 *
 * \return QString
 */
QString MimeDatabase::suffixForFileName(const QString &fileName)
{
    return mimedb->suffixForFileName(fileName);
}
/*!
 * \brief MimeDatabase::allMimeTypes Returns the list of all available MIME types.
 *
 *  This can be useful for showing all MIME types to the user, for instance
 *
 *  in a MIME type editor. Do not use unless really necessary in other cases though,
 *
 *  prefer using the mimeTypeForXxx() methods for performance reasons.
 *
 * \return QList<QMimeType>
 */
QList<QMimeType> MimeDatabase::allMimeTypes()
{
    return mimedb->allMimeTypes();
}
DFMBASE_END_NAMESPACE