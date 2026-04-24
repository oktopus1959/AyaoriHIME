using System;
using System.Collections.Generic;
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
            var sb = new StringBuilder();

            sb.AppendLine("#singleHit");
            foreach (var pair in currentDefinition.SingleHitActions.OrderBy(x => x.Key)) {
                if (pair.Value?.RawTarget._notEmpty() == true) {
                    sb.AppendLine($"-{makeKeyName(pair.Key)}>{pair.Value.RawTarget}");
                }
            }
            sb.AppendLine("#end singleHit");
            sb.AppendLine();

            foreach (var pair in currentDefinition.HoldShiftDefinitions.OrderBy(x => x.Key)) {
                var definition = pair.Value;
                if (definition == null || definition.ShiftPlane <= 0) continue;
                var planeName = ShiftPlane.GetShiftPlaneName(definition.ShiftPlane)._orElse("shift")._toLower();
                var enabledOff = definition.EnabledWhenDecoderOff ? " both" : "";
                sb.AppendLine($"#holdShift {makeKeyName(definition.HoldShiftDeckey)} {planeName}{enabledOff}");
                sb.AppendLine("{");
                foreach (var actionPair in definition.Actions.OrderBy(x => x.Key)) {
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
            if (inner.Contains("!") || inner.Contains("{") || inner.Contains("}") || inner.Contains("^") || inner.Contains("+")) return -1;
            return SpecialKeysAndFunctions.GetDeckeyByName(inner);
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
