
namespace KanchokuWS
{
    partial class FrmSplash
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
            this.buttonOK = new System.Windows.Forms.Button();
            this.label_subName = new System.Windows.Forms.Label();
            this.label_version = new System.Windows.Forms.Label();
            this.label_initializing = new System.Windows.Forms.Label();
            this.timer1 = new System.Windows.Forms.Timer(this.components);
            this.buttonSettings = new System.Windows.Forms.Button();
            this.label_version2 = new System.Windows.Forms.Label();
            this.label_explanation = new System.Windows.Forms.Label();
            this.label_explanation2 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // buttonOK
            // 
            this.buttonOK.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.buttonOK.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(128)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.buttonOK.FlatAppearance.BorderColor = System.Drawing.Color.FromArgb(((int)(((byte)(128)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.buttonOK.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonOK.Font = new System.Drawing.Font("BIZ UDゴシック", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(128)));
            this.buttonOK.ForeColor = System.Drawing.SystemColors.ButtonFace;
            this.buttonOK.Location = new System.Drawing.Point(56, 132);
            this.buttonOK.Name = "buttonOK";
            this.buttonOK.Size = new System.Drawing.Size(50, 22);
            this.buttonOK.TabIndex = 10;
            this.buttonOK.Text = "OK";
            this.buttonOK.UseVisualStyleBackColor = false;
            this.buttonOK.Visible = false;
            this.buttonOK.Click += new System.EventHandler(this.buttonOK_Click);
            // 
            // label_subName
            // 
            this.label_subName.AutoSize = true;
            this.label_subName.Font = new System.Drawing.Font("HGS正楷書体", 24F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(128)));
            this.label_subName.Location = new System.Drawing.Point(57, 45);
            this.label_subName.Name = "label_subName";
            this.label_subName.Size = new System.Drawing.Size(111, 33);
            this.label_subName.TabIndex = 7;
            this.label_subName.Text = "漢織媛";
            // 
            // label_version
            // 
            this.label_version.AutoSize = true;
            this.label_version.Font = new System.Drawing.Font("游ゴシック", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(128)));
            this.label_version.Location = new System.Drawing.Point(41, 18);
            this.label_version.Name = "label_version";
            this.label_version.Size = new System.Drawing.Size(73, 16);
            this.label_version.TabIndex = 6;
            this.label_version.Text = "Product Ver.";
            // 
            // label_initializing
            // 
            this.label_initializing.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.label_initializing.AutoSize = true;
            this.label_initializing.Font = new System.Drawing.Font("BIZ UDゴシック", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(128)));
            this.label_initializing.Location = new System.Drawing.Point(22, 136);
            this.label_initializing.Name = "label_initializing";
            this.label_initializing.Size = new System.Drawing.Size(167, 12);
            this.label_initializing.TabIndex = 11;
            this.label_initializing.Text = "辞書ファイルを読み込み中...";
            // 
            // timer1
            // 
            this.timer1.Tick += new System.EventHandler(this.timer1_Tick);
            // 
            // buttonSettings
            // 
            this.buttonSettings.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.buttonSettings.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(128)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.buttonSettings.FlatAppearance.BorderColor = System.Drawing.Color.FromArgb(((int)(((byte)(128)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.buttonSettings.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.buttonSettings.Font = new System.Drawing.Font("BIZ UDゴシック", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(128)));
            this.buttonSettings.ForeColor = System.Drawing.SystemColors.ButtonFace;
            this.buttonSettings.Location = new System.Drawing.Point(126, 132);
            this.buttonSettings.Name = "buttonSettings";
            this.buttonSettings.Size = new System.Drawing.Size(50, 22);
            this.buttonSettings.TabIndex = 12;
            this.buttonSettings.Text = "設定";
            this.buttonSettings.UseVisualStyleBackColor = false;
            this.buttonSettings.Visible = false;
            this.buttonSettings.Click += new System.EventHandler(this.buttonSettings_Click);
            // 
            // label_version2
            // 
            this.label_version2.AutoSize = true;
            this.label_version2.Font = new System.Drawing.Font("BIZ UDゴシック", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(128)));
            this.label_version2.Location = new System.Drawing.Point(75, 35);
            this.label_version2.Name = "label_version2";
            this.label_version2.Size = new System.Drawing.Size(0, 12);
            this.label_version2.TabIndex = 13;
            // 
            // label_explanation
            // 
            this.label_explanation.AutoSize = true;
            this.label_explanation.Font = new System.Drawing.Font("BIZ UDゴシック", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(128)));
            this.label_explanation.Location = new System.Drawing.Point(65, 89);
            this.label_explanation.Name = "label_explanation";
            this.label_explanation.Size = new System.Drawing.Size(83, 12);
            this.label_explanation.TabIndex = 14;
            this.label_explanation.Text = "製品名 - 説明";
            // 
            // label_explanation2
            // 
            this.label_explanation2.AutoSize = true;
            this.label_explanation2.Font = new System.Drawing.Font("BIZ UDゴシック", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(128)));
            this.label_explanation2.Location = new System.Drawing.Point(60, 107);
            this.label_explanation2.Name = "label_explanation2";
            this.label_explanation2.Size = new System.Drawing.Size(89, 12);
            this.label_explanation2.TabIndex = 15;
            this.label_explanation2.Text = "製品名 - 説明2";
            // 
            // FrmSplash
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 12F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(201)))), ((int)(((byte)(171)))), ((int)(((byte)(160)))));
            this.ClientSize = new System.Drawing.Size(224, 161);
            this.Controls.Add(this.label_explanation2);
            this.Controls.Add(this.label_explanation);
            this.Controls.Add(this.label_version2);
            this.Controls.Add(this.buttonSettings);
            this.Controls.Add(this.buttonOK);
            this.Controls.Add(this.label_subName);
            this.Controls.Add(this.label_version);
            this.Controls.Add(this.label_initializing);
            this.Name = "FrmSplash";
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "FormSplash";
            this.TopMost = true;
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.FormSplash_FormClosing);
            this.Load += new System.EventHandler(this.FormSplash_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button buttonOK;
        private System.Windows.Forms.Label label_subName;
        private System.Windows.Forms.Label label_version;
        private System.Windows.Forms.Label label_initializing;
        private System.Windows.Forms.Timer timer1;
        private System.Windows.Forms.Button buttonSettings;
        private System.Windows.Forms.Label label_version2;
        private System.Windows.Forms.Label label_explanation;
        private System.Windows.Forms.Label label_explanation2;
    }
}