using System.Collections.Generic;
using Utils;

namespace KanchokuWS.Domain
{
    static class ModifierKeyRegistry
    {
        /// <summary>無効化された拡張修飾キー</summary>
        private static HashSet<string> disabledExtKeys = new HashSet<string>();

        private static HashSet<string> disabledExtKeyLines = new HashSet<string>();

        private static Dictionary<string, uint> modifierKeysFromName = new Dictionary<string, uint>() {
            {"space", KeyModifiers.MOD_SPACE },
            {"caps", KeyModifiers.MOD_CAPS },
            {"alnum", KeyModifiers.MOD_ALNUM },
            {"nfer", KeyModifiers.MOD_NFER },
            {"xfer", KeyModifiers.MOD_XFER },
            {"kana", KeyModifiers.MOD_SINGLE },
            {"lctrl", KeyModifiers.MOD_LCTRL },
            {"rctrl", KeyModifiers.MOD_RCTRL },
            {"shift", KeyModifiers.MOD_SHIFT },
            {"rshift", KeyModifiers.MOD_RSHIFT },
            {"zenkaku", KeyModifiers.MOD_SINGLE },
        };

        private static Dictionary<string, uint> modifierKeysFromName2 = new Dictionary<string, uint>() {
            {"eisu", KeyModifiers.MOD_ALNUM },
            {"muhenkan", KeyModifiers.MOD_NFER },
            {"henkan", KeyModifiers.MOD_XFER },
        };

        public static void Initialize()
        {
            disabledExtKeys = new HashSet<string>();
            disabledExtKeyLines.Clear();
        }

        public static void AddDisabledExtKey(string name)
        {
            var canonicalName = SpecialKeysAndFunctions.GetCanonicalName(name);
            disabledExtKeys.Add(canonicalName);
        }

        public static bool IsDisabledExtKey(string name)
        {
            var canonicalName = SpecialKeysAndFunctions.GetCanonicalName(name);
            return disabledExtKeys.Contains(canonicalName);
        }

        public static void AddDisabledExtKeyLine(string line)
        {
            disabledExtKeyLines.Add(line);
        }

        public static uint GetModifierKeyByName(string name)
        {
            return modifierKeysFromName._safeGet(name)._gtZeroOr(() => modifierKeysFromName2._safeGet(name));
        }

        public static string GetModifierNameByKey(uint modKey)
        {
            foreach (var pair in modifierKeysFromName) {
                if (pair.Value == modKey) return pair.Key;
            }
            return null;
        }

        public static int CalcShiftOffset(char shiftChar)
        {
            return shiftChar == 'S' || shiftChar == 's' ? DecoderKeys.SHIFT_DECKEY_START
                : shiftChar >= 'A' && shiftChar <= 'F' ? DecoderKeys.SHIFT_A_DECKEY_START + (shiftChar - 'A') * DecoderKeys.PLANE_DECKEY_NUM
                : shiftChar >= 'a' && shiftChar <= 'f' ? DecoderKeys.SHIFT_A_DECKEY_START + (shiftChar - 'a') * DecoderKeys.PLANE_DECKEY_NUM
                : 0;
        }
    }
}
