#ifndef MYCLASS_H
#define MYCLASS_H

#include <QtCore>
#include <QtDebug>

class myclass : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Priority priority READ get_priority WRITE set_priority NOTIFY priorityChanged)
public:
    explicit myclass(QObject *parent = nullptr);

    enum Priority {low, mid, high}; Q_ENUM(Priority)

    void set_priority(Priority p);
    Priority get_priority();

signals: void priorityChanged(Priority);

private:
    Priority m_priority;

};

#endif // MYCLASS_H
