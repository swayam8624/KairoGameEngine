#!/usr/bin/env python3
"""Browse licensed local GLB assets and stage verified Kairo recipe inputs."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import tkinter as tk
from tkinter import filedialog, simpledialog, ttk
import kairo_asset_catalog as catalog


class CatalogBrowser(ttk.Frame):
    """Desktop companion; all staging and provenance rules belong to the CLI core.

    The worker never touches Tk. Its result is consumed on the GUI thread.
    Only the current visible selection is exported; metadata must match the
    reviewed snapshot, and the core rejects existing output directories.
    """
    def __init__(self, parent, editor=None):
        super().__init__(parent, padding=12)
        self.parent = parent
        self.editor = Path(editor).resolve(strict=True) if editor else None
        self.editor_process = None
        self.editor_poll = None
        self.generated_project = None
        self.operation = None
        self.manifest = None
        self.entries = {}
        self.future = None
        self.worker = ThreadPoolExecutor(max_workers=1)
        self.query = tk.StringVar()
        self.status = tk.StringVar(value='Open a local catalog JSON file')
        self.location = tk.StringVar(value='No catalog open')
        self.pack(fill='both', expand=True)
        self.columnconfigure(0, weight=1)
        self.rowconfigure(2, weight=1)
        bar = ttk.Frame(self)
        bar.grid(row=0, column=0, sticky='ew')
        bar.columnconfigure(2, weight=1)
        self.open_button = ttk.Button(bar, text='Open catalog', command=self.choose_catalog)
        self.open_button.grid(row=0, column=0, padx=(0,8))
        ttk.Label(bar, text='Search:').grid(row=0, column=1)
        self.search_box = ttk.Entry(bar, textvariable=self.query)
        self.search_box.grid(row=0, column=2, sticky='ew', padx=8)
        self.stage_button = ttk.Button(bar, text='Stage selected assets', command=self.choose_output)
        self.stage_button.grid(row=0, column=3)
        self.project_button = ttk.Button(bar, text='Create editable project', command=lambda: self.choose_output(project=True))
        self.project_button.grid(row=0, column=4, padx=(8,0))
        self.editor_button = ttk.Button(bar, text='Open in Editor', command=self.open_editor)
        self.editor_button.grid(row=0, column=5, padx=(8,0))
        ttk.Label(self, textvariable=self.location).grid(row=1, column=0, sticky='ew', pady=8)
        body = ttk.Frame(self)
        body.grid(row=2, column=0, sticky='nsew')
        body.rowconfigure(0, weight=1)
        body.columnconfigure(0, weight=1)
        self.tree = ttk.Treeview(body, columns=('name','author','license'), show='headings', selectmode='extended')
        for key,width in (('name',220),('author',150),('license',100)):
            self.tree.heading(key,text=key.title())
            self.tree.column(key,width=width)
        self.tree.grid(row=0,column=0,sticky='nsew')
        scroll = ttk.Scrollbar(body, orient='vertical', command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        scroll.grid(row=0,column=1,sticky='ns')
        self.details = tk.Text(body, width=40, height=12, wrap='word', state='disabled')
        self.details.grid(row=0,column=2,sticky='nsew',padx=(10,0))
        ttk.Label(self,textvariable=self.status,wraplength=760).grid(row=3,column=0,sticky='ew',pady=(8,0))
        self.query.trace_add('write', lambda *_: self.refresh())
        self.tree.bind('<<TreeviewSelect>>', lambda _: self.selection_changed())
        self.parent.protocol('WM_DELETE_WINDOW', self.close)
        self.selection_changed()

    def choose_catalog(self):
        path = filedialog.askopenfilename(parent=self.parent, filetypes=[('Catalog JSON','*.json')])
        if path:
            try:
                self.load(Path(path))
            except (OSError, ValueError, RecursionError) as error:
                self.status.set(f'Open failed: {error}')

    def load(self, path):
        if self.future is not None:
            raise ValueError('Wait for current staging operation')
        # Load before publishing any UI state; a rejected file leaves the
        # previously opened catalog intact.
        path = Path(path).absolute()
        _, entries = catalog.load_catalog(path)
        self.manifest, self.entries = path, entries
        self.location.set(str(path))
        self.query.set('')
        self.refresh()

    def refresh(self):
        self.tree.delete(*self.tree.get_children())
        for item in catalog.search(self.entries,self.query.get()):
            self.tree.insert('', 'end', iid=item['id'], values=(item['name'],item['author'],item['license']))
        self.selection_changed()

    def selection_changed(self):
        ids = self.tree.selection()
        self.details.configure(state='normal')
        self.details.delete('1.0','end')
        if ids:
            item = self.entries[ids[0]]
            self.details.insert('1.0', '\n'.join([item['name'], f"ID: {item['id']}",
                f"Author: {item['author']}", f"License: {item['license']}",
                f"Source: {item['source']}", f"Tags: {', '.join(item['tags'])}",
                f"SHA256: {item['sha256']}", f"License file: {item['license_file']}"]))
        self.details.configure(state='disabled')
        can_stage = self.future is None and self.manifest is not None and 1 <= len(ids) <= 128
        self.stage_button.configure(state='normal' if can_stage else 'disabled')
        self.project_button.configure(state='normal' if can_stage else 'disabled')
        self.update_editor_button()
        if self.future is None:
            self.status.set(f'{len(self.tree.get_children())} results; {len(ids)} selected. Select 1–128 assets.')

    def choose_output(self, project=False):
        parent = filedialog.askdirectory(parent=self.parent, title='Choose parent directory')
        if not parent:
            return
        name = simpledialog.askstring('New project folder' if project else 'New staging folder',
                                      'New folder name:',parent=self.parent,
                                      initialvalue='KairoCatalogGame' if project else 'KairoCatalogStage')
        if name is None:
            return
        if not name.strip() or name in ('.','..') or any(c in name for c in '/\\:'):
            self.status.set('Enter a single new folder name'); return
        try:
            self.start_stage(Path(parent)/name,project=project)
        except (ValueError, OSError) as error:
            self.status.set(str(error))

    def start_stage(self, output, *, project=False):
        ids = tuple(self.tree.selection())
        if self.future is not None or self.manifest is None or not 1 <= len(ids) <= 128:
            raise ValueError('Select 1–128 assets from an open catalog')
        reviewed = {key: self.entries[key] for key in ids}
        self.operation = 'project' if project else 'stage'
        task = catalog.create_catalog_project if project else catalog.stage
        self.future = self.worker.submit(task,self.manifest,ids,Path(output),expected_entries=reviewed)
        self.open_button.configure(state='disabled')
        self.search_box.configure(state='disabled')
        self.stage_button.configure(state='disabled')
        self.project_button.configure(state='disabled')
        self.update_editor_button()
        self.status.set('Verifying and creating project…' if project else 'Verifying asset bytes and staging recipe…')
        self.after(30,self.poll_stage)

    def poll_stage(self):
        if self.future is None:
            return
        if not self.future.done():
            self.after(30,self.poll_stage); return
        try:
            path = self.future.result()
            if self.operation == 'project':
                self.generated_project = path
                result = f'Created editable project: {path}'
            else:
                result = f'Staged recipe: {path}'
        except Exception as error:
            result = f'Project creation failed: {error}' if self.operation == 'project' else f'Staging failed: {error}'
        self.future = None
        self.operation = None
        self.open_button.configure(state='normal')
        self.search_box.configure(state='normal')
        self.selection_changed()
        self.status.set(result)

    def update_editor_button(self):
        running = self.editor_process is not None and self.editor_process.poll() is None
        enabled = (self.future is None and self.generated_project is not None
                   and self.editor is not None and not running)
        self.editor_button.configure(state='normal' if enabled else 'disabled')

    def open_editor(self):
        """Launch only an explicitly configured executable, never catalog metadata."""
        self.update_editor_button()
        if str(self.editor_button['state']) != 'normal':
            return
        try:
            if not self.generated_project.is_file() or self.generated_project.is_symlink():
                raise ValueError('Generated project descriptor is unavailable')
            self.editor_process = subprocess.Popen(
                [str(self.editor), '--project', str(self.generated_project)],
                cwd=self.generated_project.parent)
        except (OSError, ValueError) as error:
            self.status.set(f'Editor launch failed: {error}')
            return
        self.status.set(f'Editor launched: {self.generated_project}')
        self.update_editor_button()
        if self.editor_poll is None:
            self.editor_poll = self.after(250,self.poll_editor)

    def poll_editor(self):
        self.editor_poll = None
        if self.editor_process is None:
            return
        code = self.editor_process.poll()
        if code is None:
            self.editor_poll = self.after(250,self.poll_editor)
            return
        self.editor_process = None
        self.update_editor_button()
        if self.future is None:
            self.status.set(f'Editor exited with code {code}')

    def close(self):
        if self.future is not None:
            self.status.set('Staging is still running; close after it finishes.')
            return
        if self.editor_poll is not None:
            self.after_cancel(self.editor_poll)
            self.editor_poll = None
        self.worker.shutdown(wait=True)
        self.parent.destroy()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('catalog',nargs='?',type=Path)
    parser.add_argument('--editor',type=Path,help='Explicit path to built KairoEditorApp')
    args = parser.parse_args()
    root = tk.Tk()
    root.title('Kairo Asset Catalog')
    root.geometry('1220x560')
    browser = CatalogBrowser(root,args.editor)
    if args.catalog:
        try:
            browser.load(args.catalog)
        except (OSError,ValueError,RecursionError) as error:
            browser.status.set(f'Open failed: {error}')
    root.mainloop()


if __name__ == '__main__':
    main()
