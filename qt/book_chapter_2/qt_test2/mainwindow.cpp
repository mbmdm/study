#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QObject::connect(ui->pushButton_3, SIGNAL(clicked(bool)), SIGNAL(msignal(bool)));
    QObject::connect(this, SIGNAL(msignal(bool)), this, SLOT(on_pushButton_clicked()));

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{
    auto text = ui->label->text();

    int num;
    if (text.size() == 0)
        num = 0;
    else
        num = text.toInt();

    num++;

    ui->label->setText(QString::number(num));
}


void MainWindow::on_pushButton_2_clicked()
{
    bool flag = ui->pushButton->signalsBlocked();
    ui->pushButton->blockSignals(!flag);
}

void MainWindow::on_pushButton_3_clicked()
{
}
