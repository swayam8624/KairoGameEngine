#!/usr/bin/env python3
"""Kairo startup hub: browse recent projects, create templates, open Editor."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys
import tempfile
import tkinter as tk
from tkinter import filedialog, ttk

from kairo_hub_core import ROOT, TEMPLATES, RecentProjects, create, descriptor, default_recent_path, editor_command

BG, PANEL, FIELD, BORDER = "#1a1c1f", "#25282c", "#202226", "#393d43"
TEXT, MUTED, ACCENT, HOVER = "#eef0f4", "#adb4bf", "#245ac0", "#2966cc"


class Hub(ttk.Frame):
    def __init__(self, parent, build, recent, root=ROOT, renderer="auto"):
        super().__init__(parent, style="Hub.TFrame", padding=24)
        self.parent, self.root = parent, Path(root)
        self.recent = RecentProjects(recent)
        self.build = tk.StringVar(value=str(build))
        self.renderer = tk.StringVar(value=renderer)
        self.name = tk.StringVar(value="My Game")
        self.location = tk.StringVar(value=str(Path.home()))
        self.template = tk.StringVar(value="empty")
        self.status = tk.StringVar(value=self.recent.warning or "Ready")
        self.process = self.log = self.future = None
        self.worker = ThreadPoolExecutor(max_workers=1)
        self.pack(fill="both", expand=True)
        self.columnconfigure(1, weight=1)
        self.rowconfigure(1, weight=1)
        style = ttk.Style(parent)
        style.theme_use("clam")
        style.configure(".", font=("TkDefaultFont", 11), background=BG, foreground=TEXT)
        style.configure("Hub.TFrame", background=BG)
        style.configure("Card.TFrame", background=PANEL)
        style.configure("TLabel", background=BG, foreground=TEXT)
        style.configure("Muted.TLabel", foreground=MUTED)
        style.configure("Heading.TLabel", font=("TkDefaultFont", 22, "bold"))
        style.configure("Brand.TLabel", font=("TkDefaultFont", 15, "bold"))
        style.configure("TButton", background=PANEL, foreground=TEXT, bordercolor=BORDER, padding=(12, 8))
        style.map("TButton", background=[("active", FIELD)], foreground=[("disabled", MUTED)])
        style.configure("Primary.TButton", background=ACCENT, bordercolor=ACCENT, foreground=TEXT)
        style.map("Primary.TButton", background=[("active", HOVER), ("disabled", PANEL)])
        style.configure("TEntry", fieldbackground=FIELD, foreground=TEXT, insertcolor=TEXT, bordercolor=BORDER, padding=7)
        style.configure("TCombobox", fieldbackground=FIELD, background=PANEL, foreground=TEXT, arrowcolor=TEXT, padding=6)
        style.map("TCombobox", fieldbackground=[("readonly", FIELD)], foreground=[("readonly", TEXT)])
        style.configure("Treeview", background=FIELD, fieldbackground=FIELD, foreground=TEXT, rowheight=32, bordercolor=BORDER)
        style.map("Treeview", background=[("selected", "#284874")], foreground=[("selected", TEXT)])
        style.configure("Treeview.Heading", background=PANEL, foreground=MUTED, padding=7)
        style.configure("TRadiobutton", background=BG, foreground=TEXT, padding=(4, 5))
        style.map("TRadiobutton", background=[("active", BG)], foreground=[("disabled", MUTED)])
        header = ttk.Frame(self, style="Hub.TFrame")
        header.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 20))
        ttk.Label(header, text="KAIRO  /  STUDIO", style="Brand.TLabel").pack(side="left")
        ttk.Label(header, text="Project Hub · 0.1", style="Muted.TLabel").pack(side="right")
        nav = ttk.Frame(self, style="Hub.TFrame", padding=(0, 0, 20, 0))
        nav.grid(row=1, column=0, sticky="ns")
        ttk.Label(nav, text="WORKSPACE", style="Muted.TLabel").pack(anchor="w", pady=(0, 10))
        ttk.Button(nav, text="Projects", command=lambda: self.show("recent")).pack(fill="x", pady=3)
        ttk.Button(nav, text="New project", command=lambda: self.show("new")).pack(fill="x", pady=3)
        ttk.Button(nav, text="Settings", command=lambda: self.show("settings")).pack(fill="x", pady=3)
        self.content = ttk.Frame(self, style="Hub.TFrame")
        self.content.grid(row=1, column=1, sticky="nsew")
        self.content.columnconfigure(0, weight=1)
        self.content.rowconfigure(0, weight=1)
        self.pages = {}
        for key in ("recent", "new", "settings"):
            page = ttk.Frame(self.content, style="Hub.TFrame")
            page.grid(row=0, column=0, sticky="nsew")
            page.columnconfigure(0, weight=1)
            self.pages[key] = page
        self.draw_recent()
        self.draw_new()
        self.draw_settings()
        ttk.Separator(self).grid(row=2, column=0, columnspan=2, sticky="ew", pady=(16, 8))
        ttk.Label(self, textvariable=self.status, style="Muted.TLabel", wraplength=850).grid(row=3, column=0, columnspan=2, sticky="w")
        self.refresh()
        self.show("recent")
        parent.protocol("WM_DELETE_WINDOW", self.close)

    def show(self, page):
        self.pages[page].tkraise()

    def draw_recent(self):
        page = self.pages["recent"]
        ttk.Label(page, text="Your projects", style="Heading.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(page, text="Open an existing project or start with a template.", style="Muted.TLabel").grid(row=1, column=0, sticky="w", pady=(3, 16))
        bar = ttk.Frame(page)
        bar.grid(row=2, column=0, sticky="ew", pady=(0, 10))
        ttk.Button(bar, text="Open project…", command=self.browse, style="Primary.TButton").pack(side="left")
        ttk.Button(bar, text="New project", command=lambda: self.show("new")).pack(side="left", padx=8)
        self.tree = ttk.Treeview(page, columns=("project", "location", "state"), show="headings", selectmode="browse")
        for key, title, width in (("project", "Project", 160), ("location", "Location", 380), ("state", "Status", 85)):
            self.tree.heading(key, text=title)
            self.tree.column(key, width=width, minwidth=60, stretch=key == "location")
        self.tree.grid(row=3, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(page, orient="vertical", command=self.tree.yview)
        scroll.grid(row=3, column=1, sticky="ns")
        self.tree.configure(yscrollcommand=scroll.set)
        page.rowconfigure(3, weight=1)
        self.tree.bind("<Double-1>", lambda _event: self.open_selected())
        bottom = ttk.Frame(page)
        bottom.grid(row=4, column=0, sticky="w", pady=10)
        ttk.Button(bottom, text="Open selected", command=self.open_selected).pack(side="left")
        ttk.Button(bottom, text="Remove from recents", command=self.remove_selected).pack(side="left", padx=8)
        self.recent_rows = {}

    def draw_new(self):
        page = self.pages["new"]
        ttk.Label(page, text="Create a project", style="Heading.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(page, text="Choose an authored starter, then name your own copy.", style="Muted.TLabel").grid(row=1, column=0, sticky="w", pady=(3, 12))
        for row, template in enumerate(TEMPLATES, 2):
            frame = ttk.Frame(page, style="Hub.TFrame")
            frame.grid(row=row, column=0, sticky="ew", pady=2)
            radio = ttk.Radiobutton(frame, text=template.title, value=template.key, variable=self.template)
            radio.pack(anchor="w")
            if not template.available:
                radio.configure(state="disabled")
            ttk.Label(frame, text=template.description, style="Muted.TLabel", wraplength=620).pack(anchor="w", padx=24)
        row = 2 + len(TEMPLATES)
        ttk.Separator(page).grid(row=row, column=0, sticky="ew", pady=10)
        ttk.Label(page, text="Project name").grid(row=row+1, column=0, sticky="w")
        ttk.Entry(page, textvariable=self.name).grid(row=row+2, column=0, sticky="ew", pady=(4, 10))
        ttk.Label(page, text="Projects directory").grid(row=row+3, column=0, sticky="w")
        path = ttk.Frame(page)
        path.grid(row=row+4, column=0, sticky="ew", pady=4)
        ttk.Entry(path, textvariable=self.location).pack(side="left", fill="x", expand=True)
        ttk.Button(path, text="Browse…", command=lambda: self.choose_directory(self.location)).pack(side="left", padx=(8, 0))
        self.create_button = ttk.Button(page, text="Create & open editor", style="Primary.TButton", command=self.create_project)
        self.create_button.grid(row=row+5, column=0, sticky="e", pady=12)

    def draw_settings(self):
        page = self.pages["settings"]
        ttk.Label(page, text="Engine settings", style="Heading.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(page, text="Built engine directory").grid(row=1, column=0, sticky="w", pady=(20, 4))
        path = ttk.Frame(page)
        path.grid(row=2, column=0, sticky="ew")
        ttk.Entry(path, textvariable=self.build).pack(side="left", fill="x", expand=True)
        ttk.Button(path, text="Browse…", command=lambda: self.choose_directory(self.build)).pack(side="left", padx=(8, 0))
        ttk.Label(page, text="Renderer").grid(row=3, column=0, sticky="w", pady=(16, 4))
        ttk.Combobox(page, textvariable=self.renderer, values=("auto", "metal", "vulkan", "opengl", "d3d12"), state="readonly").grid(row=4, column=0, sticky="w")
        ttk.Label(page, text="Opening projects never downloads dependencies or starts a build.", style="Muted.TLabel", wraplength=600).grid(row=5, column=0, sticky="w", pady=20)

    def choose_directory(self, variable):
        value = filedialog.askdirectory(parent=self.parent)
        if value:
            variable.set(value)

    def refresh(self):
        self.tree.delete(*self.tree.get_children())
        self.recent_rows.clear()
        for i, value in enumerate(self.recent.paths):
            p = Path(value)
            try:
                descriptor(p)
                state = "Available"
            except (OSError, ValueError):
                state = "Missing"
            key = str(i)
            self.recent_rows[key] = value
            self.tree.insert("", "end", iid=key, values=(p.parent.name, str(p.parent), state))

    def selected(self):
        items = self.tree.selection()
        return self.recent_rows.get(items[0]) if items else None

    def browse(self):
        path = filedialog.askopenfilename(parent=self.parent, filetypes=[("Kairo project", "*.kproject")])
        if path:
            self.open_project(path)

    def open_selected(self):
        value = self.selected()
        if value:
            self.open_project(value)

    def remove_selected(self):
        value = self.selected()
        if value:
            try:
                self.recent.remove(value)
                self.refresh()
                self.status.set("Removed from recents; project files are unchanged.")
            except (OSError, ValueError) as error:
                self.status.set(str(error))

    def open_project(self, path):
        if self.process is not None or self.future is not None:
            self.status.set("An editor or project creation is already active.")
            return
        try:
            project = descriptor(path)
            command = editor_command(project, self.build.get(), self.renderer.get(), self.root)
            self.log = tempfile.TemporaryFile(mode="w+b")
            self.process = subprocess.Popen(command, cwd=project.parent, stdout=self.log, stderr=subprocess.STDOUT)
        except (OSError, ValueError) as error:
            if self.log is not None:
                self.log.close()
                self.log = None
            self.status.set(f"Cannot open project: {error}")
            return
        try:
            self.recent.touch(project)
            self.refresh()
            self.status.set(f"Editor running · {project.parent.name}")
        except (OSError, ValueError) as error:
            self.status.set(f"Editor running; recent list was not saved: {error}")
        self.show("recent")
        self.after(100, self.poll_editor)

    def poll_editor(self):
        code = self.process.poll()
        if code is None:
            self.after(100, self.poll_editor)
            return
        self.log.seek(0, 2)
        self.log.seek(max(0, self.log.tell() - 4096))
        output = self.log.read(4096).decode("utf-8", errors="replace").strip()
        self.log.close()
        self.log = self.process = None
        self.status.set("Editor closed." if code == 0 else f"Editor failed ({code}): {output[-500:]}")

    def create_project(self):
        if self.future is not None or self.process is not None:
            self.status.set("An editor or project creation is already active.")
            return
        self.create_button.configure(state="disabled")
        self.status.set("Creating project…")
        # Tk state is read before submission. The worker performs only file I/O.
        self.future = self.worker.submit(create, self.template.get(), self.name.get(), self.location.get(), self.root)
        self.after(60, self.poll_create)

    def poll_create(self):
        if not self.future.done():
            self.after(60, self.poll_create)
            return
        future, self.future = self.future, None
        self.create_button.configure(state="normal")
        try:
            project = future.result()
        except (OSError, ValueError) as error:
            self.status.set(f"Cannot create project: {error}")
            return
        # Preserve newly created projects in recents even if Editor is missing.
        try:
            self.recent.touch(project)
            self.refresh()
        except (OSError, ValueError) as error:
            self.status.set(f"Created {project}; recents unavailable: {error}")
        self.open_project(project)

    def close(self):
        if self.future is not None:
            self.status.set("Project creation is active; close after it finishes.")
            return
        self.worker.shutdown(wait=False)
        if self.log is not None:
            self.log.close()  # Child has its own handle; do not kill the editor.
        self.parent.destroy()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build/game-clang")
    parser.add_argument("--recent", type=Path, default=default_recent_path())
    parser.add_argument("--renderer", choices=("auto", "metal", "vulkan", "opengl", "d3d12"), default="auto")
    args = parser.parse_args(argv)
    window = tk.Tk()
    window.title("Kairo Studio")
    window.configure(background=BG)
    window.geometry("1024x760")
    window.minsize(850, 690)
    Hub(window, args.build, args.recent, renderer=args.renderer)
    window.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
