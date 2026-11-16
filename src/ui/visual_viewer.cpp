#include "../features/visual_share.hpp"

#include "../integration/feature_contracts.hpp"

#include <QCloseEvent>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace oneintwo {
namespace {

class FrameLabel : public QLabel {
public:
    explicit FrameLabel(QWidget* parent = nullptr) : QLabel(parent) {
        setObjectName(QStringLiteral("visualFrame"));
        setAlignment(Qt::AlignCenter);
        setMinimumSize(160, 90);
        setStyleSheet(QStringLiteral("background:#101010;color:#d0d0d0;"));
        setText(QStringLiteral("Sin imagen"));
    }

    void setFrame(const QImage& image) {
        image_ = image;
        rescale();
    }

    void clearFrame() {
        image_ = QImage();
        QLabel::clear();
        setText(QStringLiteral("Sin imagen"));
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        rescale();
    }

private:
    void rescale() {
        if (image_.isNull()) {
            return;
        }
        const QPixmap scaled = QPixmap::fromImage(image_).scaled(
            size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QLabel::setPixmap(scaled);
    }

    QImage image_;
};

QString modalityTitle(VisualSourceKind source) {
    return source == VisualSourceKind::monitor ? QStringLiteral("Pantalla completa")
                                               : QStringLiteral("Una ventana");
}

QString stateText(VisualSourceState state) {
    switch (state) {
    case VisualSourceState::available:
        return QStringLiteral("Sin imagen");
    case VisualSourceState::suspended:
        return QStringLiteral("Fuente en pausa");
    case VisualSourceState::closed:
        return QStringLiteral("Fuente cerrada");
    }
    return QStringLiteral("Sin imagen");
}

class ViewerWindow final : public QWidget, public VisualViewer {
public:
    ViewerWindow(ModuleRuntime runtime, std::function<void(VisualShareRef)> user_closed)
        : runtime_(std::move(runtime)), user_closed_(std::move(user_closed)) {
        setWindowTitle(QStringLiteral("OneInTwo — Visor (solo lectura)"));
        setAttribute(Qt::WA_DeleteOnClose, false);
        frame_ = new FrameLabel(this);
        state_ = new QLabel(QStringLiteral("Sin operación"), this);
        state_->setObjectName(QStringLiteral("visualStateLabel"));
        source_ = new QLabel(QStringLiteral("Origen: equipo vinculado"), this);
        source_->setObjectName(QStringLiteral("visualSourceLabel"));
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);
        layout->addWidget(source_);
        layout->addWidget(frame_, 1);
        layout->addWidget(state_);
        resize(960, 600);
    }

    void open(const VisualShareOffer& offer) override {
        ref_ = offer.ref;
        modality_ = offer.source;
        user_notified_ = false;
        programmatic_close_ = false;
        setWindowTitle(QStringLiteral("OneInTwo — %1 (solo lectura)")
                           .arg(modalityTitle(offer.source)));
        frame_->clearFrame();
        state_->setText(QStringLiteral("Esperando imagen…"));
        show();
        raise();
        activateWindow();
    }

    void present(VisualShareRef offer, const VisualFrame& frame) override {
        if (!ref_.has_value() || !sameRef(*ref_, offer) || frame.size.width == 0 ||
            frame.size.height == 0) {
            return;
        }
        const std::size_t expected =
            static_cast<std::size_t>(frame.size.width) * frame.size.height * 4;
        if (frame.bgra.size() != expected) {
            return;
        }
        const QImage view(frame.bgra.data(), static_cast<int>(frame.size.width),
                          static_cast<int>(frame.size.height),
                          static_cast<int>(frame.size.width) * 4, QImage::Format_ARGB32);
        // El visor conserva una copia con propiedad explícita antes de devolver
        // el control; el argumento es una referencia prestada.
        frame_->setFrame(view.copy());
        state_->setText(QStringLiteral("En directo"));
    }

    void clear(VisualShareRef offer, VisualSourceState state) override {
        if (!ref_.has_value() || !sameRef(*ref_, offer)) {
            return;
        }
        frame_->clearFrame();
        state_->setText(stateText(state));
    }

    void close(VisualShareRef offer) override {
        if (!ref_.has_value() || !sameRef(*ref_, offer)) {
            return;
        }
        programmatic_close_ = true;
        ref_.reset();
        frame_->clearFrame();
        state_->setText(QStringLiteral("Operación terminada"));
        hide();
        programmatic_close_ = false;
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        if (!programmatic_close_ && ref_.has_value() && !user_notified_) {
            const VisualShareRef closed = *ref_;
            user_notified_ = true;
            ref_.reset();
            frame_->clearFrame();
            state_->setText(QStringLiteral("Operación terminada"));
            notifyUserClosed(closed);
        }
        QWidget::closeEvent(event);
    }

private:
    static bool sameRef(const VisualShareRef& left, const VisualShareRef& right) {
        return left.generation == right.generation && left.initiator == right.initiator &&
               left.offer_id == right.offer_id;
    }

    void notifyUserClosed(VisualShareRef ref) {
        if (!user_closed_) {
            return;
        }
        if (runtime_.post) {
            runtime_.post([callback = user_closed_, ref] { callback(ref); });
        } else {
            user_closed_(ref);
        }
    }

    ModuleRuntime runtime_;
    std::function<void(VisualShareRef)> user_closed_;
    FrameLabel* frame_ = nullptr;
    QLabel* state_ = nullptr;
    QLabel* source_ = nullptr;
    std::optional<VisualShareRef> ref_;
    VisualSourceKind modality_ = VisualSourceKind::monitor;
    bool user_notified_ = false;
    bool programmatic_close_ = false;
};

} // namespace

std::unique_ptr<VisualViewer> makeVisualViewer(
    ModuleRuntime runtime, std::function<void(VisualShareRef)> user_closed) {
    return std::make_unique<ViewerWindow>(std::move(runtime), std::move(user_closed));
}

} // namespace oneintwo
