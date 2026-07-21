/**
 * Chat-Live Desktop entry point.
 *
 * Usage:
 *   ./chatlive-desktop
 *   ./chatlive-desktop --config /path/to/app.json
 *
 * Config is copied beside the binary at build time (config/app.json).
 * Override with CHATLIVE_API_BASE / CHATLIVE_WS_URL / CHATLIVE_KEYCLOAK_ISSUER.
 */

#include "app/Application.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[])
{
    QApplication qtApp(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Chat-Live Desktop"));
    QApplication::setOrganizationName(QStringLiteral("ChatLiveOK"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Chat-Live-OK Qt desktop client"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOpt({QStringLiteral("c"), QStringLiteral("config")},
                                 QStringLiteral("Path to app.json"),
                                 QStringLiteral("path"));
    parser.addOption(configOpt);
    parser.process(qtApp);

    ccv::Application app;
    if (!app.start(parser.value(configOpt))) {
        return 1;
    }

    return qtApp.exec();
}
