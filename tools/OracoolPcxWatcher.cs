// Oracool PCX Watcher
//
// Watches a folder and converts every .pcx that appears (or is already there) into a .jpg,
// optionally deleting the original. Built for the Oracool screenshot workflow: the in-game
// screenshot key saves .pcx (Source/capture.cpp), which nothing on Windows opens natively.
//
// Standalone WinForms app with no third-party dependencies - it compiles with the C# compiler
// that ships inside Windows itself (%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe), so
// the .exe can be rebuilt on any Windows box without installing a toolchain. See
// build_pcx_watcher.cmd next to this file.
//
// Decoding matches oracool_pcx_to_png.ps1: 128-byte header, RLE-compressed 8bpp scanlines, and a
// trailing 769-byte VGA palette (marker 0x0C + 768 bytes RGB). The image is assembled via LockBits
// rather than per-pixel writes, so a 1280x720 screenshot converts in a few tens of milliseconds.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace Oracool
{
    public class MainForm : Form
    {
        private TextBox folderBox;
        private Button browseButton;
        private NumericUpDown qualityBox;
        private CheckBox deleteBox;
        private Button startButton;
        private Button convertNowButton;
        private Label statusLabel;
        private TextBox logBox;

        private Thread worker;
        private volatile bool running;
        private volatile bool oneShot;

        // The worker reads its settings from these instead of touching the controls. Calling
        // Invoke() from the worker would deadlock against the shutdown path, where the UI thread
        // sits in Thread.Join waiting for that same worker.
        private volatile string workerFolder = "";
        private volatile bool workerDelete = true;
        private int workerQuality = 92;

        // Tracks file sizes between sweeps, so a screenshot still being written is left alone
        // until its size settles.
        private readonly Dictionary<string, long> pending = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);
        private int convertedCount;

        private ImageCodecInfo jpegCodec;
        private string settingsPath;

        [STAThread]
        public static void Main(string[] args)
        {
            // Headless mode for scripting / scheduled tasks:
            //   OracoolPcxWatcher.exe --once <folder> [--keep] [--quality N]
            // Converts everything currently in <folder> and exits without showing a window.
            if (args.Length >= 2 && args[0] == "--once")
            {
                RunOnceHeadless(args);
                return;
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }

        private static void RunOnceHeadless(string[] args)
        {
            string folder = args[1];
            bool keep = false;
            long quality = 92;
            for (int i = 2; i < args.Length; i++)
            {
                if (args[i] == "--keep") { keep = true; }
                else if (args[i] == "--quality" && i + 1 < args.Length)
                {
                    long q;
                    if (long.TryParse(args[i + 1], NumberStyles.Integer, CultureInfo.InvariantCulture, out q) && q >= 1 && q <= 100)
                    {
                        quality = q;
                    }
                    i++;
                }
            }

            if (!Directory.Exists(folder)) { Environment.ExitCode = 1; return; }

            MainForm converter = new MainForm(true);
            int done = 0;
            foreach (string path in Directory.GetFiles(folder, "*.pcx"))
            {
                string outPath = Path.ChangeExtension(path, ".jpg");
                try
                {
                    converter.ConvertPcxToJpeg(path, outPath, quality);
                    bool ok = File.Exists(outPath) && new FileInfo(outPath).Length > 0;
                    if (ok && !keep) { File.Delete(path); }
                    if (ok) { done++; }
                }
                catch { }
            }
            Environment.ExitCode = (done > 0 || Directory.GetFiles(folder, "*.pcx").Length == 0) ? 0 : 2;
        }

        /// <summary>Headless constructor: sets up only what ConvertPcxToJpeg needs, no controls.</summary>
        private MainForm(bool headless)
        {
            foreach (ImageCodecInfo codec in ImageCodecInfo.GetImageEncoders())
            {
                if (codec.MimeType == "image/jpeg") { jpegCodec = codec; break; }
            }
        }

        public MainForm()
        {
            Text = "Oracool PCX Watcher";
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(560, 340);
            Font = new Font("Segoe UI", 9f);

            foreach (ImageCodecInfo codec in ImageCodecInfo.GetImageEncoders())
            {
                if (codec.MimeType == "image/jpeg") { jpegCodec = codec; break; }
            }

            Label folderLabel = new Label();
            folderLabel.Text = "Watch folder";
            folderLabel.SetBounds(12, 15, 75, 20);
            Controls.Add(folderLabel);

            folderBox = new TextBox();
            folderBox.SetBounds(92, 12, 366, 23);
            Controls.Add(folderBox);

            browseButton = new Button();
            browseButton.Text = "Browse...";
            browseButton.SetBounds(464, 11, 84, 25);
            browseButton.Click += OnBrowse;
            Controls.Add(browseButton);

            Label qualityLabel = new Label();
            qualityLabel.Text = "JPEG quality";
            qualityLabel.SetBounds(12, 49, 75, 20);
            Controls.Add(qualityLabel);

            qualityBox = new NumericUpDown();
            qualityBox.Minimum = 1;
            qualityBox.Maximum = 100;
            qualityBox.Value = 92;
            qualityBox.SetBounds(92, 46, 60, 23);
            Controls.Add(qualityBox);

            deleteBox = new CheckBox();
            deleteBox.Text = "Delete the .pcx after a successful conversion";
            deleteBox.Checked = true;
            deleteBox.SetBounds(172, 47, 300, 22);
            Controls.Add(deleteBox);

            startButton = new Button();
            startButton.Text = "Start watching";
            startButton.SetBounds(12, 82, 140, 30);
            startButton.Click += OnStartStop;
            Controls.Add(startButton);

            convertNowButton = new Button();
            convertNowButton.Text = "Convert existing now";
            convertNowButton.SetBounds(160, 82, 150, 30);
            convertNowButton.Click += OnConvertNow;
            Controls.Add(convertNowButton);

            statusLabel = new Label();
            statusLabel.SetBounds(320, 89, 228, 20);
            statusLabel.Text = "Idle";
            Controls.Add(statusLabel);

            logBox = new TextBox();
            logBox.Multiline = true;
            logBox.ReadOnly = true;
            logBox.ScrollBars = ScrollBars.Vertical;
            logBox.BackColor = Color.White;
            logBox.Font = new Font("Consolas", 8.5f);
            logBox.SetBounds(12, 122, 536, 206);
            Controls.Add(logBox);

            settingsPath = Path.Combine(Path.GetDirectoryName(Application.ExecutablePath), "OracoolPcxWatcher.cfg");
            LoadSettings();
            if (folderBox.Text.Length == 0) { folderBox.Text = GuessDefaultFolder(); }

            // Keep the worker's copies in step, so changing these mid-run takes effect immediately.
            qualityBox.ValueChanged += delegate { Thread.VolatileWrite(ref workerQuality, (int)qualityBox.Value); };
            deleteBox.CheckedChanged += delegate { workerDelete = deleteBox.Checked; };

            FormClosing += OnClosing;
            Log("Ready. Pick a folder and press Start watching.");
        }

        private string GuessDefaultFolder()
        {
            // The tool lives in <repo>\tools. Screenshots go to build\x64-Debug\Screenshots; older
            // ones may still be sitting in Saved_Games, which is where the game used to put them.
            try
            {
                string exeDir = Path.GetDirectoryName(Application.ExecutablePath);
                string shots = Path.GetFullPath(Path.Combine(exeDir, @"..\build\x64-Debug\Screenshots"));
                if (Directory.Exists(shots)) { return shots; }
                string saves = Path.GetFullPath(Path.Combine(exeDir, @"..\build\x64-Debug\Saved_Games"));
                if (Directory.Exists(saves)) { return shots; } // prefer the new folder; created on demand below
            }
            catch { }
            return "";
        }

        /// <summary>The game only creates the Screenshots folder when the first screenshot is taken,
        /// so the watcher may legitimately be pointed at a folder that does not exist yet. Create it
        /// rather than refusing to start.</summary>
        private bool EnsureFolderExists()
        {
            if (Directory.Exists(folderBox.Text)) { return true; }
            try
            {
                Directory.CreateDirectory(folderBox.Text);
                Log("Created " + folderBox.Text);
                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, "Could not use that folder:\r\n\r\n" + folderBox.Text + "\r\n\r\n" + ex.Message,
                    "Oracool PCX Watcher", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return false;
            }
        }

        private void LoadSettings()
        {
            try
            {
                if (!File.Exists(settingsPath)) { return; }
                foreach (string line in File.ReadAllLines(settingsPath))
                {
                    int eq = line.IndexOf('=');
                    if (eq <= 0) { continue; }
                    string key = line.Substring(0, eq).Trim();
                    string val = line.Substring(eq + 1).Trim();
                    if (key == "folder") { folderBox.Text = val; }
                    else if (key == "quality")
                    {
                        int q;
                        if (int.TryParse(val, NumberStyles.Integer, CultureInfo.InvariantCulture, out q) && q >= 1 && q <= 100)
                        {
                            qualityBox.Value = q;
                        }
                    }
                    else if (key == "delete") { deleteBox.Checked = (val == "1"); }
                }
            }
            catch { }
        }

        private void SaveSettings()
        {
            try
            {
                string[] lines = new string[]
                {
                    "folder=" + folderBox.Text,
                    "quality=" + ((int)qualityBox.Value).ToString(CultureInfo.InvariantCulture),
                    "delete=" + (deleteBox.Checked ? "1" : "0")
                };
                File.WriteAllLines(settingsPath, lines);
            }
            catch { }
        }

        private void OnBrowse(object sender, EventArgs e)
        {
            FolderBrowserDialog dlg = new FolderBrowserDialog();
            dlg.Description = "Choose the folder to watch for .pcx screenshots";
            if (Directory.Exists(folderBox.Text)) { dlg.SelectedPath = folderBox.Text; }
            if (dlg.ShowDialog(this) == DialogResult.OK) { folderBox.Text = dlg.SelectedPath; }
        }

        private void OnStartStop(object sender, EventArgs e)
        {
            if (running) { StopWorker(); return; }

            if (!EnsureFolderExists()) { return; }

            SaveSettings();
            pending.Clear();
            workerFolder = folderBox.Text;
            workerDelete = deleteBox.Checked;
            Thread.VolatileWrite(ref workerQuality, (int)qualityBox.Value);
            running = true;
            oneShot = false;
            SetUiRunning(true);
            Log("Watching " + folderBox.Text);
            Log(deleteBox.Checked
                ? "Originals will be deleted once the .jpg is confirmed written."
                : "Originals will be kept.");

            worker = new Thread(WorkerLoop);
            worker.IsBackground = true;
            worker.Start();
        }

        private void OnConvertNow(object sender, EventArgs e)
        {
            if (running) { return; }
            if (!EnsureFolderExists()) { return; }

            SaveSettings();
            pending.Clear();
            workerFolder = folderBox.Text;
            workerDelete = deleteBox.Checked;
            Thread.VolatileWrite(ref workerQuality, (int)qualityBox.Value);
            running = true;
            oneShot = true;
            SetUiRunning(true);
            Log("Converting everything already in the folder...");

            worker = new Thread(WorkerLoop);
            worker.IsBackground = true;
            worker.Start();
        }

        private void StopWorker()
        {
            running = false;
            if (worker != null)
            {
                worker.Join(2000);
                worker = null;
            }
            SetUiRunning(false);
            Log("Stopped.");
        }

        private void SetUiRunning(bool isRunning)
        {
            startButton.Text = isRunning && !oneShot ? "Stop watching" : "Start watching";
            startButton.Enabled = !(isRunning && oneShot);
            convertNowButton.Enabled = !isRunning;
            browseButton.Enabled = !isRunning;
            folderBox.Enabled = !isRunning;
            statusLabel.Text = isRunning
                ? (oneShot ? "Converting..." : "Watching. Converted: " + convertedCount)
                : "Idle. Converted: " + convertedCount;
        }

        private void WorkerLoop()
        {
            // A one-shot run needs two passes: the first records file sizes, the second converts
            // whatever stayed unchanged. Files at rest therefore convert immediately.
            if (oneShot)
            {
                Sweep();
                Sweep();
                BeginInvoke(new MethodInvoker(delegate
                {
                    running = false;
                    oneShot = false;
                    SetUiRunning(false);
                    Log("Done.");
                }));
                return;
            }

            while (running)
            {
                Sweep();
                for (int i = 0; i < 5 && running; i++) { Thread.Sleep(100); }
            }
        }

        private void Sweep()
        {
            string folder = workerFolder;
            bool deleteOriginals = workerDelete;
            long quality = Thread.VolatileRead(ref workerQuality);
            if (string.IsNullOrEmpty(folder)) { return; }

            string[] files;
            try { files = Directory.GetFiles(folder, "*.pcx"); }
            catch { return; }

            foreach (string path in files)
            {
                if (!running && !oneShot) { return; }

                long size;
                try { size = new FileInfo(path).Length; }
                catch { continue; }

                long knownSize;
                if (!pending.TryGetValue(path, out knownSize) || knownSize != size)
                {
                    pending[path] = size;
                    continue;
                }

                // The game may still hold the handle; skip until an exclusive open succeeds.
                try
                {
                    using (FileStream probe = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.None)) { }
                }
                catch { continue; }

                string outPath = Path.ChangeExtension(path, ".jpg");
                string dims;
                try { dims = ConvertPcxToJpeg(path, outPath, quality); }
                catch (Exception ex)
                {
                    Log("FAILED  " + Path.GetFileName(path) + " - " + ex.Message);
                    pending.Remove(path);
                    continue;
                }

                // Only remove the source once the .jpg is verifiably on disk and non-empty.
                bool ok = false;
                try { ok = File.Exists(outPath) && new FileInfo(outPath).Length > 0; }
                catch { }

                string note;
                if (ok && deleteOriginals)
                {
                    try { File.Delete(path); note = "converted, .pcx removed"; }
                    catch (Exception ex) { note = "converted, .pcx NOT removed (" + ex.Message + ")"; }
                }
                else if (ok) { note = "converted, .pcx kept"; }
                else { note = "no output produced, .pcx kept"; }

                convertedCount++;
                Log(Path.GetFileName(path) + "  ->  " + Path.GetFileName(outPath) + "  [" + dims + "]  " + note);
                pending.Remove(path);
            }

            // Drop tracking entries whose files are gone, so the table cannot grow without bound.
            List<string> stale = new List<string>();
            foreach (KeyValuePair<string, long> kv in pending)
            {
                if (!File.Exists(kv.Key)) { stale.Add(kv.Key); }
            }
            foreach (string key in stale) { pending.Remove(key); }
        }

        internal string ConvertPcxToJpeg(string inputPath, string outputPath, long quality)
        {
            byte[] bytes = File.ReadAllBytes(inputPath);
            if (bytes.Length < 900) { throw new Exception("too small to be a valid PCX"); }

            int xmin = bytes[4] | (bytes[5] << 8);
            int ymin = bytes[6] | (bytes[7] << 8);
            int xmax = bytes[8] | (bytes[9] << 8);
            int ymax = bytes[10] | (bytes[11] << 8);
            int bitsPerPixel = bytes[3];
            int nPlanes = bytes[65];
            int bytesPerLine = bytes[66] | (bytes[67] << 8);

            int width = xmax - xmin + 1;
            int height = ymax - ymin + 1;
            if (bitsPerPixel != 8 || nPlanes != 1)
            {
                throw new Exception("unsupported PCX (bpp=" + bitsPerPixel + " planes=" + nPlanes + ")");
            }
            if (width <= 0 || height <= 0) { throw new Exception("bad dimensions"); }

            int rowBytes = nPlanes * bytesPerLine;
            int total = rowBytes * height;
            byte[] decoded = new byte[total];
            int src = 128;
            int dst = 0;
            while (dst < total && src < bytes.Length)
            {
                byte b = bytes[src++];
                if ((b & 0xC0) == 0xC0)
                {
                    int count = b & 0x3F;
                    if (src >= bytes.Length) { break; }
                    byte val = bytes[src++];
                    for (int i = 0; i < count && dst < total; i++) { decoded[dst++] = val; }
                }
                else { decoded[dst++] = b; }
            }

            using (Bitmap indexed = new Bitmap(width, height, PixelFormat.Format8bppIndexed))
            {
                int palOffset = bytes.Length - 769;
                ColorPalette palette = indexed.Palette;
                if (palOffset > 0 && bytes[palOffset] == 0x0C)
                {
                    for (int i = 0; i < 256; i++)
                    {
                        palette.Entries[i] = Color.FromArgb(
                            bytes[palOffset + 1 + i * 3],
                            bytes[palOffset + 2 + i * 3],
                            bytes[palOffset + 3 + i * 3]);
                    }
                }
                else
                {
                    for (int i = 0; i < 256; i++) { palette.Entries[i] = Color.FromArgb(i, i, i); }
                }
                indexed.Palette = palette;

                BitmapData data = indexed.LockBits(new Rectangle(0, 0, width, height),
                    ImageLockMode.WriteOnly, PixelFormat.Format8bppIndexed);
                try
                {
                    for (int y = 0; y < height; y++)
                    {
                        Marshal.Copy(decoded, y * rowBytes, IntPtr.Add(data.Scan0, y * data.Stride), width);
                    }
                }
                finally { indexed.UnlockBits(data); }

                // JPEG needs a non-indexed surface.
                using (Bitmap flat = new Bitmap(width, height, PixelFormat.Format24bppRgb))
                {
                    using (Graphics g = Graphics.FromImage(flat)) { g.DrawImageUnscaled(indexed, 0, 0); }

                    EncoderParameters encoderParams = new EncoderParameters(1);
                    encoderParams.Param[0] = new EncoderParameter(Encoder.Quality, quality);
                    if (jpegCodec != null) { flat.Save(outputPath, jpegCodec, encoderParams); }
                    else { flat.Save(outputPath, ImageFormat.Jpeg); }
                }
            }

            return width + "x" + height;
        }

        private void Log(string message)
        {
            if (InvokeRequired)
            {
                try { BeginInvoke(new MethodInvoker(delegate { Log(message); })); }
                catch { }
                return;
            }

            string line = DateTime.Now.ToString("HH:mm:ss", CultureInfo.InvariantCulture) + "  " + message;
            if (logBox.Lines.Length > 500) { logBox.Clear(); }
            logBox.AppendText(line + "\r\n");
            statusLabel.Text = running
                ? (oneShot ? "Converting..." : "Watching. Converted: " + convertedCount)
                : "Idle. Converted: " + convertedCount;
        }

        private void OnClosing(object sender, FormClosingEventArgs e)
        {
            running = false;
            if (worker != null) { worker.Join(1500); }
            SaveSettings();
        }
    }
}
