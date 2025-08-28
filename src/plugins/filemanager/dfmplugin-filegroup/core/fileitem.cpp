#include "fileitem.h"
#include <QLocale>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

DPFILEGROUP_USE_NAMESPACE

QString FileItem::getFormattedSize() const {
    if (isDirectory) {
        return QString(); // Directories don't show size
    }
    
    if (size == 0) {
        return "0 B";
    }
    
    QLocale locale;
    const qint64 kb = 1024;
    const qint64 mb = kb * 1024;
    const qint64 gb = mb * 1024;
    const qint64 tb = gb * 1024;
    
    if (size >= tb) {
        return QString("%1 TB").arg(locale.toString(static_cast<double>(size) / tb, 'f', 1));
    } else if (size >= gb) {
        return QString("%1 GB").arg(locale.toString(static_cast<double>(size) / gb, 'f', 1));
    } else if (size >= mb) {
        return QString("%1 MB").arg(locale.toString(static_cast<double>(size) / mb, 'f', 1));
    } else if (size >= kb) {
        return QString("%1 KB").arg(locale.toString(static_cast<double>(size) / kb, 'f', 1));
    } else {
        return QString("%1 B").arg(locale.toString(size));
    }
}

QString FileItem::getFormattedModifiedTime() const {
    QLocale locale;
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 secsTo = dateModified.secsTo(now);
    
    // If modified today, show time only
    if (dateModified.date() == now.date()) {
        return dateModified.toString("hh:mm");
    }
    
    // If modified yesterday, show "Yesterday"
    if (dateModified.date() == now.date().addDays(-1)) {
        return QObject::tr("Yesterday");
    }
    
    // If modified within a week, show day name
    if (secsTo < 7 * 24 * 3600) {
        return dateModified.toString("dddd");
    }
    
    // If modified this year, show month and day
    if (dateModified.date().year() == now.date().year()) {
        return dateModified.toString("MMM d");
    }
    
    // Otherwise show full date
    return dateModified.toString("yyyy/MM/dd");
}

QString FileItem::getTypeString() const {
    if (isDirectory) {
        return QObject::tr("Directory");
    }
    
    // Use MIME database for accurate file type detection
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(path);
    
    if (mimeType.isValid()) {
        QString comment = mimeType.comment();
        if (!comment.isEmpty()) {
            return comment;
        }
        
        // Fallback to MIME type name if no comment
        QString mimeTypeName = mimeType.name();
        
        // Convert common MIME types to user-friendly names
        if (mimeTypeName.startsWith("text/")) {
            return QObject::tr("Text Document");
        } else if (mimeTypeName.startsWith("image/")) {
            return QObject::tr("Image");
        } else if (mimeTypeName.startsWith("audio/")) {
            return QObject::tr("Audio");
        } else if (mimeTypeName.startsWith("video/")) {
            return QObject::tr("Video");
        } else if (mimeTypeName == "application/pdf") {
            return QObject::tr("PDF Document");
        } else if (mimeTypeName.startsWith("application/vnd.ms-") || 
                   mimeTypeName.startsWith("application/vnd.openxmlformats-")) {
            return QObject::tr("Office Document");
        } else if (mimeTypeName.startsWith("application/zip") || 
                   mimeTypeName.startsWith("application/x-")) {
            return QObject::tr("Archive");
        } else if (mimeTypeName == "application/x-executable") {
            return QObject::tr("Executable");
        }
        
        return mimeTypeName;
    }
    
    // Fallback to extension-based detection
    if (type.isEmpty()) {
        return QObject::tr("File");
    }
    
    // Convert extension to uppercase for display
    QString displayType = type.toUpper();
    if (displayType.startsWith('.')) {
        displayType = displayType.mid(1) + " " + QObject::tr("File");
    }
    
    return displayType;
}
