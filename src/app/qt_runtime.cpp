#include "qt_runtime.hpp"

#include <QCoreApplication>
#include <QMetaObject>
#include <Qt>

#include <chrono>
#include <utility>

namespace oneintwo {

ModuleRuntime makeQtModuleRuntime() {
    ModuleRuntime runtime;
    runtime.now = [] { return std::chrono::steady_clock::now(); };
    runtime.post = [](Task task) {
        auto* application = QCoreApplication::instance();
        if (application == nullptr) {
            task();
            return;
        }
        QMetaObject::invokeMethod(
            application, [task = std::move(task)]() mutable { task(); }, Qt::QueuedConnection);
    };
    return runtime;
}

} // namespace oneintwo
