using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using KanchokuWS.TableParser;
using Utils;

namespace KanchokuWS.Domain
{
    public class CommonTableAction
    {
        public int Deckey { get; set; }
        public bool RequiresDecoder { get; set; }
        public string RawTarget { get; set; }
    }

    public class CommonTableHoldShiftDefinition
    {
        public int HoldShiftDeckey { get; set; }
        public int ShiftPlane { get; set; }
        public bool EnabledWhenDecoderOff { get; set; }
        public Dictionary<int, CommonTableAction> Actions { get; } = new Dictionary<int, CommonTableAction>();
    }

    public class CommonTableDefinition
    {
        public Dictionary<int, CommonTableAction> SingleHitActions { get; } = new Dictionary<int, CommonTableAction>();
        public Dictionary<int, CommonTableHoldShiftDefinition> HoldShiftDefinitions { get; } = new Dictionary<int, CommonTableHoldShiftDefinition>();
        public string GeneratedComplexCommandString { get; set; }
    }

    static class CommonTableRuntime
    {
        private static readonly Logger logger = Logger.GetLogger();

        private static Dictionary<int, CommonTableAction> singleHitActions = new Dictionary<int, CommonTableAction>();
        private static Dictionary<int, CommonTableHoldShiftDefinition> holdShiftDefinitions = new Dictionary<int, CommonTableHoldShiftDefinition>();
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

        public static void Clear()
        {
            singleHitActions.Clear();
            holdShiftDefinitions.Clear();
        }

        public static void Initialize(CommonTableDefinition definition)
        {
            Clear();
            if (definition == null) return;

            foreach (var pair in definition.SingleHitActions) {
                singleHitActions[pair.Key] = pair.Value;
            }
            foreach (var pair in definition.HoldShiftDefinitions) {
                holdShiftDefinitions[pair.Key] = pair.Value;
                Settings.SetHoldShiftKeySetting(pair.Key, pair.Value.ShiftPlane, pair.Value.EnabledWhenDecoderOff);
                ShiftPlane.AssignHoldShiftPlane(pair.Key, pair.Value.ShiftPlane, pair.Value.EnabledWhenDecoderOff ? pair.Value.ShiftPlane : ShiftPlane.ShiftPlane_NONE);
                logger.Info(() => $"CommonTable HoldShift registered: deckey={pair.Key}, plane={pair.Value.ShiftPlane}, off={pair.Value.EnabledWhenDecoderOff}");
            }

            RegisterSystemModifiersAsHoldShift();
        }

        private static void RegisterSystemModifiersAsHoldShift()
        {
            foreach (int deckey in systemModifierDeckeys) {
                Settings.SetHoldShiftKeySetting(deckey, ShiftPlane.ShiftPlane_SHIFT, true);
                ShiftPlane.AssignHoldShiftPlane(deckey, ShiftPlane.ShiftPlane_SHIFT, ShiftPlane.ShiftPlane_SHIFT);
                logger.Info(() => $"SystemModifier HoldShift registered: deckey={deckey}");
            }
        }

        public static bool TryGetSingleHitAction(int deckey, out CommonTableAction action)
        {
            action = singleHitActions._safeGet(deckey);
            return action != null;
        }

        public static bool TryGetHoldShiftAction(int holdShiftDeckey, int targetDeckey, out CommonTableAction action)
        {
            action = holdShiftDefinitions._safeGet(holdShiftDeckey)?.Actions._safeGet(targetDeckey);
            return action != null;
        }

        public static bool HasHoldShiftDefinition(int holdShiftDeckey)
        {
            return holdShiftDefinitions.ContainsKey(holdShiftDeckey);
        }

        public static bool HasCommonTable()
        {
            return singleHitActions._notEmpty() || holdShiftDefinitions._notEmpty();
        }
    }

    class CommonTableParser
    {
        private readonly Logger logger = Logger.GetLogger();
        private readonly PlaceHolders placeHolders = new PlaceHolders();

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
