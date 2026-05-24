#include "Worker.h"
#include <QDebug>

Worker::Worker(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1000); // 1秒

    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_count++;
        emit tick(m_count);
    });
}

void Worker::startWork()
{
    m_count = 0;
    m_timer->start();
}

void Worker::stopWork()
{
    m_timer->stop();
    qDebug() << "Worker stopped, total ticks:" << m_count;
    emit finished();
}
