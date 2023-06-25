#ifndef TIMERFRONTENDMANAGER_H
#define TIMERFRONTENDMANAGER_H

#include <QObject>
#include <QList>
#include <QString>

class TimerFrontendManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<QString> timer_names MEMBER m_timer_names)

public:
//TimerFrontendManager(QObject *parent = nullptr);
    TimerFrontendManager(const TimerFrontendManager& src);
    ~TimerFrontendManager();
    TimerFrontendManager();

    // Copy assignment
    TimerFrontendManager& operator=(const TimerFrontendManager& other);
    bool operator!=(const TimerFrontendManager& other);

public slots:

private:
    QList<QString> m_timer_names;

};


#endif // TIMERFRONTENDMANAGER_H
