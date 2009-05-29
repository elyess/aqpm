/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi                                    *
 *   drf54321@yahoo.it                                                     *
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

#include "w_callbacks.h"

#include <unistd.h>
#include <sys/types.h>
#include <QWaitCondition>
#include <QDebug>
#include <QGlobalStatic>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QTimer>
#include <QStringList>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkProxy>

#include "alpm.h"

#define UPDATE_SPEED_SEC 0.2f

namespace AqpmWorker
{

class CallBacksPrivate
{
public:
    CallBacksPrivate();
    void init();

    QNetworkRequest createNetworkRequest(const QUrl &url);

    Q_DECLARE_PUBLIC(CallBacks)
    CallBacks *q_ptr;

    int answer;
    qint64 list_total;
    qint64 list_xfered;
    qint64 list_last;
    QTimer updateTimer;
    QDateTime averageRateTime;
    QTime busControlTime;
    QString currentFile;
    QNetworkAccessManager *manager;
};

class CallBacksHelper
{
public:
    CallBacksHelper() : q(0) {}
    ~CallBacksHelper() {
        delete q;
    }
    CallBacks *q;
};

Q_GLOBAL_STATIC(CallBacksHelper, s_globalCallbacks)

CallBacks *CallBacks::instance()
{
    if (!s_globalCallbacks()->q) {
        new CallBacks;
    }

    return s_globalCallbacks()->q;
}

CallBacksPrivate::CallBacksPrivate()
        : answer(-1)
        , averageRateTime()
        , list_xfered(0)
        , list_last(0)
        , list_total(0)
{
}

void CallBacksPrivate::init()
{
    Q_Q(CallBacks);
    manager = new QNetworkAccessManager(q);
    manager->setProxy(QNetworkProxy());
    updateTimer.setSingleShot(true);
    updateTimer.setInterval(UPDATE_SPEED_SEC);
}

QNetworkRequest CallBacksPrivate::createNetworkRequest(const QUrl &url)
{
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", "Aqpm 1.0");
    return request;
}

CallBacks::CallBacks(QObject *parent)
        : QObject(parent)
        , d_ptr(new CallBacksPrivate())
{
    Q_ASSERT(!s_globalCallbacks()->q);
    s_globalCallbacks()->q = this;
    d_ptr->q_ptr = this;
    qDebug() << "Constructing callbacks";
    d_ptr->init();
}

CallBacks::~CallBacks()
{
    delete d_ptr;
}

void CallBacks::cb_trans_evt(pmtransevt_t event, void *data1, void *data2)
{
    Q_D(CallBacks);

    emit streamTransEvent(event, data1, data2);

    if (event == PM_TRANS_EVT_RETRIEVE_START && d->averageRateTime.isNull()) {
        d->averageRateTime = QDateTime::currentDateTime();
    }
}

void CallBacks::setAnswer(int answer)
{
    Q_D(CallBacks);

    d->answer = answer;
    emit answerReady();
}

void CallBacks::cb_trans_conv(pmtransconv_t event, void *data1, void *data2,
                              void *data3, int *response)
{
    Q_D(CallBacks);

    qDebug() << "Alpm is asking a question.";

    QString message;

    switch (event) {
    case PM_TRANS_CONV_INSTALL_IGNOREPKG:
        if (data2)
            /* TODO we take this route based on data2 being not null? WTF */
            message = QString(tr("%1 requires installing %2 from IgnorePkg/IgnoreGroup.\n Install anyway?")).arg(alpm_pkg_get_name((pmpkg_t *)data1)).
                      arg(alpm_pkg_get_name((pmpkg_t *)data2));
        else
            message = QString(tr("%1 is in IgnorePkg/IgnoreGroup.\n Install anyway?")).arg(alpm_pkg_get_name((pmpkg_t *)data1));

        break;
    /*case PM_TRANS_CONV_REMOVE_HOLDPKG:
        message = QString(tr("%1 is designated as a HoldPkg.\n Remove anyway?")).arg(alpm_pkg_get_name((pmpkg_t *)data1));
        break;*/
    case PM_TRANS_CONV_REPLACE_PKG:
        message = QString(tr("Replace %1 with %2/%3?")).arg(alpm_pkg_get_name((pmpkg_t *)data1)).
                  arg((char *)data3).arg(alpm_pkg_get_name((pmpkg_t *)data2));
        break;
    case PM_TRANS_CONV_CONFLICT_PKG:
        message = QString(tr("%1 conflicts with %2.\nRemove %3?")).arg((char *)data1).
                  arg((char *)data2).arg((char *)data2);
        break;
    case PM_TRANS_CONV_LOCAL_NEWER:
        message = QString(tr("%1-%2: local version is newer.\nUpgrade anyway?")).arg(alpm_pkg_get_name((pmpkg_t *)data1)).
                  arg(alpm_pkg_get_version((pmpkg_t *)data1));
        break;
    case PM_TRANS_CONV_CORRUPTED_PKG:
        message = QString(tr("File %1 is corrupted.\nDo you want to delete it?")).arg((char *)data1);
        break;
    }

    emit questionStreamed(message);

    QEventLoop e;

    connect(this, SIGNAL(answerReady()), &e, SLOT(quit()));

    qDebug() << "Question Streamed, entering in custom event loop, waiting for answer";

    e.exec();

    qDebug() << "Looks like answer is there, and it's " << d->answer;

    *response = d->answer;

    return;

}

int CallBacks::cb_fetch(const char *url, const char *localpath, time_t mtimeold, time_t *mtimenew)
{
    Q_D(CallBacks);

    qDebug() << "fetching: " << url << localpath;

    QNetworkReply *reply;
    QEventLoop re;
    reply = d->manager->head(d->createNetworkRequest(QUrl(url)));
    connect(reply, SIGNAL(finished()), &re, SLOT(quit()));
    re.exec();
    disconnect(reply, SIGNAL(finished()), &re, SLOT(quit()));

    d->currentFile = QString(url).split('/').at(QString(url).split('/').length() - 1);

    qDebug() << "The file is " << d->currentFile;

    // Let's check if the modification times collide. If so, there's no need to download the file

    QDateTime dtime = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
    qDebug() << "Modified on:" << dtime;
    time_t newtime = dtime.toTime_t();
    mtimenew = &newtime;
    reply->deleteLater();
    qDebug() << "Comparing";

    if (*mtimenew == mtimeold) {
        return 1;
    }

    qDebug() << "Compare successful";

    // If we got here, it's time to download the file.
    // We should handle the request synchronously, so get ready for some loops.

    QEventLoop e;
    reply = d->manager->get(d->createNetworkRequest(QUrl(url)));
    connect(reply, SIGNAL(finished()), &e, SLOT(quit()));
    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(computeDownloadProgress(qint64, qint64)));
    e.exec();

    QFile file(localpath + d->currentFile);

    if (!file.open(QIODevice::ReadWrite)) {
        return -1;
    }

    file.write(reply->readAll());

    reply->deleteLater();
    return 0;
}

void CallBacks::computeDownloadProgress(qint64 downloaded, qint64 total)
{
    Q_D(CallBacks);

    if (downloaded == total) {
        d->list_xfered += downloaded;
        return;
    }

    d->list_last = d->list_xfered + downloaded;

    // Update Rate
    int seconds = d->averageRateTime.secsTo(QDateTime::currentDateTime());

    int rate = 0;

    if (seconds != 0 && d->list_last != 0) {
        rate = (int)(d->list_last / seconds);
    }

    // Don't you fucking hog the bus! Update maximum each 0.250 seconds
    if (d->busControlTime.msecsTo(QTime::currentTime()) > 250 || !d->busControlTime.isValid()) {
        emit streamDlProg(d->currentFile, (int)downloaded, (int)total, (int)rate, (int)d->list_last, (int)d->list_total);
        d->busControlTime = QTime::currentTime();
    }
}

void CallBacks::cb_dl_total(off_t total)
{
    Q_D(CallBacks);

    d->list_total = total;
    qDebug() << "total called, offset" << total;
    /* if we get a 0 value, it means this list has finished downloading,
     * so clear out our list_xfered as well */
    if (total == 0) {
        d->list_xfered = 0;
        d->list_last = 0;
        d->list_total = 0;
    }
}

void CallBacks::cb_trans_progress(pmtransprog_t event, const char *pkgname, int percent,
                                  int howmany, int remain)
{
    Q_D(CallBacks);

    if (d->updateTimer.isActive()) {
        return;
    } else {
        d->updateTimer.start();
    }

    if (percent > 0 && percent < 100)
        return;

    qDebug() << "Streaming trans progress";

    emit streamTransProgress((int)event, QString(pkgname), percent, howmany, remain);
}



void CallBacks::cb_log(pmloglevel_t level, char *fmt, va_list args)
{
    if (!strlen(fmt)) {
        return;
    }

    char *string = NULL;
    if (pm_vasprintf(&string, level, fmt, args) == -1)
        return;

    if (string != NULL) {
        QString msg = QString::fromLocal8Bit(string);
        emit logMsgStreamed(msg);
    }
}

/* Now the real suckness is coming... */

void cb_dl_total(off_t total)
{
    CallBacks::instance()->cb_dl_total(total);
}

int cb_fetch(const char *url, const char *localpath, time_t mtimeold, time_t *mtimenew)
{
    return CallBacks::instance()->cb_fetch(url, localpath, mtimeold, mtimenew);
}

void cb_trans_progress(pmtransprog_t event, const char *pkgname, int percent,
                       int howmany, int remain)
{
    CallBacks::instance()->cb_trans_progress(event, pkgname, percent, howmany, remain);
}

void cb_trans_conv(pmtransconv_t event, void *data1, void *data2,
                   void *data3, int *response)
{
    qDebug() << "Question Event Triggered";
    CallBacks::instance()->cb_trans_conv(event, data1, data2, data3, response);
}

void cb_trans_evt(pmtransevt_t event, void *data1, void *data2)
{
    qDebug() << "Received Event Callback";
    CallBacks::instance()->cb_trans_evt(event, data1, data2);
}

void cb_log(pmloglevel_t level, char *fmt, va_list args)
{
    CallBacks::instance()->cb_log(level, fmt, args);
}

int pm_vasprintf(char **string, pmloglevel_t level, const char *format, va_list args)
{
    int ret = 0;
    char *msg = NULL;

    /* if current logmask does not overlap with level, do not print msg */

    /* print the message using va_arg list */
    ret = vasprintf(&msg, format, args);

    /* print a prefix to the message */
    switch (level) {
    case PM_LOG_DEBUG:
        return -1;
        break;
    case PM_LOG_ERROR:
        asprintf(string, "error: %s", msg);
        break;
    case PM_LOG_WARNING:
        asprintf(string, "warning: %s", msg);
        break;
    case PM_LOG_FUNCTION:
        return -1;
        break;
    default:
        break;
    }

    free(msg);

    return(ret);
}

}