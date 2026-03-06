#ifndef EFFECTIMAGEPROVIDER_H
#define EFFECTIMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>

class EffectImageProvider : public QQuickImageProvider
{
public:
    EffectImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
        // Инициализируем чёрным кадром 1280×720 чтобы не было ошибки
        // "Failed to get image from provider" при старте до первого клипа
        m_frame = QImage(1280, 720, QImage::Format_RGB888);
        m_frame.fill(Qt::black);
    }

    void setFrame(const QImage& frame) {
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
