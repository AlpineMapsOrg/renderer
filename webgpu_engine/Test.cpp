#include "Test.h"
#include <QFile>
#include <QDebug>
#include <QTextStream>

namespace webgpu_engine {

void Test::loadAndPrintTextFromFile(QString file_path) {
    qDebug() << "File:" << file_path;

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file";
        return;
    }

    QTextStream in(&file);
    QString contents = in.readAll();
    file.close();

           // Print the first 255 characters
    qDebug() << contents.left(255);
}

}
