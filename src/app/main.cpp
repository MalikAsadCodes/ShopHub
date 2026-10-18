// ShopHub - e-commerce desktop application
// Entry point: sets up paths, logging and theme, opens the database,
// then loops between the sign-in dialog and the main window.

#include "common/Logger.h"
#include "services/AppContext.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "ui/dialogs/AuthDialog.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>

using namespace shophub;

namespace {

// Simple generated app icon: rounded square with an upward arrow.
QIcon makeAppIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient g(0, 0, size, size);
        g.setColorAt(0, QColor(0x0E, 0x7C, 0x66));
        g.setColorAt(1, QColor(0x13, 0x4E, 0x6F));
        QPainterPath box;
        box.addRoundedRect(QRectF(0, 0, size, size), size * 0.22, size * 0.22);
        p.fillPath(box, g);
        QPen pen(Qt::white, qMax(1.5, size * 0.1), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        const qreal c = size / 2.0;
        p.drawLine(QPointF(c, size * 0.74), QPointF(c, size * 0.28));
        p.drawLine(QPointF(size * 0.32, size * 0.45), QPointF(c, size * 0.27));
        p.drawLine(QPointF(size * 0.68, size * 0.45), QPointF(c, size * 0.27));
        icon.addPixmap(pm);
    }
    return icon;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("ShopHub"));
    QApplication::setApplicationName(QStringLiteral("ShopHub"));
    QApplication::setApplicationVersion(QStringLiteral(SHOPHUB_VERSION));
    QApplication::setWindowIcon(makeAppIcon());

    QFont font = QApplication::font();
    font.setFamilies({QStringLiteral("Segoe UI"), QStringLiteral("Inter"), QStringLiteral("Noto Sans"),
                      QStringLiteral("Helvetica Neue"), QStringLiteral("Arial")});
    font.setPointSize(10);
    QApplication::setFont(font);

    // Writable data folder, e.g. C:/Users/<you>/AppData/Roaming/ShopHub/ShopHub
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    Logger::install(QDir(dataDir).filePath(QStringLiteral("logs")));
    qInfo().noquote() << "ShopHub" << QApplication::applicationVersion() << "starting, data folder:" << dataDir;

    ui::Theme::apply(ui::Theme::loadSaved());

    AppConfig config;
    config.databasePath = QDir(dataDir).filePath(QStringLiteral("shophub.db"));
    config.imageDirectory = QDir(dataDir).filePath(QStringLiteral("images"));
    auto created = AppContext::create(config);
    if (!created) {
        QMessageBox::critical(nullptr, QStringLiteral("ShopHub"),
                              QStringLiteral("ShopHub could not start.\n\n%1\n\nDetails are in the log file:\n%2")
                                  .arg(created.error(), Logger::logFilePath()));
        return 1;
    }
    std::unique_ptr<AppContext> ctx = created.takeValue();

    Session session;

    // First run: no administrator yet, so create one.
    auto hasAdmin = ctx->auth().hasAdmin();
    if (!hasAdmin) {
        QMessageBox::critical(nullptr, QStringLiteral("ShopHub"), hasAdmin.error());
        return 1;
    }
    if (!hasAdmin.value()) {
        ui::AuthDialog setup(*ctx, ui::AuthDialog::Mode::FirstRun);
        if (setup.exec() != QDialog::Accepted)
            return 0;
        session = setup.session();
    }

    // Sign in -> main window -> (sign out) -> sign in again ...
    int exitCode = 0;
    for (;;) {
        if (!session.isValid()) {
            ui::AuthDialog signIn(*ctx, ui::AuthDialog::Mode::SignIn);
            if (signIn.exec() != QDialog::Accepted)
                break;
            session = signIn.session();
        }

        bool signedOut = false;
        {
            ui::MainWindow window(*ctx, session);
            QObject::connect(&window, &ui::MainWindow::signedOut, [&signedOut] { signedOut = true; });
            window.show();
            exitCode = app.exec();
        }
        if (!signedOut)
            break;
        session = Session();
    }

    qInfo() << "ShopHub shutting down";
    return exitCode;
}
