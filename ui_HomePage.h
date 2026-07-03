/********************************************************************************
** Form generated from reading UI file 'HomePage.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HOMEPAGE_H
#define UI_HOMEPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Placeholder
{
public:

    void setupUi(QWidget *Placeholder)
    {
        if (Placeholder->objectName().isEmpty())
            Placeholder->setObjectName("Placeholder");
        Placeholder->resize(400, 300);

        retranslateUi(Placeholder);

        QMetaObject::connectSlotsByName(Placeholder);
    } // setupUi

    void retranslateUi(QWidget *Placeholder)
    {
        Placeholder->setWindowTitle(QCoreApplication::translate("Placeholder", "Placeholder", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Placeholder: public Ui_Placeholder {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HOMEPAGE_H
