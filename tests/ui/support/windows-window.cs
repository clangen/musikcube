using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

public static class WindowsWindowCapture
{
    private const uint PreservePosition = 0x0002;
    private const uint PreserveZOrder = 0x0004;
    private const uint NoActivate = 0x0010;
    private const uint AsyncPosition = 0x4000;

    [StructLayout(LayoutKind.Sequential)]
    private struct Rect
    {
        public int Left, Top, Right, Bottom;
        public int Width { get { return Right - Left; } }
        public int Height { get { return Bottom - Top; } }
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Point
    {
        public int X, Y;
    }

    [DllImport("user32.dll")]
    private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);

    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern bool IsZoomed(IntPtr window);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetWindowRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetClientRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool ClientToScreen(IntPtr window, ref Point origin);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(
        IntPtr window, IntPtr insertAfter, int x, int y, int width, int height, uint flags);

    private static void Resize(IntPtr window, int width, int height)
    {
        uint flags = PreservePosition | PreserveZOrder | NoActivate | AsyncPosition;
        if (!SetWindowPos(window, IntPtr.Zero, 0, 0, width, height, flags)) {
            throw new Win32Exception();
        }
    }

    public static int Record(IntPtr window, string directory, int frames, int intervalMs, int shrinkPixels)
    {
        // use physical pixel coordinates for both the window geometry and desktop capture.
        IntPtr previousDpi = SetThreadDpiAwarenessContext(new IntPtr(-4));
        if (previousDpi == IntPtr.Zero) {
            throw new InvalidOperationException("Could not set per-monitor DPI awareness.");
        }

        try {
            if (GetForegroundWindow() != window) {
                throw new InvalidOperationException("Bring the musikcube window to the foreground before capture.");
            }
            Rect original;
            if (!GetWindowRect(window, out original)) {
                throw new Win32Exception();
            }
            if (shrinkPixels > 0 && (IsZoomed(window) || shrinkPixels >= Math.Min(original.Width, original.Height))) {
                throw new InvalidOperationException("Scripted resizing requires a restored window and a smaller resize distance.");
            }
            return RecordFrames(window, directory, frames, intervalMs, shrinkPixels, original);
        }
        finally {
            SetThreadDpiAwarenessContext(previousDpi);
        }
    }

    private static int RecordFrames(IntPtr window, string directory, int frames, int intervalMs, int shrink, Rect original)
    {
        var timer = Stopwatch.StartNew();
        int lastShrink = 0;
        bool resized = false;
        try {
            using (var log = new StreamWriter(Path.Combine(directory, "frames.csv"))) {
                log.AutoFlush = true;
                log.WriteLine("file,elapsed_ms,phase,x,y,width,height,requested_width,requested_height");
                int steps = frames - 4;
                for (int frame = 0; frame < frames; ++frame) {
                    if (GetForegroundWindow() != window) {
                        throw new InvalidOperationException("musikcube lost the foreground during capture.");
                    }
                    int step = Math.Max(0, Math.Min(frame - 1, steps));
                    int offset = shrink * Math.Min(step, steps - step) / (steps / 2);
                    if (offset != lastShrink) {
                        Resize(window, original.Width - offset, original.Height - offset);
                        lastShrink = offset;
                        resized = true;
                    }

                    Rect client;
                    var origin = new Point();
                    if (!GetClientRect(window, out client) || !ClientToScreen(window, ref origin)) {
                        throw new Win32Exception();
                    }
                    string filename = String.Format(CultureInfo.InvariantCulture, "frame-{0:D4}.png", frame);
                    string phase = shrink == 0 ? "manual" : frame < 2 ? "baseline" : frame < frames - 2 ? "resize" : "settled";
                    long elapsed = timer.ElapsedMilliseconds;
                    using (var bitmap = new Bitmap(client.Width, client.Height, PixelFormat.Format24bppRgb)) {
                        using (var graphics = Graphics.FromImage(bitmap)) {
                            graphics.CopyFromScreen(origin.X, origin.Y, 0, 0, new Size(client.Width, client.Height));
                        }
                        if (GetForegroundWindow() != window) {
                            throw new InvalidOperationException("musikcube lost the foreground during capture.");
                        }
                        bitmap.Save(Path.Combine(directory, filename), ImageFormat.Png);
                    }
                    log.WriteLine(String.Format(CultureInfo.InvariantCulture, "{0},{1},{2},{3},{4},{5},{6},{7},{8}",
                        filename, elapsed, phase, origin.X, origin.Y, client.Width, client.Height,
                        shrink == 0 ? "" : (object)(original.Width - offset),
                        shrink == 0 ? "" : (object)(original.Height - offset)));

                    long remaining = elapsed + intervalMs - timer.ElapsedMilliseconds;
                    if (frame + 1 < frames && remaining > 0) {
                        Thread.Sleep((int)remaining);
                    }
                }
            }
            return frames;
        }
        finally {
            if (resized) {
                Resize(window, original.Width, original.Height);
            }
        }
    }
}
