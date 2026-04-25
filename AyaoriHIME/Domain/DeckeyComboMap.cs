using Utils;

namespace KanchokuWS.Domain
{
    /// <summary>
    /// 修飾キーとNormalDecKeyの組み合わせ
    /// </summary>
    struct KeyCombo
    {
        public uint modifier;
        public int normalDecKey;

        public KeyCombo(uint mod, int dc)
        {
            modifier = mod;
            normalDecKey = dc;
        }
    }

    static class DeckeyComboMap
    {
        private static readonly Logger logger = Logger.GetLogger();

        /// <summary>
        /// DECKEY id から仮想キーコンビネーションを得るための配列
        /// </summary>
        private static KeyCombo?[] keyComboFromDecKey;

        public static void Initialize()
        {
            logger.Info("ENTER");
            keyComboFromDecKey = new KeyCombo?[DecoderKeys.GLOBAL_DECKEY_ID_END];
            logger.Info("LEAVE");
        }

        public static KeyCombo?[] Snapshot()
        {
            return keyComboFromDecKey == null ? null : (KeyCombo?[])keyComboFromDecKey.Clone();
        }

        public static void Restore(KeyCombo?[] snapshot)
        {
            keyComboFromDecKey = snapshot == null ? new KeyCombo?[DecoderKeys.GLOBAL_DECKEY_ID_END] : (KeyCombo?[])snapshot.Clone();
        }

        /// <summary>
        /// xfer, nfer など特殊キーに割り当てられている DecoderKey を登録
        /// </summary>
        public static void RegisterSpecialDeckey(string name, int deckey)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"name={name}, deckey={deckey:x}H({deckey})");
            if (deckey > 0) {
                uint vk = DecoderKeyVsVKey.GetFuncVkeyByName(name);
                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"vk={vk}");
                if (vk > 0) {
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"keyComboFromDecKey[{deckey}]=KeyCombo(0, {deckey})");
                    keyComboFromDecKey[deckey] = new KeyCombo(0, deckey);
                }
            }
        }

        /// <summary>修飾されたDecKeyから、修飾子とNormalDecKeyへの変換を登録する</summary>
        public static void RegisterModifiedDeckey(int modDeckey, uint mod, int arrangedDeckey)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"modDeckey={modDeckey:x}H({modDeckey}), mod={mod:x}H, arrangedDeckey={arrangedDeckey:x}H({arrangedDeckey})");
            keyComboFromDecKey[modDeckey] = new KeyCombo(mod, arrangedDeckey);
        }

        /// <summary>修飾子と仮想キーコードの組みから、DecKey への逆引きを登録する</summary>
        public static void Register(int deckey, uint modifier, int normalDeckey, bool fromComboOnly = false)
        {
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"deckey={deckey:x}H({deckey}), modifier={modifier:x}H, normalDeckey={normalDeckey:x}H({normalDeckey}), fromComboOnly={fromComboOnly}");
            if (!fromComboOnly) {
                keyComboFromDecKey[deckey] = new KeyCombo(modifier, normalDeckey);
            }
        }

        /// <summary>
        /// DECKEY id から仮想キーと修飾子のコンビネーションを得る
        /// </summary>
        public static bool TryGet(int deckey, out KeyCombo combo)
        {
            var found = keyComboFromDecKey._getNth(deckey);
            if (Settings.LoggingDecKeyInfo) logger.Info(() => $"deckey={deckey:x}H({deckey}), combo.mod={(found.HasValue ? found.Value.modifier : 0):x}, combo.normalDecKey={(found.HasValue ? found.Value.normalDecKey : 0)}");
            combo = found ?? default;
            return found.HasValue;
        }
    }
}
