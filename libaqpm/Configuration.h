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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QObject>

namespace Aqpm
{

class Configuration : public QObject
{
    Q_OBJECT

public:
    static Configuration *instance();

    virtual ~Configuration();

    bool saveConfiguration();
    void saveConfigurationAsync();

    void setValue(const QString &key, const QString &val);
    QVariant value(const QString &key) const;

    void remove(const QString &key);
    bool exists(const QString &key, const QString &val = QString()) const;

    void setOrUnset(bool set, const QString &key, const QString &val = QString());

    void reload();

private Q_SLOTS:
    void workerResult(bool result);

Q_SIGNALS:
    void configurationSaved(bool result);

private:
    Configuration();

    class Private;
    Private *d;
};

}

#endif // CONFIGURATION_H