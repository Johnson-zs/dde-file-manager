/*
 * Copyright (C) 2020 ~ 2020 Deepin Technology Co., Ltd.
 *
 * Author:     zhangsheng <zhangsheng@uniontech.com>
 *
 * Maintainer: zhangsheng <zhangsheng@uniontech.com>
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

#include "bluetoothmanager.h"
#include "bluetooth/bluetoothmodel.h"

#include <QDBusConnection>
#include <com_deepin_daemon_bluetooth.h>
#include <com_deepin_dde_controlcenter.h>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

#define BluetoothService "com.deepin.daemon.Bluetooth"
#define BluetoothPath "/com/deepin/daemon/Bluetooth"

#define BluetoothPage "bluetooth"
#define ControlcenterService "com.deepin.dde.ControlCenter"
#define ControlcenterPath "/com/deepin/dde/ControlCenter"

using DBusBluetooth = com::deepin::daemon::Bluetooth;
using DBusControlcenter = com::deepin::dde::ControlCenter;

/**
 * @brief This is BluetoothManagerPrivate class
 */
class BluetoothManagerPrivate
{
public:
    explicit BluetoothManagerPrivate(BluetoothManager *qq);

    /**
     * @brief 解析蓝牙设备, 获取适配器和设备信息
     * @param req
     */
    void resolve(const QDBusReply<QString> &req);

private:
    /**
     * @brief 蓝牙 dbus 信号的处理
     */
    void initConnects();

    /**
     * @brief 获取适配器信息
     * @param adapter
     * @param adapterObj
     */
    void inflateAdapter(BluetoothAdapter *adapter, const QJsonObject &adapterObj);

    /**
     * @brief 获取设备信息
     * @param device
     * @param deviceObj
     */
    void inflateDevice(BluetoothDevice *device, const QJsonObject &deviceObj);

public:
    BluetoothManager *q_ptr {nullptr};
    BluetoothModel *m_model {nullptr};
    DBusBluetooth *m_bluetoothInter {nullptr};
    DBusControlcenter *m_controlcenterInter {nullptr};
    QFutureWatcher<QPair<QString, QString>> *m_watcher {nullptr};

    Q_DECLARE_PUBLIC(BluetoothManager)
};

BluetoothManagerPrivate::BluetoothManagerPrivate(BluetoothManager *qq)
    : q_ptr(qq),
      m_model(new BluetoothModel(qq))
{
    Q_Q(BluetoothManager);
    // initialize dbus interface
    m_bluetoothInter = new DBusBluetooth(BluetoothService, BluetoothPath,
                                         QDBusConnection::sessionBus(), q);
    m_controlcenterInter = new DBusControlcenter(ControlcenterService, ControlcenterPath,
                                                 QDBusConnection::sessionBus(), q);

    initConnects();
}

void BluetoothManagerPrivate::resolve(const QDBusReply<QString> &req)
{
    const QString replyStr = req.value();
    QJsonDocument doc = QJsonDocument::fromJson(replyStr.toUtf8());
    QJsonArray arr = doc.array();
    for (QJsonValue val : arr) {
        BluetoothAdapter *adapter = new BluetoothAdapter(m_model);
        inflateAdapter(adapter, val.toObject());
        m_model->addAdapter(adapter);
    }
}

void BluetoothManagerPrivate::initConnects()
{
    Q_Q(BluetoothManager);

    // adapter added
    QObject::connect(m_bluetoothInter, &DBusBluetooth::AdapterAdded, q, [this](const QString & json) {
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QJsonObject obj = doc.object();

        BluetoothAdapter *adapter = new BluetoothAdapter(m_model);
        inflateAdapter(adapter, obj);
        m_model->addAdapter(adapter);
    });

    // adapter removed
    QObject::connect(m_bluetoothInter, &DBusBluetooth::AdapterRemoved, q, [this](const QString & json) {
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QJsonObject obj = doc.object();
        const QString id = obj["Path"].toString();

        const BluetoothAdapter *result = m_model->removeAdapater(id);
        BluetoothAdapter *adapter = const_cast<BluetoothAdapter *>(result);
        if (adapter) {
            adapter->deleteLater();
        }
    });

    // adapter changed
    QObject::connect(m_bluetoothInter, &DBusBluetooth::AdapterPropertiesChanged, q, [this](const QString & json) {
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        const QJsonObject obj = doc.object();
        const QString id = obj["Path"].toString();

        BluetoothAdapter *adapter = const_cast<BluetoothAdapter *>(m_model->adapterById(id));
        if (adapter) {
            inflateAdapter(adapter, obj);
        }
    });

    // device added
    QObject::connect(m_bluetoothInter, &DBusBluetooth::DeviceAdded, q, [this](const QString & json) {
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QJsonObject obj = doc.object();
        const QString adapterId = obj["AdapterPath"].toString();
        const QString id = obj["Path"].toString();

        const BluetoothAdapter *result = m_model->adapterById(adapterId);
        BluetoothAdapter *adapter = const_cast<BluetoothAdapter *>(result);
        if (adapter) {
            const BluetoothDevice *result1 = adapter->deviceById(id);
            BluetoothDevice *device = const_cast<BluetoothDevice *>(result1);
            if (!device) {
                device = new BluetoothDevice(adapter);
            }
            inflateDevice(device, obj);
            adapter->addDevice(device);
        }
    });

    // deivce removeed
    QObject::connect(m_bluetoothInter, &DBusBluetooth::DeviceRemoved, q, [this](const QString & json) {
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QJsonObject obj = doc.object();
        const QString adapterId = obj["AdapterPath"].toString();
        const QString id = obj["Path"].toString();

        const BluetoothAdapter *result = m_model->adapterById(adapterId);
        BluetoothAdapter *adapter = const_cast<BluetoothAdapter *>(result);
        if (adapter) {
            adapter->removeDevice(id);
        }
    });

    // device changed
    QObject::connect(m_bluetoothInter, &DBusBluetooth::DevicePropertiesChanged, q, [this](const QString & json) {
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        const QJsonObject obj = doc.object();
        const QString id = obj["Path"].toString();
        for (const BluetoothAdapter *adapter : m_model->adapters()) {
            BluetoothDevice *device = const_cast<BluetoothDevice *>(adapter->deviceById(id));
            if (device) {
                inflateDevice(device, obj);
            }
        }
    });


    QObject::connect(m_bluetoothInter, &DBusBluetooth::TransferCreated, q, [this](const QString & file, const QDBusObjectPath & transferPath, const QDBusObjectPath & sessionPath) {
        qDebug() << file << transferPath.path() << sessionPath.path();
    });

    QObject::connect(m_bluetoothInter, &DBusBluetooth::TransferRemoved, q, [this](const QString &file, const QDBusObjectPath &transferPath, const QDBusObjectPath &sessionPath, bool done) {
        Q_Q(BluetoothManager);
        if (!done) {
            Q_EMIT q->transferCancledByRemote(sessionPath.path());
        } else {
            Q_EMIT q->fileTransferFinished(sessionPath.path(), file);
        }
    });

    QObject::connect(m_bluetoothInter, &DBusBluetooth::ObexSessionCreated, q, [this](const QDBusObjectPath &sessionPath) {
        qDebug() << sessionPath.path();
    });

    QObject::connect(m_bluetoothInter, &DBusBluetooth::ObexSessionRemoved, q, [this](const QDBusObjectPath &sessionPath) {
        qDebug() << sessionPath.path();
    });

    QObject::connect(m_bluetoothInter, &DBusBluetooth::ObexSessionProgress, q, [this](const QDBusObjectPath &sessionPath, qulonglong totalSize, qulonglong transferred, int currentIdx) {
        Q_Q(BluetoothManager);
        Q_EMIT q->transferProgressUpdated(sessionPath.path(), totalSize, transferred, currentIdx);
    });

    QObject::connect(m_bluetoothInter, &DBusBluetooth::TransferFailed, q, [this](const QString &file, const QDBusObjectPath &sessionPath, const QString &errInfo) {
        Q_Q(BluetoothManager);
        Q_EMIT q->transferFailed(sessionPath.path(), file, errInfo);
    });
}

void BluetoothManagerPrivate::inflateAdapter(BluetoothAdapter *adapter, const QJsonObject &adapterObj)
{
    Q_Q(BluetoothManager);

    const QString path = adapterObj["Path"].toString();
    const QString alias = adapterObj["Alias"].toString();
    const bool powered = adapterObj["Powered"].toBool();
    qDebug() << "resolve adapter path:" << path;

    adapter->setId(path);
    adapter->setName(alias);
    adapter->setPowered(powered);

    QPointer<BluetoothAdapter> adapterPointer(adapter);

    // 异步获取适配器的所有设备
    QDBusObjectPath dPath(path);
    QDBusPendingCall call = m_bluetoothInter->GetDevices(dPath);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, q, [this, watcher, adapterPointer, call] {
        if (!adapterPointer) {
            qDebug() << "adapterPointer released!";
            watcher->deleteLater();
            return;
        }
        BluetoothAdapter *adapter = adapterPointer.data();
        if (!call.isError()) {
            QStringList tmpList;

            QDBusReply<QString> reply = call.reply();
            const QString replyStr = reply.value();
            QJsonDocument doc = QJsonDocument::fromJson(replyStr.toUtf8());
            QJsonArray arr = doc.array();

            for (QJsonValue val : arr) {
                const QString id = val.toObject()["Path"].toString();
                const BluetoothDevice *result = adapter->deviceById(id);
                BluetoothDevice *device = const_cast<BluetoothDevice *>(result);
                if (device == nullptr) {
                    device = new BluetoothDevice(adapter);
                }
                // 存储设备数据
                inflateDevice(device, val.toObject());
                adapter->addDevice(device);

                tmpList << id;
            }

            // 适配器设备去重
            for (const BluetoothDevice *device : adapter->devices()) {
                if (!tmpList.contains(device->id())) {
                    adapter->removeDevice(device->id());

                    BluetoothDevice *target = const_cast<BluetoothDevice *>(device);
                    if (target) {
                        target->deleteLater();
                    }
                }
            }
        } else {
            qWarning() << call.error().message();
        }

        watcher->deleteLater();
    });
}

void BluetoothManagerPrivate::inflateDevice(BluetoothDevice *device, const QJsonObject &deviceObj)
{
    const QString &id = deviceObj["Path"].toString();
    const QString &name = deviceObj["Name"].toString();
    const QString &alias = deviceObj["Alias"].toString();
    const QString &icon = deviceObj["Icon"].toString();
    const bool paired = deviceObj["Paired"].toBool();
    const bool trusted = deviceObj["Trusted"].toBool();
    const BluetoothDevice::State state = BluetoothDevice::State(deviceObj["State"].toInt());

    device->setId(id);
    device->setName(name);
    device->setAlias(alias);
    device->setIcon(icon);
    device->setPaired(paired);
    device->setTrusted(trusted);
    device->setState(state);
}

/**
 * @brief This is BluetoothManager implement
 */
BluetoothManager::BluetoothManager(QObject *parent)
    : QObject(parent),
      d_ptr(new BluetoothManagerPrivate(this))
{
    refresh();
}


BluetoothManager *BluetoothManager::instance()
{
    static BluetoothManager bluetooth;
    return &bluetooth;
}


void BluetoothManager::refresh()
{
    Q_D(BluetoothManager);

    if (!d->m_bluetoothInter->isValid())
        return;

    // 获取蓝牙设备
    QDBusPendingCall call = d->m_bluetoothInter->GetAdapters();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call);
    connect(watcher, &QDBusPendingCallWatcher::finished, [ = ] {
        if (!call.isError()) {
            QDBusReply<QString> reply = call.reply();
            d->resolve(reply);
        } else {
            qWarning() << call.error().message();
        }
        watcher->deleteLater();
    });
}

BluetoothModel *BluetoothManager::model()
{
    Q_D(BluetoothManager);

    return d->m_model;
}

void BluetoothManager::showBluetoothSettings()
{
    Q_D(BluetoothManager);

    d->m_controlcenterInter->ShowModule(BluetoothPage);
}

void BluetoothManager::sendFiles(const BluetoothDevice &device, const QStringList &filePath)
{
    return sendFiles(device.id(), filePath);
}

void BluetoothManager::sendFiles(const QString &id, const QStringList &filePath)
{
    Q_D(BluetoothManager);

    // /org/bluez/hci0/dev_90_63_3B_DA_5A_4C  --》  90:63:3B:DA:5A:4C
    QString deviceAddress = id;
    deviceAddress.remove(QRegularExpression("/org/bluez/hci[0-9]*/dev_")).replace("_", ":");

    QFuture<QPair<QString, QString>> future = QtConcurrent::run([this, deviceAddress, filePath]{
        Q_D(BluetoothManager);
        QDBusPendingReply<QDBusObjectPath> reply = d->m_bluetoothInter->SendFiles(deviceAddress, filePath);
        reply.waitForFinished();
        return qMakePair<QString, QString>(reply.value().path(), reply.error().message());
    });

    if (d->m_watcher) {
        if (d->m_watcher->isRunning())
            d->m_watcher->future().cancel();
        delete d->m_watcher;
        d->m_watcher = nullptr;
    }

    // 此处 watcher 在 run 完成之后会 delete，但无法在传输对话框关闭后立即 delete
    d->m_watcher = new QFutureWatcher<QPair<QString, QString>>();
    d->m_watcher->setFuture(future);
    connect(d->m_watcher, &QFutureWatcher<QString>::finished, this, [d, this]{
        emit transferEstablishFinish(d->m_watcher->result().first, d->m_watcher->result().second);
        delete d->m_watcher;
        d->m_watcher = nullptr;
    });
}

bool BluetoothManager::cancelTransfer(const QString &sessionPath)
{
    Q_D(BluetoothManager);
    d->m_bluetoothInter->CancelTransferSession(QDBusObjectPath(sessionPath));
    qDebug() << sessionPath;
    return true;
}

bool BluetoothManager::canSendBluetoothRequest()
{
    Q_D(BluetoothManager);
    return d->m_bluetoothInter->transportable();
}