// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textindexdbus.h"
#include "ocrindexdbus.h"
#include "env/envdetector.h"
#include "profile/indexprofile.h"

#include <dfm-base/utils/processprioritymanager.h>
#include <dfm-search/dsearch_global.h>
#include <QDBusConnection>

SERVICETEXTINDEX_USE_NAMESPACE

static TextIndexDBus *textIndexDBus = nullptr;
static OcrIndexDBus *ocrIndexDBus = nullptr;
static EnvDetector *envDetector = nullptr;

// DEBUG:
// 1. budild a debug so file and copy to isntall path
// 2. systemctl --user stop deepin-service-plugin@org.deepin.Filemanager.TextIndex.service
// 3. launch app: /usr/bin/deepin-service-manager -n org.deepin.Filemanager.TextIndex

extern "C" int DSMRegister(const char *name, void *data)
{
    Q_UNUSED(name)
    (void)data;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(service_textindex::Defines::kTextIndexDBusService)
        && bus.lastError().type() != QDBusError::NoError) {
        fmWarning() << "TextIndex plugin: failed to register text index DBus service:" << bus.lastError().message();
    }

    if (!bus.registerService(service_textindex::Defines::kOcrIndexDBusService)
        && bus.lastError().type() != QDBusError::NoError) {
        fmWarning() << "TextIndex plugin: failed to register OCR index DBus service:" << bus.lastError().message();
    }

    // Process-level environment detector shared by both Content and OCR
    // runtimes (design §2.1: single process, dual IndexRuntime). The
    // LoadMonitor tracks the data disk of the content index directory.
    envDetector = new EnvDetector();
    envDetector->setDataPath(DFMSEARCH::Global::contentIndexDirectory());
    envDetector->start();

    textIndexDBus = new TextIndexDBus(envDetector);
    ocrIndexDBus = new OcrIndexDBus(envDetector);
    dfmbase::ProcessPriorityManager::lowerAllAvailablePriorities(true);

    return 0;
}

extern "C" int DSMUnRegister(const char *name, void *data)
{
    (void)name;
    (void)data;
    if (ocrIndexDBus) {
        ocrIndexDBus->cleanup();
        ocrIndexDBus->deleteLater();
        ocrIndexDBus = nullptr;
    }

    if (textIndexDBus) {
        textIndexDBus->cleanup();
        textIndexDBus->deleteLater();
        textIndexDBus = nullptr;
    }

    if (envDetector) {
        envDetector->stop();
        envDetector->deleteLater();
        envDetector = nullptr;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterService(service_textindex::Defines::kOcrIndexDBusService);
    bus.unregisterService(service_textindex::Defines::kTextIndexDBusService);
    return 0;
}
