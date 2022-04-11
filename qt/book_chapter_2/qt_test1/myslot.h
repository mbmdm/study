#ifndef MYSLOT_H
#define MYSLOT_H

#include <QtCore>
#include <QtDebug>
#include "myclass.h"

class MySlot : public QObject
{
    Q_OBJECT
public:
    explicit MySlot(QObject *parent = nullptr);

public slots:
    void slot1();
    void slot2(myclass::Priority p);

};

#endif // MYSLOT_H
