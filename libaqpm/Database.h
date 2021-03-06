/***************************************************************************
 *   Copyright (C) 2009 by Dario Freddi                                    *
 *   drf54321@gmail.com                                                    *
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

#ifndef DATABASE_H
#define DATABASE_H

#include <QtCore/QList>
#include <QtCore/QMetaType>

#include "Package.h"

typedef struct __pmdb_t pmdb_t;

namespace Aqpm
{

class AQPM_EXPORT Database
{
public:
    typedef QList<Database*> List;

    virtual ~Database();

    QString name() const;
    QString path() const;
    QStringList servers() const;
    Package::List packages();
    pmdb_t *alpmDatabase() const;
    bool isValid() const;

private:
    explicit Database(pmdb_t *db);

    class Private;
    Private *d;

    friend class BackendThread;
};

}

Q_DECLARE_METATYPE(Aqpm::Database*)
Q_DECLARE_METATYPE(Aqpm::Database::List)

#endif // DATABASE_H
