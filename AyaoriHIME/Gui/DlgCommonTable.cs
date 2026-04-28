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
using Utils;

namespace KanchokuWS.Gui
{
    public partial class DlgCommonTable : Form
    {
        private static Logger logger = Logger.GetLogger();

        private static KeyOrFunction[] modifierKeys;
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

        private static KeyOrFunction[] singleHitKeys;

        private static KeyOrFunction[] extModifiees;

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
            modifierKeys = SpecialKeysAndFunctions.GetModifierKeys(_ => true).Where(x => x.DecKey >= 0).ToArray();
            extModifiees = SpecialKeysAndFunctions.GetModifieeKeys();
            singleHitKeys = SpecialKeysAndFunctions.GetSingleHitKeys();

            InitializeComponent();

            if (Settings.DlgCommonTableHeight > 0) Height = Settings.DlgCommonTableHeight;
            if (Settings.DlgCommonTableWidth > 0) Width = Settings.DlgCommonTableWidth;

            CancelButton = buttonCancel;
            DialogResult = DialogResult.None;
        }

        public static void Initialize()
        {
            //normalKeyNames = null;
        }

        private int defaultModkeyIndex = 0;

        private bool dgv1Locked = true;
        private bool dgv2Locked = true;
        //private bool dgv3Locked = true;

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

        private string normalizeTarget(string target)
        {
            target = target._strip();
            if (target._isEmpty()) return "";
            if (target._startsWith("!{") || target._startsWith("@")) return target;
            var canonicalName = SpecialKeysAndFunctions.GetCanonicalName(target, false);
            return canonicalName._notEmpty() ? $"!{{{canonicalName}}}" : target;
        }

        private string displayTarget(string rawTarget)
        {
            rawTarget = rawTarget._strip();
            if (rawTarget._startsWith("!{") && rawTarget._endsWith("}")) {
                var inner = rawTarget._safeSubstring(2, rawTarget.Length - 3)._strip();
                return SpecialKeysAndFunctions.GetCanonicalName(inner, false)._orElse(inner);
            }
            if (rawTarget._startsWith("\"") && rawTarget._endsWith("\"")) {
                return rawTarget._safeSubstring(1, rawTarget.Length - 2);
            }
            return rawTarget;
        }

        private string getModifiedDescription(KeyOrFunction modDef)
        {
            if (modDef == null) return "";

            string marker = "";
            var definition = CommonTableRuntime.GetHoldShiftDefinition(modDef.DecKey);
            if (definition != null) {
                int normalKeysNum = DecoderKeyVsChar.NormalKeyNames._safeCount();
                int num = normalKeysNum  + extModifiees.Length;
                for (int i = 0; i < num; ++i) {
                    int deckey = i < normalKeysNum ? i : extModifiees[i - normalKeysNum].DecKey;
                    if (definition.Actions._safeGet(deckey)?.RawTarget._notEmpty() == true) {
                        marker = " *";
                        break;
                    }
                }
            }
            return modDef.PrefName + marker;
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

            dgv.Rows.Add(singleHitKeys.Length);

            renewSingleHitDgv();
        }

        private void renewSingleHitDgv()
        {
            dgv1Locked = true;
            var dgv = dataGridView_singleHit;
            int num = dgv.Rows.Count;

            for (int i = 0; i < num; ++i) {
                dgv.Rows[i].Cells[0].Value = i;
                var kof = singleHitKeys[i];
                int deckey = kof.DecKey;
                dgv.Rows[i].Cells[1].Value = kof.PrefName;
                string assigned = "";
                string desc = "";
                var target = CommonTableRuntime.GetSingleHitRawTarget(kof.DecKey);
                if (target._notEmpty()) {
                    kof = SpecialKeysAndFunctions.GetKeyOrFuncByName(displayTarget(target));
                    assigned = kof != null ? kof.Name : target;
                    desc = kof != null ? kof.Description : "";
                }
                dgv.Rows[i].Cells[2].Value = assigned;
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

            dgv.Rows.Add(DecoderKeyVsChar.NormalKeyNames._safeCount() + extModifiees.Length);

            renewExtModifierDgv();
        }

        private void renewExtModifierDgv()
        {
            int idx = comboBox_modKeys.SelectedIndex;
            var modKeyDef = visibleModifierKeys._getNth(idx);
            if (modKeyDef == null) return;

            dgv2Locked = true;
            var dgv = dataGridView_extModifier;
            int num = dgv.Rows.Count;
            int normalKeysNum = DecoderKeyVsChar.NormalKeyNames._safeCount();
            var definition = CommonTableRuntime.GetHoldShiftDefinition(modKeyDef.DecKey);
            for (int i = 0; i < num; ++i) {
                dgv.Rows[i].Cells[0].Value = i;
                if (i < normalKeysNum) {
                    dgv.Rows[i].Cells[1].Value = DecoderKeyVsChar.NormalKeyNames[i];
                } else {
                    dgv.Rows[i].Cells[1].Value = extModifiees[i - normalKeysNum].Name;
                }
                string assigned = "";
                string desc = "";
                KeyOrFunction kof = null;
                if (definition != null) {
                    int deckey = i < normalKeysNum ? i : extModifiees[i - normalKeysNum].DecKey;
                    var target = definition.Actions._safeGet(deckey)?.RawTarget;
                    if (target._notEmpty()) {
                        var displayed = displayTarget(target);
                        kof = SpecialKeysAndFunctions.GetKeyOrFuncByName(displayed);
                        if (kof != null) {
                            assigned = kof.Name;
                            desc = kof.Description;
                        } else {
                            assigned = displayed;
                        }
                    }
                }
                dgv.Rows[i].Cells[2].Value = assigned;
                dgv.Rows[i].Cells[3].Value = desc;
            }
            dgv2Locked = false;
        }

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

        private void buttonOK_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.OK;
            Close();
        }

        private void buttonCancel_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
            Close();
        }

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

        private void radioButton_modKeys_CheckedChanged(object sender, EventArgs e)
        {
            radioButtonCheckedChanged();
        }

        private void radioButton_singleHit_CheckedChanged(object sender, EventArgs e)
        {
            radioButtonCheckedChanged();
        }

        private void comboBox_modKeys_SelectedIndexChanged(object sender, EventArgs e)
        {
            selectModKey();
        }

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

        private void comboBox_shiftPlane_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (holdShiftHeaderLocked) return;
            updateHoldShiftHeader();
        }

        private void checkBox_enabledWhenOff_CheckedChanged(object sender, EventArgs e)
        {
            if (holdShiftHeaderLocked) return;
            updateHoldShiftHeader();
        }

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
                int normalKeysNum = DecoderKeyVsChar.NormalKeyNames._safeCount();
                int deckey = row < normalKeysNum ? row : extModifiees[row - normalKeysNum].DecKey;
                var target = normalizeTarget(dgv.Rows[row].Cells[TARGET_COL].Value?.ToString() ?? "");
                CommonTableRuntime.UpdateHoldShiftTarget(modKeyDef.DecKey, deckey, target);
                renewExtModifierDgv();

            } catch (Exception ex) {
                logger.Error(ex._getErrorMsg());
            }

            logger.DebugH(() => $"LEAVE");
        }

        private void dataGridView_singleHit_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            if (dgv1Locked) return;

            logger.DebugH(() => $"ENTER: row={e.RowIndex}, col={e.ColumnIndex}");

            const int TARGET_COL = 2;

            if (e.ColumnIndex != TARGET_COL) return;

            var dgv = dataGridView_singleHit;
            int row = e.RowIndex;
            if (row < 0 || row >= dgv.Rows.Count) return;

            int deckey = singleHitKeys._getNth(row)?.DecKey ?? -1;
            if (deckey < 0) return;

            try {
                var target = normalizeTarget(dgv.Rows[row].Cells[TARGET_COL].Value?.ToString() ?? "");
                CommonTableRuntime.UpdateSingleHit(deckey, target);
                renewSingleHitDgv();

            } catch (Exception ex) {
                logger.Error(ex._getErrorMsg());
            }

            logger.DebugH(() => $"LEAVE");
        }

        private void selectKeyOrFuncName(DataGridView dgv, int ridx)
        {
            int no = dgv.Rows[ridx].Cells[0].Value.ToString()._parseInt(0);     // 元の順の番号を取得しておく
            using (var dlg = new DlgKeywordSelector()) {
                try {
                    if (dlg.ShowDialog() == DialogResult.OK) {
                        var keyword = dlg.SelectedWord;
                        if (keyword._notEmpty()) {
                            dgv.EndEdit();                              // 編集中だったセルの場合に、いったんそれをコミットする必要がある
                            dgv.Rows[no].Cells[2].Value = keyword;      // セルの値を変更しようとすると、元の順に並びが戻るので注意
                            if (no != ridx) {
                                dgv.Rows[no].Cells[2].Selected = true;
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

        private void dgvCellMouseClick(DataGridView dgv, DataGridViewCellMouseEventArgs e)
        {
            if (e.Button != MouseButtons.Right) return;
            int ridx = e.RowIndex;
            if (ridx < 0 || ridx >= dgv.Rows.Count) return;

            selectKeyOrFuncName(dgv, ridx);
        }

        private void dgvCellMouseDoubleClick(DataGridView dgv, DataGridViewCellMouseEventArgs e)
        {
            int ridx = e.RowIndex;
            if (ridx < 0 || ridx >= dgv.Rows.Count) return;

            selectKeyOrFuncName(dgv, ridx);
        }

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

        private void dataGridView_extModifier_CellMouseClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            dgvCellMouseClick(dataGridView_extModifier, e);
        }

        private void dataGridView_extModifier_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            dgvCellMouseDoubleClick(dataGridView_extModifier, e);
        }

        private void dataGridView_extModifier_KeyDown(object sender, KeyEventArgs e)
        {
            dgvKeyDown(dataGridView_extModifier, e);
        }

        private void dataGridView_singleHit_CellMouseClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            dgvCellMouseClick(dataGridView_singleHit, e);
        }

        private void dataGridView_singleHit_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
        {
            dgvCellMouseDoubleClick(dataGridView_singleHit, e);
        }

        private void dataGridView_singleHit_KeyDown(object sender, KeyEventArgs e)
        {
            dgvKeyDown(dataGridView_singleHit, e);
        }

        private void dataGridView_singleHit_ColumnWidthChanged(object sender, DataGridViewColumnEventArgs e)
        {
            if (dataGridView_singleHit.Columns.Count > 3) {
                Settings.AssignedKeyOrFuncNameColWidth = dataGridView_singleHit.Columns[2].Width;
                Settings.AssignedKeyOrFuncDescColWidth = dataGridView_singleHit.Columns[3].Width;
            }
        }

        private void dataGridView_extModifier_ColumnWidthChanged(object sender, DataGridViewColumnEventArgs e)
        {
            if (dataGridView_extModifier.Columns.Count > 3) {
                Settings.AssignedKeyOrFuncNameColWidth = dataGridView_extModifier.Columns[2].Width;
                Settings.AssignedKeyOrFuncDescColWidth = dataGridView_extModifier.Columns[3].Width;
            }
        }

        private void button_openFAQ_Click(object sender, EventArgs e)
        {
            System.Diagnostics.Process.Start(Settings.FaqKeyAssignUrl);
        }

        private void refreshModifierKeyComboBoxes(int selectedDeckey = -1)
        {
            var presetHoldShiftDeckeys = CommonTableRuntime.PresetHoldShiftDeckeys();
            var baseKeys = presetHoldShiftDeckeys
                .Select(deckey => SpecialKeysAndFunctions.GetKeyOrFuncByDeckey(deckey))
                .Where(x => x != null)
                .ToList();
            var holdShiftDefinedKeys = SpecialKeysAndFunctions.GetSpecialKeyOrFunctionList()
                .Where(x => x.DecKey >= 0)
                .GroupBy(x => x.DecKey)
                .Select(x => x.First())
                .Where(x => !presetHoldShiftDeckeys.Contains(x.DecKey))
                .Where(x => CommonTableRuntime.GetHoldShiftDefinition(x.DecKey) != null)
                .OrderBy(x => x.DecKey);
            visibleModifierKeys = baseKeys.Concat(holdShiftDefinedKeys).ToArray();
            addableModifierKeys = SpecialKeysAndFunctions.GetSpecialKeyOrFunctionList()
                .Where(x => x.DecKey >= 0)
                .GroupBy(x => x.DecKey)
                .Select(x => x.First())
                .Where(x => (x.DecKey >= DecoderKeys.ESC_DECKEY && x.DecKey <= DecoderKeys.F16_DECKEY))
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

        private void syncHoldShiftHeaderControls(KeyOrFunction modKeyDef)
        {
            holdShiftHeaderLocked = true;
            var definition = modKeyDef != null ? CommonTableRuntime.GetHoldShiftDefinition(modKeyDef.DecKey) : null;
            int shiftPlane = definition?.ShiftPlane ?? ShiftPlane.ShiftPlane_A;
            comboBox_shiftPlane.SelectedIndex = (shiftPlane - 1)._lowLimit(0)._highLimit(comboBox_shiftPlane.Items.Count - 1);
            checkBox_enabledWhenOff.Checked = definition?.EnabledWhenDecoderOff == true;
            holdShiftHeaderLocked = false;
        }

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
