#include "WinHotkey.h"

#include "AppSettings.h"
#include "I18n.h"
#include "QuickWindow.h"

#include <QApplication>
#include <QKeyCombination>
#include <QKeySequence>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
constexpr int HotkeyId = 0x5157;

QString uiText(const char* sourceText, const char* koreanFallback = nullptr)
{
    return I18n::translate("WinHotkey", sourceText, koreanFallback);
}

#ifdef Q_OS_WIN
#ifndef MOD_NOREPEAT
constexpr UINT MOD_NOREPEAT = 0x4000;
#endif

UINT toWindowsModifiers(Qt::KeyboardModifiers modifiers)
{
    UINT result = MOD_NOREPEAT;

    if (modifiers.testFlag(Qt::ControlModifier))
        result |= MOD_CONTROL;
    if (modifiers.testFlag(Qt::ShiftModifier))
        result |= MOD_SHIFT;
    if (modifiers.testFlag(Qt::AltModifier))
        result |= MOD_ALT;
    if (modifiers.testFlag(Qt::MetaModifier))
        result |= MOD_WIN;

    return result;
}

UINT toVirtualKey(Qt::Key key)
{
    const int value = static_cast<int>(key);

    if (value >= Qt::Key_A && value <= Qt::Key_Z)
        return static_cast<UINT>('A' + (value - Qt::Key_A));

    if (value >= Qt::Key_0 && value <= Qt::Key_9)
        return static_cast<UINT>('0' + (value - Qt::Key_0));

    if (value >= Qt::Key_F1 && value <= Qt::Key_F24)
        return static_cast<UINT>(VK_F1 + (value - Qt::Key_F1));

    switch (key) {
    case Qt::Key_Space:
        return VK_SPACE;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return VK_TAB;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return VK_RETURN;
    case Qt::Key_Escape:
        return VK_ESCAPE;
    case Qt::Key_Backspace:
        return VK_BACK;
    case Qt::Key_Delete:
        return VK_DELETE;
    case Qt::Key_Insert:
        return VK_INSERT;
    case Qt::Key_Home:
        return VK_HOME;
    case Qt::Key_End:
        return VK_END;
    case Qt::Key_PageUp:
        return VK_PRIOR;
    case Qt::Key_PageDown:
        return VK_NEXT;
    case Qt::Key_Left:
        return VK_LEFT;
    case Qt::Key_Right:
        return VK_RIGHT;
    case Qt::Key_Up:
        return VK_UP;
    case Qt::Key_Down:
        return VK_DOWN;
    case Qt::Key_Exclam:
        return static_cast<UINT>('1');
    case Qt::Key_At:
        return static_cast<UINT>('2');
    case Qt::Key_NumberSign:
        return static_cast<UINT>('3');
    case Qt::Key_Dollar:
        return static_cast<UINT>('4');
    case Qt::Key_Percent:
        return static_cast<UINT>('5');
    case Qt::Key_AsciiCircum:
        return static_cast<UINT>('6');
    case Qt::Key_Ampersand:
        return static_cast<UINT>('7');
    case Qt::Key_Asterisk:
        return static_cast<UINT>('8');
    case Qt::Key_ParenLeft:
        return static_cast<UINT>('9');
    case Qt::Key_ParenRight:
        return static_cast<UINT>('0');
    case Qt::Key_Period:
    case Qt::Key_Greater:
        return VK_OEM_PERIOD;
    case Qt::Key_Comma:
    case Qt::Key_Less:
        return VK_OEM_COMMA;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        return VK_OEM_MINUS;
    case Qt::Key_Equal:
    case Qt::Key_Plus:
        return VK_OEM_PLUS;
    case Qt::Key_Slash:
    case Qt::Key_Question:
        return VK_OEM_2;
    case Qt::Key_Backslash:
    case Qt::Key_Bar:
        return VK_OEM_5;
    case Qt::Key_Semicolon:
    case Qt::Key_Colon:
        return VK_OEM_1;
    case Qt::Key_Apostrophe:
    case Qt::Key_QuoteDbl:
        return VK_OEM_7;
    case Qt::Key_BracketLeft:
    case Qt::Key_BraceLeft:
        return VK_OEM_4;
    case Qt::Key_BracketRight:
    case Qt::Key_BraceRight:
        return VK_OEM_6;
    case Qt::Key_QuoteLeft:
    case Qt::Key_AsciiTilde:
        return VK_OEM_3;
    default:
        return 0;
    }
}

QString formatWindowsError(DWORD error)
{
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS;

    const DWORD size = FormatMessageW(
        flags,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);

    QString message;
    if (size > 0 && buffer)
        message = QString::fromWCharArray(buffer).trimmed();

    if (buffer)
        LocalFree(buffer);

    if (message.isEmpty())
        return uiText("Windows error %1", "Windows 오류 %1").arg(error);

    return QString("%1 (code %2)").arg(message).arg(error);
}
#endif
}

WinHotkey::WinHotkey(QuickWindow* window)
    : m_window(window)
{
}

WinHotkey::~WinHotkey()
{
    if (QCoreApplication* app = QCoreApplication::instance())
        app->removeNativeEventFilter(this);
    unregisterHotkey();
}

bool WinHotkey::isSupportedHotkey(const QKeySequence& sequence, QString* errorMessage)
{
    if (sequence.isEmpty()) {
        if (errorMessage)
            *errorMessage = uiText("No quick access hotkey is configured.", "빠른 액세스 단축키가 설정되어 있지 않습니다.");
        return false;
    }

#ifdef Q_OS_WIN
    const QKeyCombination combination = sequence[0];
    const Qt::KeyboardModifiers keyboardModifiers = combination.keyboardModifiers();
    const UINT virtualKey = toVirtualKey(combination.key());
    const bool hasModifier = keyboardModifiers.testFlag(Qt::ControlModifier)
        || keyboardModifiers.testFlag(Qt::ShiftModifier)
        || keyboardModifiers.testFlag(Qt::AltModifier)
        || keyboardModifiers.testFlag(Qt::MetaModifier);

    if (!hasModifier || virtualKey == 0) {
        if (errorMessage) {
            *errorMessage = uiText("%1 is not supported as a global hotkey yet.",
                                   "%1은 아직 전역 단축키로 지원되지 않습니다.")
                .arg(sequence.toString(QKeySequence::NativeText));
        }
        return false;
    }
    if (errorMessage)
        errorMessage->clear();
    return true;
#else
    Q_UNUSED(sequence)
    if (errorMessage)
        errorMessage->clear();
    return true;
#endif
}

bool WinHotkey::registerHotkey()
{
#ifdef Q_OS_WIN
    if (m_registered)
        return true;

    const QKeySequence sequence = AppSettings::showHotkey();
    if (!isSupportedHotkey(sequence, &m_lastError))
        return false;

    const QKeyCombination combination = sequence[0];
    const UINT modifiers = toWindowsModifiers(combination.keyboardModifiers());
    const UINT virtualKey = toVirtualKey(combination.key());

    if (RegisterHotKey(nullptr, HotkeyId, modifiers, virtualKey)) {
        m_registered = true;
        m_lastError.clear();
        return true;
    }

    m_lastError = QString("%1: %2")
        .arg(sequence.toString(QKeySequence::NativeText), formatWindowsError(GetLastError()));
    return false;
#else
    m_lastError = uiText("Global hotkeys are only implemented on Windows in this MVP.",
                         "이 MVP에서는 전역 단축키가 Windows에서만 구현되어 있습니다.");
    return false;
#endif
}

void WinHotkey::unregisterHotkey()
{
#ifdef Q_OS_WIN
    if (m_registered) {
        UnregisterHotKey(nullptr, HotkeyId);
        m_registered = false;
    }
#endif
}

QString WinHotkey::lastError() const
{
    return m_lastError;
}

bool WinHotkey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);

#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);

    if (msg && msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == HotkeyId) {
        if (QApplication::activeModalWidget())
            return true;
        if (m_window)
            m_window->showQuick();

        return true;
    }
#endif

    return false;
}
