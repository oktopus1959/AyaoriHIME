
namespace KanchokuWS.Gui
{
    partial class DlgCommonTable
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null)) {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(DlgCommonTable));
            this.buttonOK = new System.Windows.Forms.Button();
            this.buttonCancel = new System.Windows.Forms.Button();
            this.label_modKeys = new System.Windows.Forms.Label();
            this.comboBox_modKeys = new System.Windows.Forms.ComboBox();
            this.dataGridView_extModifier = new System.Windows.Forms.DataGridView();
            this.label_addModKey = new System.Windows.Forms.Label();
            this.comboBox_addModKey = new System.Windows.Forms.ComboBox();
            this.label_shiftPlane = new System.Windows.Forms.Label();
            this.comboBox_shiftPlane = new System.Windows.Forms.ComboBox();
            this.checkBox_enabledWhenOff = new System.Windows.Forms.CheckBox();
            this.radioButton_modKeys = new System.Windows.Forms.RadioButton();
            this.radioButton_singleHit = new System.Windows.Forms.RadioButton();
            this.dataGridView_singleHit = new System.Windows.Forms.DataGridView();
            this.toolTip1 = new System.Windows.Forms.ToolTip(this.components);
            this.label1 = new System.Windows.Forms.Label();
            this.button_openFAQ = new System.Windows.Forms.Button();
            this.groupBox_help = new System.Windows.Forms.GroupBox();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridView_extModifier)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridView_singleHit)).BeginInit();
            this.groupBox_help.SuspendLayout();
            this.SuspendLayout();
            // 
            // buttonOK
            // 
            this.buttonOK.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.buttonOK.Location = new System.Drawing.Point(618, 415);
            this.buttonOK.Name = "buttonOK";
            this.buttonOK.Size = new System.Drawing.Size(75, 23);
            this.buttonOK.TabIndex = 10;
            this.buttonOK.Text = "書き出し(&W)";
            this.toolTip1.SetToolTip(this.buttonOK, "設定内容をファイルに書き出して、ダイアログを閉じます。\r\n\r\n変更した設定内容を漢直WSに反映させるには、元の設定画面で\r\n「再読込」を実行してください。");
            this.buttonOK.UseVisualStyleBackColor = true;
            this.buttonOK.Click += new System.EventHandler(this.buttonOK_Click);
            // 
            // buttonCancel
            // 
            this.buttonCancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.buttonCancel.Location = new System.Drawing.Point(527, 415);
            this.buttonCancel.Name = "buttonCancel";
            this.buttonCancel.Size = new System.Drawing.Size(63, 23);
            this.buttonCancel.TabIndex = 9;
            this.buttonCancel.Text = "閉じる(&C)";
            this.toolTip1.SetToolTip(this.buttonCancel, "ダイアログを閉じます。\r\n\r\nファイルへの書き出しは行いませんが、ダイアログを閉じても修正結果は\r\nメモリ上に残るので、再度開いたときは前回の修正結果が表示されま" +
        "す。\r\n\r\n修正を元に戻したい場合は、ダイアログを閉じた後、元の画面で「再読込」を\r\n実行してください。\r\n");
            this.buttonCancel.UseVisualStyleBackColor = true;
            this.buttonCancel.Click += new System.EventHandler(this.buttonCancel_Click);
            // 
            // label_modKeys
            // 
            this.label_modKeys.AutoSize = true;
            this.label_modKeys.Location = new System.Drawing.Point(13, 9);
            this.label_modKeys.Name = "label_modKeys";
            this.label_modKeys.Size = new System.Drawing.Size(73, 12);
            this.label_modKeys.TabIndex = 2;
            this.label_modKeys.Text = "拡張修飾キー";
            // 
            // comboBox_modKeys
            // 
            this.comboBox_modKeys.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBox_modKeys.FormattingEnabled = true;
            this.comboBox_modKeys.Items.AddRange(new object[] {
            "SandS",
            "CapsLock",
            "英数",
            "無変換",
            "変換",
            "右シフト",
            "左コントロール",
            "右コントロール",
            "シフト"});
            this.comboBox_modKeys.Location = new System.Drawing.Point(88, 5);
            this.comboBox_modKeys.Name = "comboBox_modKeys";
            this.comboBox_modKeys.Size = new System.Drawing.Size(114, 20);
            this.comboBox_modKeys.TabIndex = 0;
            this.toolTip1.SetToolTip(this.comboBox_modKeys, "表示または変更対象となる拡張修飾キーを選択します。\r\n\r\nキー名の末尾に (＊) が付いているものは、\r\n何らかの被修飾キーが定義されていることを\r\n示しています" +
        "。");
            this.comboBox_modKeys.SelectedIndexChanged += new System.EventHandler(this.comboBox_modKeys_SelectedIndexChanged);
            // 
            // dataGridView_extModifier
            // 
            this.dataGridView_extModifier.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.dataGridView_extModifier.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dataGridView_extModifier.Location = new System.Drawing.Point(13, 31);
            this.dataGridView_extModifier.Name = "dataGridView_extModifier";
            this.dataGridView_extModifier.RowTemplate.Height = 21;
            this.dataGridView_extModifier.Size = new System.Drawing.Size(680, 378);
            this.dataGridView_extModifier.TabIndex = 4;
            this.dataGridView_extModifier.TabStop = false;
            this.dataGridView_extModifier.CellMouseClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.dataGridView_extModifier_CellMouseClick);
            this.dataGridView_extModifier.CellMouseDoubleClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.dataGridView_extModifier_CellMouseDoubleClick);
            this.dataGridView_extModifier.CellValueChanged += new System.Windows.Forms.DataGridViewCellEventHandler(this.dataGridView_extModifier_CellValueChanged);
            this.dataGridView_extModifier.ColumnWidthChanged += new System.Windows.Forms.DataGridViewColumnEventHandler(this.dataGridView_extModifier_ColumnWidthChanged);
            this.dataGridView_extModifier.KeyDown += new System.Windows.Forms.KeyEventHandler(this.dataGridView_extModifier_KeyDown);
            // 
            // label_addModKey
            // 
            this.label_addModKey.AutoSize = true;
            this.label_addModKey.Location = new System.Drawing.Point(211, 9);
            this.label_addModKey.Name = "label_addModKey";
            this.label_addModKey.Size = new System.Drawing.Size(41, 12);
            this.label_addModKey.TabIndex = 12;
            this.label_addModKey.Text = "←追加";
            // 
            // comboBox_addModKey
            // 
            this.comboBox_addModKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBox_addModKey.FormattingEnabled = true;
            this.comboBox_addModKey.Location = new System.Drawing.Point(254, 5);
            this.comboBox_addModKey.Name = "comboBox_addModKey";
            this.comboBox_addModKey.Size = new System.Drawing.Size(88, 20);
            this.comboBox_addModKey.TabIndex = 1;
            this.toolTip1.SetToolTip(this.comboBox_addModKey, "新たに追加する拡張修飾キーを選択します。");
            this.comboBox_addModKey.SelectedIndexChanged += new System.EventHandler(this.comboBox_addModKey_SelectedIndexChanged);
            // 
            // label_shiftPlane
            // 
            this.label_shiftPlane.AutoSize = true;
            this.label_shiftPlane.Location = new System.Drawing.Point(357, 9);
            this.label_shiftPlane.Name = "label_shiftPlane";
            this.label_shiftPlane.Size = new System.Drawing.Size(86, 12);
            this.label_shiftPlane.TabIndex = 13;
            this.label_shiftPlane.Text = "使用するシフト面";
            // 
            // comboBox_shiftPlane
            // 
            this.comboBox_shiftPlane.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBox_shiftPlane.FormattingEnabled = true;
            this.comboBox_shiftPlane.Location = new System.Drawing.Point(445, 5);
            this.comboBox_shiftPlane.Name = "comboBox_shiftPlane";
            this.comboBox_shiftPlane.Size = new System.Drawing.Size(102, 20);
            this.comboBox_shiftPlane.TabIndex = 2;
            this.toolTip1.SetToolTip(this.comboBox_shiftPlane, "選択されている拡張修飾キーに対して、使用するシフト面を選択します。");
            this.comboBox_shiftPlane.SelectedIndexChanged += new System.EventHandler(this.comboBox_shiftPlane_SelectedIndexChanged);
            // 
            // checkBox_enabledWhenOff
            // 
            this.checkBox_enabledWhenOff.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.checkBox_enabledWhenOff.AutoSize = true;
            this.checkBox_enabledWhenOff.Location = new System.Drawing.Point(565, 7);
            this.checkBox_enabledWhenOff.Name = "checkBox_enabledWhenOff";
            this.checkBox_enabledWhenOff.Size = new System.Drawing.Size(128, 16);
            this.checkBox_enabledWhenOff.TabIndex = 3;
            this.checkBox_enabledWhenOff.Text = "デコーダOFF時も有効";
            this.toolTip1.SetToolTip(this.checkBox_enabledWhenOff, "チェックすると、デコーダOFF時もこの拡張修飾キーを有効にします。");
            this.checkBox_enabledWhenOff.UseVisualStyleBackColor = true;
            this.checkBox_enabledWhenOff.CheckedChanged += new System.EventHandler(this.checkBox_enabledWhenOff_CheckedChanged);
            // 
            // radioButton_modKeys
            // 
            this.radioButton_modKeys.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.radioButton_modKeys.AutoSize = true;
            this.radioButton_modKeys.Location = new System.Drawing.Point(15, 419);
            this.radioButton_modKeys.Name = "radioButton_modKeys";
            this.radioButton_modKeys.Size = new System.Drawing.Size(91, 16);
            this.radioButton_modKeys.TabIndex = 7;
            this.radioButton_modKeys.TabStop = true;
            this.radioButton_modKeys.Text = "修飾キー設定";
            this.toolTip1.SetToolTip(this.radioButton_modKeys, "拡張修飾キーによるキー設定画面に切り替えます。\r\n\r\n選択した拡張修飾キーごとに、被修飾キーに対して割り当てる\r\n特殊キーや機能を設定します。");
            this.radioButton_modKeys.UseVisualStyleBackColor = true;
            this.radioButton_modKeys.CheckedChanged += new System.EventHandler(this.radioButton_modKeys_CheckedChanged);
            // 
            // radioButton_singleHit
            // 
            this.radioButton_singleHit.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.radioButton_singleHit.AutoSize = true;
            this.radioButton_singleHit.Location = new System.Drawing.Point(120, 419);
            this.radioButton_singleHit.Name = "radioButton_singleHit";
            this.radioButton_singleHit.Size = new System.Drawing.Size(71, 16);
            this.radioButton_singleHit.TabIndex = 8;
            this.radioButton_singleHit.TabStop = true;
            this.radioButton_singleHit.Text = "単打設定";
            this.toolTip1.SetToolTip(this.radioButton_singleHit, "単打キー設定画面に切り替えます。\r\n\r\n拡張修飾キーや特殊キーの単打(押してすぐ離すこと)に対して\r\nキーや機能を設定することができます。");
            this.radioButton_singleHit.UseVisualStyleBackColor = true;
            this.radioButton_singleHit.CheckedChanged += new System.EventHandler(this.radioButton_singleHit_CheckedChanged);
            // 
            // dataGridView_singleHit
            // 
            this.dataGridView_singleHit.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.dataGridView_singleHit.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.dataGridView_singleHit.Location = new System.Drawing.Point(13, 31);
            this.dataGridView_singleHit.Name = "dataGridView_singleHit";
            this.dataGridView_singleHit.RowTemplate.Height = 21;
            this.dataGridView_singleHit.Size = new System.Drawing.Size(680, 378);
            this.dataGridView_singleHit.TabIndex = 3;
            this.dataGridView_singleHit.TabStop = false;
            this.dataGridView_singleHit.CellMouseClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.dataGridView_singleHit_CellMouseClick);
            this.dataGridView_singleHit.CellMouseDoubleClick += new System.Windows.Forms.DataGridViewCellMouseEventHandler(this.dataGridView_singleHit_CellMouseDoubleClick);
            this.dataGridView_singleHit.CellValueChanged += new System.Windows.Forms.DataGridViewCellEventHandler(this.dataGridView_singleHit_CellValueChanged);
            this.dataGridView_singleHit.ColumnWidthChanged += new System.Windows.Forms.DataGridViewColumnEventHandler(this.dataGridView_singleHit_ColumnWidthChanged);
            this.dataGridView_singleHit.KeyDown += new System.Windows.Forms.KeyEventHandler(this.dataGridView_singleHit_KeyDown);
            // 
            // toolTip1
            // 
            this.toolTip1.AutoPopDelay = 32000;
            this.toolTip1.InitialDelay = 100;
            this.toolTip1.ReshowDelay = 100;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(8, 12);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(45, 12);
            this.label1.TabIndex = 0;
            this.label1.Text = "  説明  ";
            this.toolTip1.SetToolTip(this.label1, resources.GetString("label1.ToolTip"));
            // 
            // button_openFAQ
            // 
            this.button_openFAQ.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.button_openFAQ.Location = new System.Drawing.Point(442, 415);
            this.button_openFAQ.Name = "button_openFAQ";
            this.button_openFAQ.Size = new System.Drawing.Size(56, 23);
            this.button_openFAQ.TabIndex = 18;
            this.button_openFAQ.Text = "FAQ(&F)";
            this.toolTip1.SetToolTip(this.button_openFAQ, "「FAQ キーアサイン編」を開きます。\r\n");
            this.button_openFAQ.UseVisualStyleBackColor = true;
            this.button_openFAQ.Click += new System.EventHandler(this.button_openFAQ_Click);
            // 
            // groupBox_help
            // 
            this.groupBox_help.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.groupBox_help.Controls.Add(this.label1);
            this.groupBox_help.Location = new System.Drawing.Point(373, 410);
            this.groupBox_help.Name = "groupBox_help";
            this.groupBox_help.Size = new System.Drawing.Size(58, 28);
            this.groupBox_help.TabIndex = 15;
            this.groupBox_help.TabStop = false;
            // 
            // DlgCommonTable
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 12F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(704, 441);
            this.Controls.Add(this.button_openFAQ);
            this.Controls.Add(this.checkBox_enabledWhenOff);
            this.Controls.Add(this.comboBox_shiftPlane);
            this.Controls.Add(this.label_shiftPlane);
            this.Controls.Add(this.comboBox_addModKey);
            this.Controls.Add(this.label_addModKey);
            this.Controls.Add(this.dataGridView_singleHit);
            this.Controls.Add(this.radioButton_singleHit);
            this.Controls.Add(this.radioButton_modKeys);
            this.Controls.Add(this.dataGridView_extModifier);
            this.Controls.Add(this.buttonOK);
            this.Controls.Add(this.comboBox_modKeys);
            this.Controls.Add(this.buttonCancel);
            this.Controls.Add(this.label_modKeys);
            this.Controls.Add(this.groupBox_help);
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.MinimumSize = new System.Drawing.Size(720, 400);
            this.Name = "DlgCommonTable";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "拡張修飾キー設定";
            this.Load += new System.EventHandler(this.DlgCommonTable_Load);
            ((System.ComponentModel.ISupportInitialize)(this.dataGridView_extModifier)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.dataGridView_singleHit)).EndInit();
            this.groupBox_help.ResumeLayout(false);
            this.groupBox_help.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion
        private System.Windows.Forms.Button buttonOK;
        private System.Windows.Forms.Button buttonCancel;
        private System.Windows.Forms.Label label_modKeys;
        private System.Windows.Forms.ComboBox comboBox_modKeys;
        private System.Windows.Forms.DataGridView dataGridView_extModifier;
        private System.Windows.Forms.Label label_addModKey;
        private System.Windows.Forms.ComboBox comboBox_addModKey;
        private System.Windows.Forms.Label label_shiftPlane;
        private System.Windows.Forms.ComboBox comboBox_shiftPlane;
        private System.Windows.Forms.CheckBox checkBox_enabledWhenOff;
        private System.Windows.Forms.RadioButton radioButton_modKeys;
        private System.Windows.Forms.RadioButton radioButton_singleHit;
        private System.Windows.Forms.DataGridView dataGridView_singleHit;
        private System.Windows.Forms.ToolTip toolTip1;
        private System.Windows.Forms.GroupBox groupBox_help;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button button_openFAQ;
    }
}
