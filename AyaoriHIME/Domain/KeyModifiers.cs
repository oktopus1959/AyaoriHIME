using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KanchokuWS.Domain
{
    static class KeyModifiers
    {
        // VKEY に対する modifier ALT
        public const uint MOD_ALT = 0x0001;

        // VKEY に対する modifier CTRL
        public const uint MOD_CONTROL = 0x0002;

        // VKEY に対する modifier SHIFT
        public const uint MOD_SHIFT = 0x0004;

        // VKEY に対する modifier WIN
        public const uint MOD_WIN = 0x0008;

        // VKEY に対する modifier Space
        public const uint MOD_SPACE = 0x0100;

        // VKEY に対する modifier CapsLock
        public const uint MOD_CAPS = 0x0200;

        // VKEY に対する modifier 英数
        public const uint MOD_ALNUM = 0x0400;

        // VKEY に対する modifier NFER
        public const uint MOD_NFER = 0x0800;

        // VKEY に対する modifier XFER
        public const uint MOD_XFER = 0x1000;

        // VKEY に対する modifier RSHIFT
        public const uint MOD_RSHIFT = 0x2000;

        // VKEY に対する modifier LCTRL
        public const uint MOD_LCTRL = 0x4000;

        // VKEY に対する modifier RCTRL
        public const uint MOD_RCTRL = 0x8000;

        // 単打用キー
        public const uint MOD_SINGLE = 0x10000;

        // VKEY に対する modifier LSHIFT
        public const uint MOD_LSHIFT = 0x20000;

        public static uint MakeModifier(bool alt, bool ctrl, bool shift)
        {
            return (alt ? MOD_ALT : 0) + (ctrl ? MOD_CONTROL : 0) + (shift ? MOD_SHIFT : 0);
        }

        public static string ToDebugString(uint modifier)
        {
            List<string> list = new List<string>();
            if ((modifier & MOD_ALT) != 0) list.Add("ALT");
            if ((modifier & MOD_CONTROL) != 0) list.Add("CTRL");
            if ((modifier & MOD_SHIFT) != 0) list.Add("SHIFT");
            if ((modifier & MOD_WIN) != 0) list.Add("WIN");
            if ((modifier & MOD_SPACE) != 0) list.Add("SPACE");
            if ((modifier & MOD_CAPS) != 0) list.Add("CAPS");
            if ((modifier & MOD_ALNUM) != 0) list.Add("ALNUM");
            if ((modifier & MOD_NFER) != 0) list.Add("NFER");
            if ((modifier & MOD_XFER) != 0) list.Add("XFER");
            if ((modifier & MOD_RSHIFT) != 0) list.Add("RSHIFT");
            if ((modifier & MOD_LCTRL) != 0) list.Add("LCTRL");
            if ((modifier & MOD_RCTRL) != 0) list.Add("RCTRL");
            if ((modifier & MOD_SINGLE) != 0) list.Add("SINGLE");
            if ((modifier & MOD_LSHIFT) != 0) list.Add("LSHIFT");
            return $"{modifier:x}({string.Join("+", list)})";
        }
    }

}
