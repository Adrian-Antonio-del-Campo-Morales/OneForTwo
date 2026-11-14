#include "app/application.hpp"

#include "features/notifications.hpp"
#include "integration/feature_contracts.hpp"

#include <QApplication>
#include <QTemporaryDir>
#include <QtGlobal>

#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "../session/test_support.hpp"

namespace oneintwo {

// El producto todavía no contiene el módulo de avisos ni las fábricas nativas.
// Este target comprueba el enlace real de Application; los dobles mantienen
// fuera del núcleo esas dependencias pendientes.
struct SharedNotifications::Impl {};

SharedNotifications::SharedNotifications(NotificationDependencies)
    : impl_(std::make_unique<Impl>()) {}

SharedNotifications::~SharedNotifications() = default;

void SharedNotifications::tick() {}

void SharedNotifications::setAllowedApplications(std::set<std::string>) {}

void SharedNotifications::setAuthorization(SessionGeneration, FeatureSet, FeatureSet) {}

void SharedNotifications::sendManual(SessionGeneration, const NotificationContent&) {}

void SharedNotifications::forwardObserved(SessionGeneration, const ObservedNotification&) {}

void SharedNotifications::receiveNotice(const RemoteNotification&) {}

#if defined(Q_OS_WIN)
std::unique_ptr<DesktopVisualCapture> makeWindowsVisualCapture(PlatformContext) {
    return nullptr;
}

std::unique_ptr<DesktopNotifications> makeWindowsNotifications(PlatformContext) {
    return nullptr;
}
#elif defined(Q_OS_LINUX)
std::unique_ptr<DesktopVisualCapture> makeWaylandVisualCapture(PlatformContext) {
    return nullptr;
}

std::unique_ptr<DesktopNotifications> makeLinuxNotifications(PlatformContext) {
    return nullptr;
}
#endif

}  // namespace oneintwo

namespace {

std::string pathUnder(const QTemporaryDir& directory, const char* name) {
    return (std::filesystem::path(directory.path().toStdString()) / name)
        .string();
}

}  // namespace

TEST_CASE(application_starts_with_pairing_dialog_linked) {
    QTemporaryDir directory;
    CHECK(directory.isValid());

    oneintwo::ApplicationOptions options;
    options.storage_path = pathUnder(directory, "preferences.txt");
    options.identity_directory = pathUnder(directory, "identity");
    options.pairing_store_path = pathUnder(directory, "paired-devices.tsv");
    options.peer_name = "Equipo de prueba";

    oneintwo::Application application(std::move(options));
    application.start();
    application.start();
    QCoreApplication::processEvents();
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    return oneintwo_test::run();
}
