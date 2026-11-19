#include "core/memory/snes_memory.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>
#include <vector>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("PixelCore - SNES"));
    QApplication::setApplicationVersion(QStringLiteral(PIXELCORE_SNES_VERSION));
    QApplication::setOrganizationName(QStringLiteral("PixelCore"));
    QApplication::setDesktopFileName(QStringLiteral("pixelcore-snes.desktop"));

    const QIcon appIcon = QIcon::fromTheme(QStringLiteral("pixelcore-snes-v2"));
    application.setWindowIcon(appIcon);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("PixelCore - SNES"));
    window.setWindowIcon(appIcon);
    window.resize(1100, 720);
    window.setMinimumSize(800, 520);

    SnesMemory memory;
    QString loadedRomPath;

    auto* screen = new QLabel(
        QStringLiteral("PIXELCORE - SNES\n\n"
                       "No game loaded\n\n"
                       "Open a Super Nintendo ROM to get started."),
        &window
    );
    screen->setAlignment(Qt::AlignCenter);
    screen->setMinimumSize(512, 448);
    screen->setStyleSheet(
        QStringLiteral(
            "QLabel {"
            " background-color: #10131b;"
            " color: #aebbd0;"
            " border: 1px solid #343b4b;"
            " font-size: 18px;"
            " padding: 24px;"
            "}"
        )
    );

    auto* central = new QWidget(&window);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->addWidget(screen, 1);
    window.setCentralWidget(central);

    auto* fileMenu = window.menuBar()->addMenu(QStringLiteral("&File"));
    auto* openAction = fileMenu->addAction(QStringLiteral("&Open ROM..."));
    openAction->setShortcut(QKeySequence::Open);
    auto* closeRomAction = fileMenu->addAction(QStringLiteral("Close &ROM"));
    closeRomAction->setEnabled(false);
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction(QStringLiteral("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);

    auto* emulationMenu = window.menuBar()->addMenu(QStringLiteral("&Emulation"));
    auto* pauseAction = emulationMenu->addAction(QStringLiteral("&Pause"));
    pauseAction->setShortcut(Qt::Key_P);
    pauseAction->setEnabled(false);
    auto* resetAction = emulationMenu->addAction(QStringLiteral("&Reset"));
    resetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    resetAction->setEnabled(false);

    auto* toolbar = window.addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);
    toolbar->addAction(openAction);
    toolbar->addAction(closeRomAction);
    toolbar->addSeparator();
    toolbar->addAction(pauseAction);
    toolbar->addAction(resetAction);

    window.statusBar()->showMessage(
        QStringLiteral("Ready — open an .sfc or .smc ROM")
    );

    QObject::connect(openAction, &QAction::triggered, &window, [&]() {
        const QString path = QFileDialog::getOpenFileName(
            &window,
            QStringLiteral("Open Super Nintendo ROM"),
            QString(),
            QStringLiteral("SNES ROMs (*.sfc *.smc *.fig);;All files (*)")
        );
        if (path.isEmpty()) {
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(
                &window,
                QStringLiteral("Could not open ROM"),
                file.errorString()
            );
            return;
        }

        const QByteArray bytes = file.readAll();
        if (bytes.isEmpty()) {
            QMessageBox::warning(
                &window,
                QStringLiteral("Invalid ROM"),
                QStringLiteral("The selected file is empty.")
            );
            return;
        }

        std::vector<std::uint8_t> rom(
            reinterpret_cast<const std::uint8_t*>(bytes.constData()),
            reinterpret_cast<const std::uint8_t*>(bytes.constData()) + bytes.size()
        );
        memory.loadLoRom(std::move(rom));
        loadedRomPath = path;

        const QString filename = QFileInfo(path).fileName();
        screen->setText(
            QStringLiteral("ROM loaded\n\n%1\n\n"
                           "Size: %2 KiB\n\n"
                           "The CPU and video/audio systems are still under development.")
                .arg(filename)
                .arg((bytes.size() + 1023) / 1024)
        );
        closeRomAction->setEnabled(true);
        window.statusBar()->showMessage(
            QStringLiteral("Loaded %1 — emulation is not connected yet").arg(filename)
        );
    });

    QObject::connect(closeRomAction, &QAction::triggered, &window, [&]() {
        memory.clear();
        loadedRomPath.clear();
        screen->setText(
            QStringLiteral("PIXELCORE - SNES\n\n"
                           "No game loaded\n\n"
                           "Open a Super Nintendo ROM to get started.")
        );
        closeRomAction->setEnabled(false);
        window.statusBar()->showMessage(
            QStringLiteral("Ready — open an .sfc or .smc ROM")
        );
    });

    QObject::connect(pauseAction, &QAction::triggered, &window, [&]() {
        window.statusBar()->showMessage(
            QStringLiteral("Emulation is not available yet")
        );
    });
    QObject::connect(resetAction, &QAction::triggered, &window, [&]() {
        if (!loadedRomPath.isEmpty()) {
            window.statusBar()->showMessage(
                QStringLiteral("CPU reset will be available when emulation is connected")
            );
        }
    });
    QObject::connect(exitAction, &QAction::triggered, &application, &QApplication::quit);

    window.show();
    return application.exec();
}
