using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using Utils;

namespace KanchokuWS.Handler
{
    /// <summary>
    /// グローバルキーフッククラス
    /// cf. https://aonasuzutsuki.hatenablog.jp/entry/2018/10/15/170958 
    /// </summary>
    public static class KeyboardHook
    {
        private static Logger logger = Logger.GetLogger();

        // フックの種類
        private const int WH_KEYBOARD_LL = 0x000D;
        private const int WH_MOUSE_LL = 0x000E;

        // キーボードイベント
        private const int WM_KEYDOWN = 0x0100;
        private const int WM_KEYUP = 0x0101;
        private const int WM_SYSKEYDOWN = 0x0104;
        private const int WM_SYSKEYUP = 0x0105;

        // マウスのイベント
        private const int WM_LBUTTONDOWN = 0x0201;
        private const int WM_LBUTTONUP = 0x0202;
        private const int WM_RBUTTONDOWN = 0x0204;
        private const int WM_RBUTTONUP = 0x0205;
        private const int WM_MOUSEMOVE = 0x0200;
        private const uint WM_QUIT = 0x0012;

        #region Win32API Structures
        [StructLayout(LayoutKind.Sequential)]
        public class KBDLLHOOKSTRUCT
        {
            public uint vkCode;
            public uint scanCode;
            public KBDLLHOOKSTRUCTFlags flags;
            public uint time;
            public UIntPtr dwExtraInfo;
        }

        [Flags]
        public enum KBDLLHOOKSTRUCTFlags : uint
        {
            KEYEVENTF_EXTENDEDKEY = 0x0001,
            KEYEVENTF_KEYUP = 0x0002,
            KEYEVENTF_SCANCODE = 0x0008,
            KEYEVENTF_UNICODE = 0x0004,
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct POINT
        {
            public int x;
            public int y;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct MSLLHOOKSTRUCT
        {
            public POINT pt;
            public uint mouseData;
            public uint flags;
            public uint time;
            public UIntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct MSG
        {
            public IntPtr hwnd;
            public uint message;
            public UIntPtr wParam;
            public IntPtr lParam;
            public uint time;
            public POINT pt;
            public uint lPrivate;
        }
        #endregion

        #region Win32 Methods
        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern IntPtr SetWindowsHookEx(int idHook, KeyboardProc lpfn, IntPtr hMod, uint dwThreadId);

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern IntPtr SetWindowsHookEx(int idHook, MouseProc lpfn, IntPtr hMod, uint dwThreadId);

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool UnhookWindowsHookEx(IntPtr hhk);

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern IntPtr CallNextHookEx(IntPtr hhk, int nCode, IntPtr wParam, IntPtr lParam);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern IntPtr GetModuleHandle(string lpModuleName);

        [DllImport("user32.dll")]
        private static extern IntPtr WindowFromPoint(POINT point);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern int GetMessage(out MSG message, IntPtr hWnd, uint minFilter, uint maxFilter);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool PeekMessage(out MSG message, IntPtr hWnd, uint minFilter, uint maxFilter, uint removeMessage);

        [DllImport("user32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool PostThreadMessage(uint threadId, uint message, UIntPtr wParam, IntPtr lParam);

        [DllImport("kernel32.dll")]
        private static extern uint GetCurrentThreadId();
        #endregion

        #region Delegate
        private delegate IntPtr KeyboardProc(int nCode, IntPtr wParam, IntPtr lParam);
        private delegate IntPtr MouseProc(int nCode, IntPtr wParam, IntPtr lParam);
        #endregion

        #region Fields
        private static KeyboardProc keyboardProc;
        private static IntPtr keyboardHookId = IntPtr.Zero;
        private static MouseProc mouseProc;
        private static IntPtr mouseHookId = IntPtr.Zero;
        private static Thread mouseHookThread;
        private static uint mouseHookThreadId;
        private static ManualResetEventSlim mouseHookReady;
        private static readonly object mouseHookSync = new object();
        private static readonly uint currentProcessId = (uint)Process.GetCurrentProcess().Id;
#endregion

        public delegate bool DelegateOnKeyDownEvent(uint vkey, int scanCode, uint flags, int extraInfo);

        public delegate bool DelegateOnKeyUpEvent(uint vkey, int scanCode, uint flags, int extraInfo);

        public delegate bool DelegateOnMouseEvent(bool bLeftButton, bool bRightButton);

        public static DelegateOnKeyDownEvent OnKeyDownEvent { get; set; }

        public static DelegateOnKeyUpEvent OnKeyUpEvent { get; set; }

        public static DelegateOnMouseEvent OnMouseEvent { get; set; }

        public static void Hook()
        {
            if (keyboardHookId == IntPtr.Zero) {
                keyboardProc = HookProcedure;
                using (var curProcess = Process.GetCurrentProcess()) {
                    using (ProcessModule curModule = curProcess.MainModule) {
                        keyboardHookId = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardProc, GetModuleHandle(curModule.ModuleName), 0);
                    }
                }
            }
            StartMouseHookThread();
        }

        public static void UnHook()
        {
            UnhookWindowsHookEx(keyboardHookId);
            keyboardHookId = IntPtr.Zero;
            StopMouseHookThread();
        }

        private static void StartMouseHookThread()
        {
            lock (mouseHookSync) {
                if (mouseHookThread != null && mouseHookThread.IsAlive) return;
                mouseProc = MouseHookCallback;
                mouseHookReady = new ManualResetEventSlim(false);
                mouseHookThread = new Thread(MouseHookThreadMain) {
                    IsBackground = true,
                    Name = "AyaoriHIME mouse hook"
                };
                mouseHookThread.Start();
            }
            if (!mouseHookReady.Wait(1000)) logger.Error("Mouse hook thread did not start");
        }

        private static void StopMouseHookThread()
        {
            Thread thread;
            uint threadId;
            lock (mouseHookSync) {
                thread = mouseHookThread;
                threadId = mouseHookThreadId;
                if (mouseHookId != IntPtr.Zero) UnhookWindowsHookEx(mouseHookId);
                mouseHookId = IntPtr.Zero;
                if (threadId != 0) PostThreadMessage(threadId, WM_QUIT, UIntPtr.Zero, IntPtr.Zero);
            }
            if (thread != null && thread != Thread.CurrentThread) thread.Join(1000);
            lock (mouseHookSync) {
                mouseHookThread = null;
                mouseHookThreadId = 0;
                mouseHookReady?.Dispose();
                mouseHookReady = null;
                mouseProc = null;
            }
        }

        private static void MouseHookThreadMain()
        {
            mouseHookThreadId = GetCurrentThreadId();
            PeekMessage(out _, IntPtr.Zero, 0, 0, 0);
            using (var curProcess = Process.GetCurrentProcess()) {
                using (ProcessModule curModule = curProcess.MainModule) {
                    mouseHookId = SetWindowsHookEx(WH_MOUSE_LL, mouseProc, GetModuleHandle(curModule.ModuleName), 0);
                }
            }
            int installError = mouseHookId == IntPtr.Zero ? Marshal.GetLastWin32Error() : 0;
            mouseHookReady.Set();
            if (mouseHookId == IntPtr.Zero) {
                logger.Error($"Mouse hook installation failed: {installError}");
                return;
            }

            IntPtr installedHook = mouseHookId;
            try {
                MSG message;
                while (GetMessage(out message, IntPtr.Zero, 0, 0) > 0) {
                }
            } finally {
                UnhookWindowsHookEx(installedHook);
                if (mouseHookId == installedHook) mouseHookId = IntPtr.Zero;
            }
        }

        public class OriginalKeyEventArg : EventArgs
        {
            public int KeyCode { get; }

            public int ExtraInfo { get; }

            public OriginalKeyEventArg(int keyCode, int extraInfo)
            {
                KeyCode = keyCode;
                ExtraInfo = extraInfo;
            }
        }

        private static IntPtr IntPtrDone = new IntPtr(1);

        public static IntPtr HookProcedure(int nCode, IntPtr wParam, IntPtr lParam)
        {
            if (nCode >= 0 && (wParam == (IntPtr)WM_KEYDOWN || wParam == (IntPtr)WM_SYSKEYDOWN)) {
                var kb = (KBDLLHOOKSTRUCT)Marshal.PtrToStructure(lParam, typeof(KBDLLHOOKSTRUCT));
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"\nKeyDown: vkCode={kb.vkCode:x}H({kb.vkCode}), Scan={kb.scanCode:x}({kb.scanCode}), flag={kb.flags:x}, time={kb.time}, extraInfo={kb.dwExtraInfo}");
                //var vkCode = kb.vkCode;
                if (OnKeyDownEvent?.Invoke(kb.vkCode, (int)kb.scanCode, (uint)kb.flags, (int)kb.dwExtraInfo) ?? false) {
                    // 呼び出し先で処理が行われたので、システム側ではキー入力を破棄
                    return IntPtrDone;
                }
            } else if (nCode >= 0 && (wParam == (IntPtr)WM_KEYUP || wParam == (IntPtr)WM_SYSKEYUP)) {
                var kb = (KBDLLHOOKSTRUCT)Marshal.PtrToStructure(lParam, typeof(KBDLLHOOKSTRUCT));
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"\nKeyUp: vkCode={kb.vkCode:x}H({kb.vkCode}), Scan={kb.scanCode:x}({kb.scanCode}), flag={kb.flags:x}, time={kb.time}, extraInfo={kb.dwExtraInfo}");
                //var vkCode = kb.vkCode;
                if (OnKeyUpEvent?.Invoke(kb.vkCode, (int)kb.scanCode, (uint)kb.flags, (int)kb.dwExtraInfo) ?? false) {
                    // 呼び出し先で処理が行われたので、システム側ではキー入力を破棄
                    return IntPtrDone;
                }
            }

            return CallNextHookEx(keyboardHookId, nCode, wParam, lParam);
        }

        private static IntPtr MouseHookCallback(int nCode, IntPtr wParam, IntPtr lParam)
        {
            if (nCode >= 0) {
                bool leftButton = wParam == (IntPtr)WM_LBUTTONDOWN;
                bool rightButton = wParam == (IntPtr)WM_RBUTTONDOWN;
                if (leftButton || rightButton) {
                    var mouse = (MSLLHOOKSTRUCT)Marshal.PtrToStructure(lParam, typeof(MSLLHOOKSTRUCT));
                    IntPtr targetWindow = WindowFromPoint(mouse.pt);
                    GetWindowThreadProcessId(targetWindow, out uint targetProcessId);
                    if (targetProcessId != currentProcessId) {
                        OnMouseEvent?.Invoke(leftButton, rightButton);
                    }
                }
            }
            return CallNextHookEx(mouseHookId, nCode, wParam, lParam);
        }
    }
}
