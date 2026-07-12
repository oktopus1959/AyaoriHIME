#include <windows.h>
#include <ime.h>
#include <msctf.h>
#include <sddl.h>
#include <strsafe.h>
#include <algorithm>
#include <atomic>
#include <new>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>

// {C8C73063-C937-47C6-B02F-A24D92B65635}
static const CLSID CLSID_AyaoriHimeTsfTextService =
{ 0xc8c73063, 0xc937, 0x47c6, { 0xb0, 0x2f, 0xa2, 0x4d, 0x92, 0xb6, 0x56, 0x35 } };

// {621B0B3F-59F5-43EA-8B55-B86C3AE4A07F}
static const GUID GUID_AyaoriHimeTsfProfile =
{ 0x621b0b3f, 0x59f5, 0x43ea, { 0x8b, 0x55, 0xb8, 0x6c, 0x3a, 0xe4, 0xa0, 0x7f } };

// {82354EF4-98B3-4D35-9A17-5898B91656E2}
static const GUID GUID_AyaoriHimeInputAttribute =
{ 0x82354ef4, 0x98b3, 0x4d35, { 0x9a, 0x17, 0x58, 0x98, 0xb9, 0x16, 0x56, 0xe2 } };

static HINSTANCE g_instance = nullptr;
static std::atomic<long> g_objectCount = 0;
static std::atomic<long> g_lockCount = 0;

static const wchar_t* MessageWindowClassName = L"AyaoriHIME.TsfTextService.MessageWindow";
static const UINT WM_AYAORI_OPERATION_REQUEST = WM_APP + 0x481;
static const UINT WM_AYAORI_OPERATION_COMPLETED = WM_APP + 0x482;
static const UINT WM_AYAORI_CONTEXT_REQUEST = WM_APP + 0x483;

static const int32_t PipeMagic = 0x54465341; // ASFT
static const int16_t PipeVersion = 3;
static const int16_t MessageHello = 1;
static const int16_t MessageFocusChanged = 2;
static const int16_t MessageUpdateComposition = 3;
static const int16_t MessageCommitComposition = 4;
static const int16_t MessageCancelComposition = 5;
static const int16_t MessageOperationResult = 6;
static const int16_t MessageCompositionTerminated = 7;
static const int16_t MessageBye = 8;
static const int16_t MessageReadPrecedingContext = 9;
static const int16_t MessagePrecedingContextResult = 10;
static const int32_t MaxPayloadLength = 1024 * 1024;
static const ULONGLONG CachedOutputAnchorMaxAgeMs = 2000;
static const DWORD ImmDocumentFeedTimeoutMs = 50;
static const DWORD MaxReconvertStringBytes = 1024 * 1024;
static const LONG MaxPrecedingContextLength = 16;

// MozcがCUASによる仮のdocument manager判定に使用しているcompartment GUID。
static const GUID GUID_TsfEmulatedDocumentMgr =
{ 0xa94c5fd2, 0xc471, 0x4031, { 0x95, 0x46, 0x70, 0x9c, 0x17, 0x30, 0x0c, 0xb9 } };

static SRWLOCK g_runtimeLogLock = SRWLOCK_INIT;

static bool IsRuntimeLogEnabled()
{
#ifdef _DEBUG
    return true;
#else
    wchar_t value[8] = {};
    DWORD length = GetEnvironmentVariableW(L"AYAORIHIME_TSF_LOG", value, _countof(value));
    return length > 0 && length < _countof(value) && value[0] == L'1';
#endif
}

static void RuntimeLog(const wchar_t* format, ...)
{
    if (!IsRuntimeLogEnabled()) return;

    wchar_t tempPath[MAX_PATH] = {};
    if (!GetTempPathW(_countof(tempPath), tempPath)) return;

    wchar_t logPath[MAX_PATH] = {};
    if (FAILED(StringCchPrintfW(logPath, _countof(logPath), L"%sAyaoriHIME_tsf_runtime_%lu.log", tempPath, GetCurrentProcessId()))) return;

    wchar_t message[1024] = {};
    va_list args;
    va_start(args, format);
    StringCchVPrintfW(message, _countof(message), format, args);
    va_end(args);

    SYSTEMTIME now = {};
    GetLocalTime(&now);

    AcquireSRWLockExclusive(&g_runtimeLogLock);
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, logPath, L"a, ccs=UTF-8") == 0 && fp) {
        fwprintf(fp, L"%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu tid=%lu %s\n",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
            GetCurrentProcessId(), GetCurrentThreadId(), message);
        fflush(fp);
        fclose(fp);
    }
    ReleaseSRWLockExclusive(&g_runtimeLogLock);
}

static bool IsSameComIdentity(IUnknown* left, IUnknown* right)
{
    if (left == right) return true;
    if (!left || !right) return false;
    IUnknown* leftIdentity = nullptr;
    IUnknown* rightIdentity = nullptr;
    HRESULT leftHr = left->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&leftIdentity));
    HRESULT rightHr = right->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&rightIdentity));
    bool same = SUCCEEDED(leftHr) && SUCCEEDED(rightHr) && leftIdentity == rightIdentity;
    if (leftIdentity) leftIdentity->Release();
    if (rightIdentity) rightIdentity->Release();
    return same;
}

static bool TryGetCompartmentValue(IUnknown* owner, REFGUID guid, VARIANT* value)
{
    if (!owner || !value) return false;
    VariantInit(value);
    ITfCompartmentMgr* manager = nullptr;
    HRESULT hr = owner->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&manager));
    ITfCompartment* compartment = nullptr;
    if (SUCCEEDED(hr)) hr = manager->GetCompartment(guid, &compartment);
    if (SUCCEEDED(hr)) hr = compartment->GetValue(value);
    if (compartment) compartment->Release();
    if (manager) manager->Release();
    return SUCCEEDED(hr);
}

static bool IsTsfEmulatedDocumentMgr(ITfDocumentMgr* documentMgr)
{
    VARIANT value = {};
    bool found = TryGetCompartmentValue(documentMgr, GUID_TsfEmulatedDocumentMgr, &value);
    bool emulated = found && value.vt == VT_I4 && (value.lVal & 1) != 0;
    VariantClear(&value);
    return emulated;
}

// 呼び出し側がReleaseするfull contextを返す。nullptrはIMM32 fallback対象を表す。
static ITfContext* GetFullContext(ITfContext* context)
{
    if (!context) return nullptr;
    TF_STATUS status = {};
    HRESULT statusHr = context->GetStatus(&status);
    if (FAILED(statusHr)) {
        RuntimeLog(L"FullContext: GetStatus failed hr=0x%08X", static_cast<unsigned int>(statusHr));
        return nullptr;
    }
    if ((status.dwStaticFlags & TF_SS_TRANSITORY) == 0) {
        context->AddRef();
        RuntimeLog(L"FullContext: direct staticFlags=0x%08X", status.dwStaticFlags);
        return context;
    }

    ITfDocumentMgr* documentMgr = nullptr;
    HRESULT hr = context->GetDocumentMgr(&documentMgr);
    if (FAILED(hr) || !documentMgr) {
        RuntimeLog(L"FullContext: transitory GetDocumentMgr failed hr=0x%08X", static_cast<unsigned int>(hr));
        return nullptr;
    }

    ITfDocumentMgr* targetDocumentMgr = nullptr;
    VARIANT parent = {};
    if (TryGetCompartmentValue(documentMgr, GUID_COMPARTMENT_TRANSITORYEXTENSION_PARENT, &parent) &&
        parent.vt == VT_UNKNOWN && parent.punkVal) {
        parent.punkVal->QueryInterface(IID_ITfDocumentMgr, reinterpret_cast<void**>(&targetDocumentMgr));
    }
    VariantClear(&parent);

    if (targetDocumentMgr && !IsSameComIdentity(documentMgr, targetDocumentMgr)) {
        ITfContext* targetContext = nullptr;
        hr = targetDocumentMgr->GetTop(&targetContext);
        TF_STATUS targetStatus = {};
        HRESULT targetStatusHr = targetContext ? targetContext->GetStatus(&targetStatus) : E_FAIL;
        targetDocumentMgr->Release();
        documentMgr->Release();
        if (SUCCEEDED(hr) && targetContext && SUCCEEDED(targetStatusHr) &&
            (targetStatus.dwStaticFlags & TF_SS_TRANSITORY) == 0) {
            RuntimeLog(L"FullContext: transitory extension parent selected staticFlags=0x%08X", targetStatus.dwStaticFlags);
            return targetContext;
        }
        if (targetContext) targetContext->Release();
        RuntimeLog(L"FullContext: transitory extension parent is unavailable hr=0x%08X status=0x%08X",
            static_cast<unsigned int>(hr), static_cast<unsigned int>(targetStatusHr));
        return nullptr;
    }
    if (targetDocumentMgr) targetDocumentMgr->Release();

    bool emulated = IsTsfEmulatedDocumentMgr(documentMgr);
    documentMgr->Release();
    if (emulated) {
        RuntimeLog(L"FullContext: CUAS emulated document manager detected");
        return nullptr;
    }

    context->AddRef();
    RuntimeLog(L"FullContext: explicit transitory context selected staticFlags=0x%08X", status.dwStaticFlags);
    return context;
}

static void TrimPrecedingContext(std::wstring* text, LONG maxLength)
{
    if (!text) return;
    size_t boundary = text->find_last_of(L"\r\n、。");
    if (boundary != std::wstring::npos) text->erase(0, boundary + 1);
    if (maxLength >= 0 && text->length() > static_cast<size_t>(maxLength)) {
        text->erase(0, text->length() - static_cast<size_t>(maxLength));
    }
}

static bool ValidateReconvertString(const RECONVERTSTRING* reconvert, DWORD bufferSize)
{
    if (!reconvert || bufferSize < sizeof(RECONVERTSTRING)) return false;
    if (reconvert->dwSize < sizeof(RECONVERTSTRING) || reconvert->dwSize > bufferSize) return false;
    if (reconvert->dwVersion != 0) return false;
    if ((reconvert->dwStrOffset % sizeof(wchar_t)) != 0) return false;
    if (reconvert->dwStrLen > (MAXDWORD - reconvert->dwStrOffset) / sizeof(wchar_t)) return false;
    DWORD stringBytes = reconvert->dwStrLen * sizeof(wchar_t);
    if (reconvert->dwStrOffset > reconvert->dwSize ||
        stringBytes > reconvert->dwSize - reconvert->dwStrOffset) return false;
    if ((reconvert->dwCompStrOffset % sizeof(wchar_t)) != 0 ||
        (reconvert->dwTargetStrOffset % sizeof(wchar_t)) != 0) return false;
    if (reconvert->dwCompStrLen > MAXDWORD / sizeof(wchar_t) ||
        reconvert->dwTargetStrLen > MAXDWORD / sizeof(wchar_t)) return false;
    DWORD compositionBytes = reconvert->dwCompStrLen * sizeof(wchar_t);
    DWORD targetBytes = reconvert->dwTargetStrLen * sizeof(wchar_t);
    if (reconvert->dwCompStrOffset > stringBytes ||
        compositionBytes > stringBytes - reconvert->dwCompStrOffset) return false;
    if (reconvert->dwTargetStrOffset < reconvert->dwCompStrOffset ||
        reconvert->dwTargetStrOffset > reconvert->dwCompStrOffset + compositionBytes ||
        targetBytes > reconvert->dwCompStrOffset + compositionBytes - reconvert->dwTargetStrOffset) return false;
    return true;
}

static HRESULT ReadPrecedingContextImm32(ITfContext* context, LONG maxLength, std::wstring* result)
{
    if (!context || !result || maxLength < 0 || maxLength > MaxPrecedingContextLength) return E_INVALIDARG;
    result->clear();
    ITfContextView* view = nullptr;
    HRESULT hr = context->GetActiveView(&view);
    HWND attachedWindow = nullptr;
    if (SUCCEEDED(hr) && view) hr = view->GetWnd(&attachedWindow);
    if (view) view->Release();
    if (FAILED(hr) || !attachedWindow) {
        RuntimeLog(L"PrecedingContext: source=imm32 GetActiveView/GetWnd hr=0x%08X",
            static_cast<unsigned int>(hr));
        return FAILED(hr) ? hr : E_FAIL;
    }

    DWORD_PTR requiredSize = 0;
    LRESULT sendResult = SendMessageTimeoutW(attachedWindow, WM_IME_REQUEST, IMR_DOCUMENTFEED, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, ImmDocumentFeedTimeoutMs, &requiredSize);
    if (!sendResult) {
        DWORD error = GetLastError();
        RuntimeLog(L"PrecedingContext: source=imm32 size request failed error=%lu timeout=%d",
            error, error == ERROR_TIMEOUT);
        return error == ERROR_SUCCESS ? E_NOTIMPL : HRESULT_FROM_WIN32(error);
    }
    if (requiredSize < sizeof(RECONVERTSTRING) || requiredSize > MaxReconvertStringBytes) {
        RuntimeLog(L"PrecedingContext: source=imm32 invalid requiredSize=%llu",
            static_cast<unsigned long long>(requiredSize));
        return E_INVALIDARG;
    }

    std::vector<BYTE> buffer(static_cast<size_t>(requiredSize), 0);
    auto reconvert = reinterpret_cast<RECONVERTSTRING*>(buffer.data());
    reconvert->dwSize = static_cast<DWORD>(buffer.size());
    DWORD_PTR documentFeedResult = 0;
    sendResult = SendMessageTimeoutW(attachedWindow, WM_IME_REQUEST, IMR_DOCUMENTFEED,
        reinterpret_cast<LPARAM>(reconvert), SMTO_ABORTIFHUNG | SMTO_BLOCK,
        ImmDocumentFeedTimeoutMs, &documentFeedResult);
    if (!sendResult) {
        DWORD error = GetLastError();
        RuntimeLog(L"PrecedingContext: source=imm32 data request failed error=%lu timeout=%d",
            error, error == ERROR_TIMEOUT);
        return error == ERROR_SUCCESS ? E_NOTIMPL : HRESULT_FROM_WIN32(error);
    }
    if (!ValidateReconvertString(reconvert, static_cast<DWORD>(buffer.size()))) {
        RuntimeLog(L"PrecedingContext: source=imm32 invalid RECONVERTSTRING size=%zu result=%llu",
            buffer.size(), static_cast<unsigned long long>(documentFeedResult));
        return E_INVALIDARG;
    }

    const wchar_t* entireText = reinterpret_cast<const wchar_t*>(buffer.data() + reconvert->dwStrOffset);
    size_t precedingLength = reconvert->dwCompStrOffset / sizeof(wchar_t);
    result->assign(entireText, entireText + precedingLength);
    TrimPrecedingContext(result, maxLength);
    RuntimeLog(L"PrecedingContext: source=imm32 requested=%ld stringLength=%lu compositionOffset=%lu resultLength=%zu",
        maxLength, reconvert->dwStrLen, reconvert->dwCompStrOffset, result->length());
    return S_OK;
}

#pragma pack(push, 1)
struct PipeHeader
{
    int32_t magic;
    int16_t version;
    int16_t type;
    int32_t payloadLength;
};
#pragma pack(pop)

static_assert(sizeof(PipeHeader) == 12, "PipeHeader must be 12 bytes");
static_assert(sizeof(wchar_t) == 2, "AyaoriHIME TSF protocol assumes UTF-16 wchar_t");

enum class CompositionOperation : int32_t
{
    Update = MessageUpdateComposition,
    Commit = MessageCommitComposition,
    Cancel = MessageCancelComposition,
};

#pragma pack(push, 1)
struct OperationResultPayload
{
    int32_t operation;
    uint64_t compositionId;
    uint64_t sequence;
    int32_t hresult;
};
#pragma pack(pop)

static_assert(sizeof(OperationResultPayload) == 24, "OperationResultPayload must be 24 bytes");

struct CommitRequest
{
    CommitRequest(CompositionOperation requestOperation, uint64_t requestCompositionId, uint64_t requestSequence,
        const std::wstring& requestText, int32_t requestCaretOffset, int32_t requestCommitLength)
        : refCount(1), abandoned(0), completed(0), operation(requestOperation), compositionId(requestCompositionId),
          sequence(requestSequence), text(requestText), caretOffset(requestCaretOffset), commitLength(requestCommitLength),
          result(E_FAIL), completedEvent(nullptr)
    {
        completedEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }

    void AddRef() { InterlockedIncrement(&refCount); }

    void Release()
    {
        LONG next = InterlockedDecrement(&refCount);
        if (next == 0) {
            if (completedEvent) CloseHandle(completedEvent);
            delete this;
        }
    }

    void Complete(HRESULT value)
    {
        result = value;
        if (InterlockedCompareExchange(&completed, 1, 0) == 0 && completedEvent) SetEvent(completedEvent);
    }

    LONG refCount;
    volatile LONG abandoned;
    volatile LONG completed;
    CompositionOperation operation;
    uint64_t compositionId;
    uint64_t sequence;
    std::wstring text;
    int32_t caretOffset;
    int32_t commitLength;
    HRESULT result;
    HANDLE completedEvent;
};

struct ContextRequest
{
    ContextRequest(uint64_t id, int32_t length)
        : refCount(1), completed(0), requestId(id), maxLength(length), result(E_FAIL), completedEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}
    void AddRef() { InterlockedIncrement(&refCount); }
    void Release() { if (InterlockedDecrement(&refCount) == 0) { if (completedEvent) CloseHandle(completedEvent); delete this; } }
    void Complete(HRESULT value)
    {
        result = value;
        if (InterlockedCompareExchange(&completed, 1, 0) == 0 && completedEvent) SetEvent(completedEvent);
    }
    long refCount;
    volatile LONG completed;
    uint64_t requestId;
    int32_t maxLength;
    HRESULT result;
    std::wstring text;
    HANDLE completedEvent;
};

class PrecedingContextEditSession final : public ITfEditSession
{
public:
    PrecedingContextEditSession(ITfContext* context, LONG maxLength,
        std::wstring* result, ContextRequest* request = nullptr)
        : refCount_(1), context_(context), maxLength_(maxLength), result_(result), request_(request)
    { if (context_) context_->AddRef(); if (request_) request_->AddRef(); }
    ~PrecedingContextEditSession() { if (request_) request_->Release(); if (context_) context_->Release(); }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER; *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_ITfEditSession) { *ppv = static_cast<ITfEditSession*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }
    STDMETHODIMP_(ULONG) Release() override { LONG n = InterlockedDecrement(&refCount_); if (!n) delete this; return static_cast<ULONG>(n); }
    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        HRESULT hr = Execute(ec);
        if (request_) request_->Complete(hr);
        return hr;
    }
private:
    HRESULT Execute(TfEditCookie ec) {
        if (!context_ || !result_ || maxLength_ < 0 || maxLength_ > MaxPrecedingContextLength) return E_INVALIDARG;
        TF_SELECTION selection = {};
        ULONG selectionCount = 0;
        HRESULT hr = context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &selectionCount);
        if (FAILED(hr) || selectionCount != 1 || !selection.range) {
            RuntimeLog(L"PrecedingContextEditSession: GetSelection hr=0x%08X count=%lu",
                static_cast<unsigned int>(hr), selectionCount);
            return FAILED(hr) ? hr : TF_E_NOSELECTION;
        }

        ITfRange* range = nullptr;
        hr = selection.range->Clone(&range);
        selection.range->Release();
        if (FAILED(hr) || !range) return FAILED(hr) ? hr : E_FAIL;

        hr = range->Collapse(ec, TF_ANCHOR_START);
        LONG shifted = 0;
        const TF_HALTCOND haltCondition = { nullptr, TF_ANCHOR_START, TF_HF_OBJECT };
        if (SUCCEEDED(hr) && maxLength_ > 0) {
            hr = range->ShiftStart(ec, -maxLength_, &shifted, &haltCondition);
        }
        wchar_t buffer[17] = {};
        ULONG fetched = 0;
        ULONG readLength = shifted < 0
            ? static_cast<ULONG>((std::min)(-shifted, maxLength_))
            : 0;
        if (SUCCEEDED(hr) && readLength > 0) {
            hr = range->GetText(ec, 0, buffer, readLength, &fetched);
        }
        range->Release();
        if (FAILED(hr)) return hr;
        std::wstring text(buffer, fetched);
        TrimPrecedingContext(&text, maxLength_);
        *result_ = text;
        RuntimeLog(L"PrecedingContextEditSession: source=tsf requested=%ld shifted=%ld fetched=%lu resultLength=%zu",
            maxLength_, shifted, fetched, result_->length());
        return S_OK;
    }
    long refCount_; ITfContext* context_; LONG maxLength_;
    std::wstring* result_; ContextRequest* request_;
};

class CommitEditSession final : public ITfEditSession
{
public:
    CommitEditSession(ITfContext* context, const std::wstring& text, int32_t numBS, bool hasPreferredAnchor, LONG preferredAnchor)
        : refCount_(1), context_(context), text_(text), numBS_(numBS), hasPreferredAnchor_(hasPreferredAnchor), preferredAnchor_(preferredAnchor), hasResultAnchor_(false), resultAnchor_(0), result_(E_FAIL)
    {
        if (context_) context_->AddRef();
    }

    ~CommitEditSession()
    {
        if (context_) context_->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
            *ppv = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&refCount_));
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG next = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (next == 0) delete this;
        return next;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec) override
    {
        result_ = Apply(ec);
        return result_;
    }

    HRESULT Result() const
    {
        return result_;
    }

    bool TryGetResultAnchor(LONG* anchor) const
    {
        if (!anchor || !hasResultAnchor_) return false;
        *anchor = resultAnchor_;
        return true;
    }

private:
    HRESULT Apply(TfEditCookie ec)
    {
        if (!context_) {
            RuntimeLog(L"CommitEditSession: context is null");
            return E_FAIL;
        }
        if (numBS_ < 0) {
            RuntimeLog(L"CommitEditSession: invalid numBS=%ld", numBS_);
            return E_INVALIDARG;
        }

        TF_SELECTION selection = {};
        ULONG fetched = 0;
        HRESULT hr = context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        if (FAILED(hr)) {
            RuntimeLog(L"CommitEditSession: GetSelection failed hr=0x%08X", static_cast<unsigned int>(hr));
            return hr;
        }
        if (fetched != 1 || !selection.range) {
            RuntimeLog(L"CommitEditSession: no selection fetched=%lu", fetched);
            return TF_E_NOSELECTION;
        }

        ITfRange* range = selection.range;
        ITfRangeACP* rangeAcp = nullptr;
        HRESULT rangeAcpHr = range->QueryInterface(IID_ITfRangeACP, reinterpret_cast<void**>(&rangeAcp));
        bool useAcp = SUCCEEDED(rangeAcpHr) && rangeAcp;
        LONG selectionAnchor = 0;
        LONG selectionLength = 0;
        if (useAcp) {
            rangeAcpHr = rangeAcp->GetExtent(&selectionAnchor, &selectionLength);
            RuntimeLog(L"CommitEditSession: ITfRangeACP GetExtent hr=0x%08X anchor=%ld length=%ld",
                static_cast<unsigned int>(rangeAcpHr), selectionAnchor, selectionLength);
            if (FAILED(rangeAcpHr)) {
                rangeAcp->Release();
                range->Release();
                return rangeAcpHr;
            }
            if (numBS_ > 0 && hasPreferredAnchor_) {
                rangeAcpHr = rangeAcp->SetExtent(preferredAnchor_, 0);
                RuntimeLog(L"CommitEditSession: using cached output anchor=%ld SetExtent hr=0x%08X",
                    preferredAnchor_, static_cast<unsigned int>(rangeAcpHr));
                if (FAILED(rangeAcpHr)) {
                    rangeAcp->Release();
                    range->Release();
                    return rangeAcpHr;
                }
                selectionAnchor = preferredAnchor_;
                selectionLength = 0;
            }
        } else {
            RuntimeLog(L"CommitEditSession: ITfRangeACP unavailable hr=0x%08X", static_cast<unsigned int>(rangeAcpHr));
        }

        auto releaseRanges = [&]() {
            if (rangeAcp) rangeAcp->Release();
            range->Release();
        };

        auto logContextSelection = [&](const wchar_t* stage) {
            TF_SELECTION currentSelection = {};
            ULONG currentFetched = 0;
            HRESULT selectionHr = context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &currentSelection, &currentFetched);
            if (SUCCEEDED(selectionHr) && currentFetched == 1 && currentSelection.range) {
                ITfRangeACP* currentAcp = nullptr;
                HRESULT acpHr = currentSelection.range->QueryInterface(IID_ITfRangeACP, reinterpret_cast<void**>(&currentAcp));
                if (SUCCEEDED(acpHr) && currentAcp) {
                    LONG anchor = 0;
                    LONG length = 0;
                    HRESULT extentHr = currentAcp->GetExtent(&anchor, &length);
                    RuntimeLog(L"CommitEditSession: context selection %s hr=0x%08X anchor=%ld length=%ld",
                        stage, static_cast<unsigned int>(extentHr), anchor, length);
                    currentAcp->Release();
                } else {
                    RuntimeLog(L"CommitEditSession: context selection %s has no ITfRangeACP hr=0x%08X",
                        stage, static_cast<unsigned int>(acpHr));
                }
                currentSelection.range->Release();
            } else {
                RuntimeLog(L"CommitEditSession: context selection %s failed hr=0x%08X fetched=%lu",
                    stage, static_cast<unsigned int>(selectionHr), currentFetched);
            }
        };

        auto logDocumentExtent = [&](const wchar_t* stage) {
            ITfRange* startRange = nullptr;
            ITfRange* endRange = nullptr;
            HRESULT startHr = context_->GetStart(ec, &startRange);
            HRESULT endHr = context_->GetEnd(ec, &endRange);
            LONG startAnchor = 0;
            LONG startLength = 0;
            LONG endAnchor = 0;
            LONG endLength = 0;
            HRESULT startExtentHr = E_NOINTERFACE;
            HRESULT endExtentHr = E_NOINTERFACE;

            if (SUCCEEDED(startHr) && startRange) {
                ITfRangeACP* startAcp = nullptr;
                HRESULT acpHr = startRange->QueryInterface(IID_ITfRangeACP, reinterpret_cast<void**>(&startAcp));
                if (SUCCEEDED(acpHr) && startAcp) {
                    startExtentHr = startAcp->GetExtent(&startAnchor, &startLength);
                    startAcp->Release();
                } else {
                    startExtentHr = acpHr;
                }
            }
            if (SUCCEEDED(endHr) && endRange) {
                ITfRangeACP* endAcp = nullptr;
                HRESULT acpHr = endRange->QueryInterface(IID_ITfRangeACP, reinterpret_cast<void**>(&endAcp));
                if (SUCCEEDED(acpHr) && endAcp) {
                    endExtentHr = endAcp->GetExtent(&endAnchor, &endLength);
                    endAcp->Release();
                } else {
                    endExtentHr = acpHr;
                }
            }

            RuntimeLog(L"CommitEditSession: document extent %s GetStart=0x%08X startExtent=0x%08X start=%ld length=%ld GetEnd=0x%08X endExtent=0x%08X end=%ld length=%ld",
                stage,
                static_cast<unsigned int>(startHr), static_cast<unsigned int>(startExtentHr), startAnchor, startLength,
                static_cast<unsigned int>(endHr), static_cast<unsigned int>(endExtentHr), endAnchor, endLength);
            if (startRange) startRange->Release();
            if (endRange) endRange->Release();
        };

        logDocumentExtent(L"at entry");

        BOOL isEmpty = FALSE;
        hr = range->IsEmpty(ec, &isEmpty);
        RuntimeLog(L"CommitEditSession: IsEmpty hr=0x%08X isEmpty=%d", static_cast<unsigned int>(hr), isEmpty);
        if (FAILED(hr)) {
            releaseRanges();
            return hr;
        }

        if (!isEmpty && numBS_ > 0) {
            RuntimeLog(L"CommitEditSession: selection with numBS=%ld is unsupported", numBS_);
            releaseRanges();
            return E_NOTIMPL;
        }

        LONG insertionStart = selectionAnchor;
        if (isEmpty) {
            if (useAcp) {
                if (numBS_ > selectionAnchor) {
                    RuntimeLog(L"CommitEditSession: ACP delete exceeds preceding text anchor=%ld numBS=%ld", selectionAnchor, numBS_);
                    releaseRanges();
                    return E_FAIL;
                }

                insertionStart = selectionAnchor - numBS_;
                hr = rangeAcp->SetExtent(insertionStart, numBS_);
                RuntimeLog(L"CommitEditSession: ITfRangeACP SetExtent(replace) anchor=%ld length=%ld hr=0x%08X",
                    insertionStart, numBS_, static_cast<unsigned int>(hr));
                if (FAILED(hr)) {
                    releaseRanges();
                    return hr;
                }
            } else {
                hr = range->Collapse(ec, TF_ANCHOR_START);
                RuntimeLog(L"CommitEditSession: Collapse(start) hr=0x%08X", static_cast<unsigned int>(hr));
                if (FAILED(hr)) {
                    releaseRanges();
                    return hr;
                }

                if (numBS_ > 0) {
                    LONG shifted = 0;
                    hr = range->ShiftStart(ec, -numBS_, &shifted, nullptr);
                    RuntimeLog(L"CommitEditSession: ShiftStart requested=%ld shifted=%ld hr=0x%08X", -numBS_, shifted, static_cast<unsigned int>(hr));
                    if (FAILED(hr)) {
                        releaseRanges();
                        return hr;
                    }
                    if (shifted != -numBS_) {
                        RuntimeLog(L"CommitEditSession: ShiftStart did not reach requested position");
                        releaseRanges();
                        return E_FAIL;
                    }
                }
            }
        }

        if (useAcp) {
            LONG replaceAnchor = 0;
            LONG replaceLength = 0;
            HRESULT extentHr = rangeAcp->GetExtent(&replaceAnchor, &replaceLength);
            RuntimeLog(L"CommitEditSession: replacement extent hr=0x%08X anchor=%ld length=%ld",
                static_cast<unsigned int>(extentHr), replaceAnchor, replaceLength);
            if (FAILED(extentHr)) {
                releaseRanges();
                return extentHr;
            }
            insertionStart = replaceAnchor;
        }

        if (text_.length() > static_cast<size_t>(MAXLONG)) {
            releaseRanges();
            return E_INVALIDARG;
        }

        const WCHAR* text = text_.empty() ? L"" : text_.c_str();
        if (useAcp) {
            TF_SELECTION replaceSelection = selection;
            replaceSelection.range = range;
            replaceSelection.style.ase = TF_AE_END;
            replaceSelection.style.fInterimChar = FALSE;
            hr = context_->SetSelection(ec, 1, &replaceSelection);
            RuntimeLog(L"CommitEditSession: SetSelection(replace) hr=0x%08X", static_cast<unsigned int>(hr));
            if (FAILED(hr)) {
                releaseRanges();
                return hr;
            }
            logContextSelection(L"before insert");
            logDocumentExtent(L"before insert");

            ITfInsertAtSelection* inserter = nullptr;
            hr = context_->QueryInterface(IID_ITfInsertAtSelection, reinterpret_cast<void**>(&inserter));
            RuntimeLog(L"CommitEditSession: ITfInsertAtSelection QueryInterface hr=0x%08X", static_cast<unsigned int>(hr));
            if (FAILED(hr) || !inserter) {
                releaseRanges();
                return FAILED(hr) ? hr : E_NOINTERFACE;
            }

            ITfRange* insertedRange = nullptr;
            hr = inserter->InsertTextAtSelection(ec, TF_IAS_NO_DEFAULT_COMPOSITION, text, static_cast<LONG>(text_.length()), &insertedRange);
            inserter->Release();
            RuntimeLog(L"CommitEditSession: InsertTextAtSelection flags=TF_IAS_NO_DEFAULT_COMPOSITION hr=0x%08X range=%p textLength=%zu",
                static_cast<unsigned int>(hr), insertedRange, text_.length());
            if (FAILED(hr)) {
                if (insertedRange) insertedRange->Release();
                releaseRanges();
                return hr;
            }

            // 本文変更後の caret 調整に失敗しても、二重出力防止のため commit は成功扱いにする。
            if (insertedRange && text_.length() <= static_cast<size_t>(MAXLONG - insertionStart)) {
                ITfRangeACP* insertedAcp = nullptr;
                HRESULT insertedAcpHr = insertedRange->QueryInterface(IID_ITfRangeACP, reinterpret_cast<void**>(&insertedAcp));
                RuntimeLog(L"CommitEditSession: inserted range ITfRangeACP hr=0x%08X", static_cast<unsigned int>(insertedAcpHr));
                if (SUCCEEDED(insertedAcpHr) && insertedAcp) {
                    LONG insertedAnchor = 0;
                    LONG insertedLength = 0;
                    HRESULT extentHr = insertedAcp->GetExtent(&insertedAnchor, &insertedLength);
                    RuntimeLog(L"CommitEditSession: inserted extent hr=0x%08X anchor=%ld length=%ld",
                        static_cast<unsigned int>(extentHr), insertedAnchor, insertedLength);

                    LONG caretAnchor = insertionStart + static_cast<LONG>(text_.length());
                    HRESULT caretHr = insertedAcp->SetExtent(caretAnchor, 0);
                    RuntimeLog(L"CommitEditSession: inserted SetExtent(caret) anchor=%ld length=0 hr=0x%08X",
                        caretAnchor, static_cast<unsigned int>(caretHr));
                    if (SUCCEEDED(caretHr)) {
                        TF_SELECTION caretSelection = selection;
                        caretSelection.range = insertedRange;
                        caretSelection.style.ase = TF_AE_END;
                        caretSelection.style.fInterimChar = FALSE;
                        HRESULT selectionHr = context_->SetSelection(ec, 1, &caretSelection);
                        RuntimeLog(L"CommitEditSession: SetSelection(caret) hr=0x%08X", static_cast<unsigned int>(selectionHr));
                        logContextSelection(L"after insert");
                        logDocumentExtent(L"after insert");
                        if (SUCCEEDED(selectionHr)) SetResultAnchor(caretAnchor);
                    }
                    insertedAcp->Release();
                }
            } else if (!insertedRange) {
                RuntimeLog(L"CommitEditSession: InsertTextAtSelection returned no range");
            } else {
                RuntimeLog(L"CommitEditSession: caret ACP overflow insertionStart=%ld textLength=%zu", insertionStart, text_.length());
            }
            if (insertedRange) insertedRange->Release();
        } else {
            hr = range->SetText(ec, 0, text, static_cast<LONG>(text_.length()));
            if (FAILED(hr)) {
                RuntimeLog(L"CommitEditSession: SetText failed hr=0x%08X textLength=%zu numBS=%ld", static_cast<unsigned int>(hr), text_.length(), numBS_);
                releaseRanges();
                return hr;
            }

            HRESULT caretCollapseHr = range->Collapse(ec, TF_ANCHOR_END);
            RuntimeLog(L"CommitEditSession: Collapse(end) hr=0x%08X", static_cast<unsigned int>(caretCollapseHr));
            if (SUCCEEDED(caretCollapseHr)) {
                TF_SELECTION newSelection = selection;
                newSelection.range = range;
                newSelection.style.ase = TF_AE_END;
                newSelection.style.fInterimChar = FALSE;
                HRESULT selectionHr = context_->SetSelection(ec, 1, &newSelection);
                RuntimeLog(L"CommitEditSession: SetSelection hr=0x%08X", static_cast<unsigned int>(selectionHr));
            }
        }

        releaseRanges();
        RuntimeLog(L"CommitEditSession: committed textLength=%zu numBS=%ld selectionEmpty=%d", text_.length(), numBS_, isEmpty);
        return S_OK;
    }

    void SetResultAnchor(LONG anchor)
    {
        resultAnchor_ = anchor;
        hasResultAnchor_ = true;
    }

    long refCount_;
    ITfContext* context_;
    std::wstring text_;
    int32_t numBS_;
    bool hasPreferredAnchor_;
    LONG preferredAnchor_;
    bool hasResultAnchor_;
    LONG resultAnchor_;
    HRESULT result_;
};

static HRESULT SetCompositionDisplayAttribute(ITfContext* context, TfEditCookie ec, ITfRange* range, bool enabled)
{
    ITfProperty* property = nullptr;
    HRESULT hr = context->GetProperty(GUID_PROP_ATTRIBUTE, &property);
    if (FAILED(hr) || !property) return FAILED(hr) ? hr : E_NOINTERFACE;
    if (enabled) {
        ITfCategoryMgr* categoryMgr = nullptr;
        hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
            IID_ITfCategoryMgr, reinterpret_cast<void**>(&categoryMgr));
        TfGuidAtom atom = TF_INVALID_GUIDATOM;
        if (SUCCEEDED(hr)) hr = categoryMgr->RegisterGUID(GUID_AyaoriHimeInputAttribute, &atom);
        if (categoryMgr) categoryMgr->Release();
        if (SUCCEEDED(hr)) {
            VARIANT value;
            VariantInit(&value);
            value.vt = VT_I4;
            value.lVal = static_cast<LONG>(atom);
            hr = property->SetValue(ec, range, &value);
            VariantClear(&value);
        }
    } else {
        hr = property->Clear(ec, range);
    }
    property->Release();
    RuntimeLog(L"Composition display attribute: enabled=%d hr=0x%08X", enabled, static_cast<unsigned int>(hr));
    return hr;
}

class CompositionEditSession final : public ITfEditSession
{
public:
    CompositionEditSession(ITfContext* context, CompositionOperation operation, const std::wstring& text,
        LONG caretOffset, LONG commitLength, ITfComposition** composition, ITfCompositionSink* sink, bool* endingComposition,
        HWND completionWindow = nullptr, CommitRequest* completionRequest = nullptr)
        : refCount_(1), context_(context), operation_(operation), text_(text), caretOffset_(caretOffset),
          commitLength_(commitLength), composition_(composition), sink_(sink), endingComposition_(endingComposition),
          completionWindow_(completionWindow), completionRequest_(completionRequest), result_(E_FAIL),
          hasCommittedAnchor_(false), committedAnchor_(0)
    {
        if (context_) context_->AddRef();
        if (sink_) sink_->AddRef();
        if (completionRequest_) completionRequest_->AddRef();
    }

    ~CompositionEditSession()
    {
        if (context_) context_->Release();
        if (sink_) sink_->Release();
        if (completionRequest_) completionRequest_->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
            *ppv = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }
    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG next = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (next == 0) delete this;
        return next;
    }

    bool TryGetCommittedAnchor(LONG* anchor) const
    {
        if (!hasCommittedAnchor_) return false;
        if (anchor) *anchor = committedAnchor_;
        return true;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec) override
    {
        result_ = Apply(ec);
        if (completionRequest_) {
            completionRequest_->result = result_;
            if (completionWindow_ && PostMessageW(completionWindow_, WM_AYAORI_OPERATION_COMPLETED, 0,
                reinterpret_cast<LPARAM>(completionRequest_))) {
                completionRequest_ = nullptr;
            } else {
                completionRequest_->Complete(result_);
            }
        }
        return result_;
    }

    HRESULT Result() const { return result_; }

private:
    HRESULT ProbeInlineCompositionLayout(TfEditCookie ec)
    {
        TF_SELECTION selection = {};
        ULONG fetched = 0;
        HRESULT hr = context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        if (FAILED(hr) || fetched != 1 || !selection.range) {
            RuntimeLog(L"Inline layout probe: GetSelection hr=0x%08X fetched=%lu",
                static_cast<unsigned int>(hr), fetched);
            return FAILED(hr) ? hr : E_NOTIMPL;
        }

        ITfContextView* view = nullptr;
        hr = context_->GetActiveView(&view);
        RECT textRect = {};
        BOOL clipped = FALSE;
        if (SUCCEEDED(hr) && view) {
            hr = view->GetTextExt(ec, selection.range, &textRect, &clipped);
        }
        selection.range->Release();
        if (FAILED(hr) || !view) {
            RuntimeLog(L"Inline layout probe: GetTextExt hr=0x%08X",
                static_cast<unsigned int>(hr));
            if (view) view->Release();
            return FAILED(hr) ? hr : E_NOTIMPL;
        }

        HWND viewWindow = nullptr;
        HRESULT windowHr = view->GetWnd(&viewWindow);
        view->Release();
        bool validRect = textRect.bottom > textRect.top;
        bool insideView = true;
        RECT windowRect = {};
        if (SUCCEEDED(windowHr) && viewWindow && GetWindowRect(viewWindow, &windowRect)) {
            RECT caretRect = textRect;
            if (caretRect.right <= caretRect.left) caretRect.right = caretRect.left + 1;
            RECT intersection = {};
            insideView = IntersectRect(&intersection, &caretRect, &windowRect) != FALSE;
        }
        RuntimeLog(L"Inline layout probe: text=(%ld,%ld)-(%ld,%ld) clipped=%d window=%p windowHr=0x%08X inside=%d",
            textRect.left, textRect.top, textRect.right, textRect.bottom, clipped, viewWindow,
            static_cast<unsigned int>(windowHr), insideView);
        return validRect && insideView ? S_OK : E_NOTIMPL;
    }

    HRESULT GetCompositionRange(ITfRange** range)
    {
        if (!range) return E_POINTER;
        *range = nullptr;
        return composition_ && *composition_ ? (*composition_)->GetRange(range) : E_UNEXPECTED;
    }

    HRESULT StartComposition(TfEditCookie ec)
    {
        HRESULT hr = ProbeInlineCompositionLayout(ec);
        if (FAILED(hr)) {
            RuntimeLog(L"CompositionEditSession: inline layout unavailable hr=0x%08X",
                static_cast<unsigned int>(hr));
            return hr;
        }

        TF_SELECTION selection = {};
        ULONG fetched = 0;
        hr = context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
        if (FAILED(hr) || fetched != 1 || !selection.range) return FAILED(hr) ? hr : TF_E_NOSELECTION;

        ITfContextComposition* contextComposition = nullptr;
        hr = context_->QueryInterface(IID_ITfContextComposition, reinterpret_cast<void**>(&contextComposition));
        if (SUCCEEDED(hr) && contextComposition) {
            ITfComposition* created = nullptr;
            hr = contextComposition->StartComposition(ec, selection.range, sink_, &created);
            if (SUCCEEDED(hr) && created) {
                if (*composition_) (*composition_)->Release();
                *composition_ = created;
            } else if (SUCCEEDED(hr)) {
                hr = E_FAIL;
            }
            contextComposition->Release();
        }
        selection.range->Release();
        RuntimeLog(L"CompositionEditSession: StartComposition hr=0x%08X", static_cast<unsigned int>(hr));
        return hr;
    }

    HRESULT SetCaret(TfEditCookie ec, ITfRange* compositionRange)
    {
        ITfRange* selectionRange = nullptr;
        HRESULT hr = compositionRange->Clone(&selectionRange);
        if (FAILED(hr) || !selectionRange) return FAILED(hr) ? hr : E_FAIL;
        ITfRangeACP* acp = nullptr;
        hr = selectionRange->QueryInterface(IID_ITfRangeACP, reinterpret_cast<void**>(&acp));
        if (FAILED(hr) || !acp) {
            selectionRange->Release();
            return FAILED(hr) ? hr : E_NOINTERFACE;
        }
        LONG anchor = 0;
        LONG length = 0;
        hr = acp->GetExtent(&anchor, &length);
        if (SUCCEEDED(hr) && (caretOffset_ < 0 || caretOffset_ > length)) hr = E_INVALIDARG;
        if (SUCCEEDED(hr)) hr = acp->SetExtent(anchor + caretOffset_, 0);
        if (SUCCEEDED(hr)) {
            TF_SELECTION selection = {};
            selection.range = selectionRange;
            selection.style.ase = TF_AE_END;
            hr = context_->SetSelection(ec, 1, &selection);
        }
        RuntimeLog(L"CompositionEditSession: SetCaret offset=%ld hr=0x%08X", caretOffset_, static_cast<unsigned int>(hr));
        acp->Release();
        selectionRange->Release();
        return hr;
    }

    HRESULT Update(TfEditCookie ec)
    {
        if (text_.length() > static_cast<size_t>(MAXLONG)) return E_INVALIDARG;
        HRESULT hr = composition_ && *composition_ ? S_OK : StartComposition(ec);
        if (FAILED(hr)) return hr;

        ITfRange* range = nullptr;
        hr = GetCompositionRange(&range);
        if (SUCCEEDED(hr)) {
            hr = range->SetText(ec, 0, text_.empty() ? L"" : text_.c_str(), static_cast<LONG>(text_.length()));
            RuntimeLog(L"CompositionEditSession: Update SetText length=%zu hr=0x%08X", text_.length(), static_cast<unsigned int>(hr));
        }
        if (SUCCEEDED(hr)) {
            range->Release();
            range = nullptr;
            hr = GetCompositionRange(&range);
        }
        if (SUCCEEDED(hr)) hr = SetCaret(ec, range);
        if (SUCCEEDED(hr)) hr = SetCompositionDisplayAttribute(context_, ec, range, true);
        if (range) range->Release();
        return hr;
    }

    HRESULT Commit(TfEditCookie ec)
    {
        if (!composition_ || !*composition_) return S_OK;
        ITfRange* range = nullptr;
        HRESULT hr = GetCompositionRange(&range);
        ITfRangeACP* acp = nullptr;
        LONG anchor = 0;
        LONG length = 0;
        if (SUCCEEDED(hr)) hr = range->QueryInterface(IID_ITfRangeACP, reinterpret_cast<void**>(&acp));
        if (SUCCEEDED(hr) && acp) hr = acp->GetExtent(&anchor, &length);
        if (SUCCEEDED(hr) && (commitLength_ < -1 || commitLength_ > length)) hr = E_INVALIDARG;

        if (SUCCEEDED(hr)) SetCompositionDisplayAttribute(context_, ec, range, false);

        if (SUCCEEDED(hr) && commitLength_ >= 0 && commitLength_ < length) {
            ITfRange* suffix = nullptr;
            hr = range->Clone(&suffix);
            ITfRangeACP* suffixAcp = nullptr;
            if (SUCCEEDED(hr)) hr = suffix->QueryInterface(IID_ITfRangeACP, reinterpret_cast<void**>(&suffixAcp));
            if (SUCCEEDED(hr)) hr = suffixAcp->SetExtent(anchor + commitLength_, length - commitLength_);

            ITfRange* boundary = nullptr;
            if (SUCCEEDED(hr)) hr = range->Clone(&boundary);
            ITfRangeACP* boundaryAcp = nullptr;
            if (SUCCEEDED(hr)) hr = boundary->QueryInterface(IID_ITfRangeACP, reinterpret_cast<void**>(&boundaryAcp));
            if (SUCCEEDED(hr)) hr = boundaryAcp->SetExtent(anchor + commitLength_, 0);
            if (SUCCEEDED(hr)) hr = (*composition_)->ShiftEnd(ec, boundary);
            if (SUCCEEDED(hr) && endingComposition_) *endingComposition_ = true;
            if (SUCCEEDED(hr)) hr = (*composition_)->EndComposition(ec);
            if (endingComposition_) *endingComposition_ = false;
            if (SUCCEEDED(hr)) {
                (*composition_)->Release();
                *composition_ = nullptr;
                ITfContextComposition* owner = nullptr;
                hr = context_->QueryInterface(IID_ITfContextComposition, reinterpret_cast<void**>(&owner));
                if (SUCCEEDED(hr) && owner) {
                    hr = owner->StartComposition(ec, suffix, sink_, composition_);
                    owner->Release();
                }
                if (SUCCEEDED(hr)) SetCompositionDisplayAttribute(context_, ec, suffix, true);
            }
            if (boundaryAcp) boundaryAcp->Release();
            if (boundary) boundary->Release();
            if (suffixAcp) suffixAcp->Release();
            if (suffix) suffix->Release();
        } else if (SUCCEEDED(hr)) {
            if (endingComposition_) *endingComposition_ = true;
            hr = (*composition_)->EndComposition(ec);
            if (endingComposition_) *endingComposition_ = false;
            if (SUCCEEDED(hr)) {
                (*composition_)->Release();
                *composition_ = nullptr;
                HRESULT caretHr = acp->SetExtent(anchor + length, 0);
                if (SUCCEEDED(caretHr)) {
                    TF_SELECTION selection = {};
                    selection.range = range;
                    selection.style.ase = TF_AE_END;
                    selection.style.fInterimChar = FALSE;
                    caretHr = context_->SetSelection(ec, 1, &selection);
                }
                RuntimeLog(L"CompositionEditSession: Commit SetSelection anchor=%ld hr=0x%08X",
                    anchor + length, static_cast<unsigned int>(caretHr));
                hasCommittedAnchor_ = true;
                committedAnchor_ = anchor + length;
            }
        }
        if (acp) acp->Release();
        if (range) range->Release();
        RuntimeLog(L"CompositionEditSession: Commit length=%ld total=%ld hr=0x%08X", commitLength_, length, static_cast<unsigned int>(hr));
        return hr;
    }

    HRESULT Cancel(TfEditCookie ec)
    {
        if (!composition_ || !*composition_) return S_OK;
        ITfRange* range = nullptr;
        HRESULT hr = GetCompositionRange(&range);
        if (SUCCEEDED(hr)) SetCompositionDisplayAttribute(context_, ec, range, false);
        if (SUCCEEDED(hr)) hr = range->SetText(ec, 0, L"", 0);
        if (SUCCEEDED(hr) && endingComposition_) *endingComposition_ = true;
        if (SUCCEEDED(hr)) hr = (*composition_)->EndComposition(ec);
        if (endingComposition_) *endingComposition_ = false;
        if (SUCCEEDED(hr)) {
            (*composition_)->Release();
            *composition_ = nullptr;
        }
        if (range) range->Release();
        RuntimeLog(L"CompositionEditSession: Cancel hr=0x%08X", static_cast<unsigned int>(hr));
        return hr;
    }

    HRESULT Apply(TfEditCookie ec)
    {
        if (!context_ || !composition_) return E_POINTER;
        switch (operation_) {
        case CompositionOperation::Update: return Update(ec);
        case CompositionOperation::Commit: return Commit(ec);
        case CompositionOperation::Cancel: return Cancel(ec);
        default: return E_INVALIDARG;
        }
    }

    long refCount_;
    ITfContext* context_;
    CompositionOperation operation_;
    std::wstring text_;
    LONG caretOffset_;
    LONG commitLength_;
    ITfComposition** composition_;
    ITfCompositionSink* sink_;
    bool* endingComposition_;
    HWND completionWindow_;
    CommitRequest* completionRequest_;
    HRESULT result_;
    bool hasCommittedAnchor_;
    LONG committedAnchor_;
};

class DisplayAttributeInfo final : public ITfDisplayAttributeInfo
{
public:
    DisplayAttributeInfo() : refCount_(1) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_ITfDisplayAttributeInfo) *ppv = static_cast<ITfDisplayAttributeInfo*>(this);
        else return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }
    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG next = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (next == 0) delete this;
        return next;
    }
    STDMETHODIMP GetGUID(GUID* guid) override { if (!guid) return E_POINTER; *guid = GUID_AyaoriHimeInputAttribute; return S_OK; }
    STDMETHODIMP GetDescription(BSTR* description) override
    {
        if (!description) return E_POINTER;
        *description = SysAllocString(L"AyaoriHIME input");
        return *description ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* attribute) override
    {
        if (!attribute) return E_POINTER;
        ZeroMemory(attribute, sizeof(*attribute));
        attribute->crText.type = TF_CT_NONE;
        attribute->crBk.type = TF_CT_NONE;
        attribute->lsStyle = TF_LS_SOLID;
        attribute->fBoldLine = TRUE;
        attribute->crLine.type = TF_CT_NONE;
        attribute->bAttr = TF_ATTR_INPUT;
        RuntimeLog(L"DisplayAttributeInfo::GetAttributeInfo solid underline with application color");
        return S_OK;
    }
    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) override { return E_NOTIMPL; }
    STDMETHODIMP Reset() override { return S_OK; }
private:
    long refCount_;
};

class DisplayAttributeEnumerator final : public IEnumTfDisplayAttributeInfo
{
public:
    DisplayAttributeEnumerator() : refCount_(1), emitted_(false) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_IEnumTfDisplayAttributeInfo) *ppv = static_cast<IEnumTfDisplayAttributeInfo*>(this);
        else return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }
    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG next = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (next == 0) delete this;
        return next;
    }
    STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** clone) override
    {
        if (!clone) return E_POINTER;
        auto value = new (std::nothrow) DisplayAttributeEnumerator();
        if (!value) return E_OUTOFMEMORY;
        value->emitted_ = emitted_;
        *clone = value;
        return S_OK;
    }
    STDMETHODIMP Next(ULONG count, ITfDisplayAttributeInfo** result, ULONG* fetched) override
    {
        if (!result || (count != 1 && !fetched)) return E_POINTER;
        ULONG actual = 0;
        if (!emitted_ && count > 0) {
            result[0] = new (std::nothrow) DisplayAttributeInfo();
            if (!result[0]) return E_OUTOFMEMORY;
            emitted_ = true;
            actual = 1;
        }
        if (fetched) *fetched = actual;
        return actual == count ? S_OK : S_FALSE;
    }
    STDMETHODIMP Reset() override { emitted_ = false; return S_OK; }
    STDMETHODIMP Skip(ULONG count) override
    {
        if (count == 0) return S_OK;
        if (!emitted_) { emitted_ = true; return count == 1 ? S_OK : S_FALSE; }
        return S_FALSE;
    }
private:
    long refCount_;
    bool emitted_;
};

class TextService final : public ITfTextInputProcessorEx, public ITfThreadMgrEventSink,
    public ITfDisplayAttributeProvider, public ITfCompositionSink
{
public:
    TextService()
        : refCount_(1),
          threadMgr_(nullptr),
          source_(nullptr),
          clientId_(TF_CLIENTID_NULL),
          activationFlags_(0),
          threadMgrSinkCookie_(TF_INVALID_COOKIE),
          hwnd_(nullptr),
          stopEvent_(nullptr),
          pipeThread_(nullptr),
          focusEvent_(nullptr),
          focusThread_(nullptr),
          pendingFocusState_(-1),
          pipeHandle_(INVALID_HANDLE_VALUE),
          currentContext_(nullptr),
          composition_(nullptr),
          compositionId_(0),
          compositionSequence_(0),
          endingComposition_(false),
          lastOutputContext_(nullptr),
          lastOutputAnchor_(0),
          hasLastOutputAnchor_(false),
          lastOutputTick_(0),
          stopping_(0),
          lastPipeConnectError_(ERROR_SUCCESS)
    {
        InitializeCriticalSection(&contextLock_);
        InitializeCriticalSection(&pipeLock_);
        ++g_objectCount;
        RuntimeLog(L"TextService: constructed");
    }

    ~TextService()
    {
        RuntimeLog(L"TextService: destructing");
        StopPipeClient();
        DestroyMessageWindow();
        ClearCurrentContext();
        if (composition_) composition_->Release();
        UnadviseThreadMgrSink();
        if (threadMgr_) threadMgr_->Release();
        DeleteCriticalSection(&pipeLock_);
        DeleteCriticalSection(&contextLock_);
        --g_objectCount;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor) {
            *ppv = static_cast<ITfTextInputProcessor*>(static_cast<ITfTextInputProcessorEx*>(this));
        } else if (riid == IID_ITfTextInputProcessorEx) {
            *ppv = static_cast<ITfTextInputProcessorEx*>(this);
        } else if (riid == IID_ITfThreadMgrEventSink) {
            *ppv = static_cast<ITfThreadMgrEventSink*>(this);
        } else if (riid == IID_ITfDisplayAttributeProvider) {
            *ppv = static_cast<ITfDisplayAttributeProvider*>(this);
        } else if (riid == IID_ITfCompositionSink) {
            *ppv = static_cast<ITfCompositionSink*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** enumerator) override
    {
        if (!enumerator) return E_POINTER;
        *enumerator = new (std::nothrow) DisplayAttributeEnumerator();
        HRESULT hr = *enumerator ? S_OK : E_OUTOFMEMORY;
        RuntimeLog(L"EnumDisplayAttributeInfo: hr=0x%08X", static_cast<unsigned int>(hr));
        return hr;
    }

    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** info) override
    {
        if (!info) return E_POINTER;
        *info = nullptr;
        if (guid != GUID_AyaoriHimeInputAttribute) return E_INVALIDARG;
        *info = new (std::nothrow) DisplayAttributeInfo();
        HRESULT hr = *info ? S_OK : E_OUTOFMEMORY;
        RuntimeLog(L"GetDisplayAttributeInfo: hr=0x%08X", static_cast<unsigned int>(hr));
        return hr;
    }

    STDMETHODIMP OnCompositionTerminated(TfEditCookie, ITfComposition* composition) override
    {
        if (endingComposition_ || !composition_ || !IsSameComObject(composition_, composition)) return S_OK;
        uint64_t terminatedId = compositionId_;
        composition_->Release();
        composition_ = nullptr;
        compositionId_ = 0;
        compositionSequence_ = 0;
        RuntimeLog(L"OnCompositionTerminated: external id=%llu", terminatedId);
        return S_OK;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&refCount_));
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG next = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (next == 0) delete this;
        return next;
    }

    STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override
    {
        return ActivateInternal(ptim, tid, 0);
    }

    STDMETHODIMP ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD flags) override
    {
        return ActivateInternal(ptim, tid, flags);
    }

private:
    HRESULT ActivateInternal(ITfThreadMgr* ptim, TfClientId tid, DWORD flags)
    {
        if (!ptim) {
            RuntimeLog(L"Activate: ITfThreadMgr is null");
            return E_INVALIDARG;
        }
        RuntimeLog(L"Activate: clientId=%lu flags=0x%08X console=%d", tid, flags,
            (flags & TF_TMAE_CONSOLE) != 0);
        threadMgr_ = ptim;
        threadMgr_->AddRef();
        clientId_ = tid;
        activationFlags_ = flags;

        HRESULT hr = CreateMessageWindow();
        if (FAILED(hr)) {
            RuntimeLog(L"Activate: CreateMessageWindow failed hr=0x%08X", static_cast<unsigned int>(hr));
            return hr;
        }

        AdviseThreadMgrSink();
        UpdateCurrentContextFromThreadMgr(false);
        hr = StartPipeClient();
        if (FAILED(hr)) {
            RuntimeLog(L"Activate: StartPipeClient failed hr=0x%08X", static_cast<unsigned int>(hr));
            return hr;
        }
        RuntimeLog(L"Activate: completed hasContext=%d", HasCurrentContext());
        return S_OK;
    }

public:

    STDMETHODIMP Deactivate() override
    {
        RuntimeLog(L"Deactivate: begin");
        CommitCompositionForContextChange();
        StopPipeClient();
        DestroyMessageWindow();
        ClearCurrentContext();
        UnadviseThreadMgrSink();
        if (threadMgr_) {
            threadMgr_->Release();
            threadMgr_ = nullptr;
        }
        clientId_ = TF_CLIENTID_NULL;
        activationFlags_ = 0;
        RuntimeLog(L"Deactivate: completed");
        return S_OK;
    }

    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }

    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr*) override
    {
        ITfContext* context = nullptr;
        HRESULT hr = E_POINTER;
        if (pdimFocus) {
            hr = pdimFocus->GetTop(&context);
        }
        SetCurrentContext(context);
        bool hasFocus = context != nullptr;
        if (context) context->Release();
        RuntimeLog(L"OnSetFocus: documentMgr=%p GetTop=0x%08X hasContext=%d", pdimFocus, static_cast<unsigned int>(hr), hasFocus);
        QueueFocusChangedToPipe(hasFocus);
        return S_OK;
    }

    STDMETHODIMP OnPushContext(ITfContext* context) override
    {
        SetCurrentContext(context);
        RuntimeLog(L"OnPushContext: context=%p", context);
        QueueFocusChangedToPipe(context != nullptr);
        return S_OK;
    }

    STDMETHODIMP OnPopContext(ITfContext*) override
    {
        RuntimeLog(L"OnPopContext");
        UpdateCurrentContextFromThreadMgr(true);
        return S_OK;
    }

    void PipeThreadMain()
    {
        std::wstring pipePath;
        HRESULT pathHr = BuildPipePath(pipePath);
        if (FAILED(pathHr)) {
            RuntimeLog(L"PipeThread: BuildPipePath failed hr=0x%08X", static_cast<unsigned int>(pathHr));
            return;
        }
        RuntimeLog(L"PipeThread: started pipe=%s", pipePath.c_str());

        while (!IsStopRequested()) {
            HANDLE pipe = CreateFileW(
                pipePath.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (pipe == INVALID_HANDLE_VALUE) {
                DWORD error = GetLastError();
                if (error != lastPipeConnectError_) {
                    RuntimeLog(L"PipeThread: CreateFile failed error=%lu", error);
                    lastPipeConnectError_ = error;
                }
                if (error == ERROR_PIPE_BUSY) {
                    WaitNamedPipeW(pipePath.c_str(), 1000);
                } else {
                    WaitForSingleObject(stopEvent_, 1000);
                }
                continue;
            }

            lastPipeConnectError_ = ERROR_SUCCESS;
            RuntimeLog(L"PipeThread: connected");
            DWORD mode = PIPE_READMODE_BYTE;
            SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);
            SetConnectedPipe(pipe);

            SendPipeMessage(MessageHello, nullptr, 0);
            SendFocusChangedToPipe(HasCurrentContext());
            ReadPipeLoop(pipe);
            if (!IsStopRequested()) {
                HRESULT resetHr = DispatchOperationToTsfThread(
                    CompositionOperation::Cancel, 0, 0, L"", 0, 0);
                RuntimeLog(L"PipeThread: reset composition after disconnect hr=0x%08X",
                    static_cast<unsigned int>(resetHr));
            }
            if (IsStopRequested()) {
                SendPipeMessage(MessageBye, nullptr, 0);
            }
            ClearConnectedPipe(pipe);
            RuntimeLog(L"PipeThread: disconnected");

            if (!IsStopRequested()) {
                WaitForSingleObject(stopEvent_, 500);
            }
        }
        RuntimeLog(L"PipeThread: stopped");
    }

private:
    static DWORD WINAPI PipeThreadProc(LPVOID param)
    {
        auto self = static_cast<TextService*>(param);
        self->PipeThreadMain();
        self->Release();
        return 0;
    }

    static DWORD WINAPI FocusThreadProc(LPVOID param)
    {
        auto self = static_cast<TextService*>(param);
        HANDLE waits[2] = { self->focusEvent_, self->stopEvent_ };
        while (!self->IsStopRequested()) {
            DWORD waitResult = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
            if (waitResult != WAIT_OBJECT_0 || self->IsStopRequested()) break;
            LONG state = InterlockedExchange(&self->pendingFocusState_, -1);
            if (state >= 0) self->SendFocusChangedToPipe(state != 0);
        }
        self->Release();
        return 0;
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }

        auto self = reinterpret_cast<TextService*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self && message == WM_AYAORI_OPERATION_REQUEST) {
            auto request = reinterpret_cast<CommitRequest*>(lParam);
            if (request) {
                bool completionPending = false;
                HRESULT result = InterlockedCompareExchange(&request->abandoned, 0, 0) == 0
                    ? self->ApplyCompositionOperation(request->operation, request->compositionId, request->sequence,
                        request->text, request->caretOffset, request->commitLength, request, &completionPending)
                    : E_ABORT;
                if (!completionPending) request->Complete(result);
                request->Release();
            }
            return 0;
        }
        if (self && message == WM_AYAORI_OPERATION_COMPLETED) {
            auto request = reinterpret_cast<CommitRequest*>(lParam);
            if (request) {
                self->FinalizeAsyncCompositionOperation(request);
                request->Release();
            }
            return 0;
        }
        if (self && message == WM_AYAORI_CONTEXT_REQUEST) {
            auto request = reinterpret_cast<ContextRequest*>(lParam);
            if (request) {
                bool completionPending = false;
                HRESULT result = self->ReadPrecedingContext(
                    request->maxLength, &request->text, request, &completionPending);
                if (!completionPending) request->Complete(result);
                request->Release();
            }
            return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    static HRESULT RegisterMessageWindowClass()
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = g_instance;
        wc.lpszClassName = MessageWindowClassName;
        if (RegisterClassExW(&wc)) return S_OK;
        DWORD error = GetLastError();
        return error == ERROR_CLASS_ALREADY_EXISTS ? S_OK : HRESULT_FROM_WIN32(error);
    }

    HRESULT CreateMessageWindow()
    {
        if (hwnd_) return S_OK;
        HRESULT hr = RegisterMessageWindowClass();
        if (FAILED(hr)) {
            RuntimeLog(L"CreateMessageWindow: RegisterClassEx failed hr=0x%08X", static_cast<unsigned int>(hr));
            return hr;
        }

        hwnd_ = CreateWindowExW(
            0,
            MessageWindowClassName,
            L"",
            0,
            0,
            0,
            0,
            0,
            HWND_MESSAGE,
            nullptr,
            g_instance,
            this);
        hr = hwnd_ ? S_OK : HRESULT_FROM_WIN32(GetLastError());
        RuntimeLog(L"CreateMessageWindow: hwnd=%p hr=0x%08X", hwnd_, static_cast<unsigned int>(hr));
        return hr;
    }

    void DestroyMessageWindow()
    {
        if (!hwnd_) return;
        FlushPendingCommitMessages();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    void FlushPendingCommitMessages()
    {
        if (!hwnd_) return;
        MSG msg = {};
        while (PeekMessageW(&msg, hwnd_, WM_AYAORI_OPERATION_REQUEST, WM_AYAORI_CONTEXT_REQUEST, PM_REMOVE)) {
            if (msg.message == WM_AYAORI_CONTEXT_REQUEST) {
                auto request = reinterpret_cast<ContextRequest*>(msg.lParam);
                if (request) { request->result = E_ABORT; SetEvent(request->completedEvent); request->Release(); }
            } else {
                auto request = reinterpret_cast<CommitRequest*>(msg.lParam);
                if (request) { request->Complete(E_ABORT); request->Release(); }
            }
        }
    }

    HRESULT StartPipeClient()
    {
        if (pipeThread_) return S_OK;
        InterlockedExchange(&stopping_, 0);
        InterlockedExchange(&pendingFocusState_, -1);
        stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stopEvent_) return HRESULT_FROM_WIN32(GetLastError());
        focusEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!focusEvent_) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
            return hr;
        }

        AddRef();
        focusThread_ = CreateThread(nullptr, 0, FocusThreadProc, this, 0, nullptr);
        if (!focusThread_) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            Release();
            CloseHandle(focusEvent_);
            focusEvent_ = nullptr;
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
            return hr;
        }

        AddRef();
        pipeThread_ = CreateThread(nullptr, 0, PipeThreadProc, this, 0, nullptr);
        if (!pipeThread_) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            Release();
            InterlockedExchange(&stopping_, 1);
            SetEvent(stopEvent_);
            SetEvent(focusEvent_);
            WaitForSingleObject(focusThread_, INFINITE);
            CloseHandle(focusThread_);
            focusThread_ = nullptr;
            CloseHandle(focusEvent_);
            focusEvent_ = nullptr;
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
            return hr;
        }
        RuntimeLog(L"StartPipeClient: thread created");
        return S_OK;
    }

    void StopPipeClient()
    {
        RuntimeLog(L"StopPipeClient: begin");
        InterlockedExchange(&stopping_, 1);
        if (stopEvent_) SetEvent(stopEvent_);
        if (focusEvent_) SetEvent(focusEvent_);
        if (focusThread_) CancelSynchronousIo(focusThread_);
        if (pipeThread_) {
            CancelSynchronousIo(pipeThread_);
            WaitForSingleObject(pipeThread_, INFINITE);
            CloseHandle(pipeThread_);
            pipeThread_ = nullptr;
        }
        if (focusThread_) {
            WaitForSingleObject(focusThread_, INFINITE);
            CloseHandle(focusThread_);
            focusThread_ = nullptr;
        }
        if (focusEvent_) {
            CloseHandle(focusEvent_);
            focusEvent_ = nullptr;
        }
        if (stopEvent_) {
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
        }
        RuntimeLog(L"StopPipeClient: completed");
    }

    bool IsStopRequested() const
    {
        return InterlockedCompareExchange(const_cast<volatile LONG*>(&stopping_), 0, 0) != 0 ||
            (stopEvent_ && WaitForSingleObject(stopEvent_, 0) == WAIT_OBJECT_0);
    }

    void AdviseThreadMgrSink()
    {
        if (!threadMgr_ || source_) return;
        ITfSource* source = nullptr;
        HRESULT hr = threadMgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source));
        if (SUCCEEDED(hr)) {
            DWORD cookie = TF_INVALID_COOKIE;
            hr = source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &cookie);
            if (SUCCEEDED(hr)) {
                source_ = source;
                threadMgrSinkCookie_ = cookie;
                RuntimeLog(L"AdviseThreadMgrSink: success cookie=%lu", cookie);
                return;
            }
            RuntimeLog(L"AdviseThreadMgrSink: AdviseSink failed hr=0x%08X", static_cast<unsigned int>(hr));
            source->Release();
        } else {
            RuntimeLog(L"AdviseThreadMgrSink: QueryInterface failed hr=0x%08X", static_cast<unsigned int>(hr));
        }
    }

    void UnadviseThreadMgrSink()
    {
        if (source_) {
            if (threadMgrSinkCookie_ != TF_INVALID_COOKIE) source_->UnadviseSink(threadMgrSinkCookie_);
            source_->Release();
            source_ = nullptr;
            threadMgrSinkCookie_ = TF_INVALID_COOKIE;
        }
    }

    void SetCurrentContext(ITfContext* context)
    {
        EnterCriticalSection(&contextLock_);
        bool changed = !IsSameComObject(currentContext_, context);
        LeaveCriticalSection(&contextLock_);
        if (changed) CommitCompositionForContextChange();

        EnterCriticalSection(&contextLock_);
        if (!IsSameComObject(currentContext_, context)) {
            ClearLastOutputAnchorLocked();
        }
        if (currentContext_) currentContext_->Release();
        currentContext_ = context;
        if (currentContext_) currentContext_->AddRef();
        LeaveCriticalSection(&contextLock_);
    }

    void ClearCurrentContext()
    {
        EnterCriticalSection(&contextLock_);
        ClearLastOutputAnchorLocked();
        if (currentContext_) {
            currentContext_->Release();
            currentContext_ = nullptr;
        }
        LeaveCriticalSection(&contextLock_);
    }

    ITfContext* GetCurrentContext()
    {
        EnterCriticalSection(&contextLock_);
        ITfContext* context = currentContext_;
        if (context) context->AddRef();
        LeaveCriticalSection(&contextLock_);
        return context;
    }

    bool HasCurrentContext()
    {
        EnterCriticalSection(&contextLock_);
        bool result = currentContext_ != nullptr;
        LeaveCriticalSection(&contextLock_);
        return result;
    }

    static bool IsSameComObject(IUnknown* left, IUnknown* right)
    {
        if (left == right) return true;
        if (!left || !right) return false;

        IUnknown* leftIdentity = nullptr;
        IUnknown* rightIdentity = nullptr;
        HRESULT leftHr = left->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&leftIdentity));
        HRESULT rightHr = right->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&rightIdentity));
        bool same = SUCCEEDED(leftHr) && SUCCEEDED(rightHr) && leftIdentity == rightIdentity;
        if (leftIdentity) leftIdentity->Release();
        if (rightIdentity) rightIdentity->Release();
        return same;
    }

    void ClearLastOutputAnchorLocked()
    {
        if (lastOutputContext_) {
            lastOutputContext_->Release();
            lastOutputContext_ = nullptr;
        }
        lastOutputAnchor_ = 0;
        hasLastOutputAnchor_ = false;
        lastOutputTick_ = 0;
    }

    void ClearLastOutputAnchor()
    {
        EnterCriticalSection(&contextLock_);
        ClearLastOutputAnchorLocked();
        LeaveCriticalSection(&contextLock_);
    }

    bool TryGetRecentOutputAnchor(ITfContext* context, LONG* anchor)
    {
        EnterCriticalSection(&contextLock_);
        ULONGLONG now = GetTickCount64();
        bool valid = hasLastOutputAnchor_ && lastOutputContext_ && IsSameComObject(lastOutputContext_, context) &&
            now - lastOutputTick_ <= CachedOutputAnchorMaxAgeMs;
        if (!valid) ClearLastOutputAnchorLocked();
        if (valid && anchor) *anchor = lastOutputAnchor_;
        ULONGLONG age = valid ? now - lastOutputTick_ : 0;
        LeaveCriticalSection(&contextLock_);
        RuntimeLog(L"CachedOutputAnchor: lookup valid=%d anchor=%ld age=%llu", valid, valid && anchor ? *anchor : 0, age);
        return valid;
    }

    void SetLastOutputAnchor(ITfContext* context, bool hasAnchor, LONG anchor)
    {
        EnterCriticalSection(&contextLock_);
        ClearLastOutputAnchorLocked();
        if (context && hasAnchor) {
            lastOutputContext_ = context;
            lastOutputContext_->AddRef();
            lastOutputAnchor_ = anchor;
            hasLastOutputAnchor_ = true;
            lastOutputTick_ = GetTickCount64();
        }
        LeaveCriticalSection(&contextLock_);
        RuntimeLog(L"CachedOutputAnchor: updated available=%d anchor=%ld", context && hasAnchor, anchor);
    }

    void UpdateCurrentContextFromThreadMgr(bool notify)
    {
        ITfContext* context = nullptr;
        ITfDocumentMgr* documentMgr = nullptr;
        HRESULT focusHr = E_FAIL;
        HRESULT topHr = E_FAIL;
        if (threadMgr_) {
            focusHr = threadMgr_->GetFocus(&documentMgr);
        }
        if (SUCCEEDED(focusHr) && documentMgr) {
            topHr = documentMgr->GetTop(&context);
            documentMgr->Release();
        }
        SetCurrentContext(context);
        bool hasFocus = context != nullptr;
        if (context) context->Release();
        RuntimeLog(L"UpdateCurrentContext: notify=%d GetFocus=0x%08X GetTop=0x%08X hasContext=%d", notify, static_cast<unsigned int>(focusHr), static_cast<unsigned int>(topHr), hasFocus);
        if (notify) QueueFocusChangedToPipe(hasFocus);
    }

    HRESULT ApplyCompositionOperation(CompositionOperation operation, uint64_t compositionId, uint64_t sequence,
        const std::wstring& text, int32_t caretOffset, int32_t commitLength,
        CommitRequest* request = nullptr, bool* completionPending = nullptr)
    {
        if (completionPending) *completionPending = false;
        const bool resetAfterDisconnect = operation == CompositionOperation::Cancel &&
            compositionId == 0 && sequence == 0;
        if (resetAfterDisconnect) {
            if (!composition_) return S_OK;
            ITfContext* context = GetCurrentContext();
            if (!context) {
                composition_->Release();
                composition_ = nullptr;
                compositionId_ = 0;
                compositionSequence_ = 0;
                return S_OK;
            }
            auto editSession = new (std::nothrow) CompositionEditSession(
                context, CompositionOperation::Cancel, L"", 0, 0, &composition_,
                static_cast<ITfCompositionSink*>(this), &endingComposition_);
            HRESULT sessionResult = E_FAIL;
            HRESULT hr = editSession
                ? context->RequestEditSession(clientId_, editSession, TF_ES_SYNC | TF_ES_READWRITE, &sessionResult)
                : E_OUTOFMEMORY;
            if (editSession) editSession->Release();
            context->Release();
            HRESULT result = FAILED(hr) ? hr : sessionResult;
            if (SUCCEEDED(result)) {
                compositionId_ = 0;
                compositionSequence_ = 0;
            }
            RuntimeLog(L"CompositionOperation: reset after disconnect result=0x%08X active=%d",
                static_cast<unsigned int>(result), composition_ != nullptr);
            return result;
        }
        if (!composition_ && operation != CompositionOperation::Update) {
            RuntimeLog(L"CompositionOperation: idempotent type=%ld id=%llu sequence=%llu",
                static_cast<LONG>(operation), compositionId, sequence);
            return S_OK;
        }
        ITfContext* context = GetCurrentContext();
        if (!context) return E_FAIL;
        if (compositionId == 0 || sequence == 0) {
            context->Release();
            return E_INVALIDARG;
        }
        if (composition_) {
            if (compositionId_ != compositionId || sequence <= compositionSequence_) {
                context->Release();
                return E_INVALIDARG;
            }
        } else if (operation != CompositionOperation::Update) {
            context->Release();
            return S_OK;
        }

        auto editSession = new (std::nothrow) CompositionEditSession(
            context, operation, text, caretOffset, commitLength, &composition_,
            static_cast<ITfCompositionSink*>(this), &endingComposition_);
        if (!editSession) {
            context->Release();
            return E_OUTOFMEMORY;
        }
        HRESULT sessionResult = E_FAIL;
        HRESULT hr = context->RequestEditSession(clientId_, editSession, TF_ES_SYNC | TF_ES_READWRITE, &sessionResult);
        if ((hr == TF_E_SYNCHRONOUS || (SUCCEEDED(hr) && sessionResult == TF_E_SYNCHRONOUS)) &&
            request && completionPending) {
            RuntimeLog(L"CompositionOperation: synchronous edit rejected request=0x%08X session=0x%08X",
                static_cast<unsigned int>(hr), static_cast<unsigned int>(sessionResult));
            auto asyncSession = new (std::nothrow) CompositionEditSession(
                context, operation, text, caretOffset, commitLength, &composition_,
                static_cast<ITfCompositionSink*>(this), &endingComposition_, hwnd_, request);
            sessionResult = E_FAIL;
            hr = asyncSession
                ? context->RequestEditSession(clientId_, asyncSession, TF_ES_ASYNC | TF_ES_READWRITE, &sessionResult)
                : E_OUTOFMEMORY;
            if (asyncSession) asyncSession->Release();
            if (SUCCEEDED(hr)) {
                *completionPending = true;
                editSession->Release();
                context->Release();
                RuntimeLog(L"CompositionOperation: queued asynchronous type=%ld id=%llu sequence=%llu request=0x%08X",
                    static_cast<LONG>(operation), compositionId, sequence, static_cast<unsigned int>(hr));
                return S_OK;
            }
        }
        HRESULT result = FAILED(hr) ? hr : sessionResult;
        if (SUCCEEDED(result)) {
            compositionId_ = composition_ ? compositionId : 0;
            compositionSequence_ = composition_ ? sequence : 0;
            LONG committedAnchor = 0;
            if (operation == CompositionOperation::Commit && editSession->TryGetCommittedAnchor(&committedAnchor)) {
                SetLastOutputAnchor(context, true, committedAnchor);
            }
        }
        editSession->Release();
        context->Release();
        RuntimeLog(L"CompositionOperation: type=%ld id=%llu sequence=%llu textLength=%zu caret=%ld commitLength=%ld result=0x%08X active=%d",
            static_cast<LONG>(operation), compositionId, sequence, text.length(), caretOffset, commitLength,
            static_cast<unsigned int>(result), composition_ != nullptr);
        return result;
    }

    void FinalizeAsyncCompositionOperation(CommitRequest* request)
    {
        if (!request) return;
        HRESULT result = request->result;
        if (SUCCEEDED(result)) {
            compositionId_ = composition_ ? request->compositionId : 0;
            compositionSequence_ = composition_ ? request->sequence : 0;
        }
        RuntimeLog(L"CompositionOperation: asynchronous completed type=%ld id=%llu sequence=%llu result=0x%08X active=%d",
            static_cast<LONG>(request->operation), request->compositionId, request->sequence,
            static_cast<unsigned int>(result), composition_ != nullptr);
        request->Complete(result);
    }

    void CommitCompositionForContextChange()
    {
        if (!composition_) return;
        uint64_t terminatedId = compositionId_;
        ITfContext* context = GetCurrentContext();
        if (!context) {
            composition_->Release();
            composition_ = nullptr;
            compositionId_ = 0;
            compositionSequence_ = 0;
            return;
        }
        auto editSession = new (std::nothrow) CompositionEditSession(
            context, CompositionOperation::Commit, L"", 0, -1, &composition_,
            static_cast<ITfCompositionSink*>(this), &endingComposition_);
        HRESULT sessionResult = E_FAIL;
        HRESULT hr = editSession
            ? context->RequestEditSession(clientId_, editSession, TF_ES_SYNC | TF_ES_READWRITE, &sessionResult)
            : E_OUTOFMEMORY;
        if (editSession) editSession->Release();
        if (FAILED(hr) || FAILED(sessionResult)) {
            RuntimeLog(L"CommitCompositionForContextChange: request=0x%08X session=0x%08X",
                static_cast<unsigned int>(hr), static_cast<unsigned int>(sessionResult));
            if (composition_) {
                composition_->Release();
                composition_ = nullptr;
            }
        }
        compositionId_ = 0;
        compositionSequence_ = 0;
        context->Release();
        if (terminatedId != 0) {
            RuntimeLog(L"CommitCompositionForContextChange: terminated id=%llu", terminatedId);
        }
    }

    HRESULT DispatchOperationToTsfThread(CompositionOperation operation, uint64_t compositionId, uint64_t sequence,
        const std::wstring& text, int32_t caretOffset, int32_t commitLength)
    {
        HWND hwnd = hwnd_;
        if (!hwnd) return E_FAIL;
        auto request = new (std::nothrow) CommitRequest(operation, compositionId, sequence, text, caretOffset, commitLength);
        if (!request) return E_OUTOFMEMORY;
        if (!request->completedEvent) {
            request->Release();
            return HRESULT_FROM_WIN32(GetLastError());
        }
        request->AddRef();
        if (!PostMessageW(hwnd, WM_AYAORI_OPERATION_REQUEST, 0, reinterpret_cast<LPARAM>(request))) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            request->Release();
            request->Release();
            return hr;
        }
        HANDLE waits[2] = { request->completedEvent, stopEvent_ };
        DWORD waitResult = WaitForMultipleObjects(stopEvent_ ? 2 : 1, waits, FALSE, INFINITE);
        HRESULT result = E_ABORT;
        if (waitResult == WAIT_OBJECT_0) result = request->result;
        else InterlockedExchange(&request->abandoned, 1);
        request->Release();
        return result;
    }

    HRESULT DispatchContextRequestToTsfThread(uint64_t requestId, int32_t maxLength, std::wstring* text)
    {
        if (!text || maxLength < 0 || maxLength > 16) return E_INVALIDARG;
        HWND hwnd = hwnd_;
        if (!hwnd) return E_FAIL;
        auto request = new (std::nothrow) ContextRequest(requestId, maxLength);
        if (!request) return E_OUTOFMEMORY;
        if (!request->completedEvent) { request->Release(); return HRESULT_FROM_WIN32(GetLastError()); }
        request->AddRef();
        if (!PostMessageW(hwnd, WM_AYAORI_CONTEXT_REQUEST, 0, reinterpret_cast<LPARAM>(request))) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError()); request->Release(); request->Release(); return hr;
        }
        HANDLE waits[2] = { request->completedEvent, stopEvent_ };
        DWORD waitResult = WaitForMultipleObjects(stopEvent_ ? 2 : 1, waits, FALSE, INFINITE);
        HRESULT hr = waitResult == WAIT_OBJECT_0 ? request->result : E_ABORT;
        if (SUCCEEDED(hr)) *text = request->text;
        request->Release();
        return hr;
    }

    HRESULT ReadPrecedingContext(int32_t maxLength, std::wstring* text,
        ContextRequest* request, bool* completionPending)
    {
        if (!text || !completionPending) return E_POINTER;
        if (maxLength < 0 || maxLength > MaxPrecedingContextLength) return E_INVALIDARG;
        *completionPending = false;
        text->clear();
        ITfContext* context = GetCurrentContext();
        if (!context) return TF_E_DISCONNECTED;

        ITfContext* fullContext = GetFullContext(context);
        if (!fullContext) {
            HRESULT immHr = ReadPrecedingContextImm32(context, maxLength, text);
            context->Release();
            RuntimeLog(L"ReadPrecedingContext: route=imm32 maxLength=%ld resultLength=%zu hr=0x%08X",
                maxLength, text->length(), static_cast<unsigned int>(immHr));
            return immHr;
        }

        auto session = new (std::nothrow) PrecedingContextEditSession(
            fullContext, maxLength, text);
        if (!session) { fullContext->Release(); context->Release(); return E_OUTOFMEMORY; }
        HRESULT sessionResult = E_FAIL;
        HRESULT hr = fullContext->RequestEditSession(clientId_, session, TF_ES_SYNC | TF_ES_READ, &sessionResult);
        if ((hr == TF_E_SYNCHRONOUS || (SUCCEEDED(hr) && sessionResult == TF_E_SYNCHRONOUS)) && request) {
            RuntimeLog(L"ReadPrecedingContext: synchronous edit rejected request=0x%08X session=0x%08X",
                static_cast<unsigned int>(hr), static_cast<unsigned int>(sessionResult));
            auto asyncSession = new (std::nothrow) PrecedingContextEditSession(
                fullContext, maxLength, text, request);
            sessionResult = E_FAIL;
            hr = asyncSession
                ? fullContext->RequestEditSession(clientId_, asyncSession, TF_ES_ASYNC | TF_ES_READ, &sessionResult)
                : E_OUTOFMEMORY;
            if (asyncSession) asyncSession->Release();
            if (SUCCEEDED(hr)) {
                *completionPending = true;
                session->Release();
                fullContext->Release();
                context->Release();
                RuntimeLog(L"ReadPrecedingContext: queued asynchronous request=0x%08X",
                    static_cast<unsigned int>(hr));
                return S_OK;
            }
        }
        session->Release();
        fullContext->Release();
        context->Release();
        HRESULT result = FAILED(hr) ? hr : sessionResult;
        RuntimeLog(L"ReadPrecedingContext: route=tsf maxLength=%ld resultLength=%zu hr=0x%08X",
            maxLength, text->length(), static_cast<unsigned int>(result));
        return result;
    }

    HRESULT CommitTextToCurrentContext(const std::wstring& text, int32_t numBS)
    {
        ITfContext* context = GetCurrentContext();
        if (!context) {
            RuntimeLog(L"CommitText: no current context textLength=%zu numBS=%ld", text.length(), numBS);
            return E_FAIL;
        }

        LONG recentAnchor = 0;
        bool hasRecentAnchor = numBS > 0 && TryGetRecentOutputAnchor(context, &recentAnchor);
        auto editSession = new (std::nothrow) CommitEditSession(context, text, numBS, hasRecentAnchor, recentAnchor);
        if (!editSession) {
            context->Release();
            return E_OUTOFMEMORY;
        }

        HRESULT sessionResult = E_FAIL;
        HRESULT hr = context->RequestEditSession(clientId_, editSession, TF_ES_SYNC | TF_ES_READWRITE, &sessionResult);
        HRESULT result = FAILED(hr) ? hr : sessionResult;
        if (SUCCEEDED(result)) {
            LONG resultAnchor = 0;
            bool hasResultAnchor = editSession->TryGetResultAnchor(&resultAnchor);
            SetLastOutputAnchor(context, hasResultAnchor, resultAnchor);
        } else {
            ClearLastOutputAnchor();
        }
        editSession->Release();
        context->Release();
        RuntimeLog(L"CommitText: RequestEditSession=0x%08X sessionResult=0x%08X result=0x%08X textLength=%zu numBS=%ld", static_cast<unsigned int>(hr), static_cast<unsigned int>(sessionResult), static_cast<unsigned int>(result), text.length(), numBS);
        return result;
    }

    HRESULT DispatchCommitToTsfThread(const std::wstring& text, int32_t numBS)
    {
        HWND hwnd = hwnd_;
        if (!hwnd) {
            RuntimeLog(L"DispatchCommit: message window is null");
            return E_FAIL;
        }

        auto request = new (std::nothrow) CommitRequest(CompositionOperation::Update, 1, 1, text,
            static_cast<int32_t>(text.length()), static_cast<int32_t>(text.length()));
        if (!request) return E_OUTOFMEMORY;
        if (!request->completedEvent) {
            request->Release();
            return HRESULT_FROM_WIN32(GetLastError());
        }

        request->AddRef(); // message queue reference
        if (!PostMessageW(hwnd, WM_AYAORI_OPERATION_REQUEST, 0, reinterpret_cast<LPARAM>(request))) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            request->Release();
            request->Release();
            return hr;
        }

        HANDLE waits[2] = { request->completedEvent, stopEvent_ };
        DWORD waitResult = WaitForMultipleObjects(stopEvent_ ? 2 : 1, waits, FALSE, INFINITE);
        HRESULT hr = E_ABORT;
        if (waitResult == WAIT_OBJECT_0) {
            hr = request->result;
        } else {
            InterlockedExchange(&request->abandoned, 1);
        }
        RuntimeLog(L"DispatchCommit: waitResult=%lu result=0x%08X textLength=%zu numBS=%ld", waitResult, static_cast<unsigned int>(hr), text.length(), numBS);
        request->Release();
        return hr;
    }

    HRESULT BuildPipePath(std::wstring& pipePath)
    {
        std::wstring sid;
        HRESULT hr = GetCurrentUserSidForPipeName(sid);
        if (FAILED(hr)) return hr;
        pipePath = L"\\\\.\\pipe\\AyaoriHIME.TsfOutput." + sid;
        return S_OK;
    }

    HRESULT GetCurrentUserSidForPipeName(std::wstring& sid)
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        DWORD length = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &length);
        if (length == 0) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            CloseHandle(token);
            return hr;
        }

        std::vector<BYTE> buffer(length);
        if (!GetTokenInformation(token, TokenUser, buffer.data(), length, &length)) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            CloseHandle(token);
            return hr;
        }

        auto tokenUser = reinterpret_cast<TOKEN_USER*>(buffer.data());
        LPWSTR sidString = nullptr;
        if (!ConvertSidToStringSidW(tokenUser->User.Sid, &sidString)) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            CloseHandle(token);
            return hr;
        }

        sid.assign(sidString);
        for (auto& ch : sid) {
            if (ch == L'-') ch = L'_';
        }

        LocalFree(sidString);
        CloseHandle(token);
        return S_OK;
    }

    void SetConnectedPipe(HANDLE pipe)
    {
        EnterCriticalSection(&pipeLock_);
        pipeHandle_ = pipe;
        LeaveCriticalSection(&pipeLock_);
    }

    void ClearConnectedPipe(HANDLE pipe)
    {
        EnterCriticalSection(&pipeLock_);
        if (pipeHandle_ == pipe) pipeHandle_ = INVALID_HANDLE_VALUE;
        CloseHandle(pipe);
        LeaveCriticalSection(&pipeLock_);
    }

    HRESULT SendFocusChangedToPipe(bool hasFocus)
    {
        int32_t value = hasFocus ? 1 : 0;
        HRESULT hr = SendPipeMessage(MessageFocusChanged, &value, sizeof(value));
        RuntimeLog(L"SendFocusChanged: hasFocus=%d hr=0x%08X", hasFocus, static_cast<unsigned int>(hr));
        return hr;
    }

    void QueueFocusChangedToPipe(bool hasFocus)
    {
        // active clientはAyaoriHIME側でforeground process IDから決定する。
        // callbackから派生する同期pipe書き込みは対象アプリ停止の原因になるため送信しない。
        RuntimeLog(L"QueueFocusChanged: hasFocus=%d", hasFocus);
    }

    HRESULT SendCommitResultToPipe(HRESULT result)
    {
        int32_t value = static_cast<int32_t>(result);
        HRESULT hr = SendPipeMessage(MessageOperationResult, &value, sizeof(value));
        RuntimeLog(L"SendCommitResult: result=0x%08X write=0x%08X", static_cast<unsigned int>(result), static_cast<unsigned int>(hr));
        return hr;
    }

    HRESULT SendOperationResultToPipe(CompositionOperation operation, uint64_t compositionId, uint64_t sequence, HRESULT result)
    {
        OperationResultPayload payload = {};
        payload.operation = static_cast<int32_t>(operation);
        payload.compositionId = compositionId;
        payload.sequence = sequence;
        payload.hresult = static_cast<int32_t>(result);
        return SendPipeMessage(MessageOperationResult, &payload, sizeof(payload));
    }

    HRESULT SendContextResultToPipe(uint64_t requestId, HRESULT result, const std::wstring& text)
    {
        int32_t byteLength = static_cast<int32_t>(text.length() * sizeof(wchar_t));
        std::vector<BYTE> payload(16 + static_cast<size_t>(byteLength));
        memcpy(payload.data(), &requestId, 8);
        int32_t hr = static_cast<int32_t>(result);
        memcpy(payload.data() + 8, &hr, 4);
        memcpy(payload.data() + 12, &byteLength, 4);
        if (byteLength > 0) memcpy(payload.data() + 16, text.data(), byteLength);
        return SendPipeMessage(MessagePrecedingContextResult, payload.data(), static_cast<DWORD>(payload.size()));
    }

    HRESULT SendPipeMessage(int16_t type, const void* payload, DWORD payloadLength)
    {
        EnterCriticalSection(&pipeLock_);
        HANDLE pipe = pipeHandle_;
        if (pipe == INVALID_HANDLE_VALUE) {
            LeaveCriticalSection(&pipeLock_);
            RuntimeLog(L"SendPipeMessage: type=%d pipe is not connected", type);
            return HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED);
        }

        PipeHeader header = {};
        header.magic = PipeMagic;
        header.version = PipeVersion;
        header.type = type;
        header.payloadLength = static_cast<int32_t>(payloadLength);

        DWORD written = 0;
        BOOL ok = WriteFile(pipe, &header, sizeof(header), &written, nullptr);
        if (ok && written == sizeof(header) && payloadLength > 0) {
            ok = WriteFile(pipe, payload, payloadLength, &written, nullptr);
            ok = ok && written == payloadLength;
        }
        HRESULT hr = ok ? S_OK : HRESULT_FROM_WIN32(GetLastError());
        LeaveCriticalSection(&pipeLock_);
        if (FAILED(hr)) RuntimeLog(L"SendPipeMessage: type=%d payloadLength=%lu failed hr=0x%08X", type, payloadLength, static_cast<unsigned int>(hr));
        return hr;
    }

    bool ReadExact(HANDLE pipe, void* buffer, DWORD length)
    {
        BYTE* cursor = static_cast<BYTE*>(buffer);
        DWORD remaining = length;
        while (remaining > 0) {
            if (IsStopRequested()) return false;
            DWORD read = 0;
            if (!ReadFile(pipe, cursor, remaining, &read, nullptr) || read == 0) return false;
            cursor += read;
            remaining -= read;
        }
        return true;
    }

    void ReadPipeLoop(HANDLE pipe)
    {
        while (!IsStopRequested()) {
            PipeHeader header = {};
            if (!ReadExact(pipe, &header, sizeof(header))) {
                RuntimeLog(L"ReadPipeLoop: header read ended");
                return;
            }
            if (header.magic != PipeMagic || header.version != PipeVersion || header.payloadLength < 0 || header.payloadLength > MaxPayloadLength) {
                RuntimeLog(L"ReadPipeLoop: invalid header magic=0x%08X version=%d type=%d payloadLength=%ld", static_cast<unsigned int>(header.magic), header.version, header.type, header.payloadLength);
                return;
            }

            std::vector<BYTE> payload(static_cast<size_t>(header.payloadLength));
            if (header.payloadLength > 0 && !ReadExact(pipe, payload.data(), static_cast<DWORD>(payload.size()))) {
                RuntimeLog(L"ReadPipeLoop: payload read ended type=%d", header.type);
                return;
            }

            if (header.type == MessageUpdateComposition || header.type == MessageCommitComposition || header.type == MessageCancelComposition) {
                CompositionOperation operation = static_cast<CompositionOperation>(header.type);
                uint64_t compositionId = 0;
                uint64_t sequence = 0;
                HRESULT hr = HandleCompositionPayload(operation, payload, &compositionId, &sequence);
                RuntimeLog(L"ReadPipeLoop: operation=%d id=%llu sequence=%llu payloadLength=%ld result=0x%08X",
                    header.type, compositionId, sequence, header.payloadLength, static_cast<unsigned int>(hr));
                SendOperationResultToPipe(operation, compositionId, sequence, hr);
            } else if (header.type == MessageReadPrecedingContext) {
                uint64_t requestId = 0;
                int32_t maxLength = 0;
                std::wstring text;
                HRESULT hr = E_INVALIDARG;
                if (payload.size() == 12) {
                    memcpy(&requestId, payload.data(), 8);
                    memcpy(&maxLength, payload.data() + 8, 4);
                    hr = DispatchContextRequestToTsfThread(requestId, maxLength, &text);
                }
                RuntimeLog(L"ReadPipeLoop: context requestId=%llu maxLength=%ld resultLength=%zu hr=0x%08X",
                    requestId, maxLength, text.length(), static_cast<unsigned int>(hr));
                SendContextResultToPipe(requestId, hr, SUCCEEDED(hr) ? text : L"");
            } else {
                RuntimeLog(L"ReadPipeLoop: ignored message type=%d payloadLength=%ld", header.type, header.payloadLength);
            }
        }
    }

    HRESULT HandleCommitTextPayload(const std::vector<BYTE>& payload)
    {
        if (payload.size() < 8) return E_INVALIDARG;

        int32_t numBS = 0;
        int32_t textBytesLength = 0;
        memcpy(&numBS, payload.data(), sizeof(numBS));
        memcpy(&textBytesLength, payload.data() + 4, sizeof(textBytesLength));

        if (textBytesLength < 0 || (textBytesLength % sizeof(wchar_t)) != 0) return E_INVALIDARG;
        if (payload.size() < 8 + static_cast<size_t>(textBytesLength)) return E_INVALIDARG;

        std::wstring text;
        size_t textLength = static_cast<size_t>(textBytesLength) / sizeof(wchar_t);
        text.resize(textLength);
        if (textBytesLength > 0) {
            memcpy(&text[0], payload.data() + 8, static_cast<size_t>(textBytesLength));
        }

        RuntimeLog(L"HandleCommitText: textLength=%zu numBS=%ld", textLength, numBS);
        return DispatchCommitToTsfThread(text, numBS);
    }

    HRESULT HandleCompositionPayload(CompositionOperation operation, const std::vector<BYTE>& payload,
        uint64_t* compositionId, uint64_t* sequence)
    {
        if (!compositionId || !sequence || payload.size() < 16) return E_INVALIDARG;
        memcpy(compositionId, payload.data(), sizeof(*compositionId));
        memcpy(sequence, payload.data() + 8, sizeof(*sequence));
        if (*compositionId == 0 || *sequence == 0) return E_INVALIDARG;

        std::wstring text;
        int32_t caretOffset = 0;
        int32_t commitLength = 0;
        if (operation == CompositionOperation::Update) {
            if (payload.size() < 24) return E_INVALIDARG;
            int32_t textBytesLength = 0;
            memcpy(&caretOffset, payload.data() + 16, sizeof(caretOffset));
            memcpy(&textBytesLength, payload.data() + 20, sizeof(textBytesLength));
            if (textBytesLength < 0 || (textBytesLength % sizeof(wchar_t)) != 0 ||
                payload.size() != 24 + static_cast<size_t>(textBytesLength)) return E_INVALIDARG;
            size_t length = static_cast<size_t>(textBytesLength) / sizeof(wchar_t);
            if (caretOffset < 0 || static_cast<size_t>(caretOffset) > length) return E_INVALIDARG;
            text.resize(length);
            if (textBytesLength > 0) memcpy(&text[0], payload.data() + 24, static_cast<size_t>(textBytesLength));
        } else if (operation == CompositionOperation::Commit) {
            if (payload.size() != 20) return E_INVALIDARG;
            memcpy(&commitLength, payload.data() + 16, sizeof(commitLength));
        } else if (payload.size() != 16) {
            return E_INVALIDARG;
        }
        return DispatchOperationToTsfThread(operation, *compositionId, *sequence, text, caretOffset, commitLength);
    }

    long refCount_;
    ITfThreadMgr* threadMgr_;
    ITfSource* source_;
    TfClientId clientId_;
    DWORD activationFlags_;
    DWORD threadMgrSinkCookie_;
    HWND hwnd_;
    HANDLE stopEvent_;
    HANDLE pipeThread_;
    HANDLE focusEvent_;
    HANDLE focusThread_;
    volatile LONG pendingFocusState_;
    HANDLE pipeHandle_;
    ITfContext* currentContext_;
    ITfComposition* composition_;
    uint64_t compositionId_;
    uint64_t compositionSequence_;
    bool endingComposition_;
    ITfContext* lastOutputContext_;
    LONG lastOutputAnchor_;
    bool hasLastOutputAnchor_;
    ULONGLONG lastOutputTick_;
    volatile LONG stopping_;
    DWORD lastPipeConnectError_;
    CRITICAL_SECTION contextLock_;
    CRITICAL_SECTION pipeLock_;
};

class ClassFactory final : public IClassFactory
{
public:
    ClassFactory() : refCount_(1) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&refCount_));
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG next = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (next == 0) delete this;
        return next;
    }

    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override
    {
        if (outer) {
            RuntimeLog(L"ClassFactory::CreateInstance: aggregation is not supported");
            return CLASS_E_NOAGGREGATION;
        }
        auto service = new (std::nothrow) TextService();
        if (!service) return E_OUTOFMEMORY;
        HRESULT hr = service->QueryInterface(riid, ppv);
        service->Release();
        RuntimeLog(L"ClassFactory::CreateInstance: hr=0x%08X", static_cast<unsigned int>(hr));
        return hr;
    }

    STDMETHODIMP LockServer(BOOL lock) override
    {
        if (lock) ++g_lockCount;
        else --g_lockCount;
        return S_OK;
    }

private:
    long refCount_;
};

static HRESULT GuidToString(REFGUID guid, wchar_t* buffer, size_t count)
{
    return StringFromGUID2(guid, buffer, static_cast<int>(count)) > 0 ? S_OK : E_FAIL;
}

static HRESULT SetRegString(HKEY root, const wchar_t* subKey, const wchar_t* name, const wchar_t* value)
{
    HKEY key = nullptr;
    LONG rc = RegCreateKeyExW(root, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr);
    if (rc != ERROR_SUCCESS) return HRESULT_FROM_WIN32(rc);
    rc = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value), static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(rc);
}

static HRESULT RegisterComServer()
{
    wchar_t clsid[64] = {};
    wchar_t module[MAX_PATH] = {};
    HRESULT hr = GuidToString(CLSID_AyaoriHimeTsfTextService, clsid, _countof(clsid));
    if (FAILED(hr)) return hr;
    if (!GetModuleFileNameW(g_instance, module, _countof(module))) return HRESULT_FROM_WIN32(GetLastError());

    wchar_t key[256] = {};
    hr = StringCchPrintfW(key, _countof(key), L"Software\\Classes\\CLSID\\%s", clsid);
    if (FAILED(hr)) return hr;
    hr = SetRegString(HKEY_CURRENT_USER, key, nullptr, L"AyaoriHIME TSF Text Service");
    if (FAILED(hr)) return hr;
    hr = StringCchCatW(key, _countof(key), L"\\InProcServer32");
    if (FAILED(hr)) return hr;
    hr = SetRegString(HKEY_CURRENT_USER, key, nullptr, module);
    if (FAILED(hr)) return hr;
    hr = SetRegString(HKEY_CURRENT_USER, key, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;
    return S_OK;
}

static void UnregisterComServer()
{
    wchar_t clsid[64] = {};
    if (FAILED(GuidToString(CLSID_AyaoriHimeTsfTextService, clsid, _countof(clsid)))) return;
    wchar_t key[256] = {};
    if (FAILED(StringCchPrintfW(key, _countof(key), L"Software\\Classes\\CLSID\\%s", clsid))) return;
    RegDeleteTreeW(HKEY_CURRENT_USER, key);
}

static HRESULT RegisterTsfProfile()
{
#ifdef _DEBUG
    auto logHr = [](const wchar_t* step, HRESULT hr) {
        wchar_t dir[MAX_PATH] = {};
        if (!GetTempPathW(_countof(dir), dir)) return;
        wchar_t path[MAX_PATH] = {};
        if (FAILED(StringCchPrintfW(path, _countof(path), L"%sAyaoriHIME_tsf_register.log", dir))) return;
        FILE* fp = nullptr;
        if (_wfopen_s(&fp, path, L"a, ccs=UTF-8") != 0 || !fp) return;
        fwprintf(fp, L"%s: 0x%08X\n", step, static_cast<unsigned int>(hr));
        fclose(fp);
    };
#else
    auto logHr = [](const wchar_t*, HRESULT) {};
#endif

    ITfCategoryMgr* categoryMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, reinterpret_cast<void**>(&categoryMgr));
    logHr(L"CoCreateInstance(CLSID_TF_CategoryMgr)", hr);
    if (SUCCEEDED(hr)) {
        hr = categoryMgr->RegisterCategory(CLSID_AyaoriHimeTsfTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_AyaoriHimeTsfTextService);
        logHr(L"ITfCategoryMgr::RegisterCategory(GUID_TFCAT_TIP_KEYBOARD)", hr);
        if (SUCCEEDED(hr)) {
            hr = categoryMgr->RegisterCategory(CLSID_AyaoriHimeTsfTextService,
                GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_AyaoriHimeTsfTextService);
            logHr(L"ITfCategoryMgr::RegisterCategory(GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER)", hr);
        }
        categoryMgr->Release();
        if (FAILED(hr)) return hr;
    } else {
        return hr;
    }

    ITfInputProcessorProfiles* profiles = nullptr;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&profiles));
    logHr(L"CoCreateInstance(CLSID_TF_InputProcessorProfiles)", hr);
    if (FAILED(hr)) return hr;

    hr = profiles->Register(CLSID_AyaoriHimeTsfTextService);
    logHr(L"ITfInputProcessorProfiles::Register", hr);
    if (SUCCEEDED(hr)) {
        hr = profiles->AddLanguageProfile(
            CLSID_AyaoriHimeTsfTextService,
            MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN),
            GUID_AyaoriHimeTsfProfile,
            const_cast<WCHAR*>(L"AyaoriHIME TSF Output"),
            static_cast<ULONG>(wcslen(L"AyaoriHIME TSF Output")),
            const_cast<WCHAR*>(L""),
            0,
            0);
        logHr(L"ITfInputProcessorProfiles::AddLanguageProfile", hr);
    }
    profiles->Release();
    return hr;
}

static void UnregisterTsfProfile()
{
    ITfCategoryMgr* categoryMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, reinterpret_cast<void**>(&categoryMgr)))) {
        categoryMgr->UnregisterCategory(CLSID_AyaoriHimeTsfTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_AyaoriHimeTsfTextService);
        categoryMgr->UnregisterCategory(CLSID_AyaoriHimeTsfTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_AyaoriHimeTsfTextService);
        categoryMgr->Release();
    }

    ITfInputProcessorProfiles* profiles = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&profiles)))) return;
    profiles->RemoveLanguageProfile(CLSID_AyaoriHimeTsfTextService, MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN), GUID_AyaoriHimeTsfProfile);
    profiles->Unregister(CLSID_AyaoriHimeTsfTextService);
    profiles->Release();
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (rclsid != CLSID_AyaoriHimeTsfTextService) return CLASS_E_CLASSNOTAVAILABLE;
    auto factory = new (std::nothrow) ClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    RuntimeLog(L"DllGetClassObject: hr=0x%08X", static_cast<unsigned int>(hr));
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return g_objectCount == 0 && g_lockCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
    RuntimeLog(L"DllRegisterServer: begin");
    HRESULT hr = RegisterComServer();
    if (FAILED(hr)) {
        RuntimeLog(L"DllRegisterServer: RegisterComServer failed hr=0x%08X", static_cast<unsigned int>(hr));
        return hr;
    }

    HRESULT coinit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool shouldUninitialize = SUCCEEDED(coinit);
    if (FAILED(coinit) && coinit != RPC_E_CHANGED_MODE) return coinit;

    hr = RegisterTsfProfile();
    if (shouldUninitialize) CoUninitialize();
    RuntimeLog(L"DllRegisterServer: completed hr=0x%08X", static_cast<unsigned int>(hr));
    return hr;
}

STDAPI DllUnregisterServer()
{
    RuntimeLog(L"DllUnregisterServer: begin");
    HRESULT coinit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool shouldUninitialize = SUCCEEDED(coinit);
    UnregisterTsfProfile();
    if (shouldUninitialize) CoUninitialize();
    UnregisterComServer();
    RuntimeLog(L"DllUnregisterServer: completed");
    return S_OK;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
