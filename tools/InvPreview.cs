using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

// Renders a preview of the finished inventory panel: the composed background from
// InvCompose, plus the tab row drawn where the game will draw it.
//
// The tabs are NOT baked into inventory_panel.png - they change state at runtime, so they
// ship as a separate strip and are blitted each frame. This tool exists purely so the layout
// can be eyeballed as a whole before any of it is wired up.
//
// Usage: InvPreview <assetsDir> <outPath> [selectedTabIndex]
//
// Geometry must mirror Source/oracool/inventory_layout.h.

class InvPreview
{
    const int PanelW = 320;
    // Mirrors inventory_layout.h: one tab per grid column, butted together, bottom edge flush
    // with the top of the grid's first row. Cells are the selected size; unselected states sit
    // inset within them, so every tab blits from the same uniform grid.
    const int Cell = 28, GridCols = 10, GridY = 400;
    // Grow is 0: the selected tab is the same size as the others, distinguished by colour.
    const int TabCount = 10, TabW = Cell, TabH = Cell, Grow = 0;
    const int CellW = TabW + 2 * Grow, CellH = TabH + 2 * Grow;
    const int TabRowX = (PanelW - GridCols * Cell) / 2;
    const int TabRowY = GridY - TabH;

    // SORT button: centred on the weapon column, midway between the weapon slot and the tabs.
    const int EquipColLeft = 34, WeaponRowY = 232;
    const int WeaponRowBottom = WeaponRowY + 3 * Cell;
    const int SortSize = 28;
    const int SortX = EquipColLeft + Cell - SortSize / 2;
    const int SortY = WeaponRowBottom + (TabRowY - WeaponRowBottom - SortSize) / 2;

    static void Main(string[] args)
    {
        string assets = args[0];    // folder holding inventory_panel.png / inventory_tabs.png
        string outPath = args[1];
        int selected = args.Length > 2 ? int.Parse(args[2]) : 0;

        using (var panel = new Bitmap(Path.Combine(assets, "inventory_panel.png")))
        using (var tabs = new Bitmap(Path.Combine(assets, "inventory_tabs.png")))
        {
            var preview = new Bitmap(panel.Width, panel.Height, PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(preview))
            {
                g.DrawImage(panel, 0, 0);

                // Unselected tabs first, then the selected one, so its two-pixel overhang laps
                // over its neighbours rather than being clipped by them.
                for (int pass = 0; pass < 2; pass++)
                {
                    for (int t = 0; t < TabCount; t++)
                    {
                        bool isSel = (t == selected);
                        if (isSel != (pass == 1)) continue;
                        int state = isSel ? 1 : 0; // 0 = off, 1 = selected, 2 = pressed
                        var src = new Rectangle(t * CellW, state * CellH, CellW, CellH);
                        var dst = new Rectangle(TabRowX + t * TabW - Grow, TabRowY - Grow, CellW, CellH);
                        g.DrawImage(tabs, dst, src, GraphicsUnit.Pixel);
                    }
                }

                string sortPath = Path.Combine(assets, "inventory_sort.png");
                if (File.Exists(sortPath))
                {
                    using (var sort = new Bitmap(sortPath))
                    {
                        // States are inactive / active / clicked; show inactive at rest.
                        var src = new Rectangle(0, 0, SortSize, SortSize);
                        g.DrawImage(sort, new Rectangle(SortX, SortY, SortSize, SortSize), src, GraphicsUnit.Pixel);
                    }
                    Console.WriteLine("SORT at ({0},{1}) {2}px", SortX, SortY, SortSize);
                }
            }
            preview.Save(outPath, ImageFormat.Png);
            preview.Dispose();
            Console.WriteLine("wrote {0}   tab row x {1}..{2}, y {3}..{4}, selected tab {5}",
                outPath, TabRowX, TabRowX + TabCount * TabW, TabRowY, TabRowY + TabH, selected);
        }
    }
}
