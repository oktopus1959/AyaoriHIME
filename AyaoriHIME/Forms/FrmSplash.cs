using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using KanchokuWS.Gui;
using Utils;

namespace KanchokuWS
{
    public partial class FrmSplash : Form
    {
        private static Logger logger = Logger.GetLogger();

        private const int timerInterval = 200;

        //private FrmKanchoku frmMain;
        private int showCount = 30 * (1000 / timerInterval);

        public FrmSplash(int sec)
        {
            //frmMain = frm;
            showCount = sec * (1000 / timerInterval);

            InitializeComponent();

            // タイトルバーを消す
            FormBorderStyle = FormBorderStyle.None;

            float dpi = this.DeviceDpi;
            float scale = dpi / 96f;

            label_version.Text = $"{KanchokuWS.ProductVersion.ProductSplashName} - {KanchokuWS.ProductVersion.Version}";
            label_version2.Text = KanchokuWS.ProductVersion.Version2;
            label_subName.Text = $"{KanchokuWS.ProductVersion.ProductKanjiName}";
            label_subName.Font = Utils.FontCallback.CreateFallbackFont(
                label_subName,
                label_subName.Font.Size,
                label_subName.Font.Style,
                "HG正楷書体-PRO",
                "HG正楷書体-PRO M",
                "HG正楷書体",
                "HGS正楷書体-PRO",
                "HGS正楷書体",
                "游明朝",
                "Yu Mincho",
                "MS 明朝",
                "MS Mincho");
            label_explanation.Text = $"{KanchokuWS.ProductVersion.ProductExplanation}";
            label_explanation2.Text = $"{KanchokuWS.ProductVersion.ProductExplanation2}";

            float clientWidth = this.ClientSize.Width * scale;
            float clientHeight = this.ClientSize.Height * scale;

            label_version.Left = (int)((clientWidth - label_version.Width) / 2);
            label_explanation.Left = (int)((clientWidth - label_explanation.Width) / 2);
            label_explanation2.Left = (int)((clientWidth - label_explanation2.Width) / 2);
            label_subName.Left = (int)((clientWidth - label_subName.Width) / 2);
            buttonOK.Left = (int)((clientWidth / 2) - (10 * scale + buttonOK.Width));
            buttonSettings.Left = (int)((clientWidth / 2) + (10 * scale));

            double dpiRate = ScreenInfo.Singleton.GetScreenDpiRate(this.Left, this.Top);
            this.Width = (int)(this.Width * scale);
            this.Height = (int)(this.Height * scale);
            label_version.Top = (int)(label_version.Top * scale);
            ////label_version.Left = (int)(label_version.Left * scale);
            label_subName.Top = (int)(label_subName.Top * scale);
            //label_subName.Left = (int)(label_subName.Left * scale);
            label_explanation.Top = (int)(label_explanation.Top * scale);
            label_explanation2.Top = (int)(label_explanation2.Top * scale);
            label_initializing.Top = (int)(label_initializing.Top * scale);
            //label_initializing.Left = (int)(label_initializing.Left * scale);
            buttonOK.Top = (int)(buttonOK.Top * scale);
            //buttonOK.Left = (int)(buttonOK.Left * scale);
            //buttonOK.Width = (int)(buttonOK.Width * scale);
            //buttonOK.Height = (int)(buttonOK.Height * scale);
            buttonSettings.Top = (int)(buttonSettings.Top * scale);
            //buttonSettings.Left = (int)(buttonSettings.Left * scale);
            //buttonSettings.Width = (int)(buttonSettings.Width * scale);
            //buttonSettings.Height = (int)(buttonSettings.Height * scale);
        }

        const int CS_DROPSHADOW = 0x00020000;

        /// <summary> フォームに影をつける </summary>
        protected override CreateParams CreateParams {
            get {
                CreateParams cp = base.CreateParams;
                cp.ClassStyle |= CS_DROPSHADOW;
                return cp;
            }
        }

        private void FormSplash_Load(object sender, EventArgs e)
        {
            timer1.Interval = timerInterval;
            timer1.Start();
            logger.Info("Timer Started");
        }

        public bool IsKanchokuReady { get; set; } = false;

        public bool IsKanchokuTerminated { get; set; } = false;

        public delegate void DelegateSplashClosedListener();

        public DelegateSplashClosedListener SplashClosedListener { get; set; }

        public void Fallback()
        {
            TopMost = false;
            Hide();
            IsKanchokuReady = true;
        }

        private void checkKanchokuReady()
        {
            if (IsKanchokuReady) {
                label_initializing.Hide();
                buttonOK.Show();
                buttonSettings.Show();
            }
        }

        public bool SettingsDialogOpenFlag { get; private set; } = false;

        private void buttonSettings_Click(object sender, EventArgs e)
        {
            SettingsDialogOpenFlag = true;
            this.Close();
        }

        private void buttonOK_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            if (!buttonOK.Visible) {
                checkKanchokuReady();
            } else {
                if (showCount > 0) {
                    --showCount;
                    if (showCount == 0 || IsKanchokuTerminated) {
                        this.Close();
                    }
                }
            }
        }

        private void FormSplash_FormClosing(object sender, FormClosingEventArgs e)
        {
            timer1.Stop();
            logger.Info("Timer Stopped");
            SplashClosedListener?.Invoke();
        }

    }
}
