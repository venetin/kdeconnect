/*
 * Copyright 2013 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <KApplication>
#include <KUrl>
#include <kcmdlineargs.h>
#include <k4aboutdata.h>
#include <kaboutdata.h>
#include <interfaces/devicesmodel.h>
#include <iostream>
#include <QDBusMessage>
#include <QDBusConnection>

int main(int argc, char** argv)
{
    K4AboutData about("kctool", 0, ki18n(("kctool")), "1.0", ki18n("KDE Connect CLI tool"),
                     K4AboutData::License_GPL, ki18n("(C) 2013 Aleix Pol Gonzalez"));
    about.addAuthor( ki18n("Aleix Pol Gonzalez"), KLocalizedString(), "aleixpol@kde.org" );
    KCmdLineArgs::init(argc, argv, &about);
    KCmdLineOptions options;
    options.add("l")
           .add("list-devices", ki18n("List all devices"));
    options.add("share <path>", ki18n("Share a file"));
    options.add("device <dev>", ki18n("Device ID"));
    KCmdLineArgs::addCmdLineOptions( options );
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    KApplication app;
    if(args->isSet("l")) {
        DevicesModel devices;
        devices.setDisplayFilter(DevicesModel::StatusUnknown);
        for(int i=0, rows=devices.rowCount(); i<rows; ++i) {
            QModelIndex idx = devices.index(i);
            std::cout << "- " << idx.data(Qt::DisplayRole).toString().toStdString()
                      << ": " << idx.data(DevicesModel::IdModelRole).toString().toStdString() << std::endl;
        }
        std::cout << devices.rowCount() << " devices found" << std::endl;
    } else {
        QString device;
        if(!args->isSet("device")) {
            KCmdLineArgs::usageError(i18n("No device specified"));
        }
        device = args->getOption("device");
        QUrl url;
        if(args->isSet("share")) {
            url = args->makeURL(args->getOption("share").toLatin1());
        }
        args->clear();

        if(!url.isEmpty() && !device.isEmpty()) {
            QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kdeconnect", "/modules/kdeconnect/devices/"+device+"/share", "org.kde.kdeconnect.device.share", "shareUrl");
            msg.setArguments(QVariantList() << url.toString());

            QDBusConnection::sessionBus().call(msg);
        } else
            KCmdLineArgs::usageError(i18n("Nothing to be done with the device"));
    }
    QMetaObject::invokeMethod(&app, "quit", Qt::QueuedConnection);

    return app.exec();
}
