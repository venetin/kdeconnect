/**
 * Copyright 2013 Albert Vaca <albertvaka@gmail.com>
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

#include "landevicelink.h"
#include "core_debug.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include "../linkprovider.h"
#include "uploadjob.h"
#include "downloadjob.h"
#include "socketlinereader.h"

LanDeviceLink::LanDeviceLink(const QString& deviceId, LinkProvider* parent, QSslSocket* socket)
    : DeviceLink(deviceId, parent)
    , mSocketLineReader(new SocketLineReader(socket))
    , onSsl(false)
{
    connect(mSocketLineReader, SIGNAL(readyRead()),
            this, SLOT(dataReceived()));

    //We take ownership of the socket.
    //When the link provider distroys us,
    //the socket (and the reader) will be
    //destroyed as well
    connect(socket, SIGNAL(disconnected()),
            this, SLOT(deleteLater()));
    mSocketLineReader->setParent(this);
    socket->setParent(this);
}

void LanDeviceLink::setOnSsl(bool value) {
    onSsl = value;
}


bool LanDeviceLink::sendPackageEncrypted(QCA::PublicKey& key, NetworkPackage& np)
{
    if (np.hasPayload()) {
        QVariantMap sslInfo;
        if (onSsl) {
            sslInfo.insert("useSsl", true);
            sslInfo.insert("deviceId", deviceId());
        }
        UploadJob* job = new UploadJob(np.payload(), sslInfo);
        job->start();
        np.setPayloadTransferInfo(job->getTransferInfo());
    }

    if (!onSsl) {
        np.encrypt(key);
    }

    int written = mSocketLineReader->write(np.serialize());

    //TODO: Actually detect if a package is received or not, now we keep TCP
    //"ESTABLISHED" connections that look legit (return true when we use them),
    //but that are actually broken (until keepalive detects that they are down).
    return (written != -1);
}

bool LanDeviceLink::sendPackage(NetworkPackage& np)
{
    if (np.hasPayload()) {
        QVariantMap sslInfo;
        if (onSsl) {
            sslInfo.insert("useSsl", true);
            sslInfo.insert("deviceId", deviceId());
        }
        UploadJob* job = new UploadJob(np.payload(), sslInfo);
        job->start();
        np.setPayloadTransferInfo(job->getTransferInfo());
    }

    int written = mSocketLineReader->write(np.serialize());
    return (written != -1);
}

void LanDeviceLink::dataReceived()
{

    if (mSocketLineReader->bytesAvailable() == 0) return;

    const QByteArray package = mSocketLineReader->readLine();

    //qCDebug(KDECONNECT_CORE) << "LanDeviceLink dataReceived" << package;

    NetworkPackage unserialized(QString::null);
    NetworkPackage::unserialize(package, &unserialized);
    if (unserialized.isEncrypted()) {
        //mPrivateKey should always be set when device link is added to device, no null-checking done here
        // TODO : Check this with old device since package thorough ssl in unencrypted
        unserialized.decrypt(mPrivateKey, &unserialized);
        qDebug() << "Serialized " << unserialized.serialize();
    }

    if (unserialized.hasPayloadTransferInfo()) {
//        qCDebug(KDECONNECT_CORE) << "HasPayloadTransferInfo";
        // FIXME : Directly setting these values to payloadTransferInfo now working
        QVariantMap sslInfo = unserialized.payloadTransferInfo();
        if (onSsl) {
            sslInfo.insert("useSsl", true);
            sslInfo.insert("deviceId", deviceId());
        }
        DownloadJob* job = new DownloadJob(mSocketLineReader->peerAddress(), sslInfo);
        job->start();
        qCDebug(KDECONNECT_CORE) << "Checking payload status " << job->getPayload().isNull();
        unserialized.setPayload(job->getPayload(), unserialized.payloadSize());
    }

    Q_EMIT receivedPackage(unserialized);

    if (mSocketLineReader->bytesAvailable() > 0) {
        QMetaObject::invokeMethod(this, "dataReceived", Qt::QueuedConnection);
    }

}
