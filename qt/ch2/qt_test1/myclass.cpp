#include "myclass.h"

myclass::myclass(QObject *parent) : QObject(parent)
{
    qDebug() << "myclass::myclass(QObject *parent)";
}

void myclass::set_priority(Priority p) {

    m_priority = p;
    emit priorityChanged(p);
}

myclass::Priority myclass::get_priority(){

    return m_priority;
}

