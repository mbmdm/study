#include "myslot.h"

MySlot::MySlot(QObject *parent) : QObject(parent)
{

}

void MySlot::slot1(){

    qDebug()<< "slot(), name - " <<
                 sender()->objectName();
}

void MySlot::slot2(myclass::Priority p){

    qDebug() << "slot2(), name - " <<
                 sender()->objectName() <<
                 " priority = " << p;
}
