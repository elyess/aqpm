/***************************************************************************
 *   Copyright (C) 2009 by Dario Freddi                                    *
 *   drf@kde.org                                                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#include "Configuration.h"

#include "Backend.h"

#include <QSettings>
#include <QFile>
#include <QTemporaryFile>
#include <QEventLoop>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>

#include <Auth>

namespace Aqpm
{

class Configuration::Private
{
public:
    Private();
    QString convertPacmanConfToAqpmConf() const;

    QTemporaryFile *tempfile;
    QString arch;
    bool lastResult;
};

Configuration::Private::Private()
         : tempfile(0)
{
    QProcess proc;
    proc.start("arch");
    proc.waitForFinished(20000);

    arch = QString(proc.readAllStandardOutput()).remove('\n').remove(' ');
}

class ConfigurationHelper
{
public:
    ConfigurationHelper() : q(0) {}
    ~ConfigurationHelper() {
        delete q;
    }
    Configuration *q;
};

Q_GLOBAL_STATIC(ConfigurationHelper, s_globalConfiguration)

Configuration *Configuration::instance()
{
    if (!s_globalConfiguration()->q) {
        new Configuration;
    }

    return s_globalConfiguration()->q;
}

Configuration::Configuration()
        : QObject(0)
        , d(new Private())
{
    Q_ASSERT(!s_globalConfiguration()->q);
    s_globalConfiguration()->q = this;

    reload();
}

Configuration::~Configuration()
{
    delete d;
}

void Configuration::reload()
{
    qDebug() << "reloading";

    if (!QFile::exists("/etc/aqpm.conf")) {
        QCoreApplication::processEvents();

        qDebug() << "Loading...";

        if (!PolkitQt::Auth::computeAndObtainAuth("org.chakraproject.aqpm.convertconfiguration")) {
            qDebug() << "User unauthorized";
            configuratorResult(false);
            return;
        }

        qDebug() << "Kewl";

        QDBusMessage message = QDBusMessage::createMethodCall("org.chakraproject.aqpmconfigurator",
                                                              "/Configurator",
                                                              "org.chakraproject.aqpmconfigurator",
                                                              QLatin1String("pacmanConfToAqpmConf"));

        message << true;
        QDBusConnection::systemBus().call(message, QDBus::Block);
        qDebug() << QDBusConnection::systemBus().lastError();
    }

    if (d->tempfile) {
        d->tempfile->close();
        d->tempfile->deleteLater();
    }

    d->tempfile = new QTemporaryFile(this);
    d->tempfile->open();

    QFile file("/etc/aqpm.conf");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "prcd!";
        emit configurationSaved(false);
        return;
    }

    QTextStream out(d->tempfile);
    QTextStream in(&file);

    // Strip out comments
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.startsWith('#')) {
            out << line;
            out << '\n';
        }
    }

    file.close();

    d->tempfile->close();
}

bool Configuration::saveConfiguration()
{
    QEventLoop e;

    connect(this, SIGNAL(configurationSaved(bool)), &e, SLOT(quit()));

    saveConfigurationAsync();
    e.exec();

    return d->lastResult;
}

void Configuration::saveConfigurationAsync()
{
    if (Backend::instance()->shouldHandleAuthorization()) {
        if (!PolkitQt::Auth::computeAndObtainAuth("org.chakraproject.aqpm.saveconfiguration")) {
            qDebug() << "User unauthorized";
            configuratorResult(false);
            return;
        }
    }

    QDBusConnection::systemBus().connect("org.chakraproject.aqpmconfigurator", "/Configurator",
                                         "org.chakraproject.aqpmconfigurator",
                                         "configuratorResult", this, SLOT(configuratorResult(bool)));

    QDBusMessage message = QDBusMessage::createMethodCall("org.chakraproject.aqpmconfigurator",
                                                          "/Configurator",
                                                          "org.chakraproject.aqpmconfigurator",
                                                          QLatin1String("saveConfiguration"));

    d->tempfile->open();
    message << QString(d->tempfile->readAll());
    QDBusConnection::systemBus().call(message);
    qDebug() << QDBusConnection::systemBus().lastError();
    d->tempfile->close();
}

void Configuration::configuratorResult(bool result)
{
    QDBusConnection::systemBus().disconnect("org.chakraproject.aqpmconfigurator", "/Configurator",
                                            "org.chakraproject.aqpmconfigurator",
                                            "configuratorResult", this, SLOT(configuratorResult(bool)));

    qDebug() << "Got a result:" << result;

    if (result) {
        reload();
    }

    d->lastResult = result;

    emit configurationSaved(result);
}

void Configuration::setValue(const QString &key, const QString &val)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    settings.setValue(key, val);
}

QVariant Configuration::value(const QString &key)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    return settings.value(key);
}

QStringList Configuration::databases()
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    QStringList dbsreg = settings.childGroups();
    return settings.value("options/DbOrder").toStringList();
}

QString Configuration::getServerForDatabase(const QString &db)
{
    return getServersForDatabase(db).first();
}

QStringList Configuration::getServersForDatabase(const QString &db)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    QStringList retlist;

    settings.beginGroup("mirrors");

    qDebug() << "Checking " << db;

    if (db == "core" || db == "extra" || db == "community" || db == "testing") {
        settings.beginGroup("arch");
    } else if (db == "kdemod-core" || db == "kdemod-extragear" || db == "kdemod-unstable" ||
               db == "kdemod-legacy" || db == "kdemod-testing" || db == "kdemod-playground") {
        settings.beginGroup("kdemod");
    } else {
        foreach (const QString &mirror, settings.childGroups()) {
            if (settings.value(mirror + "/Databases").toStringList().contains(db)) {
                settings.beginGroup(mirror);
            }
        }
    }

    qDebug() << "ok, keys are " << settings.childKeys();

    for (int i = 1; i <= settings.childKeys().size(); ++i) {
        QString retstr = settings.value(QString("Server%1").arg(i)).toString();
        retstr.replace("$repo", db);
        retstr.replace("$arch", d->arch);
        retlist.append(retstr);
    }

    return retlist;
}

QStringList Configuration::getMirrorList(MirrorType type) const
{
    QFile file;

    if (type == ArchMirror) {
        file.setFileName("/etc/pacman.d/mirrorlist");
    } else if (type == KdemodMirror) {
        if (QFile::exists("/etc/pacman.d/kdemodmirrorlist")) {
            file.setFileName("/etc/pacman.d/kdemodmirrorlist");
        } else {
            return QStringList();
        }
    }

    QStringList retlist;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringList();
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();

        if (line.startsWith('#')) {
            line.remove(0, 1);
        }

        if (!line.contains("server", Qt::CaseInsensitive)) {
            continue;
        }

        QStringList list(line.split('=', QString::SkipEmptyParts));
        if (list.count() >= 1) {
            QString serverN(list.at(1));

            serverN.remove(QChar(' '), Qt::CaseInsensitive);

            retlist.append(serverN);
        }
    }

    file.close();

    return retlist;

}

bool Configuration::addMirrorToMirrorList(const QString &mirror, MirrorType type)
{
    QEventLoop e;

    connect(this, SIGNAL(configurationSaved(bool)), &e, SLOT(quit()));

    addMirrorToMirrorListAsync(mirror, type);
    e.exec();

    return d->lastResult;
}

void Configuration::addMirrorToMirrorListAsync(const QString &mirror, MirrorType type)
{
    if (Backend::instance()->shouldHandleAuthorization()) {
        if (!PolkitQt::Auth::computeAndObtainAuth("org.chakraproject.aqpm.addmirror")) {
            qDebug() << "User unauthorized";
            configuratorResult(false);
            return;
        }
    }

    QDBusConnection::systemBus().connect("org.chakraproject.aqpmconfigurator", "/Configurator", "org.chakraproject.aqpmconfigurator",
                                         "configuratorResult", this, SLOT(configuratorResult(bool)));

    QDBusMessage message = QDBusMessage::createMethodCall("org.chakraproject.aqpmconfigurator",
                                                          "/Configurator",
                                                          "org.chakraproject.aqpmconfigurator",
                                                          QLatin1String("addMirror"));

    message << mirror;
    message << (int)type;
    QDBusConnection::systemBus().call(message, QDBus::NoBlock);
}

void Configuration::remove(const QString &key)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    settings.remove(key);
}

bool Configuration::exists(const QString &key, const QString &val)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    bool result = settings.contains(key);

    if (!val.isEmpty() && result) {
        qDebug() << "Check";
        result = value(key).toString() == val;
    }

    return result;
}

void Configuration::setOrUnset(bool set, const QString &key, const QString &val)
{
    if (set) {
        setValue(key, val);
    } else {
        remove(key);
    }
}

void Configuration::setDatabases(const QStringList &databases)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    settings.setValue("DbOrder", databases);
}

void Configuration::setDatabasesForMirror(const QStringList &databases, const QString &mirror)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    settings.setValue("mirrors/" + mirror + "/Databases", databases);
}

QStringList Configuration::serversForMirror(const QString &mirror)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    QStringList retlist;
    settings.beginGroup("mirrors");
    settings.beginGroup(mirror);
    foreach (const QString &key, settings.childKeys()) {
        if (key.startsWith("Server")) {
            retlist.append(settings.value(key).toString());
        }
    }

    settings.endGroup();
    settings.endGroup();
}

void Configuration::setServersForMirror(const QStringList &servers, const QString &mirror)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    settings.beginGroup("mirrors");
    settings.beginGroup(mirror);
    foreach (const QString &key, settings.childKeys()) {
        if (key.startsWith("Server")) {
            settings.remove(key);
        }
    }

    for (int i = 1; i <= servers.size(); ++i) {
        settings.setValue(QString("Server%1").arg(i), servers.at(i-1));
    }

    settings.endGroup();
    settings.endGroup();
}

QStringList Configuration::mirrors(bool thirdpartyonly)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    settings.beginGroup("mirrors");
    QStringList retlist = settings.childGroups();
    settings.endGroup();

    if (thirdpartyonly) {
        retlist.removeOne("arch");
        retlist.removeOne("kdemod");
    }

    return retlist;
}

QStringList Configuration::databasesForMirror(const QString &mirror)
{
    QSettings settings(d->tempfile->fileName(), QSettings::IniFormat);
    settings.value("mirrors/" + mirror + "/Databases").toStringList();
}

}
