#include "visual_codec.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace oneintwo {
namespace {

constexpr int initial_quality = 75;
constexpr int minimum_quality = 10;
constexpr int quality_step = 15;

void setError(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
}

bool checkedBgraBytes(VisualSize size, std::size_t& bytes) {
    if (size.width == 0 || size.height == 0) {
        return false;
    }
    const std::uint64_t pixels =
        static_cast<std::uint64_t>(size.width) * static_cast<std::uint64_t>(size.height);
    if (pixels > std::numeric_limits<std::size_t>::max() / 4) {
        return false;
    }
    bytes = static_cast<std::size_t>(pixels) * 4;
    return true;
}

VisualSize scaledToContract(VisualSize source) {
    const double width = static_cast<double>(source.width);
    const double height = static_cast<double>(source.height);
    double scale = 1.0;
    scale = std::min(scale, static_cast<double>(max_visual_dimension) / width);
    scale = std::min(scale, static_cast<double>(max_visual_dimension) / height);
    const double pixels = width * height * scale * scale;
    if (pixels > static_cast<double>(max_visual_pixels)) {
        scale *= std::sqrt(static_cast<double>(max_visual_pixels) / pixels);
    }
    auto target_width = static_cast<std::uint32_t>(std::max(1.0, std::floor(width * scale)));
    auto target_height = static_cast<std::uint32_t>(std::max(1.0, std::floor(height * scale)));
    while (static_cast<std::uint64_t>(target_width) * target_height > max_visual_pixels ||
           target_width > max_visual_dimension || target_height > max_visual_dimension) {
        if (target_width >= target_height && target_width > 1) {
            --target_width;
        } else if (target_height > 1) {
            --target_height;
        } else {
            break;
        }
    }
    return VisualSize{std::max<std::uint32_t>(1, target_width),
                      std::max<std::uint32_t>(1, target_height)};
}

} // namespace

bool validateVisualFrameGeometry(const VisualFrame& frame, std::string* error) {
    std::size_t expected = 0;
    if (!checkedBgraBytes(frame.size, expected)) {
        setError(error, "Invalid visual dimensions");
        return false;
    }
    if (frame.bgra.size() != expected) {
        setError(error, "the buffer BGRA does not match the dimensions");
        return false;
    }
    return true;
}

bool jpegCodecAvailable() {
    static const bool available = [] {
        const QList<QByteArray> formats = QImageWriter::supportedImageFormats();
        for (const QByteArray& format : formats) {
            const QByteArray lower = format.toLower();
            if (lower == "jpeg" || lower == "jpg") {
                return true;
            }
        }
        return false;
    }();
    return available;
}

std::optional<VisualEncodedFrame> encodeVisualFrame(const VisualShareRef& ref,
                                                    VisualSourceRevision revision,
                                                    std::uint64_t sequence,
                                                    const VisualFrame& frame,
                                                    std::string* error) {
    if (!jpegCodecAvailable()) {
        setError(error, "codec JPEG is not available");
        return std::nullopt;
    }
    if (!validateVisualFrameGeometry(frame, error)) {
        return std::nullopt;
    }
    if (ref.offer_id == 0 || revision == 0 || sequence == 0) {
        setError(error, "invalid image identity or revision");
        return std::nullopt;
    }

    const QImage source(frame.bgra.data(), static_cast<int>(frame.size.width),
                        static_cast<int>(frame.size.height),
                        static_cast<int>(frame.size.width) * 4, QImage::Format_ARGB32);
    if (source.isNull()) {
        setError(error, "could not build source image");
        return std::nullopt;
    }

    const VisualSize target = scaledToContract(frame.size);
    QImage image = target.width == frame.size.width && target.height == frame.size.height
                       ? source.copy()
                       : source.scaled(static_cast<int>(target.width),
                                       static_cast<int>(target.height), Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
    if (image.isNull()) {
        setError(error, "could not scale image");
        return std::nullopt;
    }

    int quality = initial_quality;
    for (int attempt = 0; attempt < 16; ++attempt) {
        QByteArray encoded;
        QBuffer buffer(&encoded);
        if (!buffer.open(QIODevice::WriteOnly)) {
            setError(error, "Could not open encoder JPEG");
            return std::nullopt;
        }
        if (!image.save(&buffer, "JPEG", quality)) {
            setError(error, "the encoder JPEG rejected the image");
            return std::nullopt;
        }
        if (static_cast<std::size_t>(encoded.size()) <= max_visual_encoded_bytes) {
            VisualEncodedFrame result;
            result.ref = ref;
            result.revision = revision;
            result.sequence = sequence;
            result.size = VisualSize{static_cast<std::uint32_t>(image.width()),
                                     static_cast<std::uint32_t>(image.height())};
            result.key_frame = true;
            result.bytes.resize(static_cast<std::size_t>(encoded.size()));
            std::memcpy(result.bytes.data(), encoded.constData(),
                        static_cast<std::size_t>(encoded.size()));
            return result;
        }
        if (quality > minimum_quality) {
            quality = std::max(minimum_quality, quality - quality_step);
            continue;
        }
        if (image.width() <= 1 && image.height() <= 1) {
            break;
        }
        const int next_width = std::max(1, image.width() * 3 / 4);
        const int next_height = std::max(1, image.height() * 3 / 4);
        image = image.scaled(next_width, next_height, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
        quality = initial_quality;
    }
    setError(error, "image does not fit at the limit of 1 MiB");
    return std::nullopt;
}

std::optional<VisualFrame> decodeVisualFrame(const VisualEncodedFrame& frame,
                                             std::string* error) {
    if (frame.bytes.empty() || frame.bytes.size() > max_visual_encoded_bytes) {
        setError(error, "Off-limit encoded image size");
        return std::nullopt;
    }
    if (frame.size.width == 0 || frame.size.height == 0 ||
        frame.size.width > max_visual_dimension || frame.size.height > max_visual_dimension ||
        static_cast<std::uint64_t>(frame.size.width) * frame.size.height > max_visual_pixels) {
        setError(error, "dimensions announced out of bounds");
        return std::nullopt;
    }
    std::size_t announced_bytes = 0;
    if (!checkedBgraBytes(frame.size, announced_bytes)) {
        setError(error, "Invalid announced dimensions");
        return std::nullopt;
    }

    QByteArray data(reinterpret_cast<const char*>(frame.bytes.data()),
                    static_cast<int>(frame.bytes.size()));
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "jpeg");
    reader.setAutoTransform(false);
    const QSize announced = reader.size();
    if (!announced.isValid() || announced.width() != static_cast<int>(frame.size.width) ||
        announced.height() != static_cast<int>(frame.size.height)) {
        setError(error, "the JPEG does not match the announced dimensions");
        return std::nullopt;
    }
    if (announced.width() > static_cast<int>(max_visual_dimension) ||
        announced.height() > static_cast<int>(max_visual_dimension) ||
        static_cast<std::uint64_t>(announced.width()) * announced.height() > max_visual_pixels) {
        setError(error, "the JPEG exceeds the visual limits");
        return std::nullopt;
    }
    QImage image;
    if (!reader.read(&image) || image.isNull()) {
        setError(error, "Could not decode JPEG");
        return std::nullopt;
    }
    if (image.width() != announced.width() || image.height() != announced.height()) {
        setError(error, "the decoder changed the validated dimensions");
        return std::nullopt;
    }
    image = image.convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        setError(error, "could not convert decoded image");
        return std::nullopt;
    }

    VisualFrame result;
    result.size = frame.size;
    result.bgra.resize(announced_bytes);
    const std::size_t row_bytes = static_cast<std::size_t>(frame.size.width) * 4;
    for (std::uint32_t row = 0; row < frame.size.height; ++row) {
        std::memcpy(result.bgra.data() + static_cast<std::size_t>(row) * row_bytes,
                    image.constScanLine(static_cast<int>(row)), row_bytes);
    }
    return result;
}

} // namespace oneintwo
