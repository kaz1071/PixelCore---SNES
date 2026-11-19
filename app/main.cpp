#include <QApplication>
#include <QLabel>
#include <QMainWindow>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("PixelCore-SNES"));
    window.resize(960, 640);

    auto* status = new QLabel(
        QStringLiteral("PixelCore-SNES\n\nThe emulator foundation is being built."),
        &window
    );
    status->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(status);

    window.show();
    return application.exec();
}
