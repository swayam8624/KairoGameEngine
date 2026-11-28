#!/usr/bin/env python3
"""Project-scoped 2D tile authoring. The native Player runs the saved map."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import tkinter as tk
from tkinter import ttk, messagebox

HEADER = "kairo-platformer 1"
PALETTE = {".": ("Empty", "#202226"), "#": ("Solid", "#515967"),
           "S": ("Spawn", "#4384dd"), "C": ("Coin", "#d1ad48"),
           "H": ("Hazard", "#c55e57"), "G": ("Goal", "#48966c")}


def parse(data):
    if len(data) > 4096:
        raise ValueError("Tile document exceeds byte limit.")
    # Match native newline semantics: one optional terminal LF, CRLF accepted.
    lines = data.decode("ascii").split("\n")
    if lines[-1] == "":
        lines.pop()
    lines = [line.removesuffix("\r") for line in lines]
    if not lines or lines[0] != HEADER:
        raise ValueError("Unsupported platformer document.")
    rows = lines[1:]
    if not 3 <= len(rows) <= 32 or not 3 <= len(rows[0]) <= 48:
        raise ValueError("Expected 3–48 columns and 3–32 rows.")
    width = len(rows[0])
    for y, row in enumerate(rows):
        if len(row) != width or any(c not in PALETTE for c in row):
            raise ValueError("Ragged map or unknown tile.")
        if row[0] != "#" or row[-1] != "#" or (y in (0, len(rows)-1) and set(row) != {"#"}):
            raise ValueError("Outer border must remain solid.")
    if sum(row.count("S") for row in rows) != 1 or sum(row.count("G") for row in rows) != 1:
        raise ValueError("Exactly one spawn and goal are required.")
    return tuple(rows)


def encode(rows):
    data = (HEADER + "\n" + "\n".join(rows) + "\n").encode("ascii")
    parse(data)
    return data


def read(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError("Tile map must be a regular file, not a symlink.")
    with path.open("rb") as stream:
        return stream.read(4097)


class TileDocument:
    def __init__(self, path):
        self.path = Path(path)
        self.original = read(self.path)
        self.rows = parse(self.original)
        self.undo_stack, self.redo_stack = [], []
        self.saved = self.rows

    @property
    def dirty(self):
        return self.rows != self.saved

    def paint(self, x, y, tile):
        if tile not in PALETTE or not (0 < x < len(self.rows[0])-1 and 0 < y < len(self.rows)-1):
            return False
        if self.rows[y][x] == tile:
            return False
        # Moving a unique marker is one atomic undo operation. Do not erase
        # the other unique marker or leave an invalid project state.
        if self.rows[y][x] in "SG":
            return False
        rows = [list(row) for row in self.rows]
        if tile in "SG":
            for row in rows:
                for i, c in enumerate(row):
                    if c == tile:
                        row[i] = "."
        rows[y][x] = tile
        candidate = tuple("".join(row) for row in rows)
        encode(candidate)
        self.undo_stack.append(self.rows)
        self.undo_stack = self.undo_stack[-128:]
        self.redo_stack.clear()
        self.rows = candidate
        return True

    def undo(self):
        if self.undo_stack:
            self.redo_stack.append(self.rows)
            self.rows = self.undo_stack.pop()

    def redo(self):
        if self.redo_stack:
            self.undo_stack.append(self.rows)
            self.rows = self.redo_stack.pop()

    def save(self):
        if read(self.path) != self.original:
            raise ValueError("Tile map changed externally. Reopen before saving.")
        data = encode(self.rows)
        fd, name = tempfile.mkstemp(prefix=".platformer-", dir=self.path.parent)
        try:
            with os.fdopen(fd, "wb") as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(name, self.path)
        finally:
            Path(name).unlink(missing_ok=True)
        self.original, self.saved = data, self.rows


class Editor(ttk.Frame):
    def __init__(self, parent, project, build, renderer):
        super().__init__(parent, padding=16)
        self.parent, self.project, self.build, self.renderer = parent, project, build, renderer
        tile = project.parent / "Config/Platformer.k2d"
        if not tile.resolve(strict=True).is_relative_to(project.parent):
            raise ValueError("Tile map escapes project.")
        self.doc = TileDocument(tile)
        self.brush = tk.StringVar(value="#")
        self.status = tk.StringVar()
        self.child = None
        self.pack(fill="both", expand=True)
        self.columnconfigure(1, weight=1)
        self.rowconfigure(1, weight=1)
        style = ttk.Style(parent)
        style.theme_use("clam")
        style.configure(".", background="#1a1c1f", foreground="#eef0f4", padding=5)
        style.configure("TButton", background="#25282c", foreground="#eef0f4", padding=7)
        style.map("TButton", background=[("active", "#393d43")])
        style.configure("TRadiobutton", background="#1a1c1f", foreground="#eef0f4")
        style.map("TRadiobutton", background=[("active", "#25282c")])
        bar = ttk.Frame(self)
        bar.grid(row=0, column=0, columnspan=2, sticky="ew")
        ttk.Label(bar, text="KAIRO / 2D WORKSPACE").pack(side="left")
        for label, action in (("Save", self.save), ("Undo", self.undo), ("Redo", self.redo), ("Play", self.play)):
            ttk.Button(bar, text=label, command=action).pack(side="right", padx=3)
        tools = ttk.Frame(self)
        tools.grid(row=1, column=0, sticky="ns", padx=(0, 12))
        ttk.Label(tools, text="TILE PALETTE").pack(anchor="w")
        for tile, (label, color) in PALETTE.items():
            row = ttk.Frame(tools)
            row.pack(fill="x", pady=3)
            swatch = tk.Canvas(row, width=16, height=16, bg=color, highlightthickness=0)
            swatch.pack(side="left")
            ttk.Radiobutton(row, text=label, value=tile, variable=self.brush).pack(side="left")
        self.canvas = tk.Canvas(self, bg="#15171a", highlightthickness=0)
        self.canvas.grid(row=1, column=1, sticky="nsew")
        self.canvas.bind("<Configure>", lambda _: self.redraw())
        self.canvas.bind("<Button-1>", self.paint)
        self.canvas.bind("<B1-Motion>", self.paint)
        ttk.Label(self, textvariable=self.status).grid(row=2, column=0, columnspan=2, sticky="ew")
        for sequence in ("<Control-s>", "<Command-s>"):
            if sequence.startswith("<Command") and sys.platform != "darwin":
                continue
            parent.bind(sequence, lambda _: self.save())
        parent.protocol("WM_DELETE_WINDOW", self.close)
        self.redraw()

    def redraw(self):
        self.canvas.delete("all")
        rows = self.doc.rows
        cols, count = len(rows[0]), len(rows)
        w, h = self.canvas.winfo_width(), self.canvas.winfo_height()
        self.scale = max(1, min((w-24)//cols, (h-24)//count))
        self.ox, self.oy = (w-cols*self.scale)//2, (h-count*self.scale)//2
        for y, row in enumerate(rows):
            for x, tile in enumerate(row):
                a, b = self.ox+x*self.scale, self.oy+y*self.scale
                self.canvas.create_rectangle(a, b, a+self.scale, b+self.scale,
                    fill=PALETTE[tile][1], outline="#30343a")
                if tile in "SGCH" and self.scale >= 14:
                    self.canvas.create_text(a+self.scale/2, b+self.scale/2, text=tile, fill="#eef0f4")
        self.status.set(f"{cols} × {count} tiles  |  {'Unsaved changes' if self.doc.dirty else 'Saved'}  |  A/D or arrows · Space · R reset")

    def paint(self, event):
        if self.doc.paint((event.x-self.ox)//self.scale, (event.y-self.oy)//self.scale, self.brush.get()):
            self.redraw()

    def undo(self):
        self.doc.undo(); self.redraw()

    def redo(self):
        self.doc.redo(); self.redraw()

    def save(self):
        try:
            self.doc.save(); self.redraw()
            return True
        except (OSError, ValueError) as error:
            self.status.set(str(error))
            return False

    def play(self):
        if self.child is not None:
            self.status.set("Close the running Player first.")
            return
        if not self.save():
            return
        try:
            from kairo_project_workflow import ProjectTools
            tools = ProjectTools(self.project, self.build)
            player = tools.binary("KairoPlayer", "Runtime/KairoPlayer")
            command = [player, str(self.project)]
            if self.renderer != "auto":
                command += ["--renderer", self.renderer]
            self.log = tempfile.TemporaryFile(mode="w+b")
            try:
                self.child = subprocess.Popen(command, cwd=self.project.parent, stdout=self.log, stderr=subprocess.STDOUT)
            except OSError:
                self.log.close()
                raise
            self.after(100, self.poll)
        except (OSError, ValueError) as error:
            self.status.set(str(error))

    def poll(self):
        code = self.child.poll()
        if code is None:
            self.after(100, self.poll); return
        self.log.seek(0, 2); self.log.seek(max(0, self.log.tell()-4096))
        output = self.log.read().decode("utf-8", errors="replace")
        self.log.close(); self.child = None
        self.status.set("Player closed." if code == 0 else f"Player failed ({code}): {output[-500:]}")

    def close(self):
        if self.doc.dirty:
            choice = messagebox.askyesnocancel("Unsaved tile map", "Save before closing?", parent=self.parent)
            if choice is None or (choice and not self.save()):
                return
        if self.child is not None:
            self.log.close()
        self.parent.destroy()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--renderer", choices=("auto", "metal", "vulkan", "opengl", "d3d12"), default="auto")
    args = parser.parse_args()
    try:
        from kairo_hub_core import descriptor
        project = descriptor(args.project)
        window = tk.Tk()
        window.title("Kairo Studio — " + project.parent.name)
        window.geometry("1040x680")
        Editor(window, project, args.build, args.renderer)
        window.mainloop()
        return 0
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
