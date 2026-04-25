using System.Collections.Generic;
using System.Text;
using Utils;

namespace KanchokuWS.Domain
{
    /// <summary>
    /// `mod-conversion.txt` の移行用読み取り結果。
    /// runtime では使わず、`commonTable.txt` 自動生成時だけ利用する。
    /// </summary>
    class LegacyModConversionDefinition
    {
        public Dictionary<int, string> SingleHitDefs { get; } = new Dictionary<int, string>();
        public Dictionary<uint, Dictionary<int, string>> ExtModifierKeyDefs { get; } = new Dictionary<uint, Dictionary<int, string>>();
    }

    /// <summary>
    /// `mod-conversion.txt` 互換の移行専用 reader。
    /// 通常 runtime の入力解決には使わず、`commonTable.txt` が存在しない場合の
    /// 初回移行でだけ legacy 定義を読む。
    /// </summary>
    static class LegacyModConversionReader
    {
        private static readonly Logger logger = Logger.GetLogger();

        private static readonly Dictionary<string, int> specialDecKeysFromName = new Dictionary<string, int>() {
            {"esc", DecoderKeys.ESC_DECKEY},
            {"escape", DecoderKeys.ESC_DECKEY},
            {"zenkaku", DecoderKeys.HANZEN_DECKEY },
            {"hanzen", DecoderKeys.HANZEN_DECKEY },
            {"tab", DecoderKeys.TAB_DECKEY},
            {"caps", DecoderKeys.CAPS_DECKEY },
            {"capslock", DecoderKeys.CAPS_DECKEY },
            {"alnum", DecoderKeys.ALNUM_DECKEY },
            {"alphanum", DecoderKeys.ALNUM_DECKEY },
            {"eisu", DecoderKeys.ALNUM_DECKEY },
            {"nfer", DecoderKeys.NFER_DECKEY },
            {"muhenkan", DecoderKeys.NFER_DECKEY },
            {"xfer", DecoderKeys.XFER_DECKEY },
            {"henkan", DecoderKeys.XFER_DECKEY },
            {"kana", DecoderKeys.KANA_DECKEY },
            {"hiragana", DecoderKeys.KANA_DECKEY },
            {"bs", DecoderKeys.BS_DECKEY },
            {"back", DecoderKeys.BS_DECKEY },
            {"backspace", DecoderKeys.BS_DECKEY },
            {"enter", DecoderKeys.ENTER_DECKEY},
            {"ins", DecoderKeys.INS_DECKEY},
            {"insert", DecoderKeys.INS_DECKEY},
            {"del", DecoderKeys.DEL_DECKEY},
            {"delete", DecoderKeys.DEL_DECKEY},
            {"home", DecoderKeys.HOME_DECKEY},
            {"end", DecoderKeys.END_DECKEY},
            {"pgup", DecoderKeys.PAGE_UP_DECKEY},
            {"pageup", DecoderKeys.PAGE_UP_DECKEY},
            {"pgdn", DecoderKeys.PAGE_DOWN_DECKEY},
            {"pagedown", DecoderKeys.PAGE_DOWN_DECKEY},
            {"left", DecoderKeys.LEFT_ARROW_DECKEY},
            {"leftarrow", DecoderKeys.LEFT_ARROW_DECKEY},
            {"right", DecoderKeys.RIGHT_ARROW_DECKEY},
            {"rightarrow", DecoderKeys.RIGHT_ARROW_DECKEY},
            {"up", DecoderKeys.UP_ARROW_DECKEY},
            {"uparrow", DecoderKeys.UP_ARROW_DECKEY},
            {"down", DecoderKeys.DOWN_ARROW_DECKEY},
            {"downarrow", DecoderKeys.DOWN_ARROW_DECKEY},
            {"lctrl", DecoderKeys.LEFT_CONTROL_DECKEY},
            {"rctrl", DecoderKeys.RIGHT_CONTROL_DECKEY},
            {"lshift", DecoderKeys.LEFT_SHIFT_DECKEY},
            {"rshift", DecoderKeys.RIGHT_SHIFT_DECKEY},
            {"lalt", DecoderKeys.LEFT_ALT_DECKEY},
            {"ralt", DecoderKeys.RIGHT_ALT_DECKEY},
            {"lwin", DecoderKeys.LEFT_WIN_DECKEY},
            {"rwin", DecoderKeys.RIGHT_WIN_DECKEY},
            {"scrlock", DecoderKeys.SCR_LOCK_DECKEY},
            {"pause", DecoderKeys.PAUSE_DECKEY},
            {"imeon", DecoderKeys.IME_ON_DECKEY},
            {"imeoff", DecoderKeys.IME_OFF_DECKEY},
            {"fn1", DecoderKeys.F1_DECKEY},
            {"fn2", DecoderKeys.F2_DECKEY},
            {"fn3", DecoderKeys.F3_DECKEY},
            {"fn4", DecoderKeys.F4_DECKEY},
            {"fn5", DecoderKeys.F5_DECKEY},
            {"fn6", DecoderKeys.F6_DECKEY},
            {"fn7", DecoderKeys.F7_DECKEY},
            {"fn8", DecoderKeys.F8_DECKEY},
            {"fn9", DecoderKeys.F9_DECKEY},
            {"fn10", DecoderKeys.F10_DECKEY},
            {"fn11", DecoderKeys.F11_DECKEY},
            {"fn12", DecoderKeys.F12_DECKEY},
            {"fn13", DecoderKeys.F13_DECKEY},
            {"fn14", DecoderKeys.F14_DECKEY},
            {"fn15", DecoderKeys.F15_DECKEY},
            {"fn16", DecoderKeys.F16_DECKEY},
            {"space", DecoderKeys.STROKE_SPACE_DECKEY},
            {"shiftspace", DecoderKeys.SHIFT_SPACE_DECKEY},
            {"directspace", DecoderKeys.DIRECT_SPACE_DECKEY},
            {"modetoggle", DecoderKeys.TOGGLE_DECKEY},
            {"modetogglefollowcaret", DecoderKeys.MODE_TOGGLE_FOLLOW_CARET_DECKEY},
            {"activate", DecoderKeys.ACTIVE_DECKEY},
            {"deactivate", DecoderKeys.DEACTIVE_DECKEY},
            {"fullescape", DecoderKeys.FULL_ESCAPE_DECKEY},
            {"flushoutputstring", DecoderKeys.FLUSH_OUTPUT_STRING_DECKEY},
            {"toggleeditwindow", DecoderKeys.EDIT_WINDOW_TOGGLE_DECKEY},
            {"strokeback", DecoderKeys.STROKE_BACK_DECKEY},
            {"bushucomp", DecoderKeys.BUSHU_COMP_DECKEY},
            {"unblock", DecoderKeys.UNBLOCK_DECKEY},
            {"toggleblocker", DecoderKeys.TOGGLE_BLOCKER_DECKEY},
            {"blockertoggle", DecoderKeys.TOGGLE_BLOCKER_DECKEY},
            {"vkbshowhide", DecoderKeys.VKB_SHOW_HIDE_DECKEY},
            {"helprotate", DecoderKeys.STROKE_HELP_ROTATION_DECKEY},
            {"helpunrotate", DecoderKeys.STROKE_HELP_UNROTATION_DECKEY},
            {"daterotate", DecoderKeys.DATE_STRING_ROTATION_DECKEY},
            {"dateunrotate", DecoderKeys.DATE_STRING_UNROTATION_DECKEY},
            {"histfullcand", DecoderKeys.HISTORY_FULL_CAND_DECKEY},
            {"histfewcharscand", DecoderKeys.HISTORY_FEW_CHARS_CAND_DECKEY},
            {"histonecharcand", DecoderKeys.HISTORY_ONE_CHAR_CAND_DECKEY},
            {"histnext", DecoderKeys.HISTORY_NEXT_SEARCH_DECKEY},
            {"histprev", DecoderKeys.HISTORY_PREV_SEARCH_DECKEY},
            {"strokehelp", DecoderKeys.STROKE_HELP_DECKEY},
            {"bushucomphelp", DecoderKeys.BUSHU_COMP_HELP_DECKEY},
            {"zenkakuconvert", DecoderKeys.TOGGLE_ZENKAKU_CONVERSION_DECKEY},
            {"zenkakuconversion", DecoderKeys.TOGGLE_ZENKAKU_CONVERSION_DECKEY},
            {"katakanaconvert", DecoderKeys.TOGGLE_KATAKANA_CONVERSION_DECKEY},
            {"katakanaconversion", DecoderKeys.TOGGLE_KATAKANA_CONVERSION_DECKEY},
            {"katakanaconversion1", DecoderKeys.TOGGLE_KATAKANA_CONVERSION1_DECKEY},
            {"katakanaconversion2", DecoderKeys.TOGGLE_KATAKANA_CONVERSION2_DECKEY},
            {"eisumodetoggle", DecoderKeys.EISU_MODE_TOGGLE_DECKEY},
            {"eisumodecancel", DecoderKeys.EISU_MODE_CANCEL_DECKEY},
            {"eisuconversion", DecoderKeys.EISU_CONVERSION_DECKEY},
            {"eisudecapitalize", DecoderKeys.EISU_DECAPITALIZE_DECKEY},
            {"romanstrokeguide", DecoderKeys.TOGGLE_ROMAN_STROKE_GUIDE_DECKEY},
            {"upperromanstrokeguide", DecoderKeys.TOGGLE_UPPER_ROMAN_STROKE_GUIDE_DECKEY},
            {"hiraganastrokeguide", DecoderKeys.TOGGLE_HIRAGANA_STROKE_GUIDE_DECKEY},
            {"exchangecodetable", DecoderKeys.EXCHANGE_CODE_TABLE_DECKEY},
            {"exchangecodetable2", DecoderKeys.EXCHANGE_CODE_TABLE2_DECKEY},
            {"selectcodetable1", DecoderKeys.SELECT_CODE_TABLE1_DECKEY},
            {"selectcodetable2", DecoderKeys.SELECT_CODE_TABLE2_DECKEY},
            {"selectcodetable3", DecoderKeys.SELECT_CODE_TABLE3_DECKEY},
            {"multistreamtoggle", DecoderKeys.MULTI_STREAM_MODE_TOGGLE_DECKEY},
            {"multistreamnext", DecoderKeys.MULTI_STREAM_NEXT_CAND_DECKEY},
            {"multistreamprev", DecoderKeys.MULTI_STREAM_PREV_CAND_DECKEY},
            {"multistreamselectfirst", DecoderKeys.MULTI_STREAM_SELECT_FIRST_DECKEY},
            {"multistreamcommit", DecoderKeys.MULTI_STREAM_COMMIT_DECKEY},
            {"multistreamhiraganapreferrednext", DecoderKeys.MULTI_STREAM_HIRAGANA_PREFERRED_NEXT_DECKEY},
            {"multistreamkanjipreferrednext", DecoderKeys.MULTI_STREAM_KANJI_PREFERRED_NEXT_DECKEY},
            {"kanatrainingtoggle", DecoderKeys.KANA_TRAINING_TOGGLE_DECKEY},
            {"mazeconversion", DecoderKeys.MAZE_CONVERSION_DECKEY},
            {"leftshiftblocker", DecoderKeys.LEFT_SHIFT_BLOCKER_DECKEY},
            {"rightshiftblocker", DecoderKeys.RIGHT_SHIFT_BLOCKER_DECKEY},
            {"leftshiftmazestartpos", DecoderKeys.LEFT_SHIFT_MAZE_START_POS_DECKEY},
            {"rightshiftmazestartpos", DecoderKeys.RIGHT_SHIFT_MAZE_START_POS_DECKEY},
            {"copyandregisterselection", DecoderKeys.COPY_SELECTION_AND_SEND_TO_DICTIONARY_DECKEY},
            {"copyselectionandsendtodictionary", DecoderKeys.COPY_SELECTION_AND_SEND_TO_DICTIONARY_DECKEY},
            {"clearstroke", DecoderKeys.CLEAR_STROKE_DECKEY},
        };

        /// <summary>
        /// legacy `mod-conversion.txt` を読み取り、`commonTable` へ移すための定義に変換する。
        /// 読み取り中に必要な shift plane や legacy combo 登録は一時状態へ反映される。
        /// </summary>
        public static LegacyModConversionDefinition Read(string filename)
        {
            logger.Info("ENTER");
            var definition = new LegacyModConversionDefinition();
            if (filename._notEmpty()) {
                var filePath = KanchokuIni.Singleton.KanchokuDir._joinPath(Settings.UserFilesFolder, filename);
                if (Settings.LoggingDecKeyInfo) logger.Info($"modConversion file path={filePath}");
                var lines = Helper.GetFileContent(filePath, Encoding.UTF8);
                if (lines == null) {
                    logger.Error($"Can't read modConversion file: {filePath}");
                    SystemHelper.ShowErrorMessageBox($"修飾キー変換定義ファイル({filePath}の読み込みに失敗しました。");
                    return definition;
                }
                int nl = 0;
                foreach (var rawLine in lines._split('\n')) {
                    ++nl;
                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"line({nl}): {rawLine}");
                    var origLine = rawLine._reReplace("#.*", "").Trim();
                    var line = origLine.Replace(" ", "")._toLower();
                    if (line._notEmpty() && line[0] != '#') {
                        if (line._reMatch(@"^\w+=")) {
                            if (ShiftPlane.AssignShiftPlane(line, rawLine)) continue;
                        } else {
                            var origItems = origLine._splitn(':', 3);
                            var items = line._splitn(':', 3);
                            if (items._length() == 3) {
                                string modName = items[0];
                                string modifiee = items[1];
                                string target = origItems[2]._strip()._stripDq();

                                if (ModifierKeyRegistry.IsDisabledExtKey(modName)) {
                                    if (Settings.LoggingDecKeyInfo) logger.Info(() => $"modName={modName} is disabled");
                                    ModifierKeyRegistry.AddDisabledExtKeyLine(rawLine);
                                    continue;
                                }

                                uint modKey = 0;
                                int modDeckey = SpecialKeysAndFunctions.GetDeckeyByName(modName);
                                int modifieeDeckey = SpecialKeysAndFunctions.GetDeckeyByName(modifiee)._geZeroOr(modifiee._parseInt(-1));
                                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"modName={modName}, modifiee={modifiee}, target={target}, modDeckey={modDeckey}, modifieeDeckey={modifieeDeckey})");
                                if (modifieeDeckey < 0) {
                                    modifieeDeckey = modDeckey;
                                } else {
                                    modKey = ModifierKeyRegistry.GetModifierKeyByName(modName);
                                    if (SpecialKeysAndFunctions.IsPlaneAssignableModKey(modKey) && !ShiftPlane.ShiftPlaneForShiftModKey.ContainsKey(modKey)) {
                                        int pn = ShiftPlane.ShiftPlane_B;
                                        while (pn < ShiftPlane.ShiftPlane_F) {
                                            if (!ShiftPlane.ShiftPlaneForShiftModKey.FindPlane(pn) && !ShiftPlane.ShiftPlaneForShiftModKeyWhenDecoderOff.FindPlane(pn)) {
                                                break;
                                            }
                                            ++pn;
                                        }
                                        logger.Info(() => $"ShiftPlaneForShiftModKey.Add({modName})");
                                        ShiftPlane.ShiftPlaneForShiftModKey.Add(modKey, pn);
                                        ShiftPlane.ShiftPlaneForShiftModKeyWhenDecoderOff.Add(modKey, pn);
                                    }
                                }

                                int convertUnconditional(int deckey)
                                {
                                    return deckey >= 0 ? deckey + DecoderKeys.UNCONDITIONAL_DECKEY_OFFSET : deckey;
                                }

                                bool ctrl = target._startsWith("^");
                                var name = target.Replace("^", "")._toLower();
                                int targetDeckey = convertUnconditional(parseShiftPlaneDeckey(target));

                                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"ctrl={ctrl}, name={name}, targetDeckey={targetDeckey:x}H({targetDeckey})");

                                if (targetDeckey < 0) {
                                    targetDeckey = specialDecKeysFromName._safeGet(name);
                                    if (ctrl) {
                                        int decVkey = 0;
                                        if (name._safeLength() == 1 && name._ge("a") && name._le("z")) {
                                            decVkey = DecoderKeyVsChar.GetArrangedDecKeyFromFaceChar(name._toUpper()._getFirst());
                                            targetDeckey = DecoderKeys.DECKEY_CTRL_A + name[0] - 'a';
                                        } else if (targetDeckey >= DecoderKeys.FUNC_DECKEY_START && targetDeckey < DecoderKeys.FUNC_DECKEY_END) {
                                            decVkey = targetDeckey;
                                            targetDeckey += DecoderKeys.CTRL_FUNC_DECKEY_START - DecoderKeys.FUNC_DECKEY_START;
                                        }
                                        if (Settings.LoggingDecKeyInfo) logger.Info(() => $"targetDeckey={targetDeckey:x}H({targetDeckey}), ctrl={ctrl}, decVkey={decVkey:x}H({decVkey})");
                                        if (targetDeckey > 0) DeckeyComboMap.RegisterModifiedDeckey(targetDeckey, KeyModifiers.MOD_CONTROL, decVkey);
                                    }

                                    if (targetDeckey == 0) {
                                        if (modKey > 0 && modifieeDeckey >= 0) {
                                            var strokeCode = ShiftPlane.GetShiftPlanePrefix(ShiftPlane.ShiftPlaneForShiftModKey.GetPlane(modKey)) + modifieeDeckey.ToString();
                                            targetDeckey = convertUnconditional(parseShiftPlaneDeckey(strokeCode));
                                        } else {
                                            targetDeckey = -1;
                                        }
                                    } else if (!ctrl) {
                                        if (Settings.LoggingDecKeyInfo) logger.Info(() => $"AddSpecialDeckey: name={name}, targetDeckey={targetDeckey:x}H({targetDeckey})");
                                        DeckeyComboMap.RegisterSpecialDeckey(name, targetDeckey);
                                    }
                                }

                                if (Settings.LoggingDecKeyInfo) logger.Info(() => $"modKey={modKey:x}H, modifieeDeckey={modifieeDeckey:x}H, targetDeckey={targetDeckey:x}H({targetDeckey}), ctrl={ctrl}, name={name}");

                                if (modifieeDeckey >= 0 && targetDeckey > 0) {
                                    if (modKey == 0) {
                                        if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Single Hit: modDeckey={modDeckey}, target={target}, targetDeckey={targetDeckey}, modifieeDeckey={modifieeDeckey}");
                                        DeckeyComboMap.Register(targetDeckey, 0, modifieeDeckey, true);
                                        InputActionResolver.RegisterComboAction(0, modifieeDeckey, targetDeckey, InputActionSourceKind.LegacyCombo);
                                        ExtraModifiers.AddExModVkeyAssignedForDecoderFuncByVkey(modifieeDeckey);
                                        definition.SingleHitDefs[modDeckey] = target;
                                    } else {
                                        if (Settings.LoggingDecKeyInfo) logger.Info(() => $"Extra Modifier");
                                        definition.ExtModifierKeyDefs._safeGetOrNewInsert(modKey)[modifieeDeckey] = target;
                                        InputActionResolver.RegisterModifiedComboAction(modKey, modifieeDeckey, targetDeckey, InputActionSourceKind.LegacyModConversion);
                                    }
                                    continue;
                                }
                            }
                        }
                        logger.Warn($"Invalid line({nl}): {rawLine}");
                    }
                }
            }
            logger.Info("LEAVE");
            return definition;
        }

        private static int parseShiftPlaneDeckey(string str)
        {
            if (str._isEmpty()) return -1;
            var s = str._toUpper();
            int offset = ModifierKeyRegistry.CalcShiftOffset(s[0]);
            int deckey = offset > 0 ? s._safeSubstring(1)._parseInt(-1) : s._parseInt(-1);
            if (deckey < 0 || deckey >= DecoderKeys.STROKE_DECKEY_END) return -1;
            return deckey + offset;
        }
    }
}
