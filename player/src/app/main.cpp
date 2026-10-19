#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("dk2music"));
    QApplication::setOrganizationName(QStringLiteral("dk2music"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Plays the Dungeon Keeper II dynamic music"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("game-dir"), QStringLiteral("Dungeon Keeper II installation folder"));
    parser.process(app);

    MainWindow window(parser.positionalArguments().value(0));
    window.show();
    return app.exec();
}
