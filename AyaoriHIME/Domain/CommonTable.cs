using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using KanchokuWS.TableParser;
using Utils;

namespace KanchokuWS.Domain
{
    /// <summary>
    /// commonTable の 1 エントリ分の実行内容。
    /// deckey 解決済みの出力先と、decoder ON 前提かどうかを持つ。
    /// </summary>
    public class CommonTableAction
    {
        /// <summary>実行先 deckey。</summary>
        public int Deckey { get; set; }
        /// <summary>複合コマンド化されたため decoder ON が必要かどうか。</summary>
        public bool RequiresDecoder { get; set; }
        /// <summary>commonTable 上の元の target 表記。</summary>
        public string RawTarget { get; set; }
    }

    /// <summary>
    /// commonTable の 1 つの HoldShift ブロックを表す。
    /// </summary>
    public class CommonTableHoldShiftDefinition
    {
        /// <summary>HoldShift キー側の deckey。</summary>
        public int HoldShiftDeckey { get; set; }
        /// <summary>この HoldShift に割り当てる shift plane。</summary>
        public int ShiftPlane { get; set; }
        /// <summary>decoder OFF 中も有効にするかどうか。</summary>
        public bool EnabledWhenDecoderOff { get; set; }
        /// <summary>被修飾キー deckey から実行内容への対応表。</summary>
        public Dictionary<int, CommonTableAction> Actions { get; } = new Dictionary<int, CommonTableAction>();
    }

    /// <summary>
    /// commonTable 全体の解析結果。
    /// singleHit、HoldShift、必要なら複合コマンド文字列を保持する。
    /// </summary>
    public class CommonTableDefinition
    {
        /// <summary>単打 deckey 定義。</summary>
        public Dictionary<int, CommonTableAction> SingleHitActions { get; } = new Dictionary<int, CommonTableAction>();
        /// <summary>HoldShift キーごとの定義。</summary>
        public Dictionary<int, CommonTableHoldShiftDefinition> HoldShiftDefinitions { get; } = new Dictionary<int, CommonTableHoldShiftDefinition>();
        /// <summary>複合コマンドとして既存テーブルパーサへ渡す追加定義文字列。</summary>
        public string GeneratedComplexCommandString { get; set; }
    }

    static class CommonTableDirectTargetHelper
    {
        public static int TryParseExpression(string target)
        {
            var stripped = target._strip();
            if (stripped._isEmpty()) return -1;

            int deckey = parseShiftPlaneDeckey(stripped);
            if (deckey >= 0) return deckey;

            var lower = stripped._toLower();
            if (lower._startsWith("^")) {
                var name = lower._safeSubstring(1);
                if (name._safeLength() == 1 && name._ge("a") && name._le("z")) {
                    return DecoderKeys.DECKEY_CTRL_A + name[0] - 'a';
                }

                deckey = SpecialKeysAndFunctions.GetDeckeyByName(name);
                if (deckey >= DecoderKeys.FUNC_DECKEY_START && deckey < DecoderKeys.FUNC_DECKEY_END) {
                    return deckey + DecoderKeys.CTRL_FUNC_DECKEY_START - DecoderKeys.FUNC_DECKEY_START;
                }
                return -1;
            }

            return SpecialKeysAndFunctions.GetDeckeyByName(lower);
        }

        public static string NormalizeLegacyTarget(string target)
        {
            var stripped = target._strip();
            return TryParseExpression(stripped) >= 0 ? $"!{{{stripped}}}" : stripped;
        }

        private static int parseShiftPlaneDeckey(string str)
        {
            if (str._isEmpty()) return -1;
            var s = str._toUpper();
            int offset = calcShiftOffset(s[0]);
            int deckey = offset > 0 ? s._safeSubstring(1)._parseInt(-1) : s._parseInt(-1);
            if (deckey < 0 || deckey >= DecoderKeys.STROKE_DECKEY_END) return -1;
            return deckey + offset;
        }

        private static int calcShiftOffset(char ch)
        {
            switch (char.ToUpperInvariant(ch)) {
                case 'S': return ShiftPlane.ShiftPlane_SHIFT * DecoderKeys.PLANE_DECKEY_NUM;
                case 'A': return ShiftPlane.ShiftPlane_A * DecoderKeys.PLANE_DECKEY_NUM;
                case 'B': return ShiftPlane.ShiftPlane_B * DecoderKeys.PLANE_DECKEY_NUM;
                case 'C': return ShiftPlane.ShiftPlane_C * DecoderKeys.PLANE_DECKEY_NUM;
                case 'D': return ShiftPlane.ShiftPlane_D * DecoderKeys.PLANE_DECKEY_NUM;
                case 'E': return ShiftPlane.ShiftPlane_E * DecoderKeys.PLANE_DECKEY_NUM;
                case 'F': return ShiftPlane.ShiftPlane_F * DecoderKeys.PLANE_DECKEY_NUM;
                default: return 0;
            }
        }
    }

    /// <summary>
    /// commonTable の解析結果を実行時設定へ反映する。
    /// InputActionResolver への登録と HoldShift 設定反映を担当する。
    /// </summary>
    static class CommonTableRuntime
    {
        private static readonly Logger logger = Logger.GetLogger();
        private static readonly int[] systemModifierDeckeys = new[] {
            DecoderKeys.LEFT_CONTROL_DECKEY,
            DecoderKeys.RIGHT_CONTROL_DECKEY,
            DecoderKeys.LEFT_SHIFT_DECKEY,
            DecoderKeys.RIGHT_SHIFT_DECKEY,
            DecoderKeys.LEFT_ALT_DECKEY,
            DecoderKeys.RIGHT_ALT_DECKEY,
            DecoderKeys.LEFT_WIN_DECKEY,
            DecoderKeys.RIGHT_WIN_DECKEY,
        };
        private static CommonTableDefinition currentDefinition = new CommonTableDefinition();

        private static CommonTableDefinition cloneDefinition(CommonTableDefinition definition)
        {
            var cloned = new CommonTableDefinition();
            if (definition == null) return cloned;

            foreach (var pair in definition.SingleHitActions) {
                cloned.SingleHitActions[pair.Key] = new CommonTableAction() {
                    Deckey = pair.Value.Deckey,
                    RequiresDecoder = pair.Value.RequiresDecoder,
                    RawTarget = pair.Value.RawTarget,
                };
            }
            foreach (var pair in definition.HoldShiftDefinitions) {
                var holdShift = new CommonTableHoldShiftDefinition() {
                    HoldShiftDeckey = pair.Value.HoldShiftDeckey,
                    ShiftPlane = pair.Value.ShiftPlane,
                    EnabledWhenDecoderOff = pair.Value.EnabledWhenDecoderOff,
                };
                foreach (var actionPair in pair.Value.Actions) {
                    holdShift.Actions[actionPair.Key] = new CommonTableAction() {
                        Deckey = actionPair.Value.Deckey,
                        RequiresDecoder = actionPair.Value.RequiresDecoder,
                        RawTarget = actionPair.Value.RawTarget,
                    };
                }
                cloned.HoldShiftDefinitions[pair.Key] = holdShift;
            }
            cloned.GeneratedComplexCommandString = definition.GeneratedComplexCommandString;
            return cloned;
        }

        private static string makeKeyName(int deckey)
        {
            return SpecialKeysAndFunctions.GetKeyNameByDeckey(deckey)._notEmpty() ? SpecialKeysAndFunctions.GetKeyNameByDeckey(deckey) : deckey.ToString();
        }

        private static string makePlaneName(int shiftPlane)
        {
            switch (shiftPlane) {
                case ShiftPlane.ShiftPlane_SHIFT: return "shift";
                case ShiftPlane.ShiftPlane_A: return "A";
                case ShiftPlane.ShiftPlane_B: return "B";
                case ShiftPlane.ShiftPlane_C: return "C";
                case ShiftPlane.ShiftPlane_D: return "D";
                case ShiftPlane.ShiftPlane_E: return "E";
                case ShiftPlane.ShiftPlane_F: return "F";
                default: return "shift";
            }
        }

        /// <summary>
        /// commonTable 由来の runtime 定義だけをクリアする。
        /// </summary>
        public static void Clear()
        {
            InputActionResolver.ClearCommonTableActions();
        }

        public static void SetCurrentDefinition(CommonTableDefinition definition)
        {
            currentDefinition = cloneDefinition(definition);
        }

        /// <summary>
        /// 解析済み commonTable を runtime へ登録する。
        /// singleHit/HoldShift の action 登録に加え、SystemModifier も HoldShift 扱いにする。
        /// </summary>
        public static void Initialize(CommonTableDefinition definition)
        {
            Clear();
            SetCurrentDefinition(definition);
            if (definition == null) return;

            foreach (var pair in definition.SingleHitActions) {
                InputActionResolver.RegisterSingleHitAction(pair.Key, pair.Value);
            }
            foreach (var pair in definition.HoldShiftDefinitions) {
                Settings.SetHoldShiftKeySetting(pair.Key, pair.Value.ShiftPlane, pair.Value.EnabledWhenDecoderOff);
                ShiftPlane.AssignHoldShiftPlane(pair.Key, pair.Value.ShiftPlane, pair.Value.EnabledWhenDecoderOff ? pair.Value.ShiftPlane : ShiftPlane.ShiftPlane_NONE);
                logger.Info(() => $"CommonTable HoldShift registered: deckey={pair.Key}, plane={pair.Value.ShiftPlane}, off={pair.Value.EnabledWhenDecoderOff}");
                foreach (var actionPair in pair.Value.Actions) {
                    InputActionResolver.RegisterHoldShiftAction(pair.Key, actionPair.Key, actionPair.Value);
                }
            }

            RegisterSystemModifiersAsHoldShift();
        }

        /// <summary>
        /// L/R Ctrl, Shift, Alt, Win を常設 HoldShift として登録する。
        /// </summary>
        private static void RegisterSystemModifiersAsHoldShift()
        {
            foreach (int deckey in systemModifierDeckeys) {
                Settings.SetHoldShiftKeySetting(deckey, ShiftPlane.ShiftPlane_SHIFT, true);
                ShiftPlane.AssignHoldShiftPlane(deckey, ShiftPlane.ShiftPlane_SHIFT, ShiftPlane.ShiftPlane_SHIFT);
                logger.Info(() => $"SystemModifier HoldShift registered: deckey={deckey}");
            }
        }

        /// <summary>
        /// 指定 deckey が commonTable で HoldShift キーとして定義されているかを返す。
        /// </summary>
        public static bool HasHoldShiftDefinition(int holdShiftDeckey)
        {
            return InputActionResolver.HasCommonTableHoldShiftDefinition(holdShiftDeckey);
        }

        /// <summary>
        /// commonTable 機能が組み込まれていることを返す互換メソッド。
        /// </summary>
        public static bool HasCommonTable()
        {
            return true;
        }

        public static string GetSingleHitRawTarget(int deckey)
        {
            return currentDefinition.SingleHitActions._safeGet(deckey)?.RawTarget;
        }

        public static string GetHoldShiftRawTarget(int holdShiftDeckey, int targetDeckey)
        {
            return currentDefinition.HoldShiftDefinitions._safeGet(holdShiftDeckey)?.Actions._safeGet(targetDeckey)?.RawTarget;
        }

        public static CommonTableHoldShiftDefinition GetHoldShiftDefinition(int holdShiftDeckey)
        {
            return currentDefinition.HoldShiftDefinitions._safeGet(holdShiftDeckey);
        }

        public static void UpdateSingleHit(int deckey, string rawTarget)
        {
            rawTarget = rawTarget._strip();
            if (rawTarget._isEmpty()) {
                if (currentDefinition.SingleHitActions.ContainsKey(deckey)) currentDefinition.SingleHitActions.Remove(deckey);
                return;
            }
            currentDefinition.SingleHitActions[deckey] = new CommonTableAction() {
                Deckey = SpecialKeysAndFunctions.GetDeckeyByName(rawTarget._startsWith("!{") && rawTarget._endsWith("}") ? rawTarget._safeSubstring(2, rawTarget.Length - 3) : ""),
                RequiresDecoder = !rawTarget._startsWith("!{"),
                RawTarget = rawTarget,
            };
        }

        public static void UpdateHoldShiftHeader(int holdShiftDeckey, int shiftPlane, bool enabledWhenDecoderOff)
        {
            if (shiftPlane <= 0) {
                if (currentDefinition.HoldShiftDefinitions.ContainsKey(holdShiftDeckey)) {
                    currentDefinition.HoldShiftDefinitions.Remove(holdShiftDeckey);
                }
                return;
            }
            var definition = currentDefinition.HoldShiftDefinitions._safeGet(holdShiftDeckey);
            if (definition == null) {
                definition = new CommonTableHoldShiftDefinition() {
                    HoldShiftDeckey = holdShiftDeckey,
                };
                currentDefinition.HoldShiftDefinitions[holdShiftDeckey] = definition;
            }
            definition.ShiftPlane = shiftPlane;
            definition.EnabledWhenDecoderOff = enabledWhenDecoderOff;
        }

        public static void UpdateHoldShiftTarget(int holdShiftDeckey, int targetDeckey, string rawTarget)
        {
            rawTarget = rawTarget._strip();
            var definition = currentDefinition.HoldShiftDefinitions._safeGet(holdShiftDeckey);
            if (rawTarget._isEmpty()) {
                if (definition != null && definition.Actions.ContainsKey(targetDeckey)) {
                    definition.Actions.Remove(targetDeckey);
                }
                return;
            }
            if (definition == null) {
                definition = new CommonTableHoldShiftDefinition() {
                    HoldShiftDeckey = holdShiftDeckey,
                    ShiftPlane = ShiftPlane.ShiftPlane_SHIFT,
                };
                currentDefinition.HoldShiftDefinitions[holdShiftDeckey] = definition;
            }
            definition.Actions[targetDeckey] = new CommonTableAction() {
                Deckey = SpecialKeysAndFunctions.GetDeckeyByName(rawTarget._startsWith("!{") && rawTarget._endsWith("}") ? rawTarget._safeSubstring(2, rawTarget.Length - 3) : ""),
                RequiresDecoder = !rawTarget._startsWith("!{"),
                RawTarget = rawTarget,
            };
        }

        public static string MakeCommonTableContents()
        {
            return MakeCommonTableContents(currentDefinition);
        }

        public static string MakeCommonTableContents(CommonTableDefinition sourceDefinition)
        {
            var sb = new StringBuilder();

            sb.AppendLine("#singleHit");
            foreach (var pair in sourceDefinition.SingleHitActions.OrderBy(x => x.Key)) {
                if (pair.Value?.RawTarget._notEmpty() == true) {
                    sb.AppendLine($"-{makeKeyName(pair.Key)}>{pair.Value.RawTarget}");
                }
            }
            sb.AppendLine("#end singleHit");
            sb.AppendLine();

            foreach (var pair in sourceDefinition.HoldShiftDefinitions.OrderBy(x => x.Key)) {
                var holdShiftDefinition = pair.Value;
                if (holdShiftDefinition == null || holdShiftDefinition.ShiftPlane <= 0) continue;
                var planeName = makePlaneName(holdShiftDefinition.ShiftPlane);
                var enabledOff = holdShiftDefinition.EnabledWhenDecoderOff ? " both" : "";
                sb.AppendLine($"#holdShift {makeKeyName(holdShiftDefinition.HoldShiftDeckey)} {planeName}{enabledOff}");
                sb.AppendLine("{");
                foreach (var actionPair in holdShiftDefinition.Actions.OrderBy(x => x.Key)) {
                    if (actionPair.Value?.RawTarget._notEmpty() == true) {
                        sb.AppendLine($"   -{makeKeyName(actionPair.Key)}>{actionPair.Value.RawTarget}");
                    }
                }
                sb.AppendLine("}");
                sb.AppendLine("#end HoldShift");
                sb.AppendLine();
            }

            return sb.ToString();
        }
    }

    /// <summary>
    /// 初回起動時に commonTable.txt が存在しない場合、legacy の mod-conversion.txt から生成する。
    /// 生成時に legacy パーサを一時利用するが、副作用は直後に初期化し直して消す。
    /// </summary>
    static class CommonTableMigrator
    {
        private static readonly Logger logger = Logger.GetLogger();
        private const string LegacyModConversionFile = "mod-conversion.txt";

        public static void EnsureCommonTableExists()
        {
            if (Settings.CommonTableFile._isEmpty()) return;

            var rootDir = KanchokuIni.Singleton.KanchokuDir;
            var commonTablePath = rootDir._joinPath(Settings.UserFilesFolder, Settings.CommonTableFile);
            if (Helper.FileExists(commonTablePath)) return;

            var legacyPath = rootDir._joinPath(Settings.UserFilesFolder, LegacyModConversionFile);
            if (!Helper.FileExists(legacyPath)) return;

            logger.Info(() => $"commonTable not found. Generate from legacy mod-conversion: {legacyPath} -> {commonTablePath}");
            var definition = convertFromLegacyModConversion();
            var contents = CommonTableRuntime.MakeCommonTableContents(definition);
            File.WriteAllText(commonTablePath, contents, new UTF8Encoding(false));
            logger.Info(() => $"Generated commonTable: singleHit={definition.SingleHitActions.Count}, holdShift={definition.HoldShiftDefinitions.Count}");
        }

        private static CommonTableDefinition convertFromLegacyModConversion()
        {
            var deckeyComboSnapshot = DeckeyComboMap.Snapshot();
            var resolverSnapshot = InputActionResolver.Snapshot();
            var shiftPlaneSnapshot = ShiftPlane.Snapshot();

            try {
                resetTemporaryLegacyState();
                var legacyDefinition = LegacyModConversionReader.Read(LegacyModConversionFile);

                var definition = new CommonTableDefinition();

                foreach (var pair in legacyDefinition.SingleHitDefs) {
                    definition.SingleHitActions[pair.Key] = new CommonTableAction() {
                        RawTarget = CommonTableDirectTargetHelper.NormalizeLegacyTarget(pair.Value),
                    };
                }

                foreach (var pair in legacyDefinition.ExtModifierKeyDefs) {
                    var modifierName = ModifierKeyRegistry.GetModifierNameByKey(pair.Key);
                    if (modifierName._isEmpty()) continue;

                    int holdShiftDeckey = SpecialKeysAndFunctions.GetDeckeyByName(modifierName);
                    if (holdShiftDeckey < 0) continue;

                    int shiftPlane = ShiftPlane.ShiftPlaneForShiftModKey.GetPlane(pair.Key);
                    int shiftPlaneWhenOff = ShiftPlane.ShiftPlaneForShiftModKeyWhenDecoderOff.GetPlane(pair.Key);
                    if (shiftPlane <= 0) shiftPlane = ShiftPlane.ShiftPlane_SHIFT;
                    bool enabledWhenDecoderOff = shiftPlaneWhenOff > 0;
                    if (enabledWhenDecoderOff && shiftPlaneWhenOff != shiftPlane) {
                        logger.Warn($"legacy mod-conversion has different decoder-off plane. use decoder-on plane: mod={modifierName}, on={shiftPlane}, off={shiftPlaneWhenOff}");
                    }

                    var holdShift = new CommonTableHoldShiftDefinition() {
                        HoldShiftDeckey = holdShiftDeckey,
                        ShiftPlane = shiftPlane,
                        EnabledWhenDecoderOff = enabledWhenDecoderOff && shiftPlaneWhenOff == shiftPlane,
                    };

                    foreach (var actionPair in pair.Value) {
                        holdShift.Actions[actionPair.Key] = new CommonTableAction() {
                            RawTarget = CommonTableDirectTargetHelper.NormalizeLegacyTarget(actionPair.Value),
                        };
                    }

                    definition.HoldShiftDefinitions[holdShiftDeckey] = holdShift;
                }

                return definition;
            } finally {
                DeckeyComboMap.Restore(deckeyComboSnapshot);
                InputActionResolver.Restore(resolverSnapshot);
                ShiftPlane.Restore(shiftPlaneSnapshot);
                ExtraModifiers.Initialize();
            }
        }

        private static void resetTemporaryLegacyState()
        {
            DeckeyComboMap.Initialize();
            InputActionResolver.Initialize();
            ModifierKeyRegistry.Initialize();
            ExtraModifiers.Initialize();
            ShiftPlane.InitializeShiftPlaneForShiftModKey();
        }
    }

    /// <summary>
    /// userFiles/commonTable.txt を解析して CommonTableDefinition を構築する。
    /// </summary>
    class CommonTableParser
    {
        private readonly Logger logger = Logger.GetLogger();
        private readonly PlaceHolders placeHolders = new PlaceHolders();

        /// <summary>
        /// commonTable ファイルを読み込み、singleHit/HoldShift 定義と複合コマンド定義を抽出する。
        /// </summary>
        public CommonTableDefinition Parse(string filename)
        {
            var definition = new CommonTableDefinition();
            if (filename._isEmpty()) return definition;

            var filePath = KanchokuIni.Singleton.KanchokuDir._joinPath(Settings.UserFilesFolder, filename);
            logger.Info(() => $"CommonTable file path={filePath}");
            var content = Helper.GetFileContent(filePath, Encoding.UTF8);
            if (content == null) {
                logger.Warn($"Can't read commonTable file: {filePath}");
                return definition;
            }

            var complexLines = new List<string>();
            CommonTableHoldShiftDefinition currentHoldShift = null;
            bool inSingleHit = false;
            int lineNo = 0;

            foreach (var rawLine in content._split('\n')) {
                ++lineNo;
                var line = rawLine._strip().TrimEnd('\r');
                if (line._isEmpty() || line._startsWith(";;")) continue;

                if (line._startsWith("#")) {
                    var lower = line._toLower();
                    if (lower == "#singlehit") {
                        inSingleHit = true;
                        currentHoldShift = null;
                        continue;
                    }
                    if (lower._startsWith("#end")) {
                        inSingleHit = false;
                        currentHoldShift = null;
                        continue;
                    }
                    if (lower._startsWith("#holdshift")) {
                        currentHoldShift = parseHoldShiftHeader(lineNo, line);
                        if (currentHoldShift != null) {
                            definition.HoldShiftDefinitions[currentHoldShift.HoldShiftDeckey] = currentHoldShift;
                        }
                        inSingleHit = false;
                        continue;
                    }
                    continue;
                }

                if (line == "{" || line == "}") continue;

                if (inSingleHit) {
                    parseSingleHitLine(definition, complexLines, lineNo, line);
                } else if (currentHoldShift != null) {
                    parseHoldShiftLine(currentHoldShift, complexLines, lineNo, line);
                }
            }

            definition.GeneratedComplexCommandString = complexLines._notEmpty() ? string.Join("\n", complexLines) + "\n" : null;
            return definition;
        }

        /// <summary>
        /// `#holdShift` ヘッダを解析して HoldShift ブロック定義を生成する。
        /// </summary>
        private CommonTableHoldShiftDefinition parseHoldShiftHeader(int lineNo, string line)
        {
            var items = line._reReplace(@"[ \t]{2,}", " ")._split(' ').Where(x => x._notEmpty()).ToArray();
            if (items.Length < 3) {
                logger.Warn($"commonTable line({lineNo}): invalid holdShift header: {line}");
                return null;
            }

            int holdShiftDeckey = SpecialKeysAndFunctions.GetDeckeyByName(items[1]);
            int plane = parseShiftPlane(items[2]);
            bool enabledWhenOff = items.Skip(3).Any(x => isEnabledWhenOffOption(x));
            if (holdShiftDeckey < 0 || plane <= 0) {
                logger.Warn($"commonTable line({lineNo}): invalid holdShift key or plane: {line}");
                return null;
            }

            return new CommonTableHoldShiftDefinition() {
                HoldShiftDeckey = holdShiftDeckey,
                ShiftPlane = plane,
                EnabledWhenDecoderOff = enabledWhenOff,
            };
        }

        /// <summary>
        /// `#singleHit` ブロックの 1 行を解析する。
        /// </summary>
        private void parseSingleHitLine(CommonTableDefinition definition, List<string> complexLines, int lineNo, string line)
        {
            splitArrowLine(line, out string lhs, out string rhs);
            if (lhs._isEmpty() || rhs._isEmpty()) {
                logger.Warn($"commonTable line({lineNo}): invalid singleHit line: {line}");
                return;
            }

            int sourceDeckey = resolveArrowKey(lhs);
            if (sourceDeckey < 0) {
                logger.Warn($"commonTable line({lineNo}): unresolved singleHit key: {lhs}");
                return;
            }

            var action = createAction(sourceDeckey, 0, rhs, complexLines);
            if (action != null) {
                definition.SingleHitActions[sourceDeckey] = action;
            }
        }

        /// <summary>
        /// HoldShift ブロック内の 1 行を解析する。
        /// </summary>
        private void parseHoldShiftLine(CommonTableHoldShiftDefinition holdShift, List<string> complexLines, int lineNo, string line)
        {
            splitArrowLine(line, out string lhs, out string rhs);
            if (lhs._isEmpty() || rhs._isEmpty()) {
                logger.Warn($"commonTable line({lineNo}): invalid holdShift line: {line}");
                return;
            }

            int sourceDeckey = resolveArrowKey(lhs);
            if (sourceDeckey < 0) {
                logger.Warn($"commonTable line({lineNo}): unresolved holdShift key: {lhs}");
                return;
            }

            var action = createAction(sourceDeckey, holdShift.ShiftPlane, rhs, complexLines);
            if (action != null) {
                holdShift.Actions[sourceDeckey] = action;
            }
        }

        /// <summary>
        /// target を直接 deckey として解決するか、複合コマンド定義へ展開するかを判定して action 化する。
        /// </summary>
        private CommonTableAction createAction(int sourceDeckey, int shiftPlane, string rawTarget, List<string> complexLines)
        {
            string stripped = rawTarget._strip();
            int deckey = tryParseDirectDeckey(stripped);
            if (deckey >= 0) {
                return new CommonTableAction() {
                    Deckey = deckey,
                    RawTarget = stripped,
                };
            }

            // HoldShift の複合コマンドは、デコーダ側では holdShiftDeckey 自体を識別に使わず、
            // 「shiftPlane + sourceDeckey」だけで syntheticDeckey を作る。
            // そのため、異なる HoldShift キーでも同じ PLANE かつ同じ sourceDeckey なら
            // デコーダ側では同一入力として扱われる。
            // 例:
            // - lctrl + X と rshift + X が同じ PLANE なら、同じ syntheticDeckey になり区別されない
            // - ただし sourceDeckey が異なれば、同じ PLANE でも syntheticDeckey は別になり衝突しない
            // - また direct deckey 解決される target はこの syntheticDeckey を使わない
            int syntheticDeckey = shiftPlane > 0 ? sourceDeckey + shiftPlane * DecoderKeys.PLANE_DECKEY_NUM : sourceDeckey;
            string strokeCode = (shiftPlane > 0 ? ShiftPlane.GetShiftPlanePrefix(shiftPlane) : "") + sourceDeckey.ToString();
            string decoderTarget = normalizeComplexTarget(stripped);
            complexLines.Add($"-{strokeCode}>{decoderTarget}");
            return new CommonTableAction() {
                Deckey = syntheticDeckey,
                RequiresDecoder = true,
                RawTarget = stripped,
            };
        }

        private int tryParseDirectDeckey(string target)
        {
            if (!target._startsWith("!{") || !target._endsWith("}")) return -1;

            string inner = target._safeSubstring(2, target.Length - 3)._strip();
            if (inner._isEmpty()) return -1;
            if (inner.Contains("!") || inner.Contains("{") || inner.Contains("}") || inner.Contains("+")) return -1;
            return CommonTableDirectTargetHelper.TryParseExpression(inner);
        }

        private string normalizeComplexTarget(string target)
        {
            if (target._startsWith("\"") && target._endsWith("\"")) return target;
            return target._startsWith("@") ? target : $"\"{target}\"";
        }

        private void splitArrowLine(string line, out string lhs, out string rhs)
        {
            int pos = line.IndexOf('>');
            if (pos < 0) {
                lhs = "";
                rhs = "";
            } else {
                lhs = line._safeSubstring(0, pos)._strip().TrimStart('-');
                rhs = line._safeSubstring(pos + 1)._strip();
            }
        }

        private int resolveArrowKey(string s)
        {
            if (s._isEmpty()) return -1;
            string key = s._toLower();
            int deckey = key._parseInt(-1);
            if (deckey >= 0) return deckey;

            deckey = placeHolders.Get(key);
            if (deckey >= 0) return deckey;

            if (key.Length == 1) {
                deckey = placeHolders.Get(key);
                if (deckey >= 0) return deckey;
            }

            return SpecialKeysAndFunctions.GetDeckeyByName(key);
        }

        private int parseShiftPlane(string planeWord)
        {
            switch (planeWord._toLower()) {
                case "s":
                case "shift":
                case "1":
                    return ShiftPlane.ShiftPlane_SHIFT;
                case "a":
                case "shifta":
                case "2":
                    return ShiftPlane.ShiftPlane_A;
                case "b":
                case "shiftb":
                case "3":
                    return ShiftPlane.ShiftPlane_B;
                case "c":
                case "shiftc":
                case "4":
                    return ShiftPlane.ShiftPlane_C;
                case "d":
                case "shiftd":
                case "5":
                    return ShiftPlane.ShiftPlane_D;
                case "e":
                case "shifte":
                case "6":
                    return ShiftPlane.ShiftPlane_E;
                case "f":
                case "shiftf":
                case "7":
                    return ShiftPlane.ShiftPlane_F;
                default:
                    return ShiftPlane.ShiftPlane_NONE;
            }
        }

        private bool isEnabledWhenOffOption(string word)
        {
            switch (word._toLower().Trim(',')) {
                case "both":
                case "all":
                case "off":
                case "offmode":
                case "enableoffmode":
                case "true":
                    return true;
                default:
                    return false;
            }
        }
    }
}
