using System.Collections.Generic;
using Utils;

namespace KanchokuWS.Domain
{
    static class ExtraModifiers
    {
        private static readonly Logger logger = Logger.GetLogger();

        /// <summary>デコーダ機能に割り当てられた拡張修飾キーの deckey 集合。</summary>
        private static HashSet<int> decoderFuncAssignedExModKeys = new HashSet<int>();

        public static void Initialize()
        {
            logger.Info("CALLED");
            decoderFuncAssignedExModKeys = new HashSet<int>();
        }

        /// <summary>拡張修飾キーをデコーダ機能に割り当てられたキー集合へ追加する。</summary>
        public static void AddExModVkeyAssignedForDecoderFuncByVkey(int deckey)
        {
            decoderFuncAssignedExModKeys.Add(deckey);
        }

        /// <summary>インデックスで指定される拡張修飾キーをデコーダ機能に割り当てられたキー集合へ追加する。</summary>
        public static void AddExModVkeyAssignedForDecoderFuncByIndex(int idx)
        {
            if (idx > 0) decoderFuncAssignedExModKeys.Add(DecoderKeys.FUNC_DECKEY_START + idx);
        }

        /// <summary>拡張修飾キーがデコーダ機能に割り当てられているか。</summary>
        public static bool IsExModKeyIndexAssignedForDecoderFunc(int exModeDeckey)
        {
            return decoderFuncAssignedExModKeys.Contains(exModeDeckey);
        }
    }
}
