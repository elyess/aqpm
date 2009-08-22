/***************************************************************************
 *   Copyright (C) 2009 by Dario Freddi                                    *
 *   drf@chakra-project.org                                                *
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

#ifndef BACKEND_H
#define BACKEND_H

#include "Visibility.h"
#include "QueueItem.h"
#include "Globals.h"
#include "Package.h"
#include "Database.h"
#include "Group.h"

#include <QThread>
#include <QStringList>
#include <QEvent>
#include <QMetaType>

#include <alpm.h>

//typedef struct __alpm_list_t alpm_list_t;
//typedef enum __pmtransflag_t pmtransflag_t;

namespace Aqpm
{

class BackendThread;

/**
 * @brief The main and only entry point for performing operations with Aqpm
 *
 * Backend encapsulates all the needed logic to perform each and every operation with Aqpm.
 * It can work both in synchronous and asynchronous mode, and it provides access to all the most
 * common functionalities in Alpm. 90% of the features are covered in Aqpm, although some very
 * advanced and uncommon ones are not present, and you will need them only if you're attempting to
 * develop a full-fledged frontend.
 * @par
 * The class is implemented as a singleton and it spawns an additional thread where Alpm will be jailed.
 * Since alpm was not designed to support threads, Aqpm implements a queue to avoid accidental concurrent access
 * to alpm methods. You are free to use Aqpm in multithreaded environments without having to worry.
 * @par
 * Aqpm @b _NEEDS_ to work as standard, non-privileged user. Failure in doing so might lead to unsecure behavior.
 * Aqpm uses privilege escalation with PolicyKit to perform privileged actions. Everything is done for you, even
 * if you can choose to manage the authentication part by yourself, in case you want more tight integration with
 * your GUI
 *
 * @note All the methods in this class, unless noted otherwise, are thread safe
 *
 * @author Dario Freddi
 */
class AQPM_EXPORT Backend : public QObject
{
    Q_OBJECT

public:

    /**
     * Defines the action type in a synchonous operation. There is a matching entry for
     * every function available
     */
    enum ActionType {
        GetAvailableDatabases,
        GetAvailableGroups,
        GetPackagesFromDatabase,
        GetLocalDatabase,
        GetPackagesFromGroup,
        GetUpgradeablePackages,
        GetInstalledPackages,
        GetPackageDependencies,
        GetPackageGroups,
        GetDependenciesOnPackage,
        GetPackageFiles,
        CountPackages,
        GetProviders,
        IsProviderInstalled,
        GetPackageDatabase,
        IsInstalled,
        GetAlpmVersion,
        ClearQueue,
        AddItemToQueue,
        GetQueue,
        SetShouldHandleAuthorization,
        ShouldHandleAuthorization,
        SetAnswer,
        GetPackage,
        GetGroup,
        GetDatabase,
        SetUpAlpm,
        TestLibrary,
        IsOnTransaction,
        SetFlags,
        ReloadPacmanConfiguration,
        AlpmListToStringList,
        UpdateDatabase,
        ProcessQueue,
        DownloadQueue,
        Initialization,
        SystemUpgrade,
        PerformAction,
        InterruptTransaction
    };

    /**
     * @return The current instance of the Backend
     */
    static Backend *instance();
    /**
     * @return Aqpm's version
     */
    static QString version();

    virtual ~Backend();

    /**
     * Internal method
     */
    QEvent::Type getEventTypeFor(ActionType event);

    /**
     * Performs a test on the library to check if Alpm and Aqpm are ready to be used
     *
     * @return \c true if the library is ready, \c false if there was a problem while testing itt
     */
    bool testLibrary();
    bool isOnTransaction();

    void performAsyncAction(ActionType type, const QVariantMap &args);

    Database::List getAvailableDatabases() const;
    Database getLocalDatabase() const;

    Group::List getAvailableGroups();

    Package::List getPackagesFromDatabase(const Database &db);

    Package::List getPackagesFromGroup(const Group &group);

    Package::List getUpgradeablePackages();

    Package::List getInstalledPackages();

    Package::List getPackageDependencies(const Package &package);

    Group::List getPackageGroups(const Package &package);

    Package::List getDependenciesOnPackage(const Package &package);

    QStringList getPackageFiles(const Package &package);

    int countPackages(Globals::PackageStatus status);

    QStringList getProviders(const Package &package);
    bool isProviderInstalled(const QString &provider);

    Database getPackageDatabase(const Package &package, bool checkver = false) const;

    bool isInstalled(const Package &package);

    bool updateDatabase();
    void fullSystemUpgrade(const QList<pmtransflag_t> &flags, bool downgrade = false);

    bool reloadPacmanConfiguration(); // In case the user modifies it.

    QStringList alpmListToStringList(alpm_list_t *list);

    QString getAlpmVersion();

    void clearQueue();
    void addItemToQueue(const QString &name, QueueItem::Action action);

    void processQueue(const QList<pmtransflag_t> &flags);
    void downloadQueue();
    QueueItem::List queue() const;

    void interruptTransaction();

    void setShouldHandleAuthorization(bool should);
    bool shouldHandleAuthorization() const;

    void setAnswer(int answer);

    Package getPackage(const QString &name, const QString &repo) const;
    Group getGroup(const QString &name) const;
    Database getDatabase(const QString &name) const;

    BackendThread *getInnerThread();

public Q_SLOTS:
    void setUpAlpm();

Q_SIGNALS:
    void dbStatusChanged(const QString &repo, int action);
    void dbQty(const QStringList &db);

    void transactionStarted();
    void transactionReleased();

    void streamDlProg(const QString &c, int bytedone, int bytetotal, int speed,
                      int listdone, int listtotal);

    void streamTransProgress(Aqpm::Globals::TransactionProgress event, const QString &pkgname, int percent,
                             int howmany, int remain);

    void streamTransEvent(Aqpm::Globals::TransactionEvent event, const QVariantMap &args);

    void streamTransQuestion(Aqpm::Globals::TransactionQuestion event, const QVariantMap &args);

    void errorOccurred(Aqpm::Globals::Errors code, const QVariantMap &args);

    void logMessageStreamed(const QString &msg);

    void operationFinished(bool success);

    void backendReady();

    // Reserved for thread communication

    void startDbUpdate();
    void startQueue(QList<pmtransflag_t> flags);

private Q_SLOTS:
    void setUpSelf(BackendThread *t);
    void streamError(int code, const QVariantMap &args);
    void doStreamTransProgress(int event, const QString &pkgname, int percent,
                             int howmany, int remain);
    void doStreamTransEvent(int event, const QVariantMap &args);
    void doStreamTransQuestion(int event, const QVariantMap &args);

private:
    Backend();

    class Private;
    Private *d;
};

}

Q_DECLARE_METATYPE(alpm_list_t*)

#endif /* BACKEND_H */
