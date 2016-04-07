#include "filecontroller.h"
#include "fileinfo.h"
#include "fileservices.h"

#include "../models/desktopfileinfo.h"

#include <QDesktopServices>
#include <QDirIterator>

FileController::FileController(QObject *parent)
    : AbstractFileController(parent)
{
    qRegisterMetaType<QList<FileInfo*>>("QList<FileInfo*>");

    FileServices::setFileUrlHandler("file", "", this);
    FileServices::setFileUrlHandler("", "", this);
}

AbstractFileInfo *FileController::createFileInfo(const QString &fileUrl, bool &accepted) const
{
    QUrl url(fileUrl);

    if(url.scheme().isEmpty()) {
        url = QUrl::fromLocalFile(fileUrl);
    } else if(!url.isLocalFile()) {
        accepted = false;

        return Q_NULLPTR;
    }

    accepted = true;

    if(fileUrl.endsWith(QString(".") + DESKTOP_SURRIX))
        return new DesktopFileInfo(url.toLocalFile());
    else
        return new FileInfo(url.toLocalFile());
}

const QList<AbstractFileInfo*> FileController::getChildren(const QString &fileUrl, QDir::Filters filter, bool &accepted) const
{
    QUrl url(fileUrl);

    if(url.scheme().isEmpty()) {
        url = QUrl::fromLocalFile(fileUrl);
    } else if(!url.isLocalFile()) {
        accepted = false;
    }

    accepted = true;

    QList<AbstractFileInfo*> infolist;

    const QString &path = url.toLocalFile();

    if(path.isEmpty()) {
        const QFileInfoList list = QDir::drives();

        for(const QFileInfo &info : list) {
            FileInfo *fileInfo = new FileInfo(info);

            infolist.append(fileInfo);
        }
    } else {
        QDirIterator dirIt(path, filter);
        FileInfo *fileInfo;

        while (dirIt.hasNext()) {
            dirIt.next();

            if(dirIt.fileInfo().absoluteFilePath() == path)
                continue;

            if(dirIt.fileInfo().suffix() == DESKTOP_SURRIX)
                fileInfo = new DesktopFileInfo(dirIt.fileInfo());
            else
                fileInfo = new FileInfo(dirIt.fileInfo());

            infolist.append(fileInfo);
        }
    }

    return infolist;
}

bool FileController::openFile(const QString &fileUrl, bool &accepted) const
{
    QUrl url(fileUrl);

    if(url.scheme().isEmpty()) {
        url = QUrl::fromLocalFile(fileUrl);
    } else if(!url.isLocalFile()) {
        accepted = false;

        return false;
    }

    accepted = true;

    return QDesktopServices::openUrl(url);
}

bool FileController::renameFile(const QString &oldUrl, const QString &newUrl) const
{
    QFile file(oldUrl);

    return file.rename(newUrl);
}
