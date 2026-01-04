using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Text;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Utils
{
    public class FontCallback
    {
        public static Font CreateFallbackFont(
            Control control,
            float basePointSize,
            FontStyle style,
            params string[] preferredFamilies)
        {
            // インストール済みフォント一覧
            using (var fonts = new InstalledFontCollection()) {
                var installed = fonts.Families.Select(f => f.Name).ToHashSet();

                // 最初に見つかったフォントを選ぶ
                string family = preferredFamilies.FirstOrDefault(installed.Contains)
                                ?? control.Font.FontFamily.Name; // 最後の保険（現在の既定）

                // DPI スケール（96dpiを基準）
                float scale = control.DeviceDpi / 96f;
                float size = basePointSize * scale;

                return new Font(family, size, style, GraphicsUnit.Point);
            }
        }

    }
}
