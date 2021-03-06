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

#ifndef AURBACKEND_H
#define AURBACKEND_H

#include "Visibility.h"

#include <QtCore/QObject>

class QNetworkReply;
namespace Aqpm
{

namespace Aur
{

class AQPM_EXPORT Package
{
public:
    typedef QList<Package*> List;

    int id;
    QString name;
    QString version;
    QString description;
    int category;
    int location;
    QString url;
    QString path;
    QString license;
    int votes;
    bool outOfDate;
};

class AQPM_EXPORT Backend : public QObject
{
    Q_OBJECT

public:
    static Backend *instance();

    virtual ~Backend();

    void search(const QString &subject) const;
    Package::List searchSync(const QString &subject) const;

    void info(int id) const;
    Package *infoSync(int id) const;

    void prepareBuildEnvironment(int id, const QString &envpath) const;
    void prepareBuildEnvironmentSync(int id, const QString &envpath) const;

Q_SIGNALS:
    void searchCompleted(const QString &searchSubject, const Aqpm::Aur::Package::List &results);
    void infoCompleted(int id, Aqpm::Aur::Package *result);
    void buildEnvironmentReady(int id, const QString &envpath);

private:
    Backend(QObject* parent = 0);

    class Private;
    Private * const d;

    Q_PRIVATE_SLOT(d, void __k__replyFinished(QNetworkReply *reply))
};

}
}

#endif // AURBACKEND_H
