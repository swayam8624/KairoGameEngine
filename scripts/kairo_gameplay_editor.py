#!/usr/bin/env python3
"""Edit existing authored gameplay records without touching scenes or assets."""
import argparse
import os
from pathlib import Path
import tempfile
import tkinter as tk
from tkinter import ttk

# Record arities are the version-one runtime grammar, not semantic validation.
SCHEMAS = {
    'Doors.kdoor': ('kairo-doors 1', {'door': (4, 5)}),
    'Events.kevent': ('kairo-events 1', {'on': (7,)}),
    'Health.khealth': ('kairo-health 1', {'health': (3,), 'hazard': (6,), 'checkpoint': (5,)}),
    'Combat.kcombat': ('kairo-combat 1', {'attack': (3,), 'projectile': (1,), 'target': (2,)}),
    'Enemies.kenemy': ('kairo-enemies 1', {'enemy': (10,)}),
    'EnemyNav.knav': ('kairo-enemy-nav 1', {'height': (1,), 'rect': (4,)}),
    'Triggers.ktrigger': ('kairo-triggers 1', {'trigger': (8,)}),
}
LIMIT = 65536


def parse(name, raw):
    if len(raw) > LIMIT or b'\x00' in raw:
        raise ValueError('Document exceeds size or contains NUL')
    header, schema = SCHEMAS[name]
    lines = raw.decode('ascii').splitlines()
    if not lines or lines[0] != header:
        raise ValueError('Incorrect document header')
    rows = [line.split() for line in lines[1:] if line.strip()]
    for row in rows:
        if row[0] not in schema or len(row)-1 not in schema[row[0]]:
            raise ValueError('Unknown record or incorrect field count: '+ ' '.join(row))
    return rows


class Document:
    def __init__(self, project, name):
        project = Path(project).resolve(strict=True)
        if project.name != 'Project.kproject' or not project.is_file():
            raise ValueError('Choose a Project.kproject file')
        if name not in SCHEMAS:
            raise ValueError('Unsupported configuration')
        config = project.parent / 'Config'
        self.path = config / name
        if config.is_symlink() or self.path.is_symlink() or not self.path.is_file():
            raise ValueError('Configuration must be an existing regular local file')
        self.name = name
        self.original = self.read()
        self.rows = parse(name, self.original)

    def read(self):
        if self.path.parent.is_symlink() or self.path.is_symlink():
            raise ValueError('Configuration path changed to a symlink')
        with self.path.open('rb') as stream:
            return stream.read(LIMIT+1)

    def save(self):
        raw = (SCHEMAS[self.name][0]+'\n'+''.join(' '.join(row)+'\n' for row in self.rows)).encode('ascii')
        parse(self.name, raw)
        if self.read() != self.original:
            raise ValueError('File changed externally; reopen before saving')
        # Atomic replacement within the same directory; no other file is edited.
        fd, temporary = tempfile.mkstemp(prefix='.kairo-edit-', dir=self.path.parent)
        try:
            with os.fdopen(fd, 'wb') as stream:
                stream.write(raw)
            os.chmod(temporary, self.path.stat().st_mode & 0o777)
            if self.read() != self.original:
                raise ValueError('File changed externally; reopen before saving')
            os.replace(temporary, self.path)
        finally:
            Path(temporary).unlink(missing_ok=True)
        self.original = raw


class GameplayEditor(ttk.Frame):
    def __init__(self, parent, project):
        super().__init__(parent, padding=12)
        self.pack(fill='both', expand=True)
        self.project = Path(project)
        self.doc = None
        self.name = tk.StringVar(value=next(iter(SCHEMAS)))
        self.record = tk.StringVar()
        self.status = tk.StringVar(value='Open an existing gameplay configuration')
        bar = ttk.Frame(self); bar.pack(fill='x')
        ttk.Combobox(bar, textvariable=self.name, values=list(SCHEMAS), state='readonly').pack(side='left')
        ttk.Button(bar, text='Open / reload', command=self.load).pack(side='left')
        self.list = tk.Listbox(self, exportselection=False, height=14)
        self.list.pack(fill='both', expand=True, pady=8)
        self.list.bind('<<ListboxSelect>>', self.select)
        ttk.Label(self, text='Record fields (space separated)').pack(anchor='w')
        ttk.Entry(self, textvariable=self.record).pack(fill='x')
        actions = ttk.Frame(self); actions.pack(fill='x', pady=8)
        for label, action in [('Add',self.add),('Replace',self.replace),('Remove',self.remove),('Save',self.save)]:
            ttk.Button(actions,text=label,command=action).pack(side='left',padx=4)
        ttk.Label(self,textvariable=self.status,wraplength=700).pack(anchor='w')

    def attempt(self, action):
        try:
            action()
        except (OSError, ValueError, IndexError, KeyError) as error:
            self.status.set(str(error))

    def refresh(self):
        self.list.delete(0,tk.END)
        for row in self.doc.rows:
            self.list.insert(tk.END,' '.join(row))
        self.record.set('')

    def load(self):
        def action():
            if self.doc and self.dirty():
                raise ValueError('Save pending edits before switching files')
            self.doc=Document(self.project,self.name.get()); self.refresh()
            self.status.set('Opened '+self.doc.name+'; runtime semantic validation still required')
        self.attempt(action)

    def dirty(self):
        return self.doc.rows != parse(self.doc.name,self.doc.original)

    def select(self, _event=None):
        if self.list.curselection():
            self.record.set(self.list.get(self.list.curselection()[0]))

    def edit(self, mode):
        def action():
            if not self.doc:
                raise ValueError('Open a configuration first')
            rows=[row[:] for row in self.doc.rows]
            if mode=='add': rows.append(self.record.get().split())
            else:
                index=self.list.curselection()[0]
                if mode=='remove': rows.pop(index)
                else: rows[index]=self.record.get().split()
            if any(not row for row in rows): raise ValueError('Empty record')
            raw=(SCHEMAS[self.doc.name][0]+'\n'+''.join(' '.join(row)+'\n' for row in rows)).encode('ascii')
            self.doc.rows=parse(self.doc.name,raw); self.refresh(); self.status.set('Unsaved edits')
        self.attempt(action)

    def add(self): self.edit('add')
    def replace(self): self.edit('replace')
    def remove(self): self.edit('remove')
    def save(self):
        def action():
            if not self.doc: raise ValueError('Open a configuration first')
            self.doc.save(); self.status.set('Saved '+self.doc.name+'; run KairoPlayer --validate before playing')
        self.attempt(action)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project',type=Path)
    args=parser.parse_args()
    root=tk.Tk(); root.title('Kairo Gameplay Editor'); root.geometry('760x480')
    GameplayEditor(root,args.project); root.mainloop()
