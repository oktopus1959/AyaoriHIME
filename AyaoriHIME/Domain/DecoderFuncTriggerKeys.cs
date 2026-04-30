using System.Collections.Generic;
using Utils;

namespace KanchokuWS.Domain
{
    static class DecoderFuncTriggerKeys
    {
        private static readonly Logger logger = Logger.GetLogger();

        /// <summary>デコーダ機能を単打で起動する入力 deckey の集合。</summary>
        private static HashSet<int> decoderFuncTriggerDeckeys = new HashSet<int>();

        public static void Initialize()
        {
            logger.Info("CALLED");
            decoderFuncTriggerDeckeys = new HashSet<int>();
        }

        /// <summary>指定の入力 deckey を、デコーダ機能の単打トリガー集合へ追加する。</summary>
        public static void AddDecoderFuncTriggerKey(int deckey)
        {
            decoderFuncTriggerDeckeys.Add(deckey);
        }

        /// <summary>インデックスで指定される機能キーを、デコーダ機能の単打トリガー集合へ追加する。</summary>
        public static void AddDecoderFuncTriggerKeyByIndex(int idx)
        {
            if (idx > 0) decoderFuncTriggerDeckeys.Add(DecoderKeys.FUNC_DECKEY_START + idx);
        }

        /// <summary>指定の入力 deckey が、デコーダ機能の単打トリガーとして登録されているか。</summary>
        public static bool IsDecoderFuncTriggerKey(int deckey)
        {
            return decoderFuncTriggerDeckeys.Contains(deckey);
        }
    }
}
