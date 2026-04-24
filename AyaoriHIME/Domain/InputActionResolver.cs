using System.Collections.Generic;
using Utils;

namespace KanchokuWS.Domain
{
    /// <summary>
    /// 入力解決の結果として何を返すかを表す種別
    /// </summary>
    public enum InputActionKind
    {
        None,
        Deckey,
        Command,
    }

    /// <summary>
    /// 解決結果がどの設定経路から登録されたものかを表す種別
    /// </summary>
    public enum InputActionSourceKind
    {
        None,
        LegacyCombo,
        LegacyModConversion,
        CtrlSetting,
        Toggle,
        CommonTable,
    }

    /// <summary>
    /// 修飾付き入力、単打、HoldShift 解決の共通返り値
    /// </summary>
    public class InputActionResolution
    {
        /// <summary>解決先 deckey。未解決なら -1。</summary>
        public int ResolvedDeckey { get; set; } = -1;
        /// <summary>この入力解決が decoder ON 前提かどうか。</summary>
        public bool RequiresDecoder { get; set; }
        /// <summary>deckey 解決か、将来のコマンド解決かを表す。</summary>
        public InputActionKind ActionKind { get; set; } = InputActionKind.None;
        /// <summary>登録元の経路。</summary>
        public InputActionSourceKind SourceKind { get; set; } = InputActionSourceKind.None;
        /// <summary>将来のコマンド解決用。現状は deckey 中心で利用。</summary>
        public string CommandText { get; set; }

        public bool IsResolved => ActionKind != InputActionKind.None;

        /// <summary>
        /// 未解決を表す結果を返す。
        /// </summary>
        public static InputActionResolution None()
        {
            return new InputActionResolution();
        }

        /// <summary>
        /// deckey 解決結果を生成する。
        /// </summary>
        public static InputActionResolution ForDeckey(int deckey, InputActionSourceKind sourceKind, bool requiresDecoder = false)
        {
            return new InputActionResolution() {
                ResolvedDeckey = deckey,
                RequiresDecoder = requiresDecoder,
                ActionKind = InputActionKind.Deckey,
                SourceKind = sourceKind,
            };
        }
    }

    /// <summary>
    /// キー入力解決の forward lookup を集約する。
    /// legacy combo、Ctrl 変換、commonTable の単打/HoldShift をここで引く。
    /// </summary>
    static class InputActionResolver
    {
        private static readonly Logger logger = Logger.GetLogger();

        private static Dictionary<ulong, InputActionResolution> comboActions = new Dictionary<ulong, InputActionResolution>();
        private static Dictionary<ulong, InputActionResolution> modifiedComboActions = new Dictionary<ulong, InputActionResolution>();
        private static Dictionary<int, InputActionResolution> singleHitActions = new Dictionary<int, InputActionResolution>();
        private static Dictionary<ulong, InputActionResolution> holdShiftActions = new Dictionary<ulong, InputActionResolution>();
        private static HashSet<int> commonTableHoldShiftDeckeys = new HashSet<int>();

        private static ulong makeComboSerial(uint modifier, int deckey)
        {
            return ((ulong)modifier << 32) | (uint)deckey;
        }

        private static ulong makeHoldShiftSerial(int holdShiftDeckey, int targetDeckey)
        {
            return ((ulong)(uint)holdShiftDeckey << 32) | (uint)targetDeckey;
        }

        private static bool isToggleDeckey(int deckey)
        {
            return deckey >= DecoderKeys.TOGGLE_DECKEY && deckey <= DecoderKeys.DEACTIVE2_DECKEY;
        }

        /// <summary>
        /// すべての lookup ストアを初期化する。
        /// 設定再読込時にも呼ばれる。
        /// </summary>
        public static void Initialize()
        {
            logger.Info("ENTER");
            comboActions = new Dictionary<ulong, InputActionResolution>();
            modifiedComboActions = new Dictionary<ulong, InputActionResolution>();
            singleHitActions = new Dictionary<int, InputActionResolution>();
            holdShiftActions = new Dictionary<ulong, InputActionResolution>();
            commonTableHoldShiftDeckeys = new HashSet<int>();
            logger.Info("LEAVE");
        }

        /// <summary>
        /// commonTable 由来の単打/HoldShift 定義だけをクリアする。
        /// legacy 設定は残す。
        /// </summary>
        public static void ClearCommonTableActions()
        {
            foreach (var deckey in new List<int>(singleHitActions.Keys)) {
                if (singleHitActions[deckey].SourceKind == InputActionSourceKind.CommonTable) {
                    singleHitActions.Remove(deckey);
                }
            }

            foreach (var serial in new List<ulong>(holdShiftActions.Keys)) {
                if (holdShiftActions[serial].SourceKind == InputActionSourceKind.CommonTable) {
                    holdShiftActions.Remove(serial);
                }
            }

            commonTableHoldShiftDeckeys.Clear();
        }

        /// <summary>
        /// modifier + normalDeckey の通常 combo 解決を登録する。
        /// トグル deckey は SourceKind を Toggle に正規化する。
        /// </summary>
        public static void RegisterComboAction(uint modifier, int normalDeckey, int resolvedDeckey, InputActionSourceKind sourceKind)
        {
            var resolvedSource = isToggleDeckey(resolvedDeckey) ? InputActionSourceKind.Toggle : sourceKind;
            comboActions[makeComboSerial(modifier, normalDeckey)] = InputActionResolution.ForDeckey(resolvedDeckey, resolvedSource);
        }

        /// <summary>
        /// `Ctrl+face` / `Ctrl+Shift+face` の変換を登録または削除する。
        /// Settings の Ctrl 変換設定の受け口。
        /// </summary>
        public static void RegisterCtrlDeckeyFromCombo(string keyFace, int ctrlDeckey, int ctrlShiftDeckey)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"keyFace={keyFace}, ctrlDeckey={ctrlDeckey}, ctrlShiftDeckey={ctrlShiftDeckey}");
            bool bRemove = false;
            if (keyFace._startsWith("#")) {
                bRemove = true;
                keyFace = keyFace.Replace("#", "");
            }
            int deckey = DecoderKeyVsChar.GetArrangedDecKeyFromFaceStr(keyFace);
            if (deckey >= 0) {
                if (bRemove) {
                    if (ctrlDeckey > 0) RemoveModifiedComboAction(KeyModifiers.MOD_CONTROL, deckey);
                    if (ctrlShiftDeckey > 0) RemoveModifiedComboAction(KeyModifiers.MOD_CONTROL | KeyModifiers.MOD_SHIFT, deckey);
                } else {
                    if (ctrlDeckey > 0) RegisterModifiedComboAction(KeyModifiers.MOD_CONTROL, deckey, ctrlDeckey, InputActionSourceKind.CtrlSetting);
                    if (ctrlShiftDeckey > 0) RegisterModifiedComboAction(KeyModifiers.MOD_CONTROL | KeyModifiers.MOD_SHIFT, deckey, ctrlShiftDeckey, InputActionSourceKind.CtrlSetting);
                }
            }
        }

        /// <summary>
        /// Ctrl 系機能キーの登録時に、reverse mapping と combo lookup を同時に登録する。
        /// </summary>
        public static void RegisterCtrlDeckeyAndCombo(string keyFace, int ctrlDeckey, int ctrlShiftDeckey)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"keyFace={keyFace}, ctrlDeckey={ctrlDeckey:x}H({ctrlDeckey}), ctrlShiftDeckey={ctrlShiftDeckey:x}H({ctrlShiftDeckey})");
            int deckey = DecoderKeyVsChar.GetArrangedDecKeyFromFaceStr(keyFace);
            if (deckey >= 0) {
                if (ctrlDeckey > 0) {
                    DeckeyComboMap.Register(ctrlDeckey, KeyModifiers.MOD_CONTROL, deckey);
                    RegisterComboAction(KeyModifiers.MOD_CONTROL, deckey, ctrlDeckey, InputActionSourceKind.CtrlSetting);
                }
                if (ctrlShiftDeckey > 0) {
                    DeckeyComboMap.Register(ctrlShiftDeckey, KeyModifiers.MOD_CONTROL | KeyModifiers.MOD_SHIFT, deckey);
                    RegisterComboAction(KeyModifiers.MOD_CONTROL | KeyModifiers.MOD_SHIFT, deckey, ctrlShiftDeckey, InputActionSourceKind.CtrlSetting);
                }
            }
        }

        /// <summary>
        /// legacy mod-conversion などの「修飾付き専用変換」を登録する。
        /// 通常 combo より優先して引かれる。
        /// </summary>
        public static void RegisterModifiedComboAction(uint modifier, int normalDeckey, int resolvedDeckey, InputActionSourceKind sourceKind)
        {
            modifiedComboActions[makeComboSerial(modifier, normalDeckey)] = InputActionResolution.ForDeckey(resolvedDeckey, sourceKind);
        }

        /// <summary>
        /// 修飾付き専用変換を削除する。
        /// </summary>
        public static void RemoveModifiedComboAction(uint modifier, int normalDeckey)
        {
            modifiedComboActions.Remove(makeComboSerial(modifier, normalDeckey));
        }

        /// <summary>
        /// commonTable の singleHit を登録する。
        /// </summary>
        public static void RegisterSingleHitAction(int deckey, CommonTableAction action)
        {
            if (action == null) return;
            singleHitActions[deckey] = InputActionResolution.ForDeckey(action.Deckey, InputActionSourceKind.CommonTable, action.RequiresDecoder);
        }

        /// <summary>
        /// commonTable の HoldShift 組合せを登録する。
        /// </summary>
        public static void RegisterHoldShiftAction(int holdShiftDeckey, int targetDeckey, CommonTableAction action)
        {
            if (action == null) return;
            holdShiftActions[makeHoldShiftSerial(holdShiftDeckey, targetDeckey)] =
                InputActionResolution.ForDeckey(action.Deckey, InputActionSourceKind.CommonTable, action.RequiresDecoder);
            commonTableHoldShiftDeckeys.Add(holdShiftDeckey);
        }

        /// <summary>
        /// 指定 deckey に commonTable 単打定義があるかを返す。
        /// </summary>
        public static bool HasSingleHitAction(int deckey)
        {
            return singleHitActions.ContainsKey(deckey);
        }

        /// <summary>
        /// commonTable 単打を解決する。
        /// </summary>
        public static bool TryResolveSingleHit(int deckey, bool bDecoderOn, out InputActionResolution action)
        {
            action = singleHitActions._safeGet(deckey);
            return action != null && action.IsResolved;
        }

        /// <summary>
        /// 指定 deckey が commonTable で HoldShift キーとして定義されているかを返す。
        /// </summary>
        public static bool HasCommonTableHoldShiftDefinition(int holdShiftDeckey)
        {
            return commonTableHoldShiftDeckeys.Contains(holdShiftDeckey);
        }

        /// <summary>
        /// commonTable HoldShift + target の組合せを解決する。
        /// </summary>
        public static bool TryResolveHoldShift(int holdShiftDeckey, int targetDeckey, bool bDecoderOn, out InputActionResolution action)
        {
            action = holdShiftActions._safeGet(makeHoldShiftSerial(holdShiftDeckey, targetDeckey));
            return action != null && action.IsResolved;
        }

        /// <summary>
        /// 通常の modifier + normalDeckey の combo を解決する。
        /// </summary>
        public static bool TryResolveComboKey(uint modifier, int normalDeckey, out InputActionResolution action)
        {
            action = comboActions._safeGet(makeComboSerial(modifier, normalDeckey));
            return action != null && action.IsResolved;
        }

        /// <summary>
        /// 修飾付き専用変換を先に見て、なければ通常 combo lookup へフォールバックする。
        /// </summary>
        public static bool TryResolveModifiedKey(uint modifier, int normalDeckey, bool bDecoderOn, out InputActionResolution action)
        {
            action = modifiedComboActions._safeGet(makeComboSerial(modifier, normalDeckey));
            if (action != null && action.IsResolved) return true;
            return TryResolveComboKey(modifier, normalDeckey, out action);
        }

        /// <summary>
        /// 指定文字面の Ctrl 変換 deckey を返す。
        /// 現状は Ctrl-J 判定などの補助用途。
        /// </summary>
        public static int GetCtrlDecKeyOf(string face)
        {
            int deckey = DecoderKeyVsChar.GetArrangedDecKeyFromFaceStr(face);
            return (deckey > 0 && TryResolveModifiedKey(KeyModifiers.MOD_CONTROL, deckey, true, out var action)) ? action.ResolvedDeckey : -1;
        }

        /// <summary>
        /// modifier + normalDeckey が漢直 ON/OFF トグル系ならその action を返す。
        /// </summary>
        public static bool TryResolveToggleAction(uint modifier, int normalDeckey, out InputActionResolution action)
        {
            if (TryResolveModifiedKey(modifier, normalDeckey, true, out action) && isToggleDeckey(action.ResolvedDeckey)) {
                if (action.SourceKind != InputActionSourceKind.Toggle) {
                    action = InputActionResolution.ForDeckey(action.ResolvedDeckey, InputActionSourceKind.Toggle, action.RequiresDecoder);
                }
                return true;
            }
            return false;
        }
    }
}
