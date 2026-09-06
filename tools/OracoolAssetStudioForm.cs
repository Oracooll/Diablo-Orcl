// OracoolAssetStudio - WinForms UI. See OracoolAssetStudio.cs for the engine-matching logic.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Windows.Forms;

namespace OracoolAssetStudio
{
    public class MainForm : Form
    {
        Palette palette;
        Bitmap sourceImage;
        string sourcePath;

        Quantized quantized;
        Bitmap previewLit, previewDark, previewSource;

        // Left column: the three previews.
        PictureBox pbSource, pbLit, pbDark;
        Label lblSource, lblLit, lblDark, lblStats, lblPalette;

        // Right column: controls.
        TrackBar tbBrightness, tbContrast, tbSaturation, tbGamma, tbHue, tbAlpha, tbLight, tbZoom;
        Label lvBrightness, lvContrast, lvSaturation, lvGamma, lvHue, lvAlpha, lvLight, lvZoom;
        CheckBox cbGlobalHalf, cbCheckerboard;
        ComboBox cmbBackground;
        Button btnOpen, btnPalette, btnReset, btnExport;

        bool suppressUpdate;

        public MainForm()
        {
            Text = "Oracool Asset Studio";
            Width = 1180;
            Height = 820;
            StartPosition = FormStartPosition.CenterScreen;
            MinimumSize = new Size(980, 640);
            AllowDrop = true;
            DragEnter += (s, e) => e.Effect = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None;
            DragDrop += OnDragDrop;

            BuildUi();
            TryAutoLoadPalette();
            UpdateAll();
        }

        void BuildUi()
        {
            var split = new SplitContainer { Dock = DockStyle.Fill, FixedPanel = FixedPanel.Panel2 };
            Controls.Add(split);
            // Must be set AFTER the control is parented and has a real width - assigning it while
            // the container is still at its default size throws InvalidOperationException.
            split.SplitterDistance = Math.Max(400, ClientSize.Width - 330);

            // ---------------- previews ----------------
            var previews = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 4, Padding = new Padding(6) };
            previews.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
            previews.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
            previews.RowStyles.Add(new RowStyle(SizeType.Absolute, 20));
            previews.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
            previews.RowStyles.Add(new RowStyle(SizeType.Absolute, 20));
            previews.RowStyles.Add(new RowStyle(SizeType.Percent, 50));
            split.Panel1.Controls.Add(previews);

            lblSource = MakeCaption("Source (unconverted)");
            lblLit = MakeCaption("In game - full light");
            lblDark = MakeCaption("In game - reduced light");
            lblStats = new Label { Dock = DockStyle.Fill, Font = new Font("Consolas", 8.5f), ForeColor = Color.FromArgb(60, 60, 60) };

            pbSource = MakePreview();
            pbLit = MakePreview();
            pbDark = MakePreview();

            var statsHost = new Panel { Dock = DockStyle.Fill, BackColor = Color.FromArgb(246, 246, 244), BorderStyle = BorderStyle.FixedSingle, Padding = new Padding(6) };
            statsHost.Controls.Add(lblStats);

            previews.Controls.Add(lblSource, 0, 0);
            previews.Controls.Add(lblLit, 1, 0);
            previews.Controls.Add(pbSource, 0, 1);
            previews.Controls.Add(pbLit, 1, 1);
            previews.Controls.Add(lblDark, 0, 2);
            previews.Controls.Add(MakeCaption("Conversion report"), 1, 2);
            previews.Controls.Add(pbDark, 0, 3);
            previews.Controls.Add(statsHost, 1, 3);

            // ---------------- controls ----------------
            var side = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoScroll = true, Padding = new Padding(8) };
            split.Panel2.Controls.Add(side);

            btnOpen = MakeButton("Open image...", (s, e) => OpenImage());
            btnPalette = MakeButton("Load palette (.pal)...", (s, e) => OpenPalette());
            lblPalette = new Label { AutoSize = true, MaximumSize = new Size(300, 0), ForeColor = Color.FromArgb(90, 90, 90), Margin = new Padding(3, 0, 3, 8) };

            side.Controls.Add(btnOpen);
            side.Controls.Add(btnPalette);
            side.Controls.Add(lblPalette);

            side.Controls.Add(MakeHeader("Colour"));
            tbBrightness = MakeSlider(side, "Brightness", -100, 100, 0, out lvBrightness);
            tbContrast = MakeSlider(side, "Contrast", -100, 100, 0, out lvContrast);
            tbSaturation = MakeSlider(side, "Saturation", -100, 100, 0, out lvSaturation);
            tbGamma = MakeSlider(side, "Gamma", 20, 300, 100, out lvGamma);
            tbHue = MakeSlider(side, "Hue shift", -180, 180, 0, out lvHue);

            side.Controls.Add(MakeHeader("Conversion"));
            tbAlpha = MakeSlider(side, "Alpha cutoff", 1, 255, 128, out lvAlpha);
            cbGlobalHalf = new CheckBox { Text = "Match global palette half only (128-255)", Checked = true, AutoSize = true, Margin = new Padding(3, 4, 3, 2) };
            cbGlobalHalf.CheckedChanged += (s, e) => UpdateAll();
            side.Controls.Add(cbGlobalHalf);
            var noteHalf = new Label
            {
                Text = "Entries 0-127 are level-specific and colour-cycled.\nHUD and UI art must use the global half only.",
                AutoSize = true, MaximumSize = new Size(300, 0), ForeColor = Color.FromArgb(120, 120, 120), Margin = new Padding(20, 0, 3, 8)
            };
            side.Controls.Add(noteHalf);

            side.Controls.Add(MakeHeader("Preview"));
            tbLight = MakeSlider(side, "Light level", 5, 100, 45, out lvLight);
            tbZoom = MakeSlider(side, "Zoom", 100, 800, 100, out lvZoom);
            cbCheckerboard = new CheckBox { Text = "Checkerboard behind transparency", Checked = true, AutoSize = true, Margin = new Padding(3, 4, 3, 2) };
            cbCheckerboard.CheckedChanged += (s, e) => UpdateAll();
            side.Controls.Add(cbCheckerboard);

            side.Controls.Add(new Label { Text = "Backdrop", AutoSize = true, Margin = new Padding(3, 8, 3, 0) });
            cmbBackground = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 280, Margin = new Padding(3, 2, 3, 8) };
            cmbBackground.Items.AddRange(new object[] { "Panel stone (dark grey)", "Black", "Magenta (spot stray pixels)", "White" });
            cmbBackground.SelectedIndex = 0;
            cmbBackground.SelectedIndexChanged += (s, e) => UpdateAll();
            side.Controls.Add(cmbBackground);

            side.Controls.Add(MakeHeader("Output"));
            btnReset = MakeButton("Reset adjustments", (s, e) => ResetAdjustments());
            btnExport = MakeButton("Export converted PNG...", (s, e) => Export());
            side.Controls.Add(btnReset);
            side.Controls.Add(btnExport);

            var noteExport = new Label
            {
                Text = "Export writes palette-exact colours with hard alpha,\nso the engine's own conversion is a no-op and what\nyou see here is what the game draws.",
                AutoSize = true, MaximumSize = new Size(300, 0), ForeColor = Color.FromArgb(120, 120, 120), Margin = new Padding(3, 6, 3, 8)
            };
            side.Controls.Add(noteExport);
        }

        Label MakeCaption(string text)
        {
            return new Label { Text = text, Dock = DockStyle.Fill, Font = new Font(Font, FontStyle.Bold), TextAlign = ContentAlignment.MiddleLeft };
        }

        Label MakeHeader(string text)
        {
            return new Label
            {
                Text = text.ToUpperInvariant(), AutoSize = true, Font = new Font(Font.FontFamily, 8f, FontStyle.Bold),
                ForeColor = Color.FromArgb(110, 110, 110), Margin = new Padding(3, 12, 3, 2)
            };
        }

        Button MakeButton(string text, EventHandler onClick)
        {
            var b = new Button { Text = text, Width = 290, Height = 30, Margin = new Padding(3, 3, 3, 3) };
            b.Click += onClick;
            return b;
        }

        PictureBox MakePreview()
        {
            return new PictureBox { Dock = DockStyle.Fill, BorderStyle = BorderStyle.FixedSingle, BackColor = Color.FromArgb(58, 56, 52), SizeMode = PictureBoxSizeMode.CenterImage };
        }

        TrackBar MakeSlider(Control parent, string label, int min, int max, int value, out Label valueLabel)
        {
            var row = new Panel { Width = 296, Height = 18, Margin = new Padding(3, 6, 3, 0) };
            var name = new Label { Text = label, AutoSize = true, Location = new Point(0, 0) };
            valueLabel = new Label { AutoSize = true, Location = new Point(210, 0), ForeColor = Color.FromArgb(90, 90, 90), Width = 80 };
            row.Controls.Add(name);
            row.Controls.Add(valueLabel);
            parent.Controls.Add(row);

            var tb = new TrackBar { Minimum = min, Maximum = max, Value = value, Width = 296, TickStyle = TickStyle.None, Margin = new Padding(0, 0, 0, 0) };
            tb.ValueChanged += (s, e) => { if (!suppressUpdate) UpdateAll(); };
            parent.Controls.Add(tb);
            return tb;
        }

        // ---------------- loading ----------------

        void TryAutoLoadPalette()
        {
            // Look where the palette actually lives in this project before asking the user.
            string[] candidates =
            {
                "town.pal",
                @"..\..\Resources\00-original-game-art\raw\levels\towndata\town.pal",
                @"..\..\..\Resources\00-original-game-art\raw\levels\towndata\town.pal",
                @"Packaging\resources\assets\ui_art\diablo.pal",
                @"..\Packaging\resources\assets\ui_art\diablo.pal",
            };
            foreach (var rel in candidates)
            {
                try
                {
                    string full = Path.GetFullPath(rel);
                    if (File.Exists(full)) { palette = Palette.Load(full); lblPalette.Text = "Palette: " + palette.Name; return; }
                }
                catch { }
            }
            lblPalette.Text = "Palette: none loaded - previews unavailable.\nLoad town.pal to match the game.";
            lblPalette.ForeColor = Color.Firebrick;
        }

        void OpenPalette()
        {
            using (var dlg = new OpenFileDialog { Filter = "Diablo palette (*.pal)|*.pal|All files (*.*)|*.*" })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                try
                {
                    palette = Palette.Load(dlg.FileName);
                    lblPalette.Text = "Palette: " + palette.Name;
                    lblPalette.ForeColor = Color.FromArgb(90, 90, 90);
                    UpdateAll();
                }
                catch (Exception ex) { MessageBox.Show(this, ex.Message, "Could not load palette"); }
            }
        }

        void OpenImage()
        {
            using (var dlg = new OpenFileDialog { Filter = "Images (*.png;*.bmp;*.jpg;*.jpeg;*.gif)|*.png;*.bmp;*.jpg;*.jpeg;*.gif|All files (*.*)|*.*" })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                LoadImage(dlg.FileName);
            }
        }

        void OnDragDrop(object sender, DragEventArgs e)
        {
            var files = (string[])e.Data.GetData(DataFormats.FileDrop);
            if (files.Length == 0) return;
            if (files[0].EndsWith(".pal", StringComparison.OrdinalIgnoreCase))
            {
                try { palette = Palette.Load(files[0]); lblPalette.Text = "Palette: " + palette.Name; lblPalette.ForeColor = Color.FromArgb(90, 90, 90); UpdateAll(); }
                catch (Exception ex) { MessageBox.Show(this, ex.Message, "Could not load palette"); }
                return;
            }
            LoadImage(files[0]);
        }

        void LoadImage(string path)
        {
            try
            {
                if (sourceImage != null) sourceImage.Dispose();
                using (var tmp = new Bitmap(path))
                    sourceImage = new Bitmap(tmp);
                sourcePath = path;
                Text = "Oracool Asset Studio - " + Path.GetFileName(path);
                UpdateAll();
            }
            catch (Exception ex) { MessageBox.Show(this, ex.Message, "Could not open image"); }
        }

        void ResetAdjustments()
        {
            suppressUpdate = true;
            tbBrightness.Value = 0; tbContrast.Value = 0; tbSaturation.Value = 0;
            tbGamma.Value = 100; tbHue.Value = 0;
            suppressUpdate = false;
            UpdateAll();
        }

        // ---------------- pipeline ----------------

        Adjustments CurrentAdjustments()
        {
            return new Adjustments
            {
                Brightness = tbBrightness.Value / 100.0,
                Contrast = tbContrast.Value / 100.0,
                Saturation = tbSaturation.Value / 100.0,
                Gamma = tbGamma.Value / 100.0,
                HueShift = tbHue.Value,
            };
        }

        Color BackdropColor()
        {
            switch (cmbBackground.SelectedIndex)
            {
                case 1: return Color.Black;
                case 2: return Color.Magenta;
                case 3: return Color.White;
                default: return Color.FromArgb(81, 78, 73); // the inventory panel's stone
            }
        }

        void UpdateAll()
        {
            var adj = CurrentAdjustments();
            lvBrightness.Text = tbBrightness.Value.ToString();
            lvContrast.Text = tbContrast.Value.ToString();
            lvSaturation.Text = tbSaturation.Value.ToString();
            lvGamma.Text = adj.Gamma.ToString("0.00");
            lvHue.Text = tbHue.Value + "В°";
            lvAlpha.Text = tbAlpha.Value.ToString();
            lvLight.Text = tbLight.Value + "%";
            lvZoom.Text = tbZoom.Value + "%";

            if (sourceImage == null || palette == null)
            {
                lblStats.Text = sourceImage == null
                    ? "Open an image, or drag one onto the window.\n\nA .pal file can be dropped here too."
                    : "No palette loaded.";
                return;
            }

            int w, h;
            byte[] bgra = Img.GetBgra(sourceImage, out w, out h);

            // Source preview shows the adjustments but not the conversion, so the two panes
            // isolate "what my edits did" from "what the palette did to my edits".
            var adjusted = (byte[])bgra.Clone();
            Img.Apply(adjusted, adj);
            SetPreview(pbSource, ref previewSource, CompositeOver(adjusted, w, h, BackdropColor()));

            quantized = Quantizer.Run(adjusted, w, h, palette, cbGlobalHalf.Checked, tbAlpha.Value);

            SetPreview(pbLit, ref previewLit, Quantizer.Render(quantized, palette, null, BackdropColor()));

            int[] remap = Quantizer.BuildLightRemap(palette, tbLight.Value / 100.0, cbGlobalHalf.Checked);
            SetPreview(pbDark, ref previewDark, Quantizer.Render(quantized, palette, remap, Darken(BackdropColor(), tbLight.Value / 100.0)));

            int distinctDark = CountDistinct(quantized, remap);
            long total = (long)w * h;
            double transparentPct = total > 0 ? 100.0 * quantized.TransparentCount / total : 0;

            lblStats.Text =
                string.Format(
                    " size            {0} x {1}\n" +
                    " palette         {2}\n" +
                    " matched against {3}\n" +
                    " colours used    {4}\n" +
                    " colours at {5,3}%  {6}{7}\n" +
                    " transparent     {8:0.0}%  (alpha < {9})\n" +
                    " mean shift      {10:0.0}\n" +
                    " worst shift     {11:0.0}{12}",
                    w, h,
                    palette.Name,
                    cbGlobalHalf.Checked ? "entries 128-255" : "entries 0-255",
                    quantized.DistinctColors,
                    tbLight.Value, distinctDark,
                    distinctDark < quantized.DistinctColors ? "   <-- collapses in the dark" : "",
                    transparentPct, tbAlpha.Value,
                    quantized.MeanError,
                    quantized.MaxError,
                    quantized.MaxError > 60 ? "   <-- large; consider adjusting" : "");
        }

        static Color Darken(Color c, double f)
        {
            return Color.FromArgb((int)(c.R * f), (int)(c.G * f), (int)(c.B * f));
        }

        static int CountDistinct(Quantized q, int[] remap)
        {
            var seen = new bool[256];
            for (int p = 0; p < q.Index.Length; p++)
                if (!q.Transparent[p]) seen[remap == null ? q.Index[p] : remap[q.Index[p]]] = true;
            int n = 0;
            foreach (bool s in seen) if (s) n++;
            return n;
        }

        Bitmap CompositeOver(byte[] bgra, int w, int h, Color background)
        {
            var buf = (byte[])bgra.Clone();
            for (int i = 0; i < buf.Length; i += 4)
            {
                double a = buf[i + 3] / 255.0;
                buf[i] = (byte)(buf[i] * a + background.B * (1 - a));
                buf[i + 1] = (byte)(buf[i + 1] * a + background.G * (1 - a));
                buf[i + 2] = (byte)(buf[i + 2] * a + background.R * (1 - a));
                buf[i + 3] = 255;
            }
            return Img.FromBgra(buf, w, h);
        }

        void SetPreview(PictureBox box, ref Bitmap slot, Bitmap fresh)
        {
            Bitmap shown = fresh;
            int zoom = tbZoom.Value;
            if (zoom != 100)
            {
                int zw = Math.Max(1, fresh.Width * zoom / 100), zh = Math.Max(1, fresh.Height * zoom / 100);
                var scaled = new Bitmap(zw, zh, PixelFormat.Format32bppArgb);
                using (var g = Graphics.FromImage(scaled))
                {
                    // Nearest neighbour: at this size any smoothing would hide exactly the
                    // per-pixel artefacts this tool exists to show.
                    g.InterpolationMode = InterpolationMode.NearestNeighbor;
                    g.PixelOffsetMode = PixelOffsetMode.Half;
                    g.DrawImage(fresh, new Rectangle(0, 0, zw, zh));
                }
                fresh.Dispose();
                shown = scaled;
            }

            if (cbCheckerboard.Checked) shown = WithCheckerboard(shown);

            if (slot != null) slot.Dispose();
            slot = shown;
            box.Image = shown;
        }

        static Bitmap WithCheckerboard(Bitmap img)
        {
            // Drawn *behind* nothing here (previews are already composited); this is a border
            // marker so the asset's extent is visible against a matching backdrop.
            var outBmp = new Bitmap(img.Width + 8, img.Height + 8, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(outBmp))
            {
                const int cell = 8;
                using (var a = new SolidBrush(Color.FromArgb(70, 70, 70)))
                using (var b = new SolidBrush(Color.FromArgb(90, 90, 90)))
                {
                    for (int y = 0; y < outBmp.Height; y += cell)
                        for (int x = 0; x < outBmp.Width; x += cell)
                            g.FillRectangle(((x / cell + y / cell) % 2 == 0) ? a : b, x, y, cell, cell);
                }
                g.DrawImage(img, 4, 4);
            }
            img.Dispose();
            return outBmp;
        }

        void Export()
        {
            if (quantized == null || palette == null) { MessageBox.Show(this, "Nothing converted yet.", "Export"); return; }
            using (var dlg = new SaveFileDialog { Filter = "PNG (*.png)|*.png", FileName = SuggestName() })
            {
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                try
                {
                    using (var bmp = Quantizer.Export(quantized, palette))
                        bmp.Save(dlg.FileName, ImageFormat.Png);
                    MessageBox.Show(this, "Written:\n" + dlg.FileName, "Export");
                }
                catch (Exception ex) { MessageBox.Show(this, ex.Message, "Could not export"); }
            }
        }

        string SuggestName()
        {
            if (string.IsNullOrEmpty(sourcePath)) return "asset.png";
            return Path.GetFileNameWithoutExtension(sourcePath) + "_diablo.png";
        }

        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }
}
