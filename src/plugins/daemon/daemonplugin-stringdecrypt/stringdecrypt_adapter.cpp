/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -i stringdecryptdbus.h -c StringDecryptAdapter -l StringDecryptDBus -a stringdecrypt_adapter stringdecryptdbus.xml
 *
 * qdbusxml2cpp is Copyright (C) 2017 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "stringdecrypt_adapter.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class StringDecryptAdapter
 */

StringDecryptAdapter::StringDecryptAdapter(StringDecryptDBus *parent)
    : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

StringDecryptAdapter::~StringDecryptAdapter()
{
    // destructor
}

QString StringDecryptAdapter::PublicKey()
{
    // handle method call com.deepin.filemanager.daemon.EncryptKeyHelper.PublicKey
    return parent()->PublicKey();
}

