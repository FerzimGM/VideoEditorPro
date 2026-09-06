#ifndef EFFECTIMAGEPROVIDER_H
#define EFFECTIMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>

// Bridges decoded/composited frames from the C++ side to QML via the
// "image://effects/..." URL scheme, so the preview Image element can
// simply bind its source and receive live frame updates.
class EffectImageProvider : public QQuickImageProvider
{
public:
    EffectImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
        // Start with a black 1280x720 placeholder so QML doesn't report
        // "Failed to get image from provider" before the first frame
        // (or the first clip) is available.
        m_frame = QImage(1280, 720, QImage::Format_RGB888);
        m_frame.fill(Qt::black);
    }

    void setFrame(const QImage& frame)
    {
        QMutexLocker lock(&m_mutex);
        m_frame = frame;
    }

    QImage requestImage(const QString& /*id*/,
                        QSize* size,
                        const QSize& /*requestedSize*/) override
    {
        QMutexLocker lock(&m_mutex);
        if (size) *size = m_frame.size();
        return m_frame;
    }

private:
    QImage  m_frame;
    QMutex  m_mutex;
};

#endif // EFFECTIMAGEPROVIDER_H

