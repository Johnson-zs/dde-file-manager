// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "eventshandler.h"
#include "dfmplugin_disk_encrypt_global.h"
#include "gui/encryptprogressdialog.h"
#include "gui/unlockpartitiondialog.h"
#include "gui/encryptparamsinputdialog.h"
#include "utils/encryptutils.h"
#include "menu/diskencryptmenuscene.h"

#include <dfm-framework/dpf.h>
#include <dfm-base/utils/finallyutil.h>

#include <QApplication>
#include <QtConcurrent/QtConcurrent>
#include <QSettings>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusReply>

#include <DDialog>

#ifdef COMPILE_ON_V2X
#    define APP_MANAGER_SERVICE "org.deepin.dde.SessionManager1"
#    define APP_MANAGER_PATH "/org/deepin/dde/SessionManager1"
#    define APP_MANAGER_INTERFACE "org.deepin.dde.SessionManager1"
#else
#    define APP_MANAGER_SERVICE "com.deepin.SessionManager"
#    define APP_MANAGER_PATH "/com/deepin/SessionManager"
#    define APP_MANAGER_INTERFACE "com.deepin.SessionManager"
#endif

Q_DECLARE_METATYPE(QString *)
Q_DECLARE_METATYPE(bool *)

using namespace dfmplugin_diskenc;
using namespace disk_encrypt;
DWIDGET_USE_NAMESPACE;

EventsHandler *EventsHandler::instance()
{
    static EventsHandler ins;
    return &ins;
}

void EventsHandler::bindDaemonSignals()
{
    // FIXME(xust) split the unlock module into another plugin.
    // for unlocking devices in file dialog, this plugin is loaded,
    // which cause when en/decrypt devices, the signal are handled
    // by both file manager and file dialog, so multiple progress
    // dialog and finished dialog are shown.
    // this class is singleton but in different process it's not.
    if (qApp->applicationName() != "dde-file-manager")
        return;

    auto conn = [this](const char *sig, const char *slot) {
        QDBusConnection::systemBus().connect(kDaemonBusName,
                                             kDaemonBusPath,
                                             kDaemonBusIface,
                                             sig,
                                             this,
                                             slot);
    };
    conn("EncryptProgress", SLOT(onEncryptProgress(const QString &, const QString &, double)));
    conn("DecryptProgress", SLOT(onDecryptProgress(const QString &, const QString &, double)));
    conn("InitEncResult", SLOT(onInitEncryptFinished(const QVariantMap &)));
    conn("EncryptResult", SLOT(onEncryptFinished(const QVariantMap &)));
    conn("DecryptResult", SLOT(onDecryptFinished(const QVariantMap &)));
    conn("ChangePassResult", SLOT(onChgPwdFinished(const QVariantMap &)));
    conn("WaitAuthInput", SLOT(onRequestAuthArgs(const QVariantMap &)));
}

void EventsHandler::hookEvents()
{
    dpfHookSequence->follow("dfmplugin_computer", "hook_Device_AcquireDevPwd",
                            this, &EventsHandler::onAcquireDevicePwd);
}

/**
 * @brief EventsHandler::isTaskWorking, if any device is running encrypt, decrypt and change passphrase background
 * @return
 */
bool EventsHandler::isTaskWorking()
{
    QDBusInterface iface(kDaemonBusName,
                         kDaemonBusPath,
                         kDaemonBusIface,
                         QDBusConnection::systemBus());
    QDBusReply<bool> reply = iface.call("IsTaskRunning");
    return reply.isValid() && reply.value();
}

/**
 * @brief EventsHandler::hasPendingTask, if task files existed in /etc/usec-crypt/
 * @return
 */
bool EventsHandler::hasPendingTask()
{
    QDBusInterface iface(kDaemonBusName,
                         kDaemonBusPath,
                         kDaemonBusIface,
                         QDBusConnection::systemBus());
    QDBusReply<bool> reply = iface.call("IsTaskEmpty");
    return reply.isValid() && !reply.value();
}

QString EventsHandler::unfinishedDecryptJob()
{
    QDBusInterface iface(kDaemonBusName,
                         kDaemonBusPath,
                         kDaemonBusIface,
                         QDBusConnection::systemBus());
    QDBusReply<QString> reply = iface.call("PendingDecryptionDevice");
    return reply.value();
}

/**
 * @brief EventsHandler::isUnderOperating, If the device is performing a task in the foreground
 * @param device
 * @return
 */
bool EventsHandler::isUnderOperating(const QString &device)
{
    return encryptDialogs.contains(device)
            || decryptDialogs.contains(device)
            || encryptInputs.contains(device);
}

int EventsHandler::deviceEncryptStatus(const QString &device)
{
    QDBusInterface iface(kDaemonBusName,
                         kDaemonBusPath,
                         kDaemonBusIface,
                         QDBusConnection::systemBus());
    QDBusReply<int> reply = iface.call("DeviceStatus", device);
    if (reply.isValid())
        return reply.value();
    fmWarning() << "Failed to get encryption status for device:" << device;
    return -1;
}

void EventsHandler::resumeEncrypt(const QString &device)
{
    QDBusInterface iface(kDaemonBusName,
                         kDaemonBusPath,
                         kDaemonBusIface,
                         QDBusConnection::systemBus());
    iface.asyncCall("ResumeEncryption", QVariantMap{{encrypt_param_keys::kKeyDevice, device}});
}

QString EventsHandler::holderDevice(const QString &device)
{
    QDBusInterface iface(kDaemonBusName,
                         kDaemonBusPath,
                         kDaemonBusIface,
                         QDBusConnection::systemBus());
    QDBusReply<QString> reply = iface.call("HolderDevice", device);
    if (reply.isValid())
        return reply.value();
    fmWarning() << "Failed to get holder device for:" << device << "using original device";
    return device;
}

void EventsHandler::onInitEncryptFinished(const QVariantMap &result)
{
    QApplication::restoreOverrideCursor();

    auto code = result.value(encrypt_param_keys::kKeyOperationResult).toInt();
    auto dev = result.value(encrypt_param_keys::kKeyDevice).toString();
    auto name = result.value(encrypt_param_keys::kKeyDeviceName).toString();

    if (code == -kRebootRequired) {
        fmInfo() << "Reboot required for device:" << dev << "requesting reboot";
        requestReboot();
    } else if (code < 0) {
        fmWarning() << "Pre-encrypt error for device:" << dev << "code:" << code;
        showPreEncryptError(dev, name, code);
        return;
    }

    autoStartDFM();
}

void EventsHandler::onEncryptFinished(const QVariantMap &result)
{
    QApplication::restoreOverrideCursor();

    auto code = result.value(encrypt_param_keys::kKeyOperationResult).toInt();
    auto dev = result.value(encrypt_param_keys::kKeyDevice).toString();
    auto name = result.value(encrypt_param_keys::kKeyDeviceName).toString();

    // delay delete input dialogs. avoid when new request comes new dialog raises.
    QTimer::singleShot(1000, this, [=] {
        if (encryptInputs.contains(dev))
            encryptInputs.take(dev)->deleteLater();
    });

    QString device = QString("%1(%2)").arg(name).arg(dev.mid(5));
    QString title, msg;
    bool success = false;
    switch (-code) {
    case kUserCancelled:
        fmInfo() << "Encryption cancelled by user for device:" << device;
        ignoreParamRequest();
        return;
    case kSuccess:
    case KErrorRequestExportRecKey:
        title = tr("Encrypt done");
        msg = tr("Device %1 has been encrypted").arg(device);
        success = true;
        fmInfo() << "Encryption completed successfully for device:" << device;
        break;
    default:
        title = tr("Encrypt failed");
        msg = tr("Device %1 encrypt failed, please see log for more information.(%2)")
                      .arg(device)
                      .arg(code);
        fmWarning() << "Encryption failed for device:" << device << "with code:" << code;
        break;
    }

    auto dialog = encryptDialogs.take(dev);
    if (!dialog)
        dialog_utils::showDialog(title, msg, code != 0 ? dialog_utils::kError : dialog_utils::kInfo);
    else {
        auto pos = dialog->geometry().topLeft();
        dialog->showResultPage(success, title, msg);
        if (code == -KErrorRequestExportRecKey) {
            auto recKey = result.value(encrypt_param_keys::kKeyRecoveryKey).toString();
            dialog->setRecoveryKey(recKey, dev);
            dialog->showExportPage();
        }
        dialog->move(pos);
    }

    // delete auto start file.
    auto configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    auto autoStartFilePath = configPath + "/autostart/dfm-reencrypt.desktop";
    int ret = ::remove(autoStartFilePath.toStdString().c_str());
    fmDebug() << "Autostart file removal result:" << ret << "for path:" << autoStartFilePath;
}

void EventsHandler::onDecryptFinished(const QVariantMap &result)
{
    QApplication::restoreOverrideCursor();

    auto code = result.value(encrypt_param_keys::kKeyOperationResult).toInt();
    auto dev = result.value(encrypt_param_keys::kKeyDevice).toString();
    auto name = result.value(encrypt_param_keys::kKeyDeviceName).toString();

    if (code == -kRebootRequired) {
        fmInfo() << "Reboot required after decryption for device:" << dev;
        requestReboot();
    } else {
        showDecryptError(dev, name, code);

        // delete auto start file.
        auto configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        auto autoStartFilePath = configPath + "/autostart/dfm-reencrypt.desktop";
        int ret = ::remove(autoStartFilePath.toStdString().c_str());
        fmDebug() << "Autostart file removal result:" << ret << "for path:" << autoStartFilePath;
    }
}

void EventsHandler::onChgPwdFinished(const QVariantMap &result)
{
    auto code = result.value(encrypt_param_keys::kKeyOperationResult).toInt();
    auto dev = result.value(encrypt_param_keys::kKeyDevice).toString();
    auto name = result.value(encrypt_param_keys::kKeyDeviceName).toString();

    QApplication::restoreOverrideCursor();
    showChgPwdError(dev, name, code);
}

void EventsHandler::onRequestAuthArgs(const QVariantMap &devInfo)
{
    qApp->restoreOverrideCursor();

    QString devPath = devInfo.value(encrypt_param_keys::kKeyDevice).toString();
    if (devPath.isEmpty()) {
        fmWarning() << "Invalid encrypt config, missing device path:" << devInfo;
        return;
    }

    if (encryptInputs.value(devPath, nullptr)) {
        fmDebug() << "Input dialog already exists for device:" << devPath;
        return;
    }

    QString objPath = "/org/freedesktop/UDisks2/block_devices/" + devPath.mid(5);
    auto blkDev = device_utils::createBlockDevice(objPath);
    auto dlg = new EncryptParamsInputDialog(devInfo, qApp->activeWindow());
    encryptInputs.insert(devPath, dlg);

    connect(dlg, &DDialog::finished, this, [=](auto ret) {
        if (ret != QDialog::Accepted) {
            fmInfo() << "User cancelled auth input for device:" << devPath;
            ignoreParamRequest();
            encryptInputs.take(devPath)->deleteLater();   // also will be deleted when encryption started.
        } else {
            fmInfo() << "User provided auth input for device:" << devPath << "proceeding with re-encryption";
            DiskEncryptMenuScene::doReencryptDevice(dlg->getInputs());
        }
    });
    dlg->show();
}

void EventsHandler::ignoreParamRequest()
{
    fmDebug() << "Ignoring parameter request";
    QDBusInterface iface(kDaemonBusName,
                         kDaemonBusPath,
                         kDaemonBusIface,
                         QDBusConnection::systemBus());
    iface.asyncCall("IgnoreAuthSetup");
    fmInfo() << "Parameter request ignored";
}

void EventsHandler::onEncryptProgress(const QString &dev, const QString &devName, double progress)
{
    if (!encryptDialogs.contains(dev)) {
        QString device = QString("%1(%2)").arg(devName).arg(dev.mid(5));

        QApplication::restoreOverrideCursor();
        auto dlg = new EncryptProgressDialog(qApp->activeWindow());
        dlg->setText(tr("%1 is under encrypting...").arg(device),
                     tr("The encrypting process may have system lag, please minimize the system operation"));
        encryptDialogs.insert(dev, dlg);
    }
    auto dlg = encryptDialogs.value(dev);
    dlg->updateProgress(progress);
    if (!dlg->isVisible())
        dlg->show();

    // when start encrypt, delete the inputs widget.
    if (encryptInputs.contains(dev))
        delete encryptInputs.take(dev);
}

void EventsHandler::onDecryptProgress(const QString &dev, const QString &devName, double progress)
{
    if (!decryptDialogs.contains(dev)) {
        QString device = QString("%1(%2)").arg(devName).arg(dev.mid(5));

        QApplication::restoreOverrideCursor();
        auto dlg = new EncryptProgressDialog(qApp->activeWindow());
        dlg->setText(tr("%1 is under decrypting...").arg(device),
                     tr("The decrypting process may have system lag, please minimize the system operation"));
        decryptDialogs.insert(dev, dlg);
    }

    auto dlg = decryptDialogs.value(dev);
    dlg->updateProgress(progress);
    if (!dlg->isVisible())
        dlg->show();
}

bool EventsHandler::onAcquireDevicePwd(const QString &dev, QString *pwd, bool *cancelled)
{
    if (!pwd || !cancelled) {
        fmWarning() << "Invalid parameters for password acquisition - pwd or cancelled is null";
        return false;
    }

    if (!canUnlock(dev)) {
        fmWarning() << "Device cannot be unlocked:" << dev;
        *cancelled = true;
        return true;
    }

    int type = device_utils::encKeyType(dev);
    fmDebug() << "Device" << dev << "encryption key type:" << type;

    // test tpm
    bool testTPM = (type == kPin || type == kTpm);
    if (testTPM && tpm_utils::checkTPM() != 0) {
        fmWarning() << "TPM service is not available for device:" << dev;
        int ret = dialog_utils::showDialog(tr("Error"), tr("TPM status is abnormal, please use the recovery key to unlock it"),
                                           dialog_utils::DialogType::kError);
        // unlock by recovery key.
        if (ret == 0)
            *pwd = acquirePassphraseByRec(dev, *cancelled);

        return true;
    }

    switch (type) {
    case SecKeyType::kPin:
        fmDebug() << "Acquiring passphrase by PIN for device:" << dev;
        *pwd = acquirePassphraseByPIN(dev, *cancelled);
        break;
    case SecKeyType::kTpm:
        fmDebug() << "Acquiring passphrase by TPM for device:" << dev;
        *pwd = acquirePassphraseByTPM(dev, *cancelled);
        break;
    case SecKeyType::kPwd:
        fmDebug() << "Acquiring passphrase by password for device:" << dev;
        *pwd = acquirePassphrase(dev, *cancelled);
        break;
    default:
        fmWarning() << "Unknown encryption key type:" << type << "for device:" << dev;
        return false;
    }

    if (pwd->isEmpty() && !*cancelled) {
        fmWarning() << "Failed to acquire password for device:" << dev << "type:" << type;
        QString title;
        if (type == kPin)
            title = tr("Wrong PIN");
        else if (type == kPwd)
            title = tr("Wrong passphrase");
        else
            title = tr("TPM error");

        dialog_utils::showDialog(title, tr("Please use recovery key to unlock device."),
                                 dialog_utils::kInfo);

        *pwd = acquirePassphraseByRec(dev, *cancelled);
    }

    return true;
}

QString EventsHandler::acquirePassphrase(const QString &dev, bool &cancelled)
{
    UnlockPartitionDialog dlg(UnlockPartitionDialog::kPwd);
    int ret = dlg.exec();
    if (ret != 1) {
        cancelled = true;
        fmInfo() << "Password dialog cancelled for device:" << dev;
        return "";
    }
    return dlg.getUnlockKey().second;
}

QString EventsHandler::acquirePassphraseByPIN(const QString &dev, bool &cancelled)
{
    UnlockPartitionDialog dlg(UnlockPartitionDialog::kPin);
    int ret = dlg.exec();
    if (ret != 1) {
        cancelled = true;
        fmInfo() << "PIN dialog cancelled for device:" << dev;
        return "";
    }
    auto keys = dlg.getUnlockKey();
    if (keys.first == UnlockPartitionDialog::kPin) {
        fmDebug() << "Getting passphrase from TPM using PIN for device:" << dev;
        return tpm_passphrase_utils::getPassphraseFromTPM_NonBlock(dev, keys.second);
    } else {
        fmDebug() << "Using recovery key directly for device:" << dev;
        return keys.second;
    }
}

QString EventsHandler::acquirePassphraseByTPM(const QString &dev, bool &)
{
    return tpm_passphrase_utils::getPassphraseFromTPM_NonBlock(dev, "");
}

QString EventsHandler::acquirePassphraseByRec(const QString &dev, bool &cancelled)
{
    UnlockPartitionDialog dlg(UnlockPartitionDialog::kRec);
    int ret = dlg.exec();
    if (ret != 1) {
        cancelled = true;
        fmInfo() << "Recovery key dialog cancelled for device:" << dev;
        return "";
    }
    auto keys = dlg.getUnlockKey();
    return keys.second;
}

void EventsHandler::showPreEncryptError(const QString &dev, const QString &devName, int code)
{
    QString title;
    QString msg;
    QString device = QString("%1(%2)").arg(devName).arg(dev.mid(5));

    bool showError = false;
    switch (-code) {
    case (kSuccess):
        title = tr("Preencrypt done");
        msg = tr("Device %1 has been preencrypt, please reboot to finish encryption.")
                      .arg(device);
        fmInfo() << "Pre-encryption successful for device:" << device;
        break;
    case kUserCancelled:
        fmInfo() << "Pre-encryption cancelled by user for device:" << device;
        return;
    default:
        title = tr("Preencrypt failed");
        msg = tr("Device %1 preencrypt failed, please see log for more information.(%2)")
                      .arg(device)
                      .arg(code);
        showError = true;
        fmWarning() << "Pre-encryption failed for device:" << device << "code:" << code;
        break;
    }

    dialog_utils::showDialog(title, msg,
                             showError ? dialog_utils::kError : dialog_utils::kInfo);
}

void EventsHandler::showDecryptError(const QString &dev, const QString &devName, int code)
{
    QString title;
    QString msg;
    QString device = QString("%1(%2)").arg(devName).arg(dev.mid(5));

    bool showFailed = true;
    switch (-code) {
    case (kSuccess):
        title = tr("Decrypt done");
        msg = tr("Device %1 has been decrypted").arg(device);
        showFailed = false;
        fmInfo() << "Decryption successful for device:" << device;
        break;
    case kUserCancelled:
        fmInfo() << "Decryption cancelled by user for device:" << device;
        return;
    case kErrorWrongPassphrase:
        title = tr("Decrypt disk");
        msg = tr("Wrong passpharse or PIN");
        fmWarning() << "Wrong passphrase/PIN for device:" << device;
        break;
    case kErrorNotFullyEncrypted:
        title = tr("Decrypt failed");
        msg = tr("Device %1 is under encrypting, please decrypt after encryption finished.")
                      .arg(device);
        fmWarning() << "Device not fully encrypted:" << device;
        break;
    default:
        title = tr("Decrypt failed");
        msg = tr("Device %1 Decrypt failed, please see log for more information.(%2)")
                      .arg(device)
                      .arg(code);
        fmWarning() << "Decryption failed for device:" << device << "code:" << code;
        break;
    }

    auto dialog = decryptDialogs.take(dev);
    if (dialog) {
        auto pos = dialog->geometry().topLeft();
        dialog->showResultPage(code == 0, title, msg);
        dialog->move(pos);
    } else {
        dialog_utils::showDialog(title, msg,
                                 showFailed ? dialog_utils::kError : dialog_utils::kInfo);
    }
}

void EventsHandler::showChgPwdError(const QString &dev, const QString &devName, int code)
{
    QString title;
    QString msg;
    QString device = QString("%1(%2)").arg(devName).arg(dev.mid(5));

    int encType = device_utils::encKeyType(dev);
    QString codeType;
    switch (encType) {
    case SecKeyType::kPwd:
        codeType = tr("passphrase");
        break;
    default:
        codeType = tr("PIN");
        break;
    }

    bool showError = false;
    switch (-code) {
    case (kSuccess):
        title = tr("Change %1 done").arg(codeType);
        msg = tr("%1's %2 has been changed").arg(device).arg(codeType);
        fmInfo() << "Password change successful for device:" << device << "type:" << codeType;
        break;
    case kUserCancelled:
        fmInfo() << "Password change cancelled by user for device:" << device;
        return;
    case kErrorChangePassphraseFailed:
        title = tr("Change %1 failed").arg(codeType);
        msg = tr("Wrong %1").arg(codeType);
        showError = true;
        fmWarning() << "Wrong" << codeType << "for device:" << device;
        break;
    default:
        title = tr("Change %1 failed").arg(codeType);
        msg = tr("Device %1 change %2 failed, please see log for more information.(%3)")
                      .arg(device)
                      .arg(codeType)
                      .arg(code);
        showError = true;
        fmWarning() << "Password change failed for device:" << device << "type:" << codeType << "code:" << code;
        break;
    }

    dialog_utils::showDialog(title, msg,
                             showError ? dialog_utils::kError : dialog_utils::kInfo);
}

void EventsHandler::requestReboot()
{
    fmInfo() << "Requesting system reboot";
    QDBusInterface sessMng(APP_MANAGER_SERVICE,
                           APP_MANAGER_PATH,
                           APP_MANAGER_INTERFACE);
    sessMng.asyncCall("RequestReboot");
}

bool EventsHandler::canUnlock(const QString &device)
{
    if (EventsHandler::instance()->isUnderOperating(device)) {
        fmWarning() << "Device is under operation, cannot unlock:" << device;
        return false;
    }

    if (device == unfinishedDecryptJob()) {
        fmWarning() << "Device has unfinished decryption job:" << device;
        dialog_utils::showDialog(tr("Error"),
                                 tr("Device is not fully decrypted, please finish decryption before access."),
                                 dialog_utils::DialogType::kInfo);
        return false;
    }

    int states = EventsHandler::instance()->deviceEncryptStatus(device);
    if ((states & kStatusOnline) && (states & kStatusEncrypt)) {
        fmWarning() << "Device is online and encrypting, cannot unlock:" << device << "status:" << states;
        dialog_utils::showDialog(tr("Unlocking device failed"),
                                 tr("Please click the right disk menu \"Continue partition encryption\" to complete partition encryption."),
                                 dialog_utils::DialogType::kError);
        return false;
    }

    return true;
}

void EventsHandler::autoStartDFM()
{
    fmInfo() << "Adding file manager to autostart";
    QDBusInterface sessMng(APP_MANAGER_SERVICE,
                           APP_MANAGER_PATH,
                           APP_MANAGER_INTERFACE);
    sessMng.asyncCall("AddAutostart", QString(kReencryptDesktopFile));
}

EventsHandler::EventsHandler(QObject *parent)
    : QObject { parent }
{
}
