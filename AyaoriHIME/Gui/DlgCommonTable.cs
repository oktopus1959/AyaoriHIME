using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using KanchokuWS.Domain;
using KanchokuWS.Handler;
using Utils;

namespace KanchokuWS.Gui
{
    public partial class DlgCommonTable : Form
    {
        private static Logger logger = Logger.GetLogger();

        //private static KeyOrFunction[] modifierKeys;
        private static KeyOrFunction[] visibleModifierKeys;
        private static KeyOrFunction[] addableModifierKeys;

        private static int SelectedModKeysIndex = -1;

        private static string[] shiftPlaneNames = new string[] {
            "なし",
            "通常シフト",
            "拡張シフトA",
            "拡張シフトB",
            "拡張シフトC",
            "拡張シフトD",
            "拡張シフトE",
            "拡張シフトF",
        };

        private bool holdShiftHeaderLocked = true;

        private bool addModKeyLocked = true;

        //public int AssignedKeyOrFuncNameColWidth {
        //    get { return dataGridView_extModifier.Columns != null && dataGridView_extModifier.Columns.Count > 2 ? dataGridView_extModifier.Columns[2].Width : 0; }
        //}
        //public int AssignedKeyOrFuncNameColWidth { get; private set; }

        //public int AssignedKeyOrFuncDescColWidth {
        //    get { return dataGridView_extModifier.Columns != null && dataGridView_extModifier.Columns.Count > 3 ? dataGridView_extModifier.Columns[3].Width : 0; }
        //}
        //public int AssignedKeyOrFuncDescColWidth { get; private set; }

        /// <summary>コンストラクタ</summary>
        public DlgCommonTable()
        {
            //modifierKeys = SpecialKeysAndFunctions.GetModifierKeys(_ => true).Where(x => x.DecKey >= 0).ToArray();
            InitializeComponent();

            if (Settings.DlgCommonTableHeight > 0) Height = Settings.DlgCommonTableHeight;
            if (Settings.DlgCommonTableWidth > 0) Width = Settings.DlgCommonTableWidth;

            CancelButton = buttonCancel;
            DialogResult = DialogResult.None;
        }

        // ダイアログ用の静的初期化ポイント
        public static void Initialize()
        {
            //normalKeyNames = null;
        }

        private int defaultModkeyIndex = 0;

        private bool dgv1Locked = true;
        private bool dgv2Locked = true;
        //private bool dgv3Locked = true;

        // ダイアログ表示時に各コントロールを初期化する
        private void DlgCommonTable_Load(object sender, EventArgs e)
        {
            comboBox_modKeys.Visible = true;
            comboBox_shiftPlane._setItems(shiftPlaneNames.Skip(1));

            dataGridView_singleHit.Visible = false;
            dataGridView_extModifier.Visible = true;

            //AssignedKeyOrFuncNameColWidth = Settings.AssignedKeyOrFuncNameColWidth._gtZeroOr(180);
            //AssignedKeyOrFuncDescColWidth = Settings.AssignedKeyOrFuncDescColWidth._gtZeroOr(290);

            setDataGridViewForSingleHit();
            setDataGridViewForExtModifier();

            refreshModifierKeyComboBoxes();
            defaultModkeyIndex = visibleModifierKeys._findIndex(x => CommonTableRuntime.GetHoldShiftDefinition(x.DecKey) != null)._lowLimit(0);
            comboBox_modKeys.SelectedIndex = (SelectedModKeysIndex >= 0 ? SelectedModKeysIndex : defaultModkeyIndex)._highLimit(comboBox_modKeys.Items.Count - 1);
            radioButton_modKeys.Checked = true;
        }

        private (string modifiers, string name) splitTarget(string target)
        {
            var items = target._reScan(@"^([<>!+^]+)(\w+)$");
            if (items._length() == 3) {
                logger.Info(() => $"target={target}, mod={items[1]}, name={items[2]}");
                return (items[1], items[2]);
            } else {
                logger.Info(() => $"target={target}, mod='', name={target}");
                return ("", target);
            }
        }

        private string modifiersDesc(string modifiers)
        {
            if (modifiers._isEmpty()) return "";

            var desc = new StringBuilder();
            foreach (var modifier in modifiers) {
                if (modifier == '^') {
                    desc.Append("Ctrl");
                } else if (modifier == '+') {
                    desc.Append("Shift");
                } else if (modifier == '!') {
                    desc.Append("Alt");
                } else if (modifier == '<') {
                    desc.Append("左");
                    continue;
                } else if (modifier == '>') {
                    desc.Append("右");
                    continue;
                } else {
                    desc.Append(modifier);
                }
                desc.Append('-');
            }
            return desc.ToString();
        }

        private bool isAlphabet(string str)
        {
            return str._reMatch("^[A-Za-z]$");
        }

        private bool isDeckeyCode(string str)
        {
            return str._reMatch("^[A-Fa-f]?[0-9]+$");
        }

        // 入力された割り当て文字列を内部保存用の表記に正規化する
        private string normalizeTarget(string target)
        {
            target = target._strip();
            if (target._isEmpty()) return "";
            if (target._startsWith("!{") || target._startsWith("@")) return target;
            var (modifiers, name) = splitTarget(target);
            var canonicalName = SpecialKeysAndFunctions.GetCanonicalName(name, false);
            if (canonicalName._notEmpty()) {
                // ^Left や MazeConversion など
                return $"!{{{modifiers}{canonicalName}}}";
            }
            if (modifiers._notEmpty() && isAlphabet(name)) {
                // ^X など
                return $"!{{{modifiers}{name}}}";
            }
            if (isDeckeyCode(target)) {
                // 配列コード
                return $"!{{{target}}}";
            }
            if (target._startsWith("\"")) {
                return target;
            }
            // 通常文字列
            return $"\"{target}\"";
        }

        // 内部保存用の割り当て文字列を表示用の表記に戻す
        private (string modifiers, string name) displayTarget(string rawTarget)
        {
            rawTarget = rawTarget._strip();
            if (rawTarget._startsWith("!{") && rawTarget._endsWith("}") && !rawTarget.Substring(1).Contains("!{")) {
                var inner = rawTarget._safeSubstring(2, rawTarget.Length - 3)._strip();
                var (modifiers, name) = splitTarget(inner);
                var cannon = SpecialKeysAndFunctions.GetCanonicalName(name, false);
                if (cannon._notEmpty()) {
                    return (modifiers, cannon);
                }
                if (isAlphabet(name)) {
                    return (modifiers, name);
                }
                return ("", inner);
            }
            if (rawTarget._startsWith("\"") && rawTarget._endsWith("\"") && rawTarget._reMatch("[^0-9]")) {
                // ダブルクォートで囲まれていて、数字以外を含む
                return ("", rawTarget._safeSubstring(1, rawTarget.Length - 2));
            }
            return ("", rawTarget);
        }

        // 表示用の表記と説明文を返す
        private (string disp, string desc) getDisplayTargetAndDescription(string rawTarget)
        {
            var displayed = "";
            var description = "";
            if (rawTarget._notEmpty()) {
                var (modifiers, name) = displayTarget(rawTarget);
                logger.Info(() => $"ENTER: rawTarget={rawTarget}, modifiers={modifiers}, name={name}");
                displayed = modifiers + name;
                var kof = SpecialKeysAndFunctions.GetKeyOrFuncByName(name);
                if (kof != null) {
                    displayed = modifiers + kof.Name;
                    description = modifiersDesc(modifiers) + kof.Description;
                } else if (rawTarget._startsWith("!{")) {
                    if (isDeckeyCode(displayed)) {
                        description = "配列コード";
                    } else if (isAlphabet(name)) {
                        description = modifiersDesc(modifiers) + name;
                    }
                } else if (TernaryOperatorParser.IsTernaryOperator(rawTarget)) {
                    description = "三項演算子";
                } else {
                    description = "文字列";
                }
                logger.Info(() => $"LEAVE: rawTarget={rawTarget}, disp={displayed}, desc={description}");
            }
            return (displayed, description);
        }

        // 修飾キー名に割り当て済みマーカーを付けて返す
        private string getModifiedDescription(KeyOrFunction modDef)
        {
            if (modDef == null) return "";

            string marker = "";
            var definition = CommonTableRuntime.GetHoldShiftDefinition(modDef.DecKey);
            if (definition != null) {
                for (int deckey = 0; deckey < DecoderKeys.PLANE_DECKEY_NUM; ++deckey) {
                    if (definition.Actions._safeGet(deckey)?.RawTarget._notEmpty() == true) {
                        marker = " *";
                        break;
                    }
                }
            }
            return modDef.PrefName + marker;
        }

        // 被修飾キーの表示名を deckey から取得する
        private string getModifieeKeyName(int deckey)
        {
            if (deckey >= 0 && deckey < DecoderKeyVsChar.NormalKeyNames._safeCount()) {
                return DecoderKeyVsChar.NormalKeyNames[deckey];
            }
            return SpecialKeysAndFunctions.GetKeyOrFuncByDeckey(deckey)?.PrefName ?? "N/A";
        }

        // 単打用DGV(dataGridView_singleHit)の設定
        private void setDataGridViewForSingleHit()
        {
            double dpiRate = ScreenInfo.Singleton.PrimaryScreenDpiRate._lowLimit(1.0);
            int rowHeight = (int)(20 * dpiRate);

            var dgv = dataGridView_singleHit;
            dgv._defaultSetup(rowHeight, rowHeight);
            dgv._setSelectionColorReadOnly();
            dgv._setDefaultFont(DgvHelpers.FontYUG9);
            int keyCodeWidth = (int)(30 * dpiRate);
            int keyNameWidth = (int)(80 * dpiRate);
            //int funcNameWidth = (int)(180 * dpiRate);
            int funcNameWidth = (int)(Settings.AssignedKeyOrFuncNameColWidth * dpiRate);
            //int funcDescWidth = (int)(dgv.Width - 20 * dpiRate - keyCodeWidth - keyNameWidth - funcNameWidth);
            int funcDescWidth = (int)(Settings.AssignedKeyOrFuncDescColWidth * dpiRate);
            dgv.Columns.Add(dgv._makeTextBoxColumn_ReadOnly_Sortable_Centered("keyCode", "No", keyCodeWidth, DgvHelpers.READONLY_SELECTION_COLOR));
            dgv.Columns.Add(dgv._makeTextBoxColumn_ReadOnly_Sortable("keyName", "単打キー", keyNameWidth, DgvHelpers.READONLY_SELECTION_COLOR));
            dgv.Columns.Add(dgv._makeTextBoxColumn_Sortable("funcName", "割り当てキー/機能名", funcNameWidth, DgvHelpers.HIGHLIGHT_SELECTION_COLOR));
            dgv.Columns.Add(dgv._makeTextBoxColumn_ReadOnly_Sortable("funcDesc", "機能説明", funcDescWidth, DgvHelpers.READONLY_SELECTION_COLOR));

            dgv.Rows.Add(DecoderKeys.FUNC_DECKEY_NUM);

            renewSingleHitDgv();
        }

        // 単打キー一覧の表示内容を現在設定で更新する
        private void renewSingleHitDgv()
        {
            dgv1Locked = true;
            var dgv = dataGridView_singleHit;
            int num = dgv.Rows.Count;

            for (int i = 0; i < num; ++i) {
                int deckey = DecoderKeys.FUNC_DECKEY_START + i;
                dgv.Rows[i].Cells[0].Value = deckey;
                dgv.Rows[i].Cells[1].Value = getModifieeKeyName(deckey);
                var target = CommonTableRuntime.GetSingleHitRawTarget(deckey);
                var (disp, desc) = getDisplayTargetAndDescription(target);
                dgv.Rows[i].Cells[2].Value = disp;
                dgv.Rows[i].Cells[3].Value = desc;
            }
            dgv1Locked = false;
        }

        // 拡張修飾キー設定用DGV(dataGridView_extModifier)の設定
        private void setDataGridViewForExtModifier()
        {
            double dpiRate = ScreenInfo.Singleton.PrimaryScreenDpiRate._lowLimit(1.0);
            int rowHeight = (int)(20 * dpiRate);

            var dgv = dataGridView_extModifier;
            dgv._defaultSetup(rowHeight, rowHeight);
            dgv._setSelectionColorReadOnly();
            dgv._setDefaultFont(DgvHelpers.FontYUG9);
            int keyCodeWidth = (int)(30 * dpiRate);
            int keyNameWidth = (int)(80 * dpiRate);
            int funcNameWidth = (int)(Settings.AssignedKeyOrFuncNameColWidth * dpiRate);
            int funcDescWidth = (int)(Settings.AssignedKeyOrFuncDescColWidth * dpiRate);
            dgv.Columns.Add(dgv._makeTextBoxColumn_ReadOnly_Sortable_Centered("keyCode", "No", keyCodeWidth));
            dgv.Columns.Add(dgv._makeTextBoxColumn_ReadOnly_Sortable("keyName", "被修飾キー", keyNameWidth));
            dgv.Columns.Add(dgv._makeTextBoxColumn_Sortable("funcName", "割り当てキー/機能名", funcNameWidth, DgvHelpers.HIGHLIGHT_SELECTION_COLOR));
            dgv.Columns.Add(dgv._makeTextBoxColumn_ReadOnly_Sortable("funcDesc", "機能説明", funcDescWidth));

            dgv.Rows.Add(DecoderKeys.PLANE_DECKEY_NUM);

            renewExtModifierDgv();
        }

        // 修飾キー一覧の表示内容を現在選択中の定義で更新する
        private void renewExtModifierDgv()
        {
            int idx = comboBox_modKeys.SelectedIndex;
            var modKeyDef = visibleModifierKeys._getNth(idx);
            if (modKeyDef == null) return;

            dgv2Locked = true;
            var dgv = dataGridView_extModifier;
            int num = dgv.Rows.Count;
            var definition = CommonTableRuntime.GetHoldShiftDefinition(modKeyDef.DecKey);
            for (int i = 0; i < num; ++i) {
                dgv.Rows[i].Cells[0].Value = i;
                dgv.Rows[i].Cells[1].Value = getModifieeKeyName(i);
                var target = definition?.Actions._safeGet(i)?.RawTarget;
                var (disp, desc) = getDisplayTargetAndDescription(target);
                dgv.Rows[i].Cells[2].Value = disp;
                dgv.Rows[i].Cells[3].Value = desc;
            }
            dgv2Locked = false;
        }

        // 選択された修飾キーに応じてヘッダと一覧を切り替える
        private void selectModKey()
        {
            try {
                int idx = SelectedModKeysIndex = comboBox_modKeys.SelectedIndex;
                var modKeyDef = visibleModifierKeys._getNth(idx);
                syncHoldShiftHeaderControls(modKeyDef);

                renewExtModifierDgv();
            } catch (Exception ex) {
                logger.Error(ex._getErrorMsg());
            }
        }

        // OK ボタンでダイアログを確定して閉じる
        private void buttonOK_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.OK;
            Close();
        }

        // Cancel ボタンで変更を破棄して閉じる
        private void buttonCancel_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
            Close();
        }

        // 表示モード切り替えに合わせて表示中のグリッドを更新する
        private void radioButtonCheckedChanged()
        {
            try {
                dataGridView_singleHit.Visible = radioButton_singleHit.Checked;
                dataGridView_extModifier.Visible = radioButton_modKeys.Checked;

                if (radioButton_singleHit.Checked) {
                    if (dataGridView_singleHit.Columns.Count > 3) {
                        dataGridView_singleHit.Columns[2].Width = Settings.AssignedKeyOrFuncNameColWidth;
                        dataGridView_singleHit.Columns[3].Width = Settings.AssignedKeyOrFuncDescColWidth;
                    }
                }
                if (radioButton_modKeys.Checked) {
                    if (dataGridView_extModifier.Columns.Count > 3) {
                        dataGridView_extModifier.Columns[2].Width = Settings.AssignedKeyOrFuncNameColWidth;
                        dataGridView_extModifier.Columns[3].Width = Settings.AssignedKeyOrFuncDescColWidth;
                    }
                }
                bool bModkeysVisible = !radioButton_singleHit.Checked;

                label_modKeys.Visible = bModkeysVisible;
                comboBox_modKeys.Visible = bModkeysVisible;
                label_addModKey.Visible = bModkeysVisible;
                comboBox_addModKey.Visible = bModkeysVisible;
                label_shiftPlane.Visible = bModkeysVisible;
                comboBox_shiftPlane.Visible = bModkeysVisible;
                checkBox_enabledWhenOff.Visible = bModkeysVisible;
            } catch (Exception ex) {
                logger.Error(ex._getErrorMsg());
            }
        }

        // 修飾キー表示への切り替えを共通処理に委譲する
        private void radioButton_modKeys_CheckedChanged(object sender, EventArgs e)
        {
            radioButtonCheckedChanged();
        }

        // 単打キー表示への切り替えを共通処理に委譲する
        private void radioButton_singleHit_CheckedChanged(object sender, EventArgs e)
        {
            radioButtonCheckedChanged();
        }

        // 修飾キー選択変更時に対象定義を切り替える
        private void comboBox_modKeys_SelectedIndexChanged(object sender, EventArgs e)
        {
            selectModKey();
        }

        // 追加対象の修飾キーを定義一覧に取り込む
        private void comboBox_addModKey_SelectedIndexChanged(object sender, EventArgs e)
        {
            try {
                if (addModKeyLocked) return;
                var modKeyDef = addableModifierKeys._getNth(comboBox_addModKey.SelectedIndex);
                if (modKeyDef == null) return;
                if (CommonTableRuntime.GetHoldShiftDefinition(modKeyDef.DecKey) == null) {
                    CommonTableRuntime.UpdateHoldShiftHeader(modKeyDef.DecKey, ShiftPlane.ShiftPlane_A, false);
                }
                refreshModifierKeyComboBoxes(modKeyDef.DecKey);
                addModKeyLocked = true;
                comboBox_addModKey.SelectedIndex = -1;
                addModKeyLocked = false;
                renewExtModifierDgv();
            } catch (Exception ex) {
                logger.Error(ex._getErrorMsg());
            }
        }

        // シフト面選択変更を修飾キー定義へ反映する
        private void comboBox_shiftPlane_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (holdShiftHeaderLocked) return;
            updateHoldShiftHeader();
        }

        // デコーダOFF時有効フラグ変更を修飾キー定義へ反映する
        private void checkBox_enabledWhenOff_CheckedChanged(object sender, EventArgs e)
        {
            if (holdShiftHeaderLocked) return;
            updateHoldShiftHeader();
        }

        // 修飾キー一覧の編集結果を共通テーブルへ保存する
        private void dataGridView_extModifier_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            if (dgv2Locked) return;

            logger.DebugH(() => $"ENTER: row={e.RowIndex}, col={e.ColumnIndex}");

            const int TARGET_COL = 2;

            if (e.ColumnIndex != TARGET_COL) return;

            var dgv = dataGridView_extModifier;
            int row = e.RowIndex;
            if (row < 0 || row >= dgv.Rows.Count) return;

            int idx = comboBox_modKeys.SelectedIndex;
            var modKeyDef = visibleModifierKeys._getNth(idx);
            if (modKeyDef == null) return;

            try {
                var target = normalizeTarget(dgv.Rows[row].Cells[TARGET_COL].Value?.ToString() ?? "");
                CommonTableRuntime.UpdateHoldShiftTarget(modKeyDef.DecKey, row, target);
                renewExtModifierDgv();

            } catch (Exception ex) {
                logger.Error(ex._getErrorMsg());
            }

            logger.DebugH("LEAVE");
        }

        // 単打キー一覧の編集結果を共通テーブルへ保存する
        private void dataGridView_singleHit_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            if (dgv1Locked) return;

            logger.DebugH(() => $"ENTER: row={e.RowIndex}, col={e.ColumnIndex}");

            const int TARGET_COL = 2;

            if (e.ColumnIndex != TARGET_COL) return;

            var dgv = dataGridView_singleHit;
            int row = e.RowIndex;
            if (row < 0 || row >= dgv.Rows.Count) return;

            int deckey = DecoderKeys.FUNC_DECKEY_START + row;
            if (deckey < 0) return;

            try {
                var target = normalizeTarget(dgv.Rows[row].Cells[TARGET_COL].Value?.ToString() ?? "");
                CommonTableRuntime.UpdateSingleHit(deckey, target);
                renewSingleHitDgv();

            } catch (Exception ex) {
                logger.Error(ex._getErrorMsg());
            }

            logger.DebugH("LEAVE");
        }

        // キーワード選択ダイアログから割り当て先を選ばせる
        private void selectKeyOrFuncName(DataGridView dgv, int ridx)
        {
            using (var dlg = new DlgKeywordSelector()) {
                try {
                    if (dlg.ShowDialog() == DialogResult.OK) {
                        var keyword = dlg.SelectedWord;
                        if (keyword._notEmpty()) {
                            dgv.EndEdit();                              // 編集中だったセルの場合に、いったんそれをコミットする必要がある
                            if (ridx >= 0 && ridx < dgv.Rows.Count) {
                                dgv.CurrentCell = dgv.Rows[ridx].Cells[2];
                                dgv.Rows[ridx].Cells[2].Value = keyword;
                                dgv.Rows[ridx].Cells[2].Selected = true;
                            }
                        }
                    }
                    if (Settings.DlgKeywordSelectorHeight != dlg.Height) {
                        Settings.SetUserIni("dlgKeywordSelectorHeight", dlg.Height);
                        Settings.DlgKeywordSelectorHeight = dlg.Height;
                    }
                    if (Settings.DlgKeywordSelectorWidth != dlg.Width) {
                        Settings.SetUserIni("dlgKeywordSelectorWidth", dlg.Width);
                        Settings.DlgKeywordSelectorWidth = dlg.Width;
                    }
                } catch (Exception ex) {
                    logger.Error(ex._getErrorMsg());
                }
            }
        }

        // 右クリック時に割り当て選択ダイアログを開く
        private void dgvCellMouseClick(DataGridView dgv, DataGridViewCellMouseEventArgs e)
        {
            if (e.Button != MouseButtons.Right) return;
            int ridx = e.RowIndex;
            if (ridx < 0 || ridx >= dgv.Rows.Count) return;

            selectKeyOrFuncName(dgv, ridx);
        }

        // ダブルクリック時に割り当て選択ダイアログを開く
        private void dgvCellMouseDoubleClick(DataGridView dgv, DataGridViewCellMouseEventArgs e)
        {
            int ridx = e.RowIndex;
            if (ridx < 0 || ridx >= dgv.Rows.Count) return;

            selectKeyOrFuncName(dgv, ridx);
        }

        // 割り当て列で Delete/Backspace によるクリアを扱う
        private void dgvKeyDown(DataGridView dgv, KeyEventArgs e)
        {
            if (!dgv.IsCurrentCellInEditMode) {
                if (dgv.CurrentCell != null && dgv.CurrentCell.ColumnIndex == 2) {
                    if (e.KeyCode == Keys.Back || e.KeyCode == Keys.Delete) {
                        dgv.CurrentCell.Value = "";
                    }
                }
            }
        }

        // 修飾キー一覧の右クリックを共通処理へ渡す
        private void dataGridView_extModifier_CellMouseClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            dgvCellMouseClick(dataGridView_extModifier, e);
        }

        // 修飾キー一覧のダブルクリックを共通処理へ渡す
        private void dataGridView_extModifier_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            dgvCellMouseDoubleClick(dataGridView_extModifier, e);
        }

        // 修飾キー一覧のキー操作を共通処理へ渡す
        private void dataGridView_extModifier_KeyDown(object sender, KeyEventArgs e)
        {
            dgvKeyDown(dataGridView_extModifier, e);
        }

        // 単打キー一覧の右クリックを共通処理へ渡す
        private void dataGridView_singleHit_CellMouseClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            dgvCellMouseClick(dataGridView_singleHit, e);
        }

        // 単打キー一覧のダブルクリックを共通処理へ渡す
        private void dataGridView_singleHit_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            dgvCellMouseDoubleClick(dataGridView_singleHit, e);
        }

        // 単打キー一覧のキー操作を共通処理へ渡す
        private void dataGridView_singleHit_KeyDown(object sender, KeyEventArgs e)
        {
            dgvKeyDown(dataGridView_singleHit, e);
        }

        // 単打キー一覧の列幅変更を設定へ保存する
        private void dataGridView_singleHit_ColumnWidthChanged(object sender, DataGridViewColumnEventArgs e)
        {
            if (dataGridView_singleHit.Columns.Count > 3) {
                Settings.AssignedKeyOrFuncNameColWidth = dataGridView_singleHit.Columns[2].Width;
                Settings.AssignedKeyOrFuncDescColWidth = dataGridView_singleHit.Columns[3].Width;
            }
        }

        // 修飾キー一覧の列幅変更を設定へ保存する
        private void dataGridView_extModifier_ColumnWidthChanged(object sender, DataGridViewColumnEventArgs e)
        {
            if (dataGridView_extModifier.Columns.Count > 3) {
                Settings.AssignedKeyOrFuncNameColWidth = dataGridView_extModifier.Columns[2].Width;
                Settings.AssignedKeyOrFuncDescColWidth = dataGridView_extModifier.Columns[3].Width;
            }
        }

        // FAQ のキー割り当て説明ページを開く
        private void button_openFAQ_Click(object sender, EventArgs e)
        {
            System.Diagnostics.Process.Start(Settings.FaqKeyAssignUrl);
        }

        // 修飾キー選択用コンボボックスの内容を再構築する
        private void refreshModifierKeyComboBoxes(int selectedDeckey = -1)
        {
            var presetHoldShiftDeckeys = CommonTableRuntime.PresetHoldShiftDeckeys();
            var baseKeys = presetHoldShiftDeckeys
                .Select(deckey => SpecialKeysAndFunctions.GetKeyOrFuncByDeckey(deckey))
                .Where(x => x != null)
                .ToList();
            var holdShiftDefinedKeys = SpecialKeysAndFunctions.GetSpecialKeyOrFunctionList()
                .Where(x => x.DecKey >= 0 && x.DecKey < DecoderKeys.FUNC_DECKEY_END)
                .GroupBy(x => x.DecKey)
                .Select(x => x.First())
                .Where(x => !presetHoldShiftDeckeys.Contains(x.DecKey))
                .Where(x => CommonTableRuntime.GetHoldShiftDefinition(x.DecKey) != null)
                .OrderBy(x => x.DecKey);
            visibleModifierKeys = baseKeys.Concat(holdShiftDefinedKeys).ToArray();
            addableModifierKeys = SpecialKeysAndFunctions.GetSpecialKeyOrFunctionList()
                .Where(x => x.DecKey >= 0 && x.DecKey < DecoderKeys.FUNC_DECKEY_END)
                .GroupBy(x => x.DecKey)
                .Select(x => x.First())
                .Where(x => !visibleModifierKeys.Any(y => y.DecKey == x.DecKey))
                .OrderBy(x => x.DecKey)
                .ToArray();

            int currentDeckey = selectedDeckey >= 0 ? selectedDeckey : visibleModifierKeys._getNth(comboBox_modKeys.SelectedIndex)?.DecKey ?? -1;

            comboBox_modKeys._setItems(visibleModifierKeys.Select(x => getModifiedDescription(x)));
            addModKeyLocked = true;
            comboBox_addModKey._setItems(addableModifierKeys.Select(x => x.PrefName));
            comboBox_addModKey.SelectedIndex = -1;
            addModKeyLocked = false;

            defaultModkeyIndex = visibleModifierKeys._findIndex(x => CommonTableRuntime.GetHoldShiftDefinition(x.DecKey) != null)._lowLimit(0);
            int selectedIndex = visibleModifierKeys._findIndex(x => x.DecKey == currentDeckey);
            if (selectedIndex < 0) selectedIndex = defaultModkeyIndex._highLimit(visibleModifierKeys.Length - 1);
            if (comboBox_modKeys.Items.Count > 0) {
                comboBox_modKeys.SelectedIndex = selectedIndex._highLimit(comboBox_modKeys.Items.Count - 1);
            }
        }

        // 修飾キー定義のヘッダ情報を上部コントロールへ反映する
        private void syncHoldShiftHeaderControls(KeyOrFunction modKeyDef)
        {
            holdShiftHeaderLocked = true;
            var definition = modKeyDef != null ? CommonTableRuntime.GetHoldShiftDefinition(modKeyDef.DecKey) : null;
            int shiftPlane = definition?.ShiftPlane ?? ShiftPlane.ShiftPlane_A;
            comboBox_shiftPlane.SelectedIndex = (shiftPlane - 1)._lowLimit(0)._highLimit(comboBox_shiftPlane.Items.Count - 1);
            checkBox_enabledWhenOff.Checked = definition?.EnabledWhenDecoderOff == true;
            holdShiftHeaderLocked = false;
        }

        // 上部コントロールの内容を修飾キー定義のヘッダへ反映する
        private void updateHoldShiftHeader()
        {
            int modkeyIdx = comboBox_modKeys.SelectedIndex;
            var modKeyDef = visibleModifierKeys._getNth(modkeyIdx);
            if (modKeyDef == null) return;

            int plane = comboBox_shiftPlane.SelectedIndex + 1;
            if (plane <= 0) plane = ShiftPlane.ShiftPlane_SHIFT;
            CommonTableRuntime.UpdateHoldShiftHeader(modKeyDef.DecKey, plane, checkBox_enabledWhenOff.Checked);
            refreshModifierKeyComboBoxes(modKeyDef.DecKey);
            renewExtModifierDgv();
        }
    }
}
