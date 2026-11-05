// Responsabilidad: publicación nativa en Windows.
// Modo caché: CF_HDROP con archivos ya verificados en disco.
// Modo bajo demanda: CFSTR_FILEDESCRIPTORW + CFSTR_FILECONTENTS con streams que
// piden el contenido al origen cuando Explorer los solicita.
#include "platform/clipboard.hpp"

#include <windows.h>
#include <objidl.h>
#include <ole2.h>
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>

namespace o2f {
namespace {

constexpr std::size_t kMaxDescriptorPath = 259;  // MAX_PATH - 1 en FILEDESCRIPTORW

std::wstring toWide(const std::string& utf8, bool& ok) {
    ok = false;
    if (utf8.empty()) {
        ok = true;
        return std::wstring();
    }
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(),
                                           static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return std::wstring();
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(),
                                            static_cast<int>(utf8.size()), out.data(), needed);
    if (written != needed) return std::wstring();
    ok = true;
    return out;
}

CLIPFORMAT descriptorFormat() {
    static const CLIPFORMAT format =
        static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"FileGroupDescriptorW"));
    return format;
}

CLIPFORMAT contentsFormat() {
    static const CLIPFORMAT format =
        static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"FileContents"));
    return format;
}

// Stream de solo lectura entregado a Explorer. Cada Read puede bloquearse
// mientras el contenido remoto llega; el tiempo máximo lo fija el transporte.
class ContentStream : public IStream {
public:
    ContentStream(std::shared_ptr<ContentSource> source, Entry entry)
        : source_(std::move(source)), entry_(std::move(entry)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (ppv == nullptr) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IStream || riid == IID_ISequentialStream) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --refs_;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override {
        if (pcbRead != nullptr) *pcbRead = 0;
        if (pv == nullptr) return STG_E_INVALIDPOINTER;
        if (position_ >= entry_.size) return S_FALSE;
        const ULONG want = static_cast<ULONG>(
            std::min<std::uint64_t>(cb, entry_.size - position_));
        if (want == 0) return S_FALSE;
        std::size_t got = 0;
        std::string err;
        if (!source_ || !source_->read(entry_, position_, pv, want, got, err)) {
            return STG_E_READFAULT;
        }
        if (got == 0) return STG_E_READFAULT;
        position_ += got;
        if (pcbRead != nullptr) *pcbRead = static_cast<ULONG>(got);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Write(const void*, ULONG, ULONG*) override {
        return STG_E_ACCESSDENIED;
    }

    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move, DWORD origin,
                                   ULARGE_INTEGER* newPosition) override {
        std::int64_t base = 0;
        switch (origin) {
            case STREAM_SEEK_SET: base = 0; break;
            case STREAM_SEEK_CUR: base = static_cast<std::int64_t>(position_); break;
            case STREAM_SEEK_END: base = static_cast<std::int64_t>(entry_.size); break;
            default: return STG_E_INVALIDFUNCTION;
        }
        const std::int64_t target = base + move.QuadPart;
        if (target < 0) return STG_E_INVALIDFUNCTION;
        position_ = static_cast<std::uint64_t>(
            std::min<std::int64_t>(target, static_cast<std::int64_t>(entry_.size)));
        if (newPosition != nullptr) newPosition->QuadPart = position_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*,
                                     ULARGE_INTEGER*) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Revert() override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override {
        return STG_E_INVALIDFUNCTION;
    }

    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override {
        return STG_E_INVALIDFUNCTION;
    }

    HRESULT STDMETHODCALLTYPE Stat(STATSTG* stat, DWORD flags) override {
        if (stat == nullptr) return STG_E_INVALIDPOINTER;
        std::memset(stat, 0, sizeof(*stat));
        stat->type = STGTY_STREAM;
        stat->cbSize.QuadPart = entry_.size;
        stat->grfMode = STGM_READ;
        if ((flags & STATFLAG_NONAME) == 0) {
            const std::size_t slash = entry_.relPath.rfind('/');
            const std::string name =
                slash == std::string::npos ? entry_.relPath : entry_.relPath.substr(slash + 1);
            bool ok = false;
            const std::wstring wide = toWide(name, ok);
            if (ok) {
                const std::size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
                auto* buffer = static_cast<wchar_t*>(CoTaskMemAlloc(bytes));
                if (buffer != nullptr) {
                    std::memcpy(buffer, wide.c_str(), bytes);
                    stat->pwcsName = buffer;
                }
            }
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Clone(IStream**) override { return E_NOTIMPL; }

private:
    std::atomic<ULONG> refs_{1};
    std::shared_ptr<ContentSource> source_;
    Entry entry_;
    std::uint64_t position_ = 0;
};

class FormatEnum : public IEnumFORMATETC {
public:
    explicit FormatEnum(std::vector<FORMATETC> formats) : formats_(std::move(formats)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (ppv == nullptr) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IEnumFORMATETC) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --refs_;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Next(ULONG count, FORMATETC* out, ULONG* fetched) override {
        if (out == nullptr) return E_POINTER;
        ULONG copied = 0;
        while (copied < count && index_ < formats_.size()) {
            out[copied] = formats_[index_];
            ++copied;
            ++index_;
        }
        if (fetched != nullptr) *fetched = copied;
        return copied == count ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Skip(ULONG count) override {
        index_ = std::min<std::size_t>(index_ + count, formats_.size());
        return index_ < formats_.size() ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Reset() override {
        index_ = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC** out) override {
        if (out == nullptr) return E_POINTER;
        auto* copy = new FormatEnum(formats_);
        copy->index_ = index_;
        *out = copy;
        return S_OK;
    }

private:
    std::atomic<ULONG> refs_{1};
    std::vector<FORMATETC> formats_;
    std::size_t index_ = 0;
};

class ClipboardObject : public IDataObject {
public:
    explicit ClipboardObject(const ClipboardRequest& request)
        : request_(request), cacheMode_(!request.localPaths.empty()) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (ppv == nullptr) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --refs_;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* format, STGMEDIUM* medium) override {
        if (format == nullptr || medium == nullptr) return E_INVALIDARG;
        std::memset(medium, 0, sizeof(*medium));
        try {
            if (cacheMode_ && format->cfFormat == CF_HDROP) {
                return provideHdrop(format, medium);
            }
            if (!cacheMode_ && format->cfFormat == descriptorFormat()) {
                return provideDescriptor(format, medium);
            }
            if (!cacheMode_ && format->cfFormat == contentsFormat()) {
                return provideContents(format, medium);
            }
        } catch (...) {
            return E_FAIL;
        }
        return DV_E_FORMATETC;
    }

    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* format) override {
        if (format == nullptr) return E_INVALIDARG;
        if (cacheMode_) {
            if (format->cfFormat == CF_HDROP) {
                return (format->tymed & TYMED_HGLOBAL) != 0 ? S_OK : DV_E_TYMED;
            }
        } else {
            if (format->cfFormat == descriptorFormat()) {
                return (format->tymed & TYMED_HGLOBAL) != 0 ? S_OK : DV_E_TYMED;
            }
            if (format->cfFormat == contentsFormat()) {
                if ((format->tymed & TYMED_ISTREAM) == 0) return DV_E_TYMED;
                if (format->lindex < 0 ||
                    static_cast<std::size_t>(format->lindex) >= request_.offer.entries.size()) {
                    return DV_E_LINDEX;
                }
                return S_OK;
            }
        }
        return DV_E_FORMATETC;
    }

    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override {
        if (out != nullptr) out->ptd = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC** out) override {
        if (out == nullptr) return E_POINTER;
        if (direction != DATADIR_GET) return E_NOTIMPL;
        std::vector<FORMATETC> formats;
        FORMATETC format{};
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1;
        format.ptd = nullptr;
        if (cacheMode_) {
            format.cfFormat = CF_HDROP;
            format.tymed = TYMED_HGLOBAL;
            formats.push_back(format);
        } else {
            format.cfFormat = descriptorFormat();
            format.tymed = TYMED_HGLOBAL;
            formats.push_back(format);
            format.cfFormat = contentsFormat();
            format.tymed = TYMED_ISTREAM;
            formats.push_back(format);
        }
        *out = new FormatEnum(std::move(formats));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }

private:
    HRESULT provideDescriptor(FORMATETC* format, STGMEDIUM* medium) {
        if (format->lindex > 0) return DV_E_LINDEX;
        if ((format->tymed & TYMED_HGLOBAL) == 0) return DV_E_TYMED;
        const std::size_t count = request_.offer.entries.size();
        if (count == 0) return DV_E_FORMATETC;
        const std::size_t bytes = sizeof(UINT) + count * sizeof(FILEDESCRIPTORW);
        HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
        if (global == nullptr) return E_OUTOFMEMORY;
        auto* group = static_cast<FILEGROUPDESCRIPTORW*>(GlobalLock(global));
        if (group == nullptr) {
            GlobalFree(global);
            return E_OUTOFMEMORY;
        }
        group->cItems = static_cast<UINT>(count);
        bool ok = true;
        for (std::size_t i = 0; i < count; ++i) {
            const Entry& entry = request_.offer.entries[i];
            FILEDESCRIPTORW& descriptor = group->fgd[i];
            std::string relative = entry.relPath;
            std::replace(relative.begin(), relative.end(), '/', '\\');
            bool converted = false;
            const std::wstring wide = toWide(relative, converted);
            if (!converted || wide.size() > kMaxDescriptorPath) {
                ok = false;
                break;
            }
            descriptor.dwFlags = FD_ATTRIBUTES | FD_FILESIZE | FD_PROGRESSUI;
            descriptor.dwFileAttributes =
                entry.isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            descriptor.nFileSizeHigh = static_cast<DWORD>(entry.size >> 32);
            descriptor.nFileSizeLow = static_cast<DWORD>(entry.size & 0xffffffffull);
            std::memcpy(descriptor.cFileName, wide.c_str(),
                        (wide.size() + 1) * sizeof(wchar_t));
        }
        GlobalUnlock(global);
        if (!ok) {
            GlobalFree(global);
            return DV_E_FORMATETC;
        }
        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = global;
        medium->pUnkForRelease = nullptr;
        return S_OK;
    }

    HRESULT provideContents(FORMATETC* format, STGMEDIUM* medium) {
        if ((format->tymed & TYMED_ISTREAM) == 0) return DV_E_TYMED;
        if (format->lindex < 0 ||
            static_cast<std::size_t>(format->lindex) >= request_.offer.entries.size()) {
            return DV_E_LINDEX;
        }
        if (!request_.source) return E_UNEXPECTED;
        const Entry& entry = request_.offer.entries[static_cast<std::size_t>(format->lindex)];
        auto* stream = new ContentStream(request_.source, entry);
        medium->tymed = TYMED_ISTREAM;
        medium->pstm = stream;
        medium->pUnkForRelease = nullptr;
        return S_OK;
    }

    HRESULT provideHdrop(FORMATETC* format, STGMEDIUM* medium) {
        if ((format->tymed & TYMED_HGLOBAL) == 0) return DV_E_TYMED;
        std::size_t chars = 0;
        std::vector<std::wstring> paths;
        for (const std::string& path : request_.localPaths) {
            bool ok = false;
            const std::wstring wide = toWide(path, ok);
            if (!ok) return DV_E_FORMATETC;
            chars += wide.size() + 1;
            paths.push_back(wide);
        }
        const std::size_t bytes = sizeof(DROPFILES) + (chars + 1) * sizeof(wchar_t);
        HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
        if (global == nullptr) return E_OUTOFMEMORY;
        auto* drop = static_cast<DROPFILES*>(GlobalLock(global));
        if (drop == nullptr) {
            GlobalFree(global);
            return E_OUTOFMEMORY;
        }
        drop->pFiles = sizeof(DROPFILES);
        drop->fWide = TRUE;
        auto* cursor = reinterpret_cast<wchar_t*>(reinterpret_cast<std::uint8_t*>(drop) +
                                                  sizeof(DROPFILES));
        for (const std::wstring& wide : paths) {
            std::memcpy(cursor, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t));
            cursor += wide.size() + 1;
        }
        *cursor = L'\0';
        GlobalUnlock(global);
        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = global;
        medium->pUnkForRelease = nullptr;
        return S_OK;
    }

    std::atomic<ULONG> refs_{1};
    ClipboardRequest request_;
    bool cacheMode_ = false;
};

std::atomic<DWORD> g_mainThread{0};

BOOL WINAPI consoleHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        PostThreadMessageW(g_mainThread.load(), WM_QUIT, 0, 0);
        return TRUE;
    }
    return FALSE;
}

bool validateDescriptorPaths(const Offer& offer, std::string& err) {
    for (const Entry& entry : offer.entries) {
        std::string relative = entry.relPath;
        std::replace(relative.begin(), relative.end(), '/', '\\');
        bool ok = false;
        const std::wstring wide = toWide(relative, ok);
        if (!ok) {
            err = "la oferta contiene un nombre que Windows no puede representar";
            return false;
        }
        if (wide.size() > kMaxDescriptorPath) {
            err = "una ruta relativa supera el límite de 259 caracteres del descriptor de Windows";
            return false;
        }
    }
    return true;
}

}  // namespace

bool clipboardAvailable(std::string& why) {
    why.clear();
    return true;
}

bool onDemandClipboardAvailable(std::string& why) {
    why.clear();
    return true;
}

int runClipboardPublisher(const ClipboardRequest& request, bool flushOnExit, std::string& err) {
    err.clear();
    if (request.localPaths.empty() && request.offer.entries.empty()) {
        err = "no hay una oferta que publicar";
        return 1;
    }
    if (request.localPaths.empty() && !request.source) {
        err = "falta la fuente de contenido para la entrega bajo demanda";
        return 1;
    }
    for (const std::string& path : request.localPaths) {
        bool converted = false;
        const std::wstring wide = toWide(path, converted);
        if (!converted || GetFileAttributesW(wide.c_str()) == INVALID_FILE_ATTRIBUTES) {
            err = "una ruta local de la oferta no existe o no es accesible";
            return 1;
        }
    }
    if (request.localPaths.empty() && !validateDescriptorPaths(request.offer, err)) return 1;

    if (OleInitialize(nullptr) != S_OK) {
        err = "no se pudo inicializar OLE en este hilo";
        return 1;
    }
    auto* object = new ClipboardObject(request);
    const HRESULT set = OleSetClipboard(object);
    if (FAILED(set)) {
        object->Release();
        OleUninitialize();
        err = "Windows rechazó la oferta en el portapapeles";
        return 1;
    }

    g_mainThread.store(GetCurrentThreadId());
    SetConsoleCtrlHandler(consoleHandler, TRUE);
    MSG message;
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    BOOL result = 0;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    SetConsoleCtrlHandler(consoleHandler, FALSE);

    if (flushOnExit) {
        // Los datos ya están en disco: se materializan para que sobrevivan al proceso.
        const HRESULT flushed = OleFlushClipboard();
        if (FAILED(flushed)) {
            object->Release();
            OleUninitialize();
            err = "no se pudieron materializar las rutas en el portapapeles";
            return 1;
        }
    } else {
        // Con entrega bajo demanda no se puede materializar una copia: se retira.
        OleSetClipboard(nullptr);
    }
    object->Release();
    OleUninitialize();
    if (result == -1) {
        err = "fallo en el bucle de mensajes de la oferta";
        return 1;
    }
    return 0;
}

}  // namespace o2f
