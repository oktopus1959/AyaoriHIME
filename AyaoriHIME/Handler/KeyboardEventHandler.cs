using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Windows.Forms;

using KanchokuWS.Domain;
using Utils;
using KanchokuWS.CombinationKeyStroke;
using KanchokuWS.CombinationKeyStroke.DeterminerLib;
using System.Threading;

namespace KanchokuWS.Handler
{
    public class KeyboardEventHandler : IDisposable
    {
        private static Logger logger = Logger.GetLogger();

        private FrmKanchoku frmKanchoku = null;

        private FrmVirtualKeyboard frmVkb = null;

        private Queue<KeyInfo> strokeLogQueue = new Queue<KeyInfo>();

        struct KeyInfo
        {
            public DateTime dtNow;
            public bool bDown;
            public uint vkey;
            public int scanCode;
            public uint flags;
            public int extraInfo;

            public KeyInfo(bool bDown, uint vkey, int scanCode, uint flags, int extraInfo)
            {
                this.dtNow = DateTime.Now;
                this.bDown = bDown;
                this.vkey = vkey;
                this.scanCode = scanCode;
                this.flags = flags;
                this.extraInfo = extraInfo;
            }

            public override string ToString()
            {
                return $"{dtNow._toStringWithMillisec()}: {(bDown ? "DOWN" : "  UP")}: '{CharVsVKey.GetFaceStringFromVKey(vkey)}': vkey={vkey:x}, scanCode={scanCode:x}, flags={flags:x}, extraInfo={extraInfo}";
            }
        }

        private void appendKeyInfo(bool bDown, uint vkey, int scanCode, uint flags, int extraInfo)
        {
            if (Settings.KeyLoggingEnabled) {
                strokeLogQueue.Enqueue(new KeyInfo(bDown, vkey, scanCode, flags, extraInfo));
                if (strokeLogQueue.Count > 200) strokeLogQueue.Dequeue();
            }
        }

        public IEnumerable<string> getRecentKeyInfos()
        {
            return strokeLogQueue.Select(x => x.ToString());
        }

        public void clearRecentKeyInfos()
        {
            strokeLogQueue.Clear();
        }

        /// <summary>デコーダが ON か</summary>
        private bool isDecoderActivated() {
            return frmKanchoku?.IsDecoderActivated() ?? false;
        }

        /// <summary>デコーダが第1打鍵待ちか </summary>
        private bool isDecoderWaitingFirstStroke() {
            return frmKanchoku?.IsDecoderWaitingFirstStroke() == true;
        }

        /// <summary>仮想鍵盤のミニバッファにフォーカスがあるか </summary>
        private bool isVkbTopTextFocused()
        {
            return frmVkb?.IsTopTextFocused ?? false;
        }

        /// <summary>コンストラクタ</summary>
        public KeyboardEventHandler()
        {
        }

        /// <summary>
        /// 初期化
        /// </summary>
        public void Initialize(FrmKanchoku frm, FrmVirtualKeyboard vkb)
        {
            logger.Info("ENTER");

            frmKanchoku = frm;
            frmVkb = vkb;

            setInvokeHandlerToDeterminer();

            keyInfoManager = new ExModiferKeyInfoManager();

            // キーボードイベントのディスパッチ開始
            installKeyboardHook();

            logger.Info("LEAVE");
        }

        /// <summary> 内部状態の再初期化</summary>
        public void Reinitialize()
        {
            keyInfoManager.Reinitialize();
            leftShiftState.Reset();
            rightShiftState.Reset();
            activeNonShiftVkeys.Clear();
        }

        /// <summary>
        /// キーボードフックされているか
        /// </summary>
        private bool bHooked = false;

        /// <summary>
        /// キーボードフックを設定する
        /// </summary>
        private void installKeyboardHook()
        {
            logger.Info($"ENTER");
            KeyboardHook.OnKeyDownEvent = onKeyboardDownHandler;
            KeyboardHook.OnKeyUpEvent = onKeyboardUpHandler;
            KeyboardHook.OnMouseEvent = mouseButtonHandler;
            KeyboardHook.Hook();
            bHooked = true;
            logger.Info($"LEAVE");
        }

        /// <summary>
        /// キーボードフックを解放する
        /// </summary>
        private void releaseKeyboardHook()
        {
            logger.Info($"ENTER");
            if (bHooked) {
                bHooked = false;
                KeyboardHook.UnHook();
                logger.Info($"UNHOOKED");
            }
            logger.Info($"LEAVE");
        }

        public void Dispose()
        {
            releaseKeyboardHook();
        }

        /// <summary>マウスボタン押下時のハンドラ</summary>
        private bool mouseButtonHandler(bool leftButton, bool rightButton)
        {
            if (isDecoderActivated()) frmKanchoku?.CommitMultStream();
            return false;
        }

        //----------------------------------------------------------------------------------------------------------
        [DllImport("user32.dll")]
        private static extern ushort GetAsyncKeyState(uint vkey);

        private const int vkeyNum = 256;

        /// <summary>やまぶきRが送ってくる SendInput の scanCode </summary>
        private const int YamabukiRscanCode = 0x7f;

        /// <summary>他のアプリからキーコードがINJECTされた </summary>
        private const uint LLKHF_INJECTED = 0x10;

        private const uint VKEY_EISU = 0xf0;
        private const uint VKEY_HANZEN = 0xf3;
        private const uint VKEY_KANJI = 0xf4;

        /// <summary> 漢直として有効なキーか</summary>
        //private bool isEffectiveVkey(uint vkey, int scanCode, uint flags, int extraInfo, bool ctrl)
        //{
        //    // 0xa0 = LSHIFT, 0xa1 = RSHIFT, 0xa2=LCTRL, 0xa3=RCTRL, 0xa4=LALT, 0xa5 = RALT, 0xf3 = Zenkaku, 0xf4 = Kanji
        //    return
        //        (Settings.IgnoreOtherHooker ? (flags & LLKHF_INJECTED) == 0 : extraInfo != SendInputHandler.MyMagicNumber) &&
        //        scanCode != 0 && scanCode != YamabukiRscanCode &&
        //        ((vkey > 0 && vkey < 0xa0) ||
        //         ((vkey == FuncVKeys.LSHIFT || vkey == FuncVKeys.RSHIFT) &&
        //          (!ctrl || Settings.ActiveKeyWithCtrl == vkey || Settings.ActiveKeyWithCtrl2 == vkey || Settings.SelectedTableActivatedWithCtrl == vkey || Settings.DeactiveKeyWithCtrl == vkey)) ||
        //         //(vkey >= 0xa6 && vkey < 0xf3) ||
        //         //(vkey >= 0xf5 && vkey < vkeyNum));
        //         (vkey >= 0xa6 && vkey < vkeyNum));
        //}
        private bool isEffectiveVkey(uint vkey, int scanCode, uint flags, int extraInfo, bool ctrl)
        {
            return
                (Settings.IgnoreOtherHooker ? (flags & LLKHF_INJECTED) == 0 : extraInfo != SendInputHandler.MyMagicNumber) &&
                scanCode != 0 && scanCode != YamabukiRscanCode;
        }

        private static bool isAltKeyPressed()
        {
            return (GetAsyncKeyState(FuncVKeys.ALT) & 0x8000) != 0;
        }

        private static bool isWinKeyPressed()
        {
            return (GetAsyncKeyState(0x5b) & 0x8000) != 0 || (GetAsyncKeyState(0x5c) & 0x8000) != 0;
        }

        private bool ctrlKeyPressed()
        {
            return (Settings.UseLeftControlToConversion && (GetAsyncKeyState(FuncVKeys.LCONTROL) & 0x8000) != 0)
                || (Settings.UseRightControlToConversion && (GetAsyncKeyState(FuncVKeys.RCONTROL) & 0x8000) != 0);
        }

        //private bool shiftKeyPressed(bool bWithOutCtrl)
        //{
        //    return (!bWithOutCtrl && spaceHoldShiftState == ExModKeyState.SHIFTED) || (GetAsyncKeyState(FuncVKeys.LSHIFT) & 0x8000) != 0 || (GetAsyncKeyState(FuncVKeys.RSHIFT) & 0x8000) != 0;
        //}

        private bool shiftKeyPressed(uint vkey)
        {
            return effectiveShiftPressed();
        }

        private int getSpaceHoldShiftRepeatMillisec()
        {
            return Settings.SandSEnableSpaceOrRepeatMillisec;
        }

        private void updateStrokeHelpHoldShiftPlane(bool bDecoderOn)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info($"CALLED: bDecoderOn={bDecoderOn}");
            var effectiveInfo = keyInfoManager?.getEffectiveHoldShiftKeyInfoForDisplay(bDecoderOn);
            var setting = effectiveInfo != null && effectiveInfo.IsGenericHoldShift ? Settings.GetHoldShiftKeySetting(effectiveInfo.Deckey) : null;
            bool forceShow = setting?.ShowStrokeHelp == true;
            int shiftPlane = forceShow ? setting.ShiftPlane : keyInfoManager?.getShiftPlane(bDecoderOn) ?? ShiftPlane.ShiftPlane_NONE;
            frmKanchoku?.SetStrokeHelpShiftPlane(shiftPlane, forceShow);
        }

        /// <summary> 特殊キーの押下状態</summary>
        class ExModiferKeyInfo {
            public enum ExModKeyBehavior
            {
                Default,
                GenericHoldShift,
            }

            /// <summary> 特殊キーの押下状態</summary>
            public enum ExModKeyState
            {
                RELEASED,
                PRESSED,
                SHIFTED,
                REPEATED,
            }

            public uint Vkey = 0;
            public int Deckey = 0;
            public uint ModFlag = 0;
            public ExModKeyState KeyState = ExModKeyState.RELEASED;
            public ExModKeyBehavior Behavior = ExModKeyBehavior.Default;
            public long PressedSerial = 0;
            public long ShiftedSerial = 0;
            public DateTime PrevUpDt = DateTime.MinValue;

            public string Name = "";

            public bool Released { get { return KeyState == ExModKeyState.RELEASED; } }
            public bool Pressed { get { return KeyState == ExModKeyState.PRESSED; } }
            public bool Shifted { get { return KeyState == ExModKeyState.SHIFTED; } }
            public bool Repeated { get { return KeyState == ExModKeyState.REPEATED; } }
            public bool IsGenericHoldShift { get { return Behavior == ExModKeyBehavior.GenericHoldShift; } }
            public bool IsHoldShift { get { return IsGenericHoldShift; } }

            private static long shiftedSerialSeed = 0;

            public static bool IsReleased(ExModKeyState state) { return state == ExModKeyState.RELEASED; }
            public static bool IsPressed(ExModKeyState state) { return state == ExModKeyState.PRESSED; }
            public static bool IsShifted(ExModKeyState state) { return state == ExModKeyState.SHIFTED; }
            public static bool IsRepeated(ExModKeyState state) { return state == ExModKeyState.REPEATED; }

            public void SetReleased() {
                if (Settings.LoggingDecKeyInfo) logger.Info($"{Name}:Set RELEASED");
                KeyState = ExModKeyState.RELEASED;
            }
            public void SetPressed() {
                if (Settings.LoggingDecKeyInfo) logger.Info($"{Name}:Set PRESSED");
                KeyState = ExModKeyState.PRESSED;
                PressedSerial = Interlocked.Increment(ref shiftedSerialSeed);
            }
            public void SetShifted() {
                if (Settings.LoggingDecKeyInfo) logger.Info($"{Name}:Set SHIFTED");
                KeyState = ExModKeyState.SHIFTED;
                ShiftedSerial = Interlocked.Increment(ref shiftedSerialSeed);
            }
            public void SetRepeated() {
                if (Settings.LoggingDecKeyInfo) logger.Info($"{Name}:Set REPEATED");
                KeyState = ExModKeyState.REPEATED;
            }

            /// <summary> シフト単打が有効か</summary>
            public bool IsSingleShiftHitEffecive(bool bCtrl)
            {
                if (Settings.LoggingDecKeyInfo) logger.Info($"{Name}:Vkey={Vkey}, Deckey={Deckey}, bCtrl={bCtrl}, ActiveKey={Settings.ActiveKey}, ActiveKeyWithCtrl={Settings.ActiveKeyWithCtrl}, IsExModKeyIndexAssignedForDecoderFunc={ExtraModifiers.IsExModKeyIndexAssignedForDecoderFunc(Deckey)}");
                bool bEffective = (Settings.ActiveKey == Vkey && (!bCtrl || Settings.ActiveKeyWithCtrl != Vkey)) || ExtraModifiers.IsExModKeyIndexAssignedForDecoderFunc(Deckey);
                if (Settings.LoggingDecKeyInfo) logger.Info($"{Name}:IsSingleShiftHitEffecive={bEffective}");
                return bEffective;
            }

            private bool? bShiftPlaneAssignedOn = null;
            private bool? bShiftPlaneAssignedOff = null;

            /// <summary> 拡張シフト面が定義されているか</summary>
            public bool IsShiftPlaneAssigned(bool bDecoderOn)
            {
                return bDecoderOn ? isShiftPlaneAssignedOn() : isShiftPlaneAssignedOff();
            }

            private bool isShiftPlaneAssignedOn()
            {
                if (bShiftPlaneAssignedOn == null) {
                    bShiftPlaneAssignedOn = ShiftPlane.IsShiftPlaneAssignedForShiftModFlag(ModFlag, true);
                }
                if (Settings.LoggingDecKeyInfo) logger.Info($"{Name}:decoderOn=True: IsShiftPlaneAssigned={bShiftPlaneAssignedOn}");
                return bShiftPlaneAssignedOn.Value;
            }

            private bool isShiftPlaneAssignedOff()
            {
                if (bShiftPlaneAssignedOff == null) {
                    bShiftPlaneAssignedOff = ShiftPlane.IsShiftPlaneAssignedForShiftModFlag(ModFlag, false);
                }
                if (Settings.LoggingDecKeyInfo) logger.Info($"{Name}:decoderOn=False: IsShiftPlaneAssigned={bShiftPlaneAssignedOff}");
                return bShiftPlaneAssignedOff.Value;
            }

            /// <summary> 内部状態の再初期化</summary>
            public void Reinitialize(uint vkey)
            {
                logger.Info(() => $"ENTER: {Name}, vkey={vkey}");
                Vkey = vkey;
                KeyState = ExModKeyState.RELEASED;
                PressedSerial = 0;
                ShiftedSerial = 0;
                PrevUpDt = DateTime.MinValue;
                bShiftPlaneAssignedOn = null;
                bShiftPlaneAssignedOff = null;
            }
        }

        /// <summary> 特殊キーの押下状態の管理クラス</summary>
        class ExModiferKeyInfoManager
        {
            /// <summary> CapsLockキーの押下状態</summary>
            private ExModiferKeyInfo capsKeyInfo = new ExModiferKeyInfo() { Vkey = FuncVKeys.CAPSLOCK, Deckey = DecoderKeys.CAPS_DECKEY, ModFlag = KeyModifiers.MOD_CAPS, Name = "CapsLock" };

            /// <summary> 英数キーの押下状態</summary>
            private ExModiferKeyInfo alnumKeyInfo = new ExModiferKeyInfo() { Vkey = FuncVKeys.EISU, Deckey = DecoderKeys.ALNUM_DECKEY, ModFlag = KeyModifiers.MOD_ALNUM, Name = "AlpahNum" };

            /// <summary> 無変換キーの押下状態</summary>
            private ExModiferKeyInfo nferKeyInfo = new ExModiferKeyInfo() { Vkey = FuncVKeys.MUHENKAN, Deckey = DecoderKeys.NFER_DECKEY, ModFlag = KeyModifiers.MOD_NFER, Name = "Nfer" };

            /// <summary> 変換キーの押下状態</summary>
            private ExModiferKeyInfo xferKeyInfo = new ExModiferKeyInfo() { Vkey = FuncVKeys.HENKAN, Deckey = DecoderKeys.XFER_DECKEY, ModFlag = KeyModifiers.MOD_XFER, Name = "Xfer" };

            /// <summary> その他キーの押下状態</summary>
            private ExModiferKeyInfo otherKeyState = new ExModiferKeyInfo() { Name = "Other" };

            /// <summary>
            /// ホールドシフトキーの押下状態の辞書。キーはVKEY。DecoderKeys.STROKE_SPACE_DECKEYやDecoderKeys.FUNC_DECKEY_START以上のDeckeyに対応するVKEYが入る。
            /// </summary>
            private Dictionary<uint, ExModiferKeyInfo> holdShiftKeyInfos = new Dictionary<uint, ExModiferKeyInfo>();

            public void Reinitialize()
            {
                logger.Info($"ENTER");
                capsKeyInfo.Reinitialize(FuncVKeys.CAPSLOCK);
                alnumKeyInfo.Reinitialize(FuncVKeys.EISU);
                nferKeyInfo.Reinitialize(FuncVKeys.MUHENKAN);
                xferKeyInfo.Reinitialize(FuncVKeys.HENKAN);
                capsKeyInfo.Behavior = ExModiferKeyInfo.ExModKeyBehavior.Default;
                alnumKeyInfo.Behavior = ExModiferKeyInfo.ExModKeyBehavior.Default;
                nferKeyInfo.Behavior = ExModiferKeyInfo.ExModKeyBehavior.Default;
                xferKeyInfo.Behavior = ExModiferKeyInfo.ExModKeyBehavior.Default;
                otherKeyState.Reinitialize(0);
                holdShiftKeyInfos = new Dictionary<uint, ExModiferKeyInfo>();
            }

            private void refreshSpecialCommonTableHoldShiftBehavior()
            {
                foreach (var info in new[] { capsKeyInfo, alnumKeyInfo, nferKeyInfo, xferKeyInfo }) {
                    info.Behavior = Settings.GetHoldShiftKeySetting(info.Deckey) != null
                        ? ExModiferKeyInfo.ExModKeyBehavior.GenericHoldShift
                        : ExModiferKeyInfo.ExModKeyBehavior.Default;
                }
            }

            private void refreshHoldShiftKeyInfos()
            {
                refreshSpecialCommonTableHoldShiftBehavior();
                var newInfos = new Dictionary<uint, ExModiferKeyInfo>();
                foreach (var pair in Settings.HoldShiftKeySettings) {
                    int deckey = pair.Key;
                    if (deckey != DecoderKeys.STROKE_SPACE_DECKEY &&
                        (deckey < DecoderKeys.FUNC_DECKEY_START || deckey >= DecoderKeys.FUNC_DECKEY_END)) continue;
                    if (deckey == DecoderKeys.CAPS_DECKEY || deckey == DecoderKeys.ALNUM_DECKEY ||
                        deckey == DecoderKeys.NFER_DECKEY || deckey == DecoderKeys.XFER_DECKEY) {
                        continue;
                    }

                    uint vkey = DecoderKeyVsVKey.GetVKeyFromDecKey(deckey);
                    if (vkey == 0) continue;

                    ExModiferKeyInfo info = holdShiftKeyInfos._safeGet(vkey);
                    if (info == null) {
                        info = new ExModiferKeyInfo() {
                            Vkey = vkey,
                            Deckey = deckey,
                            Name = deckey == DecoderKeys.STROKE_SPACE_DECKEY ? "Space" : $"HoldShift({deckey})",
                            Behavior = ExModiferKeyInfo.ExModKeyBehavior.GenericHoldShift
                        };
                    } else {
                        info.Vkey = vkey;
                    }
                    info.Deckey = deckey;
                    info.Behavior = ExModiferKeyInfo.ExModKeyBehavior.GenericHoldShift;
                    newInfos[vkey] = info;
                }
                holdShiftKeyInfos = newInfos;
            }

            /// <summary> 拡張修飾キーからキー状態を得る</summary>
            public ExModiferKeyInfo getModiferKeyInfoByVkey(uint vkey)
            {
                if (Settings.LoggingDecKeyInfo) { logger.Info($"CALLED: vkey={vkey}, nfer.Vkey={nferKeyInfo.Vkey}, xfer.Vkey={xferKeyInfo.Vkey}"); }
                refreshHoldShiftKeyInfos();

                if (vkey == capsKeyInfo.Vkey) return capsKeyInfo;
                if (vkey == alnumKeyInfo.Vkey) return alnumKeyInfo;
                if (vkey == nferKeyInfo.Vkey) return nferKeyInfo;
                if (vkey == xferKeyInfo.Vkey) return xferKeyInfo;
                var holdShiftInfo = holdShiftKeyInfos._safeGet(vkey);
                if (holdShiftInfo != null) return holdShiftInfo;

                if (Settings.LoggingDecKeyInfo) { logger.Info($"LEAVE: no result"); }
                return null;
            }

            /// <summary> 拡張修飾キーの修飾フラグを得る</summary>
            public uint getModFlagForExModVkey(uint vkey)
            {
                if (Settings.LoggingDecKeyInfo) { logger.Info($"CALLED: vkey={vkey}, MUHENKAN={FuncVKeys.MUHENKAN}, HENKAN={FuncVKeys.HENKAN}"); }

                if (vkey == FuncVKeys.CAPSLOCK) return KeyModifiers.MOD_CAPS;
                if (vkey == FuncVKeys.EISU) return KeyModifiers.MOD_ALNUM;
                if (vkey == FuncVKeys.MUHENKAN) return KeyModifiers.MOD_NFER;
                if (vkey == FuncVKeys.HENKAN) return KeyModifiers.MOD_XFER;
                return 0;
            }

            /// <summary> 拡張修飾キーの修飾フラグからキー状態を得る</summary>
            public ExModiferKeyInfo getModiferKeyInfoByModFlag(uint modFlag)
            {
                if (modFlag == KeyModifiers.MOD_CAPS) return capsKeyInfo;
                if (modFlag == KeyModifiers.MOD_ALNUM) return alnumKeyInfo;
                if (modFlag == KeyModifiers.MOD_NFER) return nferKeyInfo;
                if (modFlag == KeyModifiers.MOD_XFER) return xferKeyInfo;
                return otherKeyState;
            }

            /// <summary> SHIFT状態にある拡張修飾キーの修飾フラグを得る</summary>
            public uint getShiftedExModKey()
            {
                refreshHoldShiftKeyInfos();
                if (capsKeyInfo.Shifted) return KeyModifiers.MOD_CAPS;
                if (alnumKeyInfo.Shifted) return KeyModifiers.MOD_ALNUM;
                if (nferKeyInfo.Shifted) return KeyModifiers.MOD_NFER;
                if (xferKeyInfo.Shifted) return KeyModifiers.MOD_XFER;
                return 0;
            }

            /// <summary> 拡張修飾キーの押下またシフト状態を得る</summary>
            public uint getPressedOrShiftedExModFlag()
            {
                refreshHoldShiftKeyInfos();
                if (capsKeyInfo.Pressed || capsKeyInfo.Shifted) return KeyModifiers.MOD_CAPS;
                if (alnumKeyInfo.Pressed || alnumKeyInfo.Shifted) return KeyModifiers.MOD_ALNUM;
                if (nferKeyInfo.Pressed || nferKeyInfo.Shifted) return KeyModifiers.MOD_NFER;
                if (xferKeyInfo.Pressed || xferKeyInfo.Shifted) return KeyModifiers.MOD_XFER;
                return 0;
            }

            /// <summary>すでに押下状態にある拡張修飾キーをSHIFT状態に遷移させる</summary>
            public void makeExModKeyShifted(bool bDecoderOn)
            {
                refreshHoldShiftKeyInfos();
                bool shouldShiftForCommonHoldShift(ExModiferKeyInfo info)
                {
                    return info.IsGenericHoldShift && ShiftPlane.IsHoldShiftPlaneAssigned(info.Deckey, bDecoderOn);
                }

                if (capsKeyInfo.Pressed && (capsKeyInfo.IsShiftPlaneAssigned(bDecoderOn) || shouldShiftForCommonHoldShift(capsKeyInfo))) capsKeyInfo.SetShifted();
                if (alnumKeyInfo.Pressed && (alnumKeyInfo.IsShiftPlaneAssigned(bDecoderOn) || shouldShiftForCommonHoldShift(alnumKeyInfo))) alnumKeyInfo.SetShifted();
                if (nferKeyInfo.Pressed && (nferKeyInfo.IsShiftPlaneAssigned(bDecoderOn) || shouldShiftForCommonHoldShift(nferKeyInfo))) nferKeyInfo.SetShifted();
                if (xferKeyInfo.Pressed && (xferKeyInfo.IsShiftPlaneAssigned(bDecoderOn) || shouldShiftForCommonHoldShift(xferKeyInfo))) xferKeyInfo.SetShifted();
                foreach (var info in holdShiftKeyInfos.Values.Where(x => x.Pressed).OrderBy(x => x.PressedSerial)) {
                    info.SetShifted();
                }
            }

            public int getShiftPlane(bool bDecoderOn)
            {
                refreshHoldShiftKeyInfos();
                var holdShiftInfo = getEffectiveHoldShiftKeyInfo(bDecoderOn);
                if (holdShiftInfo != null) {
                    int holdShiftPlane = ShiftPlane.GetHoldShiftPlane(holdShiftInfo.Deckey, bDecoderOn);
                    if (holdShiftPlane != ShiftPlane.ShiftPlane_NONE) return holdShiftPlane;
                }
                if (capsKeyInfo.Shifted) return ShiftPlane.GetShiftPlaneFromShiftModFlag(KeyModifiers.MOD_CAPS, bDecoderOn);
                if (alnumKeyInfo.Shifted) return ShiftPlane.GetShiftPlaneFromShiftModFlag(KeyModifiers.MOD_ALNUM, bDecoderOn);
                if (nferKeyInfo.Shifted) return ShiftPlane.GetShiftPlaneFromShiftModFlag(KeyModifiers.MOD_NFER, bDecoderOn);
                if (xferKeyInfo.Shifted) return ShiftPlane.GetShiftPlaneFromShiftModFlag(KeyModifiers.MOD_XFER, bDecoderOn);
                return ShiftPlane.ShiftPlane_NONE;
            }

            public bool isGenericHoldShiftShifted()
            {
                refreshHoldShiftKeyInfos();
                return holdShiftKeyInfos.Values.Any(x => x.Shifted);
            }

            public bool isGenericHoldShiftPressedOrShifted()
            {
                refreshHoldShiftKeyInfos();
                return holdShiftKeyInfos.Values.Any(x => x.Pressed || x.Shifted);
            }

            public ExModiferKeyInfo getEffectiveHoldShiftKeyInfo(bool bDecoderOn)
            {
                refreshHoldShiftKeyInfos();
                ExModiferKeyInfo effectiveInfo = null;
                foreach (var info in holdShiftKeyInfos.Values) {
                    if (info.Shifted && ShiftPlane.IsHoldShiftPlaneAssigned(info.Deckey, bDecoderOn)) {
                        if (effectiveInfo == null || info.ShiftedSerial > effectiveInfo.ShiftedSerial) {
                            effectiveInfo = info;
                        }
                    }
                }
                return effectiveInfo;
            }

            public ExModiferKeyInfo getEffectiveCommonTableHoldShiftKeyInfo(bool bDecoderOn)
            {
                refreshHoldShiftKeyInfos();
                ExModiferKeyInfo effectiveInfo = getEffectiveHoldShiftKeyInfo(bDecoderOn);
                foreach (var info in new[] { capsKeyInfo, alnumKeyInfo, nferKeyInfo, xferKeyInfo }) {
                    var setting = Settings.GetHoldShiftKeySetting(info.Deckey);
                    if (setting == null || !info.Shifted || !ShiftPlane.IsHoldShiftPlaneAssigned(info.Deckey, bDecoderOn)) continue;
                    if (effectiveInfo == null || info.ShiftedSerial > effectiveInfo.ShiftedSerial) {
                        effectiveInfo = info;
                    }
                }
                return effectiveInfo;
            }

            public bool hasMultipleCommonTableHoldShiftsShifted(bool bDecoderOn)
            {
                refreshHoldShiftKeyInfos();
                int count = holdShiftKeyInfos.Values.Count(x => x.Shifted && ShiftPlane.IsHoldShiftPlaneAssigned(x.Deckey, bDecoderOn));
                foreach (var info in new[] { capsKeyInfo, alnumKeyInfo, nferKeyInfo, xferKeyInfo }) {
                    var setting = Settings.GetHoldShiftKeySetting(info.Deckey);
                    if (setting != null && info.Shifted && ShiftPlane.IsHoldShiftPlaneAssigned(info.Deckey, bDecoderOn)) {
                        ++count;
                    }
                }
                return count >= 2;
            }

            public ExModiferKeyInfo getEffectiveHoldShiftKeyInfoForDisplay(bool bDecoderOn)
            {
                refreshHoldShiftKeyInfos();
                ExModiferKeyInfo effectiveInfo = null;
                foreach (var info in holdShiftKeyInfos.Values) {
                    var setting = Settings.GetHoldShiftKeySetting(info.Deckey);
                    if (info.Shifted && (ShiftPlane.IsHoldShiftPlaneAssigned(info.Deckey, bDecoderOn) || setting?.ShowStrokeHelp == true)) {
                        if (effectiveInfo == null || info.ShiftedSerial > effectiveInfo.ShiftedSerial) {
                            effectiveInfo = info;
                        }
                    }
                }
                return effectiveInfo;
            }

            public string modifiersStateStr()
            {
                return Logger.IsInfoEnabled
                    ? $"capsKeyState={capsKeyInfo.KeyState}"
                    + $"\nalnumKeyState={alnumKeyInfo.KeyState}"
                    + $"\nnferKeyState={nferKeyInfo.KeyState}"
                    + $"\nxferKeyState={xferKeyInfo.KeyState}"
                    + $"\nholdShiftKeys={holdShiftKeyInfos.Values.Select(x => $"{x.Name}:{x.KeyState}")._join(",")}\n"
                    : "";
            }

        }

        /// <summary> 拡張修飾キーの押下状態の管理オブジェクト</summary>
        ExModiferKeyInfoManager keyInfoManager = null;

        private void sendOriginalVkey(uint vkey)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Send original vkey={vkey:x}H({vkey})");
            SendInputHandler.Singleton?.SendVKeyCombo(0, vkey, 1);
        }

        class ShiftKeyRuntimeState
        {
            public uint Vkey;
            public int Deckey;
            public uint ModFlag;
            public DateTime PressedAt = DateTime.MinValue;
            public bool IsPendingNormalKey;
            public bool IsCombined;
            public bool HasOverlap;
            public bool ShouldFallbackToSystemShift;
            public bool DispatchedToDeterminer;
            public List<uint> FallbackVkeys = new List<uint>();

            public void Reset()
            {
                PressedAt = DateTime.MinValue;
                IsPendingNormalKey = false;
                IsCombined = false;
                HasOverlap = false;
                ShouldFallbackToSystemShift = false;
                DispatchedToDeterminer = false;
                FallbackVkeys.Clear();
            }
        }

        private readonly ShiftKeyRuntimeState leftShiftState = new ShiftKeyRuntimeState() {
            Vkey = FuncVKeys.LSHIFT,
            Deckey = DecoderKeys.LEFT_SHIFT_DECKEY,
            ModFlag = KeyModifiers.MOD_LSHIFT,
        };

        private readonly ShiftKeyRuntimeState rightShiftState = new ShiftKeyRuntimeState() {
            Vkey = FuncVKeys.RSHIFT,
            Deckey = DecoderKeys.RIGHT_SHIFT_DECKEY,
            ModFlag = KeyModifiers.MOD_RSHIFT,
        };

        class PendingCommonSingleHitState
        {
            public int Deckey;
            public bool Consumed;
            public bool DispatchedToDeterminer;
            public bool SwallowedKeyDown;
        }

        private readonly HashSet<uint> activeNonShiftVkeys = new HashSet<uint>();
        private readonly HashSet<uint> consumedHoldShiftTargetVkeys = new HashSet<uint>();
        private readonly Dictionary<uint, int> holdShiftTargetDeterminerDeckeys = new Dictionary<uint, int>();
        private readonly Dictionary<uint, PendingCommonSingleHitState> pendingCommonSingleHitStates = new Dictionary<uint, PendingCommonSingleHitState>();

        private ShiftKeyRuntimeState getShiftState(uint vkey)
        {
            return vkey == FuncVKeys.LSHIFT ? leftShiftState :
                vkey == FuncVKeys.RSHIFT ? rightShiftState : null;
        }

        private bool isSystemModifierDeckey(int deckey)
        {
            return deckey == DecoderKeys.LEFT_CONTROL_DECKEY ||
                deckey == DecoderKeys.RIGHT_CONTROL_DECKEY ||
                deckey == DecoderKeys.LEFT_SHIFT_DECKEY ||
                deckey == DecoderKeys.RIGHT_SHIFT_DECKEY ||
                deckey == DecoderKeys.LEFT_ALT_DECKEY ||
                deckey == DecoderKeys.RIGHT_ALT_DECKEY ||
                deckey == DecoderKeys.LEFT_WIN_DECKEY ||
                deckey == DecoderKeys.RIGHT_WIN_DECKEY;
        }

        private bool isSystemModifierVkey(uint vkey)
        {
            int deckey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
            return isSystemModifierDeckey(deckey);
        }

        private bool isSystemShiftDeckey(int deckey)
        {
            return deckey == DecoderKeys.LEFT_SHIFT_DECKEY || deckey == DecoderKeys.RIGHT_SHIFT_DECKEY;
        }

        private void consumePendingCommonSingleHits(uint currentVkey)
        {
            foreach (var pair in pendingCommonSingleHitStates.Where(x => x.Key != currentVkey)) {
                pair.Value.Consumed = true;
            }
        }

        private void setPendingCommonSingleHit(uint vkey, int deckey)
        {
            pendingCommonSingleHitStates[vkey] = new PendingCommonSingleHitState() {
                Deckey = deckey,
                Consumed = false,
                DispatchedToDeterminer = false,
                SwallowedKeyDown = false,
            };
        }

        private PendingCommonSingleHitState popPendingCommonSingleHit(uint vkey)
        {
            var state = pendingCommonSingleHitStates._safeGet(vkey);
            if (state != null) pendingCommonSingleHitStates.Remove(vkey);
            return state;
        }

        private bool invokeResolvedAction(InputActionResolution action, int origDecKey, bool bDecoderOn)
        {
            if (action == null) return false;
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Invoke resolved action: source={action.SourceKind}, kind={action.ActionKind}, deckey={action.ResolvedDeckey}, requiresDecoder={action.RequiresDecoder}");
            return invokeHandler(action.ResolvedDeckey, origDecKey, 0, false, !bDecoderOn && action.RequiresDecoder);
        }

        private bool tryInvokeCommonSingleHitByDeckey(int deckey, bool bDecoderOn)
        {
            return InputActionResolver.TryResolveSingleHit(deckey, bDecoderOn, out var action) && invokeResolvedAction(action, deckey, bDecoderOn);
        }

        private bool tryBeginPendingCommonSingleHit(uint vkey, int deckey, ExModiferKeyInfo keyInfo, bool bCtrl, bool bShift, bool bAlt, bool bWin, uint modPressedOrShifted, bool bDecoderOn)
        {
            if (!InputActionResolver.HasSingleHitAction(deckey)) return false;
            if (bCtrl || bShift || bAlt || bWin) return false;
            if (modPressedOrShifted != 0 || keyInfoManager.isGenericHoldShiftPressedOrShifted()) return false;
            if (pendingCommonSingleHitStates.ContainsKey(vkey)) return true;
            if (keyInfo != null && (keyInfo.IsHoldShift || keyInfo.IsShiftPlaneAssigned(bDecoderOn))) return false;

            setPendingCommonSingleHit(vkey, deckey);
            return true;
        }

        private KeyCombination getComboForCommonHoldShift(int holdShiftDeckey, int targetDeckey, bool bDecoderOn)
        {
            var dt = HRDateTime.Now;
            var strokes = new[] {
                new Stroke(holdShiftDeckey, bDecoderOn, dt),
                new Stroke(targetDeckey, bDecoderOn, dt)
            };
            return KeyCombinationPool._GetEntry(strokes);
        }

        private void markPendingCommonSingleHitConsumed(uint vkey)
        {
            var pendingState = pendingCommonSingleHitStates._safeGet(vkey);
            if (pendingState != null) pendingState.Consumed = true;
        }

        private bool dispatchSystemModifierToDeterminer(uint vkey, int deckey, bool bDecoderOn)
        {
            ++keyDownCount;
            CombinationKeyStroke.Determiner.Singleton.KeyDown(
                deckey,
                bDecoderOn,
                keyDownCount,
                (decKeys) => handleComboKeyRepeat(vkey, decKeys));

            var pendingState = pendingCommonSingleHitStates._safeGet(vkey);
            if (pendingState != null) {
                pendingState.Consumed = true;
                pendingState.DispatchedToDeterminer = true;
                pendingState.SwallowedKeyDown = true;
            }
            return true;
        }

        private bool handleSystemModifierDown(uint vkey, bool bDecoderOn)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"ENTER: vkey={vkey:x}H, bDocderOn={bDecoderOn}");

            int deckey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
            if (!isSystemModifierDeckey(deckey)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: NOT a SystemModifier: vkey={vkey:x}H, deckey={deckey}");
                return false;
            }

            setPendingCommonSingleHit(vkey, deckey);

            var determiner = CombinationKeyStroke.Determiner.Singleton;
            if (CombinationKeyStroke.DeterminerLib.KeyCombinationPool._Enabled &&
                determiner.HasCombinationWithCurrentStrokes(deckey, bDecoderOn)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"SystemModifier combo dispatch: vkey={vkey:x}H, deckey={deckey}");
                return dispatchSystemModifierToDeterminer(vkey, deckey, bDecoderOn);
            }

            if (CombinationKeyStroke.DeterminerLib.KeyCombinationPool.IsSpaceOrFuncComboShift(deckey)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"SystemModifier space/func combo shift dispatch: vkey={vkey:x}H, deckey={deckey}");
                return dispatchSystemModifierToDeterminer(vkey, deckey, bDecoderOn);
            }

            var keyInfo = keyInfoManager.getModiferKeyInfoByVkey(vkey);
            if (keyInfo != null) {
                bool wasShifted = keyInfo.Shifted;
                keyInfo.SetShifted();
                if (wasShifted) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"CALL updateStrokeHelpHoldShiftPlane(bDecoderOn={bDecoderOn})");
                    updateStrokeHelpHoldShiftPlane(bDecoderOn);
                }
            }
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: SystemModifier HoldShift ON and pass-through: vkey={vkey:x}H, deckey={deckey}");
            return false;
        }

        private bool handleSystemModifierUp(uint vkey, bool bDecoderOn, bool leftCtrl, bool rightCtrl)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"ENTER: vkey={vkey:x}H, bDocderOn={bDecoderOn}");

            int deckey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
            if (!isSystemModifierDeckey(deckey)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: NOT a SystemModifier: vkey={vkey:x}H, deckey={deckey}");
                return false;
            }

            var pendingState = popPendingCommonSingleHit(vkey);
            var keyInfo = keyInfoManager.getModiferKeyInfoByVkey(vkey);
            bool wasActiveHoldShift = keyInfo != null && (keyInfo.Pressed || keyInfo.Shifted || keyInfo.Repeated);
            if (keyInfo != null) {
                keyInfo.SetReleased();
                if (wasActiveHoldShift) keyInfo.PrevUpDt = HRDateTime.Now;
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"CALL updateStrokeHelpHoldShiftPlane(bDecoderOn={bDecoderOn})");
                updateStrokeHelpHoldShiftPlane(bDecoderOn);
            }

            if (vkey == FuncVKeys.LCONTROL) bLCtrlShifted = false;
            if (vkey == FuncVKeys.RCONTROL) bRCtrlShifted = false;

            if (pendingState?.DispatchedToDeterminer == true) {
                keyboardUpHandler(bDecoderOn, vkey, leftCtrl, rightCtrl, 0);
                return true;
            }

            if (pendingState != null && !pendingState.Consumed) {
                bool invoked = tryInvokeCommonSingleHitByDeckey(pendingState.Deckey, bDecoderOn);
                if (pendingState.SwallowedKeyDown) return invoked;
            }

            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"SystemModifier up pass-through: vkey={vkey:x}H, deckey={deckey}, consumed={pendingState?.Consumed}");
            return false;
        }

        private bool? handleActiveHoldShiftBeforeNormalKey(uint vkey, bool bDecoderOn)
        {
            if (isSystemModifierVkey(vkey)) return null;
            if (keyInfoManager.hasMultipleCommonTableHoldShiftsShifted(bDecoderOn)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Multiple HoldShift shifted: ignore HoldShift precedence for vkey={vkey:x}H");
                return null;
            }

            var activeHoldShiftInfo = keyInfoManager.getEffectiveCommonTableHoldShiftKeyInfo(bDecoderOn);
            if (activeHoldShiftInfo == null) return null;
            if (activeHoldShiftInfo.Vkey == vkey) return null;
            if (!InputActionResolver.HasCommonTableHoldShiftDefinition(activeHoldShiftInfo.Deckey)) return null;

            int normalDecKey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
            if (normalDecKey < 0) return null;

            var combo = getComboForCommonHoldShift(activeHoldShiftInfo.Deckey, normalDecKey, bDecoderOn);
            if (combo != null && combo.IsTerminal && !combo.IsSubKey && combo.DecKeyList._notEmpty()) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"HoldShift combo action: hold={activeHoldShiftInfo.Deckey}, key={normalDecKey}, combo={combo.DecKeysDebugString()}");
                markPendingCommonSingleHitConsumed(activeHoldShiftInfo.Vkey);
                consumedHoldShiftTargetVkeys.Add(vkey);
                return invokeHandlerForKeyList(combo.DecKeyList._toSingleHitResultKeyStrokeList(), false);
            }

            if ((combo == null || combo.IsTerminal) && InputActionResolver.TryResolveHoldShift(activeHoldShiftInfo.Deckey, normalDecKey, bDecoderOn, out var commonAction)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"CommonTable HoldShift action: hold={activeHoldShiftInfo.Deckey}, key={normalDecKey}, action={commonAction.ResolvedDeckey}, source={commonAction.SourceKind}");
                markPendingCommonSingleHitConsumed(activeHoldShiftInfo.Vkey);
                consumedHoldShiftTargetVkeys.Add(vkey);
                return invokeResolvedAction(commonAction, normalDecKey, bDecoderOn);
            }

            if (isSystemShiftDeckey(activeHoldShiftInfo.Deckey) && normalDecKey >= 0 && normalDecKey < DecoderKeys.NORMAL_DECKEY_NUM) {
                int shiftedDeckey = normalDecKey + ShiftPlane.ShiftPlane_SHIFT * DecoderKeys.PLANE_DECKEY_NUM;
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"System Shift HoldShift fallback to shifted deckey: hold={activeHoldShiftInfo.Deckey}, key={normalDecKey}, shifted={shiftedDeckey}");
                markPendingCommonSingleHitConsumed(activeHoldShiftInfo.Vkey);
                holdShiftTargetDeterminerDeckeys[vkey] = shiftedDeckey;
                ++keyDownCount;
                CombinationKeyStroke.Determiner.Singleton.KeyDown(
                    shiftedDeckey,
                    bDecoderOn,
                    keyDownCount,
                    (decKeys) => handleComboKeyRepeat(vkey, decKeys));
                return true;
            }

            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"HoldShift undefined combo pass-through: hold={activeHoldShiftInfo.Deckey}, key={normalDecKey}");
            markPendingCommonSingleHitConsumed(activeHoldShiftInfo.Vkey);
            return isSystemModifierDeckey(activeHoldShiftInfo.Deckey) ? (bool?)null : false;
        }

        private bool isShiftVkey(uint vkey)
        {
            return vkey == FuncVKeys.LSHIFT || vkey == FuncVKeys.RSHIFT;
        }

        private bool isMyInjectedEvent(int extraInfo)
        {
            return extraInfo == SendInputHandler.MyMagicNumber;
        }

        private bool isShiftSingleHitEffective(uint vkey, bool bCtrl)
        {
            int deckey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
            return (Settings.ActiveKey == vkey && (!bCtrl || Settings.ActiveKeyWithCtrl != vkey)) ||
                ExtraModifiers.IsExModKeyIndexAssignedForDecoderFunc(deckey);
        }

        private bool isShiftPlaneAssigned(uint modFlag, bool bDecoderOn)
        {
            return ShiftPlane.IsShiftPlaneAssignedForShiftModFlag(modFlag, bDecoderOn);
        }

        private bool isShiftPendingAsNormal(uint vkey)
        {
            return getShiftState(vkey)?.IsPendingNormalKey == true;
        }

        private bool effectiveShiftPressed()
        {
            bool leftPressed = (GetAsyncKeyState(FuncVKeys.LSHIFT) & 0x8000) != 0 && !leftShiftState.IsPendingNormalKey;
            bool rightPressed = (GetAsyncKeyState(FuncVKeys.RSHIFT) & 0x8000) != 0 && !rightShiftState.IsPendingNormalKey;
            return leftPressed || rightPressed;
        }

        private bool hasShiftComboDefinition(int shiftDeckey, int deckey, bool bDecoderOn)
        {
            var dt = HRDateTime.Now;
            var shiftStroke = new Stroke(shiftDeckey, bDecoderOn, dt);
            var stroke = new Stroke(deckey, bDecoderOn, dt);
            return KeyCombinationPool._GetEntry(new[] { shiftStroke, stroke }) != null ||
                KeyCombinationPool._GetEntry(new[] { stroke, shiftStroke }) != null;
        }

        private void dispatchPendingShiftToDeterminer(ShiftKeyRuntimeState shiftState, bool bDecoderOn)
        {
            if (shiftState == null || shiftState.DispatchedToDeterminer) return;
            if (!KeyCombinationPool._Enabled || KeyCombinationPool._GetEntry(shiftState.Deckey) == null) return;

            ++keyDownCount;
            CombinationKeyStroke.Determiner.Singleton.KeyDown(
                shiftState.Deckey,
                bDecoderOn,
                keyDownCount,
                (decKeys) => handleComboKeyRepeat(shiftState.Vkey, decKeys));
            shiftState.DispatchedToDeterminer = true;
        }

        private bool handleShiftDownAsNormalCandidate(uint vkey, bool bCtrl)
        {
            var shiftState = getShiftState(vkey);
            if (shiftState == null) return false;

            bool bDecoderOn = isDecoderActivated();
            bool comboPossible = KeyCombinationPool._Enabled && KeyCombinationPool._GetEntry(shiftState.Deckey) != null;
            bool singlePossible = isShiftSingleHitEffective(vkey, bCtrl);
            if (!comboPossible && !singlePossible) return false;

            var activeDeckeys = activeNonShiftVkeys
                .Select(x => DecoderKeyVsVKey.GetDecKeyFromVKey(x))
                .Where(x => x >= 0)
                .ToList();
            bool hasActiveOverlap = activeDeckeys.Any();
            bool hasComboWithActiveKey = hasActiveOverlap &&
                activeDeckeys.Any(x => hasShiftComboDefinition(shiftState.Deckey, x, bDecoderOn));

            // 他キーが先に押されていて同時打鍵定義が無い場合は、
            // Shift を通常の修飾キーとしてシステム側に処理させる。
            if (hasActiveOverlap && !hasComboWithActiveKey) return false;

            shiftState.Reset();
            shiftState.IsPendingNormalKey = true;
            shiftState.PressedAt = HRDateTime.Now;
            shiftState.HasOverlap = hasActiveOverlap;
            if (hasActiveOverlap) {
                shiftState.IsCombined = hasComboWithActiveKey;
                if (shiftState.IsCombined) dispatchPendingShiftToDeterminer(shiftState, bDecoderOn);
            }
            return true;
        }

        private bool shouldSuppressForPendingShiftFallback(uint vkey)
        {
            return leftShiftState.IsPendingNormalKey && leftShiftState.ShouldFallbackToSystemShift && leftShiftState.FallbackVkeys.Contains(vkey) ||
                rightShiftState.IsPendingNormalKey && rightShiftState.ShouldFallbackToSystemShift && rightShiftState.FallbackVkeys.Contains(vkey);
        }

        private bool handlePendingShiftBeforeNormalKey(uint vkey)
        {
            bool bDecoderOn = isDecoderActivated();
            int deckey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
            if (deckey < 0) return false;

            bool suppress = false;
            foreach (var shiftState in new[] { leftShiftState, rightShiftState }.Where(x => x.IsPendingNormalKey)) {
                if (shiftState.IsCombined) continue;
                shiftState.HasOverlap = true;
                if (hasShiftComboDefinition(shiftState.Deckey, deckey, bDecoderOn)) {
                    shiftState.IsCombined = true;
                    dispatchPendingShiftToDeterminer(shiftState, bDecoderOn);
                } else {
                    shiftState.ShouldFallbackToSystemShift = true;
                    if (!shiftState.FallbackVkeys.Contains(vkey)) shiftState.FallbackVkeys.Add(vkey);
                    suppress = true;
                }
            }
            return suppress;
        }

        private void replayPendingShiftFallback(ShiftKeyRuntimeState shiftState, bool leftCtrl, bool rightCtrl)
        {
            if (shiftState?.FallbackVkeys._isEmpty() != false) return;
            if (isDecoderActivated()) {
                foreach (var fallbackVkey in shiftState.FallbackVkeys) {
                    keyboardDownHandler(fallbackVkey, leftCtrl, rightCtrl, true);
                    keyboardUpHandler(true, fallbackVkey, leftCtrl, rightCtrl, 0);
                }
            } else {
                SendInputHandler.Singleton?.SendSpecificShiftModifiedVKeys(shiftState.Vkey, shiftState.FallbackVkeys);
            }
        }

        private bool invokeSingleShiftTap(uint vkey, bool leftCtrl, bool rightCtrl)
        {
            int normalDecKey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
            if (normalDecKey < 0) return false;

            if (InputActionResolver.TryResolveComboKey(0, normalDecKey, out var action)) {
                return invokeResolvedAction(action, normalDecKey, isDecoderActivated());
            }
            return false;
        }

        /// <summary> extraInfo=0 の時のキー押下時のリザルトフラグ </summary>
        //private bool normalInfoKeyDownResult = false;

        private const int KEY_REPEAT_INTERVAL = 500;

        /// <summary> 同時打鍵キーのリピート中か(仮想鍵盤に表示する打鍵ガイドを切り替えるのに使う) </summary>
        private bool bComboKeyRepeat = false;
        private uint prevComboVkey = 0;

        /// <summary>
        /// 同時打鍵キーのオートリピートが開始されたら打鍵ガイドを切り替える
        /// </summary>
        /// <param name="vkey">同時打鍵キーの仮想キーコード</param>
        /// <param name="decKey">同時打鍵キーのデコーダ用コード</param>
        void handleComboKeyRepeat(uint vkey, List<int> decKeys)
        {
            if (Settings.LoggingDecKeyInfo) {
                logger.Info(() => $"CALLED: vkey={vkey}, prevComboVkey={prevComboVkey}, bComboKeyRepeat={bComboKeyRepeat}");
            }
            if (prevComboVkey == vkey) {
                // KeyRepeat
                if (!bComboKeyRepeat) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"SetNextStrokeHelpDecKey({decKeys._keyString()})");
                    frmKanchoku?.SetNextStrokeHelpDecKey(decKeys);
                    bComboKeyRepeat = true;
                }
            } else {
                prevComboVkey = vkey;
            }
        }

        /// <summary>
        /// 同時打鍵キーのオートリピートが終了したら打鍵ガイドを元に戻す
        /// </summary>
        /// <param name="vkey"></param>
        /// <returns>同時打鍵キーのリピート停止をハンドルした場合は true を返す</returns>
        bool handleComboKeyRepeatStop(uint vkey)
        {
            if (Settings.LoggingDecKeyInfo) {
                logger.Info(() => $"CALLED: vkey={vkey}, prevComboVkey={prevComboVkey}, bComboKeyRepeat={bComboKeyRepeat}");
            }
            bool result = false;
            if (prevComboVkey == vkey) {
                if (bComboKeyRepeat) {
                    result = true;
                    bComboKeyRepeat = false;
                    if (Settings.LoggingDecKeyInfo) logger.Info("SetNextStrokeHelpDecKey(null");
                    frmKanchoku?.SetNextStrokeHelpDecKey(null);
                }
                prevComboVkey = 0;
            }
            return result;
        }

        bool bLCtrlShifted = false;
        bool bRCtrlShifted = false;

        bool bWinKeyOn = false ;

        /// <summary>キーボード押下時のハンドラ</summary>
        /// <param name="vkey"></param>
        /// <param name="extraInfo"></param>
        /// <returns>キー入力を破棄する場合は true を返す。flase を返すとシステム側でキー入力処理が行われる</returns>
        private bool onKeyboardDownHandler(uint vkey, int scanCode, uint flags, int extraInfo)
        {
            appendKeyInfo(true, vkey, scanCode, flags, extraInfo);

            // Pauseで一時停止?
            if (Settings.SuspendByPauseKey && vkey == (uint)Keys.Pause) {
                frmKanchoku?.DecoderSuspendToggle();
                return true;
            }
            // 一時停止?
            if (Settings.DecoderSuspended) return false;

            if (Settings.LoggingDecKeyInfo) {
                logger.Info(() =>
                    $"\nENTER: IsVkbTopTextFocused={isVkbTopTextFocused()}, vkey={vkey:x}H({vkey}), scanCode={scanCode:x}H, extraInfo={extraInfo}\n" +
                    keyInfoManager.modifiersStateStr());
            }

            if (vkey == (uint)Keys.LWin || vkey == (uint)Keys.RWin) {
                // LWIN or RWIN
                bWinKeyOn = true;
                if (Settings.LoggingDecKeyInfo) logger.Info("WinKey On");
            }

            if (bWinKeyOn) {
                if (Settings.LoggingDecKeyInfo) logger.Info($"WinKey On: continue SystemModifier/HoldShift path for {vkey:x}");
            }

            if (DecoderKeyVsVKey.IsUSonJPmode || DecoderKeyVsVKey.IsEisuDisabled) {
                // EISU disabled
                // 英数キーはCapsに変換
                if (vkey == VKEY_EISU) vkey = (uint)Keys.Capital;
            }
            if (DecoderKeyVsVKey.IsUSonJPmode) {
                // US-on-JP Mode
                // 漢字キー(0xf4)は 半/全(0xf3) に寄せる
                if (vkey == VKEY_KANJI) vkey = VKEY_HANZEN;
            } else {
                // JP or US Mode
                // 半/全キーは、false(システム側による処理) を返す
                if (vkey == VKEY_HANZEN || vkey == VKEY_KANJI) return false;
            }

            // キー入力を破棄する場合は true を返す。flase を返すとシステム側でキー入力処理が行われる
            bool handleKeyDown()
            {
                if (isMyInjectedEvent(extraInfo)) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Ignore self injected key down: vkey={vkey:x}H");
                    return false;
                }

                bool leftShift = (GetAsyncKeyState(FuncVKeys.LSHIFT) & 0x8000) != 0;
                bool leftCtrl = (GetAsyncKeyState(FuncVKeys.LCONTROL) & 0x8000) != 0;
                bool rightCtrl = (GetAsyncKeyState(FuncVKeys.RCONTROL) & 0x8000) != 0;
                bool bCtrl = leftCtrl || rightCtrl;
                bool bDecoderOn = isDecoderActivated();

                // とりあえず、やっつけコード
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"vkey={vkey:x}H({vkey}), leftCtrl={leftCtrl}, rightCtrl={rightCtrl}, leftShift={leftShift}");
                if (extraInfo == 0 && leftCtrl) bLCtrlShifted = true;    // 左CtrlがONのときに何かキーが押されたら左Ctrlをシフト状態にする
                if (extraInfo == 0 && rightCtrl) bRCtrlShifted = true;   // 右CtrlがONのときに何かキーが押されたら右Ctrlをシフト状態にする
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"bLCtrlShifted={bLCtrlShifted}, bRCtrlShifted={bRCtrlShifted}");

                if (extraInfo == 0 && isSystemModifierVkey(vkey)) {
                    return handleSystemModifierDown(vkey, bDecoderOn);
                }

                if (!isEffectiveVkey(vkey, scanCode, flags, extraInfo, bCtrl)) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"not EffectiveVkey{(extraInfo == 0 ? " and clear StrokeList" : "")}");
                    if (extraInfo == 0) CombinationKeyStroke.Determiner.Singleton.ClearUnprocList();     // 未処理の同時打鍵キューのクリア
                    return false;
                }

                var holdShiftResult = extraInfo == 0 ? handleActiveHoldShiftBeforeNormalKey(vkey, bDecoderOn) : null;
                if (holdShiftResult != null) {
                    return holdShiftResult.Value;
                }

                if (isShiftVkey(vkey) && extraInfo == 0) {
                    if (handleShiftDownAsNormalCandidate(vkey, bCtrl)) {
                        if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Shift pending as normal key: vkey={vkey:x}H");
                        return true;
                    }
                    if (vkey == FuncVKeys.LSHIFT) {
                        if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LSHIFT pass-through");
                        return false;
                    }
                }

                if (!isShiftVkey(vkey)) {
                    activeNonShiftVkeys.Add(vkey);
                    consumePendingCommonSingleHits(vkey);
                    if (handlePendingShiftBeforeNormalKey(vkey)) {
                        if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Pending shift fallback queued: vkey={vkey:x}H");
                        return true;
                    }
                }

                bool bShift = shiftKeyPressed(vkey);
                bool bAlt = isAltKeyPressed();
                bool bWin = isWinKeyPressed();
                uint modFlag = keyInfoManager.getModFlagForExModVkey(vkey);
                uint modPressedOrShifted = keyInfoManager.getPressedOrShiftedExModFlag();

                // この処理は、keyboardDownHandler() 内でやるようにした
                //if (!bDecoderOn && !bCtrl && modPressedOrShifted == 0 && vkey >= (int)Keys.Left && vkey <= (int)Keys.Down) {
                //    // デコーダOFFで無修飾の矢印キーなら、システムに任せる
                //    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Normal Arrow Key");
                //    return false;
                //}

                var keyInfo = keyInfoManager.getModiferKeyInfoByVkey(vkey);
                int currentDeckey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
                if (currentDeckey >= 0 && tryBeginPendingCommonSingleHit(vkey, currentDeckey, keyInfo, bCtrl, bShift, bAlt, bWin, modPressedOrShifted, bDecoderOn)) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"CommonTable singleHit pending: vkey={vkey:x}H, deckey={currentDeckey}");
                    return true;
                }
                if (keyInfo != null) {
                    // CapsLock/英数/Nfer/Xfer/Space
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"{keyInfo.Name}Key Pressed: ctrl={bCtrl}, shift={bShift}, decoderOn={bDecoderOn}, modFlag={modFlag:x}, modPressedOrShifted={modPressedOrShifted:x}");
                    if (keyInfo.IsHoldShift) {
                        if (Settings.LoggingDecKeyInfo) logger.Info(() => $"GenericHoldShift: keyState={keyInfo.KeyState}, ctrl={bCtrl}, shift={bShift}, alt={bAlt}, win={bWin}, modPressedOrShifted={modPressedOrShifted:x}");
                        if (bCtrl || bShift || bAlt || bWin) {
                            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"GenericHoldShift: modifier shortcut pass-through");
                            return false;
                        }
                        if (keyInfo.Shifted) {
                            return true;
                        }
                        if (keyInfo.Repeated) {
                            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"GenericHoldShift: repeated -> pass through");
                        } else if (keyInfo.Pressed) {
                            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"GenericHoldShift: prevUpDt={keyInfo.PrevUpDt}.{keyInfo.PrevUpDt:fff}");
                            if (HRDateTime.Now > keyInfo.PrevUpDt.AddMilliseconds(getSpaceHoldShiftRepeatMillisec())) {
                                keyInfo.SetShifted();
                                updateStrokeHelpHoldShiftPlane(bDecoderOn);
                                return true;
                            } else {
                                keyInfo.SetRepeated();
                            }
                        } else if (keyInfo.Released) {
                            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"GenericHoldShift: released prevUpDt={keyInfo.PrevUpDt}.{keyInfo.PrevUpDt:fff}");
                            if (keyInfo.PrevUpDt > DateTime.MinValue &&
                                HRDateTime.Now <= keyInfo.PrevUpDt.AddMilliseconds(getSpaceHoldShiftRepeatMillisec())) {
                                keyInfo.SetRepeated();
                            } else {
                                keyInfo.SetPressed();
                                return true;
                            }
                        }
                    } else {
                        // HoldShift 以外 (Caps/英数/Nfer/Xfer)
                        if (keyInfo.IsShiftPlaneAssigned(bDecoderOn)) {
                            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"ShiftPlaneAssigned: {keyInfo.Name}");
                            // 拡張シフト面が割り当てられている拡張修飾キーの場合
                            if (keyInfo.Pressed || modPressedOrShifted != 0) {
                                // 当拡張修飾キーが押下されている、またはその他の拡張修飾キーが押下orシフト状態なら、その他の拡張修飾キーを含めてシフト状態に遷移する
                                keyInfo.SetShifted();
                                keyInfoManager.makeExModKeyShifted(bDecoderOn);
                            } else if (keyInfo.Released) {
                                // 最初の押下
                                keyInfo.SetPressed();
                                //} else if (keyInfo.Shifted) {
                                //    // SHIFT状態なら何もしない
                            }
                            return true; // keyboardDownHandler() をスキップ、システム側の本来のSHIFT処理もスキップ
                        } else if (keyInfo.IsSingleShiftHitEffecive(bCtrl)) {
                            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"SingleShiftHitEffecive(ctrl={bCtrl}): {keyInfo.Name}");
                            // 拡張シフト面が割り当てはないが、単打系ありの場合
                            if (keyInfo.Released) {
                                //if (bCtrl || bShift || modPressedOrShifted != 0) {
                                //    if (Settings.LoggingDecKeyInfo) logger.DebugH(() => $"RELEASED -> PRESSED");
                                //    keyInfo.SetPressed();
                                //    return true; // keyboardDownHandler() をスキップ、システム側の本来のSHIFT処理もスキップ
                                //}
                                // 最初の押下で他のCtrlやShiftや拡張修飾が押されていない場合は、keyboardDownHandler() を呼び出す
                            } else if (keyInfo.Pressed) {
                                keyInfo.SetShifted();
                                keyInfoManager.makeExModKeyShifted(bDecoderOn);
                            }
                        } else {
                            // 拡張シフト面が割り当てられておらず単打系でもない拡張修飾キー
                            // すでに押下状態にあれば拡張修飾キーをSHIFT状態に遷移させる
                            keyInfoManager.makeExModKeyShifted(bDecoderOn);
                            // 拡張修飾キーがテーブルファイルに記述されている可能性もあるので keyboardDownHandler() を呼び出す
                        }
                    }
                } else {
                    // 通常キーの場合は、すでに押下状態にある拡張修飾キーをSHIFT状態に遷移させる
                    keyInfoManager.makeExModKeyShifted(bDecoderOn);
                }
                // keyboardDownHandler()の呼び出し
                if (Settings.LoggingDecKeyInfo) {
                    logger.Info(() => $"CALL: keyboardDownHandler({vkey}, {leftCtrl}, {rightCtrl})\n" + keyInfoManager.modifiersStateStr());
                }
                return keyboardDownHandler(vkey, leftCtrl, rightCtrl, bShift);
            }

            bool result = handleKeyDown();
            if (Settings.LoggingDecKeyInfo) {
                logger.Info(() => $"LEAVE: result={result}, vkey={vkey:x}H({vkey}), extraInfo={extraInfo}\n" + keyInfoManager.modifiersStateStr());
            }
            return result;
        }

        int keyDownCount = 0;

        private struct DecKeyInfo
        {
            int kanchokuCode;
            int normalDecKey;
            uint modifiers;

            public DecKeyInfo(int kc, int nc, uint mod)
            {
                kanchokuCode = kc;
                normalDecKey = nc;
                modifiers = mod;
            }
        }

        //private const int vkeyQueueMaxSize = 4;

        //private Queue<int> vkeyQueue = new Queue<int>();

        private static bool bHandlerBusy = false;

        /// <summary>キーボード押下時のハンドラ</summary>
        /// <param name="vkey"></param>
        /// <returns>キー入力を破棄する場合は true を返す。flase を返すとシステム側でキー入力処理が行われる</returns>
        private bool keyboardDownHandler(uint vkey, bool leftCtrl, bool rightCtrl, bool shift)
        {
            //if (vkey == FuncVKeys.HENKAN) logger.WarnH($"ENTER: vkey=HENKAN");
            //bool leftCtrl = (GetAsyncKeyState(FuncVKeys.LCONTROL) & 0x8000) != 0;
            //bool rightCtrl = (GetAsyncKeyState(FuncVKeys.RCONTROL) & 0x8000) != 0;
            bool bDecoderOn = isDecoderActivated();
            bool ctrl = leftCtrl || rightCtrl;
            bool alt = isAltKeyPressed();
            uint mod = KeyModifiers.MakeModifier(alt, ctrl, shift);
            uint modEx = keyInfoManager.getShiftedExModKey();
            bool suppressHoldShift = keyInfoManager.hasMultipleCommonTableHoldShiftsShifted(bDecoderOn) && !isSystemModifierVkey(vkey);
            int holdShiftPlane = suppressHoldShift ? ShiftPlane.ShiftPlane_NONE : keyInfoManager.getShiftPlane(bDecoderOn);

            //int normalDecKey = VKeyComboRepository.GetDecKeyFromVKey(vkey);
            int normalDecKey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
            int kanchokuCode = InputActionResolver.TryResolveToggleAction(mod, normalDecKey, out var toggleAction) ? toggleAction.ResolvedDeckey : -1; // 漢直モードのトグルをやるキーか

            if (Settings.LoggingDecKeyInfo) {
                logger.Info(() => $"ENTER: kanchokuCode={kanchokuCode}, normalDecKey={normalDecKey}, mod={mod:x}H({mod}), modEx={modEx:x}H({modEx}), vkey={vkey:x}H({vkey}), ctrl={ctrl}, shift={shift}");
            }

            // 漢直トグルでなく、VirtualKeyboard のミニバッファがActiveの場合は、システムに返す
            if (kanchokuCode < 0 && isVkbTopTextFocused()) return false;

            var activeHoldShiftInfo = suppressHoldShift ? null : keyInfoManager.getEffectiveCommonTableHoldShiftKeyInfo(bDecoderOn);
            if (kanchokuCode < 0 &&
                activeHoldShiftInfo != null &&
                InputActionResolver.HasCommonTableHoldShiftDefinition(activeHoldShiftInfo.Deckey) &&
                activeHoldShiftInfo.Vkey != vkey &&
                !ctrl && !shift && normalDecKey >= 0) {
                var combo = getComboForCommonHoldShift(activeHoldShiftInfo.Deckey, normalDecKey, bDecoderOn);
                if (combo != null && combo.IsTerminal && !combo.IsSubKey && combo.DecKeyList._notEmpty()) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"CommonTable HoldShift prioritized by combo: hold={activeHoldShiftInfo.Deckey}, key={normalDecKey}, combo={combo.DecKeysDebugString()}");
                    var pendingState = pendingCommonSingleHitStates._safeGet(activeHoldShiftInfo.Vkey);
                    if (pendingState != null) pendingState.Consumed = true;
                    return invokeHandlerForKeyList(combo.DecKeyList._toSingleHitResultKeyStrokeList(), false);
                }
                if ((combo == null || combo.IsTerminal) && InputActionResolver.TryResolveHoldShift(activeHoldShiftInfo.Deckey, normalDecKey, bDecoderOn, out var commonAction)) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"CommonTable HoldShift action: hold={activeHoldShiftInfo.Deckey}, key={normalDecKey}, action={commonAction.ResolvedDeckey}, source={commonAction.SourceKind}");
                    var pendingState = pendingCommonSingleHitStates._safeGet(activeHoldShiftInfo.Vkey);
                    if (pendingState != null) pendingState.Consumed = true;
                    return invokeResolvedAction(commonAction, normalDecKey, bDecoderOn);
                }
            }

            if (kanchokuCode < 0 && (modEx != 0 || holdShiftPlane != ShiftPlane.ShiftPlane_NONE) && !ctrl && !shift) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"PATH-B: IN: kanchokuCode={kanchokuCode}, modEx={modEx:x}, ctrl={ctrl}, shift={shift}");
                // 拡張シフトが有効なのは、Ctrlキーと物理Shiftが押されていない場合とする
                kanchokuCode = modEx != 0 && InputActionResolver.TryResolveModifiedKey(modEx, normalDecKey, bDecoderOn, out var modExAction) ? modExAction.ResolvedDeckey : -1;
                if (kanchokuCode < 0) {
                    // 拡張シフト面のコードを得る
                    kanchokuCode = normalDecKey;
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"PATH-A: shiftPlane={holdShiftPlane}, kanchokuCode={kanchokuCode}");
                    if (holdShiftPlane != ShiftPlane.ShiftPlane_NONE && kanchokuCode >= 0 && kanchokuCode < DecoderKeys.NORMAL_DECKEY_NUM) {
                        kanchokuCode += holdShiftPlane * DecoderKeys.PLANE_DECKEY_NUM;
                    }
                }
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"PATH-B: OUT: kanchokuCode={kanchokuCode:x}H({kanchokuCode}), modEx={modEx:x}, ctrl={ctrl}, shift={shift}");
            }

            if (kanchokuCode < 0) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"PATH-C: IN: kanchokuCode={kanchokuCode}, ctrl={ctrl}, shift={shift}");
                if (leftCtrl) {
                    // legacy 修飾変換で lctrl に定義されているものを検索
                    kanchokuCode = InputActionResolver.TryResolveModifiedKey(KeyModifiers.MOD_LCTRL, normalDecKey, bDecoderOn, out var lctrlAction) ? lctrlAction.ResolvedDeckey : -1;
                }
                if (kanchokuCode < 0 && rightCtrl) {
                    // legacy 修飾変換で rctrl に定義されているものを検索
                    kanchokuCode = InputActionResolver.TryResolveModifiedKey(KeyModifiers.MOD_RCTRL, normalDecKey, bDecoderOn, out var rctrlAction) ? rctrlAction.ResolvedDeckey : -1;
                }
                if (kanchokuCode < 0) {
                    bool preferModified = (Settings.GlobalCtrlKeysEnabled && ((Settings.UseLeftControlToConversion && leftCtrl) || (Settings.UseRightControlToConversion && rightCtrl))) || shift;
                    kanchokuCode = preferModified
                        ? (InputActionResolver.TryResolveModifiedKey(mod, normalDecKey, bDecoderOn, out var modAction) ? modAction.ResolvedDeckey : -1)
                        : (InputActionResolver.TryResolveComboKey(mod, normalDecKey, out var comboAction) ? comboAction.ResolvedDeckey : -1);
                }
                if (kanchokuCode >= 0) mod = 0;     // 何かのコードに変換されたら、 Ctrl や Shift の修飾は無かったことにしておく
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"PATH-C: OUT: kanchokuCode={kanchokuCode:x}H({kanchokuCode}), ctrl={ctrl}, shift={shift}");
            }

            if (holdShiftPlane != ShiftPlane.ShiftPlane_NONE &&
                !ctrl && !shift &&
                kanchokuCode >= 0 &&
                kanchokuCode == normalDecKey &&
                normalDecKey >= 0 && normalDecKey < DecoderKeys.NORMAL_DECKEY_NUM) {
                if (Settings.LoggingDecKeyInfo) {
                    logger.Info(() => $"PATH-H: apply holdShiftPlane={holdShiftPlane}, kanchokuCode(before)={kanchokuCode}, normalDecKey={normalDecKey}");
                }
                kanchokuCode += holdShiftPlane * DecoderKeys.PLANE_DECKEY_NUM;
                if (Settings.LoggingDecKeyInfo) {
                    logger.Info(() => $"PATH-H: OUT: kanchokuCode={kanchokuCode:x}H({kanchokuCode})");
                }
            }

            if (!bDecoderOn && (kanchokuCode < 0 || normalDecKey < 0 || (kanchokuCode == normalDecKey && normalDecKey >= DecoderKeys.FUNC_DECKEY_START))) {
                // デコーダーがOFFで、どの DecoderKey にもヒモ付けられていないか、または通常キーでもないキーが押されたら、そのままシステムに処理させる
                // ⇒ Astah など、なぜか自身で キーボード入力を監視していると思われるソフトがあるため
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: false: Decoder=OFF, no assigned deckey and not normal key");
                if (vkey == FuncVKeys.HENKAN) logger.Warn(() => $"LEAVE: HENKAN: result=False");
                frmKanchoku?.ResetPrevDeckey();
                return false;
            }

            bool result = true;
            bool forceDecoderByHoldShift = !bDecoderOn &&
                holdShiftPlane != ShiftPlane.ShiftPlane_NONE &&
                kanchokuCode >= DecoderKeys.SHIFT_DECKEY_START &&
                kanchokuCode < DecoderKeys.STROKE_DECKEY_END &&
                normalDecKey >= 0 && normalDecKey < DecoderKeys.NORMAL_DECKEY_NUM;
            if (bHandlerBusy) {
                logger.WarnH($"Handler Busy: vkey={vkey}, bDecoderOn={bDecoderOn}, mod={mod:x}H, kanchokuCode={kanchokuCode}, normalDecKey={normalDecKey}, keyDownCount={keyDownCount}");
            } else {
                bHandlerBusy = true;
                ++keyDownCount;
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"bDecoderOn={bDecoderOn}, mod={mod:x}H, kanchokuCode={kanchokuCode}, normalDecKey={normalDecKey}, keyDownCount={keyDownCount}, forceDecoderByHoldShift={forceDecoderByHoldShift}");
                var determiner = CombinationKeyStroke.Determiner.Singleton;
                var comboPoolEnabled = CombinationKeyStroke.DeterminerLib.KeyCombinationPool._Enabled;
                if (/*(bDecoderOn || currentPool.HasComboEffectiveAlways) &&*/
                    mod == 0 &&                                                                         // 修飾子がない
                    comboPoolEnabled &&                   // 同時打鍵定義が有効
                    kanchokuCode >= 0 && kanchokuCode < DecoderKeys.STROKE_DECKEY_END &&                // 機能コード以外
                    ((kanchokuCode % DecoderKeys.PLANE_DECKEY_NUM) < DecoderKeys.NORMAL_DECKEY_NUM ||   // 通常キーであるか、
                     CombinationKeyStroke.DeterminerLib.KeyCombinationPool._GetEntry(kanchokuCode) != null)    // 特殊キーであっても同時打鍵テーブルで使われている
                    ) {
                    // KeyDown時処理を呼び出す。同時打鍵キーのオートリピートが開始されたら打鍵ガイドを切り替える
                    determiner.KeyDown(kanchokuCode, bDecoderOn, keyDownCount, (decKeys) => handleComboKeyRepeat(vkey, decKeys));
                    result = true;
                } else {
                    // 直接ハンドラを呼び出す
                    //if (bDecoderOn && vkey == FuncVKeys.HENKAN) logger.WarnH($"invokeHandler HENKAN: comboPoolEnabled={comboPoolEnabled}, mod={mod:x}H, kanchokuCode={kanchokuCode}");
                    result = invokeHandler(kanchokuCode, normalDecKey, mod, false, forceDecoderByHoldShift);
                    //if (bDecoderOn && vkey == FuncVKeys.HENKAN) logger.WarnH($"invokeHandler HENKAN: result={result}");
                }
                bHandlerBusy = false;
            }
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: result={result}");
            if (vkey == FuncVKeys.HENKAN) logger.Warn(() => $"LEAVE: HENKAN: result={result}");
            return result;
        }

        private uint prevUpVkey = 0;

        /// <summary>キーアップ時のハンドラ</summary>
        /// <param name="vkey"></param>
        /// <param name="extraInfo"></param>
        /// <returns>キー入力を破棄する場合は true を返す。flase を返すとシステム側でキー入力処理が行われる</returns>
        private bool onKeyboardUpHandler(uint vkey, int scanCode, uint flags, int extraInfo)
        {
            appendKeyInfo(false, vkey, scanCode, flags, extraInfo);

            // Pauseで一時停止?
            if (Settings.SuspendByPauseKey && vkey == (uint)Keys.Pause) {
                return true;
            }
            // 一時停止?
            if (Settings.DecoderSuspended) return false;

            if (Settings.LoggingDecKeyInfo) {
                logger.Info(() =>
                    $"\nENTER: IsVkbTopTextFocused={isVkbTopTextFocused()}, vkey={vkey:x}H({vkey}), scanCode={scanCode:x}H, extraInfo={extraInfo}");
            }

            if (vkey == 0x5b || vkey == 0x5c) {
                // LWIN or RWIN
                bWinKeyOn = false;
                if (Settings.LoggingDecKeyInfo) logger.Info("WinKey Off");
            }

            if (bWinKeyOn) {
                if (Settings.LoggingDecKeyInfo) logger.Info($"WinKey On: continue SystemModifier/HoldShift up path for {vkey:x}");
            }

            // 半/全キーは、US-on-JP モードなら true(入力破棄; つまり無視) JPモードなら false (システム処理; つまりIMEのON/OFF)を返す
            if (vkey == VKEY_HANZEN || vkey == VKEY_KANJI) return DecoderKeyVsVKey.IsUSonJPmode;

            uint prevVkey = prevUpVkey;
            prevUpVkey = vkey;

            if (isMyInjectedEvent(extraInfo)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Ignore self injected key up: vkey={vkey:x}H");
                return false;
            }

            bool leftShift = (GetAsyncKeyState(FuncVKeys.LSHIFT) & 0x8000) != 0;
            bool leftCtrl = (GetAsyncKeyState(FuncVKeys.LCONTROL) & 0x8000) != 0;
            bool rightCtrl = (GetAsyncKeyState(FuncVKeys.RCONTROL) & 0x8000) != 0;
            bool bCtrl = leftCtrl || rightCtrl;

            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"vkey={vkey:x}H({vkey}), leftCtrl={leftCtrl}, rightCtrl={rightCtrl}, leftShift={leftShift}");

            int holdShiftDeterminerDeckey = holdShiftTargetDeterminerDeckeys._safeGet(vkey, -1);
            if (holdShiftDeterminerDeckey >= 0) {
                holdShiftTargetDeterminerDeckeys.Remove(vkey);
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"HoldShift target key up for Determiner: vkey={vkey:x}H, deckey={holdShiftDeterminerDeckey}");
                CombinationKeyStroke.Determiner.Singleton.KeyUp(holdShiftDeterminerDeckey, isDecoderActivated());
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: true; Determiner.Singleton.KeyUp DONE and result=true");
                return true;
            }

            if (consumedHoldShiftTargetVkeys.Remove(vkey)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: true; Suppress key up for consumed HoldShift target: vkey={vkey:x}H");
                return true;
            }

            if (extraInfo == 0 && isSystemModifierVkey(vkey)) {
                bool flag = handleSystemModifierUp(vkey, isDecoderActivated(), leftCtrl, rightCtrl);
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: {flag}; handleSystemModifierUp DONE");
                return flag;
            }

            if (!isShiftVkey(vkey)) {
                activeNonShiftVkeys.Remove(vkey);
                if (shouldSuppressForPendingShiftFallback(vkey)) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: true; Suppress key up for pending shift fallback: vkey={vkey:x}H");
                    return true;
                }
            }

            var shiftState = getShiftState(vkey);
            if (shiftState != null && shiftState.IsPendingNormalKey) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() =>
                    $"Pending shift up: vkey={vkey:x}H, overlap={shiftState.HasOverlap}, combined={shiftState.IsCombined}, fallback={shiftState.ShouldFallbackToSystemShift}");

                if (shiftState.DispatchedToDeterminer) {
                    keyboardUpHandler(isDecoderActivated(), vkey, leftCtrl, rightCtrl, 0);
                }

                if (shiftState.ShouldFallbackToSystemShift && shiftState.FallbackVkeys._notEmpty()) {
                    replayPendingShiftFallback(shiftState, leftCtrl, rightCtrl);
                    shiftState.Reset();
                    return true;
                }

                if (!shiftState.HasOverlap) {
                    if (tryInvokeCommonSingleHitByDeckey(shiftState.Deckey, isDecoderActivated())) {
                        shiftState.Reset();
                        return true;
                    }
                    bool result = invokeSingleShiftTap(vkey, leftCtrl, rightCtrl);
                    if (!result) {
                        sendOriginalVkey(vkey);
                        result = true;
                    }
                    shiftState.Reset();
                    return result;
                }

                shiftState.Reset();
                return true;
            }

            if (vkey == FuncVKeys.LSHIFT) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LSHIFT up pass-through");
                return false;
            }

            if (extraInfo == 0) {
                // とりあえず、やっつけコード
                void checkAndInvoke(bool bShifted)
                {
                    int normalDecKey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
                    if (!bShifted && /*bDecoderOn &&*/ ExtraModifiers.IsExModKeyIndexAssignedForDecoderFunc(normalDecKey)) {
                        if (InputActionResolver.TryResolveComboKey(0, normalDecKey, out var action)) {
                            invokeResolvedAction(action, -1, isDecoderActivated());
                        }
                    }
                }

                if (vkey == FuncVKeys.LCONTROL) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LCONTROL up");
                    checkAndInvoke(bLCtrlShifted);
                    bLCtrlShifted = false;
                } else if (vkey == FuncVKeys.RCONTROL) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"RCONTROL up");
                    checkAndInvoke(bRCtrlShifted);
                    bRCtrlShifted = false;
                }
            }

            // 同時打鍵キーのオートリピートが終了したら打鍵ガイドを元に戻す
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"CALL handleComboKeyRepeatStop");
            if (handleComboKeyRepeatStop(vkey)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: true; handleComboKeyRepeatStop handled combo key repeat stop");
                return true;
            }

            if (!isEffectiveVkey(vkey, scanCode, flags, extraInfo, leftCtrl || rightCtrl)) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: result=False, not EffectiveVkey");
                return false;
            }

            bool bDecoderOn = isDecoderActivated();
            bool bAlt = isAltKeyPressed();
            bool bWin = isWinKeyPressed();

            uint modFlag = keyInfoManager.getModFlagForExModVkey(vkey);
            var keyInfo = keyInfoManager.getModiferKeyInfoByVkey(vkey);
            //bool result = false;
            if (keyInfo != null) {
                bool bPrevPressed = keyInfo.Pressed;
                keyInfo.SetReleased();
                if (Settings.LoggingDecKeyInfo) logger.DebugH(() =>
                    $"{keyInfo.Name}Key up: prevPressed={bPrevPressed}, decoderOn={bDecoderOn}, modFlag={modFlag:x}, newKeyState={keyInfo.KeyState}");
                if (keyInfo.IsHoldShift) {
                    if (bCtrl || leftShift || bAlt || bWin) {
                        if (Settings.LoggingDecKeyInfo) logger.Info(() => $"GenericHoldShift UP: modifier shortcut pass-through");
                        return false;
                    }
                    if (bPrevPressed || keyInfo.Repeated || keyInfo.Shifted) keyInfo.PrevUpDt = HRDateTime.Now;
                    updateStrokeHelpHoldShiftPlane(bDecoderOn);
                    if (bPrevPressed) {
                        var pendingState = popPendingCommonSingleHit(vkey);
                        if (pendingState != null && !pendingState.Consumed && tryInvokeCommonSingleHitByDeckey(pendingState.Deckey, bDecoderOn)) {
                            return true;
                        }
                        if (InputActionResolver.TryResolveSingleHit(keyInfo.Deckey, bDecoderOn, out var commonAction) && invokeResolvedAction(commonAction, keyInfo.Deckey, bDecoderOn)) {
                            return true;
                        }
                        if (bDecoderOn) {
                            keyboardDownHandler(vkey, leftCtrl, rightCtrl, false);
                            keyboardUpHandler(bDecoderOn, vkey, leftCtrl, rightCtrl, 0);
                        } else {
                            sendOriginalVkey(vkey);
                        }
                    }
                    return true;
                } else {
                    if (bPrevPressed && keyInfo.IsShiftPlaneAssigned(bDecoderOn) && keyInfo.IsSingleShiftHitEffecive(bCtrl)) {
                        // 拡張シフト面が割り当てられ、かつ単打系がある拡張修飾キーで、それが押下状態の場合
                        keyboardDownHandler(vkey, leftCtrl, rightCtrl, false);
                    }
                    keyboardUpHandler(bDecoderOn, vkey, leftCtrl, rightCtrl, 0);
                    return false;
                }
            }

            // VirtualKeyboard のミニバッファがActiveの場合は、システムに返す
            if (isVkbTopTextFocused()) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: false; VkbTopTextFocused");
                return false;
            }

            var commonSingleHit = popPendingCommonSingleHit(vkey);
            if (commonSingleHit != null) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"commonSingleHit=({commonSingleHit.Deckey}, {commonSingleHit.Consumed})");
                if (!commonSingleHit.Consumed && tryInvokeCommonSingleHitByDeckey(commonSingleHit.Deckey, bDecoderOn)) {
                    return true;
                }
                return true;
            }

            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"CALL keyboardUpHandler");
            keyboardUpHandler(bDecoderOn, vkey, leftCtrl, rightCtrl, modFlag);
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: false");
            return false;
        }

        /// <summary>キーボードUP時のハンドラ</summary>
        /// <param name="vkey"></param>
        /// <returns>キー入力を破棄する場合は true を返す。flase を返すとシステム側でキー入力処理が行われる</returns>
        private void keyboardUpHandler(bool bDecoderOn, uint vkey, bool leftCtrl, bool rightCtrl, uint modFlag)
        {
            if (/*(bDecoderOn || currentPool.HasComboEffectiveAlways) &&*/
                CombinationKeyStroke.DeterminerLib.KeyCombinationPool._Enabled &&  !leftCtrl && !rightCtrl && modFlag == 0) {
                //int deckey = /* vkey == (int)Keys.Space ? DecoderKeys.STROKE_SPACE_DECKEY :*/ VKeyComboRepository.GetDecKeyFromCombo(0, normalDecKey); /* ここではまだ、Spaceはいったん文字として扱う */
                int deckey = DecoderKeyVsVKey.GetDecKeyFromVKey(vkey);
                if (deckey >= 0 && deckey < DecoderKeys.STROKE_DECKEY_END) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"CALL Determiner.Singleton.KeyUp(deckey={deckey}, bDecoderOn={bDecoderOn})");
                    CombinationKeyStroke.Determiner.Singleton.KeyUp(deckey, bDecoderOn);
                }
            }
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE");
        }

        private void setInvokeHandlerToDeterminer()
        {
            CombinationKeyStroke.Determiner.Singleton.KeyProcHandler = (keyList, bUncond) => invokeHandlerForKeyList(keyList, bUncond);
        }

        private bool invokeHandlerForKeyList(List<ResultKeyStroke> keyList, bool bUnconditional)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"ENTER: keyList={(keyList._isEmpty() ? "(empty)" : keyList._keyString())}");
            bool result = true;
            if (keyList._notEmpty()) {
                foreach (var k in keyList) {
                    result = invokeHandler(k.decKey, -1, 0, k.bRollOverStroke, bUnconditional) && result;
                }
            }
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: result={result}");
            return result;
        }

        /// <summary> キーボードハンドラの処理中か </summary>
        private bool bInvokeHandlerBusy = false;

        private bool invokeHandler(int kanchokuCode, int origDecKey, uint mod, bool rollOverStroke, bool bUnconditional = false)
        {
            if (kanchokuCode == -1) {
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"invokeHandler: kanchokuCode={kanchokuCode} is UNDEFINED");
                kanchokuCode = DecoderKeys.UNDEFINED_DECKEY;
            }
            if (Settings.LoggingDecKeyInfo) {
                logger.Info(() => $"ENTER: kanchokuCode={DecoderKeys.ToDebugString(kanchokuCode)}, mod={mod:x}H({mod}), bUnconditional={bUnconditional}");
            }

            bool result = false;

            if (bInvokeHandlerBusy) {
                logger.WarnH("Handler Busy");
            } else if (kanchokuCode >= 0) {
                bInvokeHandlerBusy = true;
                if (Settings.LoggingDecKeyInfo) logger.Info("CALL: frmKanchoku.FuncDispatcher()");
                result = frmKanchoku?.FuncDispatcher(kanchokuCode, origDecKey, mod, bUnconditional, rollOverStroke) ?? false;
            }
            bInvokeHandlerBusy = false;

            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"LEAVE: result={result}");

            return result;
        }

    }
}
