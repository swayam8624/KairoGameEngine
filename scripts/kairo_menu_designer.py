#!/usr/bin/env python3
"""Graphical authoring for the existing KairoPlayer Config/Menu.kmenu format."""
import argparse
from dataclasses import dataclass, replace
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import tkinter as tk
from tkinter import ttk

MAX_BYTES = 32768
DEFAULT = b'kairo-menu 1\nbutton "01-play" "PLAY" resume 300 320 400 140\nbutton "02-quit" "QUIT" quit 300 520 400 140\n'


@dataclass(frozen=True)
class Button:
    id: str
    label: str
    action: str
    x: int
    y: int
    w: int
    h: int


def quote(value):
    """Match std::quoted's double-quote/backslash convention, not shell syntax."""
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'


def encode(buttons):
    """The C++ validator, not this formatter, owns semantic acceptance."""
    return ('kairo-menu 1\n' + ''.join(f'button {quote(b.id)} {quote(b.label)} {b.action} {b.x} {b.y} {b.w} {b.h}\n' for b in buttons)).encode('ascii')


class MenuModel:
    """Transactional editor state. The Player's real parser is the oracle.

    Invalid edits do not enter history or change the file. Save is an atomic
    same-directory replacement with an optimistic external-change check.
    The local project directory must not be concurrently mutated by others.
    """
    def __init__(self, project, validator):
        self.project = Path(project).resolve(strict=True)
        if not (self.project / 'Project.kproject').is_file():
            raise ValueError('Select an existing Kairo project directory')
        self.path = self.project / 'Config/Menu.kmenu'
        self.validator = Path(validator).resolve(strict=True)
        self.undo_stack = []
        self.original = self.read()
        self.data = self.canonical(DEFAULT if self.original is None else self.original)

    def read(self):
        if self.path.parent.is_symlink() or self.path.is_symlink():
            raise ValueError('Symbolic-link menu paths are not allowed')
        if not self.path.exists():
            return None
        if not self.path.is_file():
            raise ValueError('Menu path is not a regular file')
        with self.path.open('rb') as stream:
            data = stream.read(MAX_BYTES + 1)
        if len(data) > MAX_BYTES:
            raise ValueError('Menu exceeds byte budget')
        return data

    def canonical(self, data):
        if len(data) > MAX_BYTES:
            raise ValueError('Menu exceeds byte budget')
        result = subprocess.run([str(self.validator)], input=data, capture_output=True, timeout=5)
        if result.returncode:
            raise ValueError(result.stderr.decode('utf-8', errors='replace').strip() or 'Player validator failed')
        if len(result.stdout) > MAX_BYTES or not result.stdout.startswith(b'kairo-menu 1\n'):
            raise ValueError('Invalid validator response')
        return result.stdout

    def buttons(self):
        result = []
        for line in self.data.decode('ascii').splitlines()[1:]:
            kind, ident, label, action, x, y, w, h = shlex.split(line, posix=True)
            if kind != 'button':
                raise ValueError('Unexpected canonical record')
            result.append(Button(ident, label, action, int(x), int(y), int(w), int(h)))
        return result

    def apply(self, buttons):
        candidate = self.canonical(encode(buttons))
        if candidate != self.data:
            self.undo_stack = (self.undo_stack + [self.data])[-100:]
            self.data = candidate

    def undo(self):
        if self.undo_stack:
            self.data = self.undo_stack.pop()

    def save(self):
        data = self.canonical(self.data)
        if self.read() != self.original:
            raise ValueError('Menu changed on disk; reopen before saving')
        self.path.parent.mkdir(exist_ok=True)
        fd, temporary = tempfile.mkstemp(prefix='.Menu-', dir=self.path.parent)
        try:
            with os.fdopen(fd, 'wb') as stream:
                stream.write(data)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, self.path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
        self.original = data


class Designer(ttk.Frame):
    """Live normalized preview plus property inspector; each drag commits
    one validated model transaction on release, never raw pixel positions.
    """
    WIDTH, HEIGHT = 640, 360
    FIELDS = ('id', 'label', 'action', 'x', 'y', 'w', 'h')

    def __init__(self, parent, model):
        super().__init__(parent, padding=12)
        self.model = model
        self.selected = None
        self.drag = None
        self.status = tk.StringVar(value='Select or add a button')
        self.values = {key: tk.StringVar() for key in self.FIELDS}
        self.pack(fill='both', expand=True)
        ttk.Label(self, text='Kairo menu designer').grid(row=0, column=0, sticky='w')
        self.canvas = tk.Canvas(self, width=self.WIDTH, height=self.HEIGHT, bg='#101925', highlightthickness=0)
        self.canvas.grid(row=1, column=0, rowspan=9, padx=(0, 12))
        for i, key in enumerate(self.FIELDS):
            ttk.Label(self, text=key.upper()).grid(row=i+1, column=1, sticky='w')
            entry = ttk.Combobox(self, textvariable=self.values[key], values=('resume','quit'), state='readonly') if key == 'action' else ttk.Entry(self, textvariable=self.values[key], width=24)
            entry.grid(row=i+1, column=2, sticky='ew')
        for i, (label, fn) in enumerate((('Apply',self.apply_fields),('Add',self.add),('Delete',self.delete),('Undo',self.undo),('Save menu',self.save))):
            ttk.Button(self, text=label, command=fn).grid(row=10, column=i, sticky='ew', padx=2)
        ttk.Label(self, textvariable=self.status, wraplength=800).grid(row=11, column=0, columnspan=5, sticky='w')
        self.canvas.bind('<ButtonPress-1>',self.press)
        self.canvas.bind('<B1-Motion>',self.move)
        self.canvas.bind('<ButtonRelease-1>',self.release)
        self.refresh()

    def refresh(self, preview=None):
        self.canvas.delete('all')
        for b in self.model.buttons():
            if preview is not None and b.id==preview.id:
                b=preview
            x,y,w,h=b.x*self.WIDTH/1000,b.y*self.HEIGHT/1000,b.w*self.WIDTH/1000,b.h*self.HEIGHT/1000
            self.canvas.create_rectangle(x,y,x+w,y+h,fill='#203146',outline='#f5c54c' if b.id==self.selected else '#8294aa',width=3,tags=('button',b.id))
            self.canvas.create_text(x+w/2,y+h/2,text=b.label,fill='white',width=max(1,w-8),tags=('button',b.id))

    def select(self, ident):
        self.selected=ident
        b=next((b for b in self.model.buttons() if b.id==ident),None)
        for key in self.FIELDS:
            self.values[key].set(str(getattr(b,key)) if b else '')
        self.refresh()

    def transaction(self, buttons, selected):
        try:
            self.model.apply(buttons)
            self.select(selected)
            self.status.set('Validated by the Player parser; save to publish')
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            self.status.set(str(error))
            self.refresh()

    def apply_fields(self):
        if self.selected is None:
            return
        try:
            b=Button(*(self.values[k].get() for k in self.FIELDS[:3]),*(int(self.values[k].get()) for k in self.FIELDS[3:]))
        except ValueError as error:
            self.status.set(str(error));return
        self.transaction([b if old.id==self.selected else old for old in self.model.buttons()],b.id)

    def add(self):
        buttons=self.model.buttons()
        ident=next((f'button-{i:02}' for i in range(1,65) if all(b.id!=f'button-{i:02}' for b in buttons)),None)
        if ident is None:
            self.status.set('Button limit reached');return
        for y in range(0,901,100):
            for x in range(0,801,200):
                b=Button(ident,'NEW BUTTON','resume',x,y,200,100)
                if not any(x<o.x+o.w and o.x<x+200 and y<o.y+o.h and o.y<y+100 for o in buttons):
                    self.transaction(buttons+[b],ident);return
        self.status.set('No free 200 by 100 slot; resize existing buttons')

    def delete(self):
        if self.selected is not None:
            self.transaction([b for b in self.model.buttons() if b.id!=self.selected],None)

    def undo(self):
        self.model.undo();self.select(None);self.status.set('Previous document restored')

    def save(self):
        try:
            self.model.save();self.status.set(f'Saved {self.model.path}')
        except (ValueError,OSError,subprocess.SubprocessError) as error:
            self.status.set(str(error))

    def press(self,event):
        x,y=event.x*1000/self.WIDTH,event.y*1000/self.HEIGHT
        b=next((b for b in self.model.buttons() if b.x<=x<b.x+b.w and b.y<=y<b.y+b.h),None)
        self.select(b.id if b else None)
        self.drag=(b,event.x,event.y) if b else None

    def position(self,event):
        b,x,y=self.drag
        return replace(b,x=max(0,min(1000-b.w,b.x+round((event.x-x)*1000/self.WIDTH))),y=max(0,min(1000-b.h,b.y+round((event.y-y)*1000/self.HEIGHT))))

    def move(self,event):
        if self.drag:
            self.refresh(self.position(event))

    def release(self,event):
        if self.drag:
            b=self.position(event);self.drag=None
            self.transaction([b if old.id==b.id else old for old in self.model.buttons()],b.id)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project',type=Path)
    parser.add_argument('--validator',type=Path,required=True)
    args=parser.parse_args()
    model=MenuModel(args.project,args.validator)
    root=tk.Tk();root.title('Kairo Menu Designer');Designer(root,model);root.mainloop()


if __name__=='__main__':
    main()
