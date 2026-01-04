#pragma once

#include "string_utils.h"

namespace analyzer
{
    typedef unsigned int PVT;

    /**
     * Character Info
     */
    class CharInfo {
    private:
      // shift bit length
        static const unsigned int CHRTYP_SHIFT = 14;
        static const unsigned int PRITYP_SHIFT = 6;
        static const unsigned int LENGTH_SHIFT = 2;
        static const unsigned int GROUP_SHIFT = 1;
        static const unsigned int INVOKE_SHIFT = 0;

        // static const unsigned intue mask
        static const unsigned int CHRTYP_VMASK = 0x3ffff;
        static const unsigned int PRITYP_VMASK = 0xff;
        static const unsigned int LENGTH_VMASK = 0xf;
        static const unsigned int GROUP_VMASK = 0x1;
        static const unsigned int INVOKE_VMASK = 0x1;

        // negative mask
        static const unsigned int CHRTYP_NMASK = 0x00003fff;
        static const unsigned int PRITYP_NMASK = 0xffffc03f;
        static const unsigned int LENGTH_NMASK = 0xffffffc3;
        static const unsigned int GROUP_NMASK = 0xfffffffd;
        static const unsigned int INVOKE_NMASK = 0xfffffffe;

        // positive mask
        static const unsigned int CHRTYP_PMASK = 0xffffc000;
        static const unsigned int PRITYP_PMASK = 0x00003fc0;
        static const unsigned int LENGTH_PMASK = 0x0000003c;
        static const unsigned int GROUP_PMASK = 0x00000002;
        static const unsigned int INVOKE_PMASK = 0x00000001;

    public:
        inline static unsigned int apply(int chrTyp, int invoke, int group, int length, int priTyp) {
            return ((chrTyp << CHRTYP_SHIFT) & CHRTYP_PMASK) |
                ((invoke << INVOKE_SHIFT) & INVOKE_PMASK) |
                ((group << GROUP_SHIFT) & GROUP_PMASK) |
                ((length << LENGTH_SHIFT) & LENGTH_PMASK) |
                ((priTyp << PRITYP_SHIFT) & PRITYP_PMASK);
        }

        inline static unsigned int charType(PVT pv) {
            return (pv >> CHRTYP_SHIFT) & CHRTYP_VMASK;
        }

        inline static unsigned int primaryType(PVT pv) {
            return (pv >> PRITYP_SHIFT) & PRITYP_VMASK;
        }

        inline static unsigned int length(PVT pv) {
            return (pv >> LENGTH_SHIFT) & LENGTH_VMASK;
        }

    private:
        //  def group      (pv:PVT) = (pv >>> GROUP_SHIFT)  & GROUP_VMASK
        bool group(PVT pv) {
            return (pv & GROUP_PMASK) != 0;
        }

        //  def invoke     (pv:PVT) = (pv >>> INVOKE_SHIFT) & INVOKE_VMASK
        bool invoke(PVT pv) {
            return (pv & INVOKE_PMASK) != 0;
        }

    public:
        inline static unsigned int addCharType(PVT pv, int v) {
            return (pv) | ((v << CHRTYP_SHIFT) & CHRTYP_PMASK);
        }

        inline static unsigned int setCharType(PVT pv, int v) {
            return (pv & CHRTYP_NMASK) | ((v << CHRTYP_SHIFT) & CHRTYP_PMASK);
        }

        inline static unsigned int setPrimaryType(PVT pv, int v) {
            return (pv & PRITYP_NMASK) | ((v << PRITYP_SHIFT) & PRITYP_PMASK);
        }

        inline static unsigned int setLength(PVT pv, int v) {
            return (pv & LENGTH_NMASK) | ((v << LENGTH_SHIFT) & LENGTH_PMASK);
        }

        inline static unsigned int setGroup(PVT pv, int v) {
            return (pv & GROUP_NMASK) | ((v << GROUP_SHIFT) & GROUP_PMASK);
        }

        inline static unsigned int setInvoke(PVT pv, int v) {
            return (pv & INVOKE_NMASK) | ((v << INVOKE_SHIFT) & INVOKE_PMASK);
        }

        inline static unsigned int typeToBitflag(int typ) {
            return 1 << typ;
        }

        inline static unsigned int primaryBitflag(PVT pv) {
            return typeToBitflag(primaryType(pv));
        }

        inline static unsigned int primaryOnly(PVT pv) {
            return setCharType(pv, primaryBitflag(pv));
        }

        inline static bool isSameKind(PVT v1, PVT v2) {
            return ((v1 & v2) & CHRTYP_PMASK) != 0;
        }

        //  def isSameKind(ci1:CharInfo, ci2:CharInfo): Boolean = isSameKind(ci1.packed_val, ci2.packed_val)
    public:
        PVT packed_val = 0;

        CharInfo(PVT pval = 0) : packed_val(pval) {}

        /**
         * ビットフラグにより、所属するカテゴリーグループを定義する
         */
        unsigned int charType() {
            return charType(packed_val);
        }

        /**
         * メインで所属するカテゴリーのID
         */
        unsigned int primaryType() {
            return primaryType(packed_val);
        }

        /**
         * 未知語のスパン制限 ( 1 ～ length 文字までの未知語が追加される)
         */
        unsigned int spanLimit() {
            return length(packed_val);
        }

        /**
         * 同じカテゴリーの文字が連続している場合にそれをまとめて新しい未知語を作るか否か
         */
        bool group() {
            return group(packed_val);
        }

        /**
         * 未知語処理を起動するか否か
         */
        bool invoke() {
            return invoke(packed_val);
        }

        /**
         * 主カテゴリー以外のフラグをOFFにした CharInfo を返す
         */
        CharInfo primarize() {
            return CharInfo(primaryOnly(packed_val));
        }

        String toString() {
            return std::format(L"charType:{}, defType={}, lenghth={}, group={}, invoke={}",
                toBinaryString(charType()), primaryType(), spanLimit(), group(), invoke());
        }

        private:
            String toBinaryString(unsigned int v) {
                String result;
                unsigned int flag = 0x80000000;
                for (int i = 0; i < 32; ++i) {
                    result.push_back((v & flag) != 0 ? '1' : '0');
                    flag >>= 1;
                }
                return result;
            }

    };
} // namespace analyzer
