#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QTimer>

class Worker : public QObject
{
    Q_OBJECT

public:
    explicit Worker(QObject *parent = nullptr);

public slots:
    void startWork();
    void stopWork();

signals:
    void tick(int count);
    void finished();

private:
    QTimer *m_timer;
    int m_count = 0;
};

#endif // WORKER_H
