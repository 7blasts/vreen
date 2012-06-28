#include "contentdownloader.h"
#include "utils.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QThread>
#include <QCryptographicHash>
#include <QUrl>
#include <QDesktopServices>
#include <QFileInfo>
#include <QNetworkReply>
#include <QTimer>
#include <QDir>

namespace vk {

class NetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    NetworkAccessManager(QObject *parent = 0) : QNetworkAccessManager(parent)
    {

    }

    QString fileHash(const QUrl &url) const
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(url.toString().toUtf8());
        return hash.result().toHex();
    }
    QString cacheDir() const
    {
        auto dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
        return dir + QLatin1String("/vk/");
    }
};

static QPointer<NetworkAccessManager> networkManager;

ContentDownloader::ContentDownloader(QObject *parent) :
    QObject(parent)
{
    if (!networkManager) {
        networkManager = new NetworkAccessManager;
        //use another thread for more smooth gui
        //auto thread = new QThread;
        //networkManager->moveToThread(thread);
        //connect(networkManager.data(), SIGNAL(destroyed()), thread, SLOT(quit()));
        //connect(thread, SIGNAL(finished()), SLOT(deleteLater()));
        //thread->start(QThread::LowPriority);
    }
}

QString ContentDownloader::download(const QUrl &link)
{
    QString path = networkManager->cacheDir()
            + networkManager->fileHash(link)
            + QLatin1String(".")
            + QFileInfo(link.path()).completeSuffix();

    if (QFileInfo(path).exists()) {
        //FIXME it maybe not work in some cases (use event instead emit)
        emit downloadFinished(path);
    } else {
        QNetworkRequest request(link);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        auto reply = networkManager->get(request);
        reply->setProperty("path", path);
        connect(reply, SIGNAL(finished()), this, SLOT(replyDone()));
    }
    return path;
}

void ContentDownloader::replyDone()
{
    auto reply = sender_cast<QNetworkReply*>(sender());
    QString cacheDir = networkManager->cacheDir();
    QDir dir(cacheDir);
    if (!dir.exists()) {
        if(!dir.mkpath(cacheDir)) {
            qWarning("Unable to create cache dir");
            return;
        }
    }
    //TODO move method to manager in other thread
    QString path = reply->property("path").toString();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("Unable to write file!");
        return;
    }
    file.write(reply->readAll());
    file.close();

    emit downloadFinished(path);
}

} // namespace vk

#include "contentdownloader.moc"
