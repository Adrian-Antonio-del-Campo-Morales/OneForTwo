#include "app/application.hpp"

#include <QApplication>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("OneInTwo"));
    application.setQuitOnLastWindowClosed(false);
    oneintwo::Application product;
    product.start();
    return application.exec();
}
