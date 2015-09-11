/*
 * Copyright 2013 Albert Vaca <albertvaka@gmail.com>
 * Copyright 2015 Aleix Pol i Gonzalez <aleixpol@kde.org>
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

#include "filetransferjob.h"
#include "daemon.h"
#include <core_debug.h>

#include <qalgorithms.h>
#include <QFileInfo>
#include <QDebug>

#include <KLocalizedString>

FileTransferJob::FileTransferJob(const QSharedPointer<QIODevice>& origin, qint64 size, const QUrl& destination)
    : KJob()
    , mOrigin(origin)
    , mReply(Q_NULLPTR)
    , mDeviceName("KDE Connect") //TODO: Actually fetch the device name
    , mDestination(destination)
    , mSpeedBytes(0)
    , mWritten(0)
{
    Q_ASSERT(mOrigin);
    if (mDestination.scheme().isEmpty()) {
        qWarning() << "Destination QUrl" << mDestination << "lacks a scheme. Setting its scheme to 'file'.";
        mDestination.setScheme("file");
    }

    if (size >= 0) {
        setTotalAmount(Bytes, size);
    }
    setCapabilities(Killable);
    qCDebug(KDECONNECT_CORE) << "FileTransferJob Downloading payload to" << destination;
}

void FileTransferJob::start()
{
    QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
    //qCDebug(KDECONNECT_CORE) << "FileTransferJob start";
}

void FileTransferJob::doStart()
{
    description(this, i18n("Receiving file over KDE-Connect"),
        QPair<QString, QString>(i18nc("File transfer origin", "From"),
        mDeviceName)
    );

    if (mDestination.isLocalFile() && QFile::exists(mDestination.toLocalFile())) {
        setError(2);
        setErrorText(i18n("Filename already present"));
        emitResult();
    }

    startTransfer();
}

void FileTransferJob::startTransfer()
{
    setProcessedAmount(Bytes, 0);
    mTime = QTime::currentTime();
    description(this, i18n("Receiving file over KDE-Connect"),
                        QPair<QString, QString>(i18nc("File transfer origin", "From"),
                        mDeviceName),
                        QPair<QString, QString>(i18nc("File transfer destination", "To"), mDestination.toLocalFile()));

    mReply = Daemon::instance()->networkAccessManager()->put(QNetworkRequest(mDestination), mOrigin.data());
    connect(mReply, &QNetworkReply::uploadProgress, this, [this](qint64 bytesSent, qint64 /*bytesTotal*/) {
        setProcessedAmount(Bytes, bytesSent);
        emitSpeed(bytesSent/mTime.elapsed());
    });
    connect(mReply, &QNetworkReply::finished, this, &FileTransferJob::transferFinished);
}

void FileTransferJob::transferFinished()
{
    //TODO: MD5-check the file
    if (mReply->error()) {
        qCDebug(KDECONNECT_CORE) << "Couldn't transfer the file successfully" << mReply->errorString();
        setError(mReply->error());
        setErrorText(i18n("Received incomplete file: %1", mReply->errorString()));
    } else {
        qCDebug(KDECONNECT_CORE) << "Finished transfer" << mDestination;
    }

    emitResult();
}

bool FileTransferJob::doKill()
{
    if (mReply) {
        mReply->close();
    }
    if (mOrigin) {
        mOrigin->close();
    }
    return true;
}
