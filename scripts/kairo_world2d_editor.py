#!/usr/bin/env python3
"""Structured 2D scene authoring. KairoWorld2DTool validates every saved world."""
import argparse
from copy import deepcopy
from dataclasses import dataclass
import os
from pathlib import Path
import subprocess
import sys
import tempfile

PALETTE = (0x15191f,0x384657,0x52748d,0x5cae82,0xe4bc58,0xd76565,0x4f8bdf,0xf5f1e8,
           0x282e38,0x7e91a2,0xb780d9,0x46b9b0,0xec9557,0x8fbd60,0xa16d57,0xffffff)
KINDS = ("player","solid","collect","hazard","goal","patrol","decor")
LIMIT = 256*1024


@dataclass
class Actor:
    id: int
    kind: str
    x: int
    y: int
    w: int
    h: int
    sprite: str
    patrol: int


@dataclass
class World:
    size: tuple
    mode: str
    camera: tuple
    motion: tuple
    colors: dict
    tiles: dict
    sprites: dict
    background: list
    map: list
    actors: list
    physics: str = ""

    @classmethod
    def parse(cls, data):
        # The native validator is the authoritative semantic parser; this
        # projection reads its bounded token format into editable records.
        if len(data) > LIMIT:
            raise ValueError("World exceeds byte limit")
        tokens = iter(data.decode("ascii").split())
        def token():
            try:
                return next(tokens)
            except StopIteration as error:
                raise ValueError("Incomplete world document") from error
        def expect(value):
            if token() != value:
                raise ValueError("Expected "+value)
        def number(lo, hi):
            value = int(token())
            if not lo <= value <= hi:
                raise ValueError("Number outside world limit")
            return value
        expect("kairo-world2d"); version=number(1,3); expect("size")
        size = (number(3,128), number(3,128))
        expect("mode"); mode = token()
        if mode not in ("topdown","platform"):
            raise ValueError("Unknown movement mode")
        expect("camera"); camera = (number(3,min(24,size[0])), number(3,min(18,size[1])), number(0,100))
        expect("motion"); motion = (number(1,16),number(1,8),number(1,32))
        expect("colors"); colors = {}
        for _ in range(number(0,16)):
            i = number(0,15); rgb = token()
            if i in colors or len(rgb) != 6 or any(c not in "0123456789abcdefABCDEF" for c in rgb):
                raise ValueError("Invalid palette entry")
            colors[i] = int(rgb,16)
        expect("tiles"); tiles = {}
        for _ in range(number(1,64)):
            c = token()
            if len(c)!=1 or c in tiles or not 33<=ord(c)<=126:
                raise ValueError("Invalid tile symbol")
            tiles[c] = (number(0,15),number(0,1))
        if "." not in tiles or tiles["."][1]:
            raise ValueError("Missing empty tile")
        expect("sprites"); sprites = {}
        for _ in range(number(1,64)):
            name=token()
            width,height=(number(1,32),number(1,32)) if version>=2 else (4,4)
            period,count=number(1,600),number(1,16)
            frames = tuple(token() for _ in range(count))
            if name in sprites or len(name)>32 or not all(c.isascii() and (c.isalnum() or c=="_") for c in name):
                raise ValueError("Invalid sprite name")
            if any(len(frame)!=width*height or any(c not in "0123456789abcdefABCDEF" for c in frame) for frame in frames):
                raise ValueError("Invalid sprite frame")
            sprites[name] = (width,height,period,frames)
        def layer(name):
            expect(name)
            rows = [token() for _ in range(size[1])]
            if any(len(row)!=size[0] or any(c not in tiles for c in row) for row in rows):
                raise ValueError("Invalid tile layer")
            return rows
        background, foreground = layer("background"),layer("map")
        expect("actors"); actors = []
        for _ in range(number(1,128)):
            id, kind = number(1,0xffffffff),token()
            x,y,w,h = number(0,size[0]*16),number(0,size[1]*16),number(4,64),number(4,64)
            sprite,patrol = token(),number(0,512)
            if kind not in KINDS or sprite not in sprites or any(a.id==id for a in actors):
                raise ValueError("Invalid actor")
            actors.append(Actor(id,kind,x,y,w,h,sprite,patrol))
        physics=""
        if version==3:
            expect("physics")
            lines=["physics "+token()+" "+token()]
            expect("bodies"); count=number(1,128);lines.append(f"bodies {count}")
            for _ in range(count):lines.append(" ".join(token() for _ in range(11)))
            expect("joints"); count=number(0,64);lines.append(f"joints {count}")
            for _ in range(count):lines.append(" ".join(token() for _ in range(3)))
            physics="\n".join(lines)+"\n"
        if next(tokens,None) is not None:
            raise ValueError("Trailing world data")
        return cls(size,mode,camera,motion,colors,tiles,sprites,background,foreground,actors,physics)

    def encode(self):
        lines = ["kairo-world2d 3" if self.physics else "kairo-world2d 2",f"size {self.size[0]} {self.size[1]}",f"mode {self.mode}",
                 "camera "+" ".join(map(str,self.camera)),"motion "+" ".join(map(str,self.motion)),
                 f"colors {len(self.colors)}"]
        lines += [f"{i} {rgb:06x}" for i,rgb in sorted(self.colors.items())]
        lines += [f"tiles {len(self.tiles)}"]+[f"{c} {color} {solid}" for c,(color,solid) in self.tiles.items()]
        lines += [f"sprites {len(self.sprites)}"]
        for name,(width,height,period,frames) in self.sprites.items():
            lines += [f"{name} {width} {height} {period} {len(frames)}",*frames]
        lines += ["background",*self.background,"map",*self.map,f"actors {len(self.actors)}"]
        lines += [f"{a.id} {a.kind} {a.x} {a.y} {a.w} {a.h} {a.sprite} {a.patrol}" for a in self.actors]
        if self.physics:lines.append(self.physics.strip())
        data = ("\n".join(lines)+"\n").encode("ascii")
        World.parse(data)
        return data

    def rgb(self, index):
        return f"#{self.colors.get(index,PALETTE[index]):06x}"


def read_regular(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError("World must be a regular non-linked file")
    with path.open("rb") as stream:
        data = stream.read(LIMIT+1)
    if len(data)>LIMIT:
        raise ValueError("World exceeds byte limit")
    return data


class Document:
    def __init__(self,path,validator):
        self.path=Path(path)
        self.validator=validator
        self.original=read_regular(self.path)
        self.validate(self.path)
        self.world=World.parse(self.original)
        self.saved=self.world.encode()
        self.undo_stack=[];self.redo_stack=[]

    def validate(self,path):
        result=subprocess.run([str(self.validator),str(path)],capture_output=True,text=True,timeout=10)
        if result.returncode:
            raise ValueError(result.stderr.strip()[:1000] or "Native world validation failed")

    @property
    def dirty(self):
        return self.world.encode()!=self.saved

    def edit(self,fn):
        candidate=deepcopy(self.world)
        fn(candidate)
        if candidate.encode()==self.world.encode():
            return False
        self.undo_stack=(self.undo_stack+[self.world])[-64:]
        self.redo_stack.clear();self.world=candidate
        return True

    def undo(self):
        if self.undo_stack:
            self.redo_stack.append(self.world);self.world=self.undo_stack.pop()

    def redo(self):
        if self.redo_stack:
            self.undo_stack.append(self.world);self.world=self.redo_stack.pop()

    def save(self):
        if read_regular(self.path)!=self.original:
            raise ValueError("World changed externally. Reopen before saving.")
        data=self.world.encode()
        fd,name=tempfile.mkstemp(prefix=".world2d-",dir=self.path.parent)
        temporary=Path(name)
        try:
            with os.fdopen(fd,"wb") as stream:
                stream.write(data);stream.flush();os.fsync(stream.fileno())
            self.validate(temporary)
            if read_regular(self.path)!=self.original:
                raise ValueError("World changed during validation")
            os.replace(temporary,self.path)
        finally:
            temporary.unlink(missing_ok=True)
        self.original=self.saved=data


def bounded_config(path,limit):
    if path.is_symlink():raise ValueError("Linked configuration rejected")
    if not path.exists():return None
    if not path.is_file():raise ValueError("Configuration must be a regular file")
    with path.open("rb") as stream:data=stream.read(limit+1)
    if len(data)>limit:raise ValueError("Configuration exceeds byte budget")
    return data


def publish_config(path,baseline,data,limit,command):
    if not data or len(data)>limit:raise ValueError("Invalid configuration size")
    if bounded_config(path,limit)!=baseline:raise ValueError("Configuration changed externally")
    fd,name=tempfile.mkstemp(prefix=".world-config-",dir=path.parent)
    temporary=Path(name)
    try:
        with os.fdopen(fd,"wb") as stream:
            stream.write(data);stream.flush();os.fsync(stream.fileno())
        result=subprocess.run([*map(str,command),str(temporary)],capture_output=True,text=True,timeout=15)
        if result.returncode:raise ValueError(result.stderr.strip()[:1000] or "Native validation failed")
        if bounded_config(path,limit)!=baseline:raise ValueError("Configuration changed during validation")
        os.replace(temporary,path)
    finally:temporary.unlink(missing_ok=True)


DEFAULT_CAMPAIGN=b'kairo-campaign 1\ntitle "KAIRO ADVENTURE"\nlevels 1\nmain\nmix 70 80\ncollect 660 90 70\ndeath 160 160 85\ncomplete 880 250 70\n'


def campaign_level_path(root,name):
    if not 1<=len(name)<=32 or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789_" for c in name):
        raise ValueError("Use 1–32 lowercase letters, numbers or underscores")
    path=root/("Config/World.k2d" if name=="main" else f"Config/Worlds/{name}.k2d")
    for parent in (root/"Config",root/"Config/Worlds",path):
        if parent.is_symlink():raise ValueError("Linked level path rejected")
    return path


def run_editor(project,build,renderer):
    import tkinter as tk
    from tkinter import ttk,messagebox,simpledialog,filedialog,colorchooser
    from kairo_project_workflow import ProjectTools
    tools=ProjectTools(project,build)
    project_root=tools.project.parent
    path=campaign_level_path(project_root,"main")
    if path.parent.is_symlink() or not path.resolve(strict=True).is_relative_to(tools.project.parent):
        raise ValueError("World path escapes project")
    validator=tools.binary("KairoWorld2DTool","Runtime/KairoPlayer")
    doc=Document(path,validator)
    root=tk.Tk();root.title("Kairo Studio / World2D");root.geometry("1200x800")
    style=ttk.Style(root);style.theme_use("clam")
    style.configure(".",background="#202329",foreground="#e4e8ef",padding=4)
    style.configure("TButton",background="#303640",foreground="#e4e8ef")
    style.configure("TEntry",fieldbackground="#161a20",foreground="#e4e8ef")
    style.configure("TCombobox",fieldbackground="#303640",foreground="#e4e8ef")
    root.configure(bg="#202329")
    root.columnconfigure(1,weight=1);root.rowconfigure(1,weight=1)
    toolbar=ttk.Frame(root);toolbar.grid(row=0,column=0,columnspan=3,sticky="ew")
    ttk.Label(toolbar,text="KAIRO  /  2D WORLD").pack(side="left",padx=8)
    status=tk.StringVar();layer=tk.StringVar(value="map");brush=tk.StringVar(value="#")
    action=tk.StringVar(value="paint");zoom=tk.IntVar(value=24)
    child=None;selected=None;last_cell=None
    left=ttk.Frame(root,padding=8);left.grid(row=1,column=0,sticky="ns")
    center=ttk.Frame(root);center.grid(row=1,column=1,sticky="nsew")
    center.columnconfigure(0,weight=1);center.rowconfigure(0,weight=1)
    canvas=tk.Canvas(center,bg="#161a20",highlightthickness=0)
    canvas.grid(row=0,column=0,sticky="nsew")
    sx=ttk.Scrollbar(center,orient="horizontal",command=canvas.xview)
    sy=ttk.Scrollbar(center,orient="vertical",command=canvas.yview)
    sx.grid(row=1,column=0,sticky="ew");sy.grid(row=0,column=1,sticky="ns")
    canvas.configure(xscrollcommand=sx.set,yscrollcommand=sy.set)
    inspector=ttk.Frame(root,padding=8);inspector.grid(row=1,column=2,sticky="ns")
    ttk.Label(root,textvariable=status).grid(row=2,column=0,columnspan=3,sticky="ew")
    ttk.Label(left,text="LAYERS").pack(anchor="w")
    ttk.Combobox(left,textvariable=layer,values=("map","background"),state="readonly",width=13).pack(fill="x")
    ttk.Label(left,text="TOOLS").pack(anchor="w")
    for text,value in (("Paint tiles","paint"),("Select actors","actor")):
        ttk.Radiobutton(left,text=text,variable=action,value=value).pack(anchor="w")
    palette=ttk.Frame(left);palette.pack(fill="x",pady=6)
    ttk.Label(left,text="Tile scale").pack(anchor="w")
    ttk.Spinbox(left,from_=8,to=48,textvariable=zoom,width=8,command=lambda:draw()).pack(anchor="w")
    ttk.Label(inspector,text="ACTOR INSPECTOR").pack(anchor="w")
    actor_list=tk.Listbox(inspector,height=9,width=28,bg="#161a20",fg="#e4e8ef",exportselection=False)
    actor_list.pack(fill="x")
    fields={key:tk.StringVar() for key in ("kind","x","y","w","h","sprite","patrol")}
    for key,value in fields.items():
        row=ttk.Frame(inspector);row.pack(fill="x")
        ttk.Label(row,text=key,width=8).pack(side="left")
        if key in ("kind","sprite"):
            widget=ttk.Combobox(row,textvariable=value,values=KINDS if key=="kind" else tuple(doc.world.sprites),state="readonly",width=15)
            if key=="sprite":sprite_combo=widget
        else:widget=ttk.Entry(row,textvariable=value,width=16)
        widget.pack(side="right")
    def report(error):
        messagebox.showerror("Kairo World2D",str(error),parent=root)
    def actor_index():
        return next((i for i,a in enumerate(doc.world.actors) if a.id==selected),None)
    def refresh_actor():
        actor_list.delete(0,"end")
        sprite_combo.configure(values=tuple(doc.world.sprites))
        for a in doc.world.actors:
            actor_list.insert("end",f"{a.id:>4}  {a.kind}  {a.sprite}")
        index=actor_index()
        if index is not None:
            actor_list.selection_set(index)
            a=doc.world.actors[index]
            for key,value in fields.items():value.set(str(getattr(a,key)))
        else:
            for value in fields.values():value.set("")
    def draw():
        world=doc.world
        try:scale=max(8,min(48,int(zoom.get())))
        except (ValueError,tk.TclError):scale=24
        canvas.delete("all");canvas.configure(scrollregion=(0,0,world.size[0]*scale,world.size[1]*scale))
        # One rectangle per tile; background and foreground preview together.
        for y,(back,front) in enumerate(zip(world.background,world.map)):
            for x,(b,f) in enumerate(zip(back,front)):
                c=f if f!="." else b
                canvas.create_rectangle(x*scale,y*scale,(x+1)*scale,(y+1)*scale,
                    fill=world.rgb(world.tiles[c][0]),outline="#303640")
        for a in world.actors:
            x,y,w,h=(v*scale/16 for v in (a.x,a.y,a.w,a.h))
            sw,sh,_,frames=world.sprites[a.sprite]
            frame=frames[0]
            for i,pixel in enumerate(frame):
                color=int(pixel,16)
                if color:
                    px,py=i%sw,i//sw
                    canvas.create_rectangle(x+px*w/sw,y+py*h/sh,x+(px+1)*w/sw,y+(py+1)*h/sh,
                        fill=world.rgb(color),outline="")
            if a.id==selected:
                canvas.create_rectangle(x,y,x+w,y+h,outline="#70b6ff",width=2)
            if a.kind=="patrol" and a.patrol:
                canvas.create_line(x-a.patrol*scale/16,y+h/2,x+w+a.patrol*scale/16,y+h/2,fill="#e4bc58",dash=(4,4))
        status.set(f"{world.size[0]} × {world.size[1]} tiles  |  {world.mode}  |  {len(world.actors)} actors  |  {'Unsaved' if doc.dirty else 'Saved'}")
    def palette_refresh():
        for widget in palette.winfo_children():widget.destroy()
        for c,(color,solid) in doc.world.tiles.items():
            ttk.Radiobutton(palette,text=f"{c}  {'solid' if solid else 'visual'}",variable=brush,value=c).pack(anchor="w")
        if brush.get() not in doc.world.tiles:brush.set(".")
    def refresh():
        palette_refresh();refresh_actor();draw()
    def select_list(_):
        nonlocal selected
        if actor_list.curselection():
            selected=doc.world.actors[actor_list.curselection()[0]].id
            refresh_actor();draw()
    actor_list.bind("<<ListboxSelect>>",select_list)
    def click(event,drag=False):
        nonlocal selected,last_cell
        try:
            scale=max(8,min(48,int(zoom.get())))
            x,y=canvas.canvasx(event.x)*16/scale,canvas.canvasy(event.y)*16/scale
            if action.get()=="actor":
                if drag:return
                matches=[a for a in doc.world.actors if a.x<=x<a.x+a.w and a.y<=y<a.y+a.h]
                selected=matches[-1].id if matches else None
                refresh_actor();draw();return
            cell=(int(x//16),int(y//16))
            if cell==last_cell:return
            last_cell=cell
            tx,ty=cell
            if not (0<=tx<doc.world.size[0] and 0<=ty<doc.world.size[1]):return
            def paint(w):
                rows=w.map if layer.get()=="map" else w.background
                rows[ty]=rows[ty][:tx]+brush.get()+rows[ty][tx+1:]
            if doc.edit(paint):draw()
        except (ValueError,tk.TclError) as error:report(error)
    canvas.bind("<Button-1>",click)
    canvas.bind("<B1-Motion>",lambda event:click(event,True))
    def release(_):
        nonlocal last_cell
        last_cell=None
    canvas.bind("<ButtonRelease-1>",release)
    def apply_actor():
        try:
            index=actor_index()
            if index is None:raise ValueError("Select an actor")
            a=Actor(selected,fields["kind"].get(),*(int(fields[k].get()) for k in ("x","y","w","h")),
                    fields["sprite"].get(),int(fields["patrol"].get()))
            doc.edit(lambda w:w.actors.__setitem__(index,a));refresh()
        except (ValueError,tk.TclError) as error:report(error)
    def add_actor():
        nonlocal selected
        try:
            id=max((a.id for a in doc.world.actors),default=0)+1
            a=Actor(id,"decor",16,16,16,16,next(iter(doc.world.sprites)),0)
            doc.edit(lambda w:w.actors.append(a));selected=id;refresh()
        except ValueError as error:report(error)
    def delete_actor():
        nonlocal selected
        index=actor_index()
        if index is None:return
        if doc.world.actors[index].kind=="player":
            report(ValueError("Keep the single player actor"));return
        try:
            doc.edit(lambda w:w.actors.pop(index));selected=None;refresh()
        except ValueError as error:report(error)
    for text,fn in (("Apply actor",apply_actor),("Add actor",add_actor),("Delete actor",delete_actor)):
        ttk.Button(inspector,text=text,command=fn).pack(fill="x",pady=2)
    def config():
        # Structured settings remain part of the same undoable world document.
        try:
            w=doc.world
            value=simpledialog.askstring("World settings","mode viewWidth viewHeight parallax speed gravity jump",
                initialvalue=" ".join(map(str,(w.mode,*w.camera,*w.motion))),parent=root)
            if value is None:return
            mode,*numbers=value.split()
            if len(numbers)!=6:raise ValueError("Expected mode and six integers")
            def change(w):
                w.mode=mode;w.camera=tuple(map(int,numbers[:3]));w.motion=tuple(map(int,numbers[3:]))
            doc.edit(change);refresh()
        except ValueError as error:report(error)
    def edit_sprite():
        try:
            name=simpledialog.askstring("Sprite","Existing or new sprite name",parent=root)
            if name is None:return
            width,height,period,frames=doc.world.sprites.get(name,(4,4,8,("1111111111111111",)))
            raw=simpledialog.askstring("Indexed animation","Width height ticks; then one hex string per frame (0 transparent)",
                initialvalue=" ".join(map(str,(width,height,period,*frames))),parent=root)
            if raw is None:return
            sw,sh,ticks,*new_frames=raw.split()
            doc.edit(lambda w:w.sprites.__setitem__(name,(int(sw),int(sh),int(ticks),tuple(new_frames))));refresh()
        except ValueError as error:report(error)
    def edit_tile():
        try:
            raw=simpledialog.askstring("Tile definition","Symbol, color index (0–15), solid (0/1)",parent=root)
            if raw is None:return
            symbol,color,solid=raw.split()
            doc.edit(lambda w:w.tiles.__setitem__(symbol,(int(color),int(solid))));refresh()
        except ValueError as error:report(error)
    def edit_color():
        try:
            index=simpledialog.askinteger("Palette index","Choose color index (0–15)",minvalue=0,maxvalue=15,parent=root)
            if index is None:return
            _,value=colorchooser.askcolor(doc.world.rgb(index),parent=root)
            if value:doc.edit(lambda w:w.colors.__setitem__(index,int(value[1:],16)));refresh()
        except ValueError as error:report(error)
    def import_sheet():
        try:
            path=filedialog.askopenfilename(parent=root,filetypes=[("PNG sprite sheet","*.png")])
            if not path:return
            # Bound decoded PNG dimensions before Tk loads the pixels.
            source=Path(path)
            if source.stat().st_size>8*1024*1024:raise ValueError("PNG exceeds 8 MiB")
            with source.open("rb") as stream:header=stream.read(24)
            if len(header)!=24 or header[:8]!=b"\x89PNG\r\n\x1a\n" or header[12:16]!=b"IHDR":
                raise ValueError("Expected PNG header")
            iw,ih=int.from_bytes(header[16:20],"big"),int.from_bytes(header[20:24],"big")
            if not 1<=iw<=512 or not 1<=ih<=512:raise ValueError("PNG dimensions exceed 512x512")
            image=tk.PhotoImage(file=path)
            if image.width()!=iw or image.height()!=ih:raise ValueError("PNG dimensions changed")
            raw=simpledialog.askstring("Sprite sheet","Name frameWidth frameHeight ticks (row-major frames; max 16)",parent=root)
            if raw is None:return
            name,sw,sh,period=raw.split();sw,sh,period=map(int,(sw,sh,period))
            if not 1<=sw<=32 or not 1<=sh<=32 or iw%sw or ih%sh or not 1<=iw//sw*(ih//sh)<=16:
                raise ValueError("Sheet must contain 1–16 complete frames, each at most 32x32")
            palette_values=[tuple(int(doc.world.rgb(i)[p:p+2],16) for p in (1,3,5)) for i in range(16)]
            frames=[]
            for fy in range(0,ih,sh):
                for fx in range(0,iw,sw):
                    pixels=[]
                    for y in range(fy,fy+sh):
                        for x in range(fx,fx+sw):
                            if image.transparency_get(x,y):pixels.append("0");continue
                            rgb=image.get(x,y)
                            index=min(range(1,16),key=lambda i:sum((a-b)**2 for a,b in zip(rgb,palette_values[i])))
                            pixels.append(format(index,"x"))
                    frames.append("".join(pixels))
            doc.edit(lambda w:w.sprites.__setitem__(name,(sw,sh,period,tuple(frames))));refresh()
        except (ValueError,OSError,tk.TclError) as error:report(error)
    for text,fn in (("World settings",config),("Edit sprites",edit_sprite),("Import PNG sheet",import_sheet),
                    ("Define tile",edit_tile),("Edit palette",edit_color)):
        ttk.Button(left,text=text,command=fn).pack(fill="x",pady=3)
    def save():
        try:doc.save();refresh();return True
        except (ValueError,OSError,subprocess.SubprocessError) as error:report(error);return False
    def switch_level():
        nonlocal doc,selected
        if doc.dirty and not save():return
        name=simpledialog.askstring("Open world","Level name (main or a named campaign level)",parent=root)
        if name is None:return
        try:
            candidate=Document(campaign_level_path(project_root,name),validator)
            doc=candidate;selected=None;refresh()
            root.title(f"Kairo Studio / World2D / {name}")
        except (OSError,ValueError,subprocess.SubprocessError) as error:report(error)
    def clone_level():
        nonlocal doc,selected
        if doc.dirty and not save():return
        name=simpledialog.askstring("Clone world","New lowercase level name; add it to the campaign order afterwards",parent=root)
        if name is None:return
        try:
            target=campaign_level_path(project_root,name)
            if target.exists():raise ValueError("Level already exists")
            target.parent.mkdir(parents=True,exist_ok=True)
            data=doc.world.encode()
            # Exclusive creation; native validation checks the complete clone.
            with target.open("xb") as stream:stream.write(data)
            try:candidate=Document(target,validator)
            except BaseException:target.unlink(missing_ok=True);raise
            doc=candidate;selected=None;refresh()
            root.title(f"Kairo Studio / World2D / {name}")
        except (OSError,ValueError,subprocess.SubprocessError) as error:report(error)
    def config_window(campaign):
        if child is not None and child.poll() is None:
            report(ValueError("Close Player before editing gameplay configuration"));return
        if doc.dirty and not save():return
        try:
            if campaign:
                target=project_root/"Config/Campaign.kcampaign";limit=8192
                command=[validator,"--campaign",project_root]
                default=DEFAULT_CAMPAIGN
            else:
                # Resolve the exact authored descriptor input-map path. shlex
                # handles quoted portable paths; the native parser validates content.
                import shlex
                relative="Config/Input.kinput"
                for line in tools.project.read_text(encoding="utf-8").splitlines():
                    if line.startswith("input-map "):relative=shlex.split(line)[1]
                target=project_root/relative
                if target.is_symlink() or not target.resolve(strict=True).is_relative_to(project_root):
                    raise ValueError("Input map escapes project")
                limit=256*1024;default=b""
                command=[tools.binary("KairoWorld2DInputTool","Runtime/KairoPlayer")]
            baseline=bounded_config(target,limit)
            window=tk.Toplevel(root);window.title("Campaign / audio mixer" if campaign else "Input actions")
            window.geometry("690x510");window.transient(root);window.grab_set()
            ttk.Label(window,text=("Title, ordered level names, master/effects gains, event frequency/duration/gain." if campaign
                else "Edit named action bindings. Native validation checks controls before saving."),wraplength=640).pack(fill="x",padx=12,pady=8)
            editor=tk.Text(window,bg="#161a20",fg="#e4e8ef",insertbackground="white",wrap="none")
            editor.pack(fill="both",expand=True,padx=12)
            editor.insert("1.0",(baseline if baseline is not None else default).decode("utf-8"))
            def commit():
                try:
                    data=editor.get("1.0","end-1c").encode("utf-8")
                    publish_config(target,baseline,data,limit,command)
                    window.destroy()
                except (OSError,ValueError,subprocess.SubprocessError) as error:report(error)
            ttk.Button(window,text="Validate and save",command=commit).pack(pady=8)
        except (OSError,ValueError,subprocess.SubprocessError) as error:report(error)
    def physics_window():
        if child is not None and child.poll() is None:
            report(ValueError("Close Player before editing physics"));return
        window=tk.Toplevel(root);window.title("World physics")
        window.geometry("780x580");window.transient(root);window.grab_set()
        eligible=[a for a in doc.world.actors if a.kind in ("solid","decor")]
        ttk.Label(window,text="Physics actors: "+(", ".join(f"{a.id}: {a.kind}" for a in eligible) or "Add a solid or decor actor first."),wraplength=740).pack(fill="x",padx=12,pady=8)
        ttk.Label(window,text="physics gravityX gravityY | bodies count | actorID motion shape mass restitution friction layer mask sensor velocityX velocityY | joints count | actorA actorB length. Map layer 1, player layer 2. Clear text to disable physics.",wraplength=740).pack(fill="x",padx=12,pady=8)
        editor=tk.Text(window,bg="#161a20",fg="#e4e8ef",insertbackground="white",wrap="none")
        editor.pack(fill="both",expand=True,padx=12)
        default=f"physics 0 180\nbodies 1\n{eligible[0].id} dynamic box 1 0.2 0.6 4 3 0 0 0\njoints 0\n" if eligible else ""
        editor.insert("1.0",doc.world.physics or default)
        def apply():
            try:
                candidate=deepcopy(doc.world)
                candidate.physics=editor.get("1.0","end-1c").strip()
                data=candidate.encode()
                fd,name=tempfile.mkstemp(prefix=".physics-preview-",dir=doc.path.parent)
                temporary=Path(name)
                try:
                    with os.fdopen(fd,"wb") as stream:stream.write(data)
                    doc.validate(temporary)
                finally:temporary.unlink(missing_ok=True)
                parsed=World.parse(data)
                doc.edit(lambda w:setattr(w,"physics",parsed.physics))
                refresh();window.destroy()
            except (OSError,ValueError,subprocess.SubprocessError) as error:report(error)
        ttk.Button(window,text="Validate and apply (undoable)",command=apply).pack(pady=8)

    def ui_designer():
        if child is not None and child.poll() is None:
            report(ValueError("Close Player before editing UI"));return
        try:
            from kairo_ui2d_designer import open_designer
            target=project_root/"Config/UI.kui"
            if target.parent.is_symlink() or target.is_symlink():raise ValueError("Linked UI path rejected")
            open_designer(root,target,validator,bounded_config,publish_config,report)
        except (OSError,ValueError,subprocess.SubprocessError) as error:report(error)

    def audio_editor():
        if child is not None and child.poll() is None:
            report(ValueError("Close Player before editing audio"));return
        if doc.dirty and not save():return
        try:
            from kairo_audio2d_editor import open_audio
            tool=tools.binary("KairoWorld2DAudioTool","Runtime/KairoPlayer")
            open_audio(root,project_root,tool,bounded_config,publish_config,report)
        except (OSError,ValueError,subprocess.SubprocessError) as error:report(error)

    for text,fn in (("Audio studio",audio_editor),("Game UI designer",ui_designer),("Physics",physics_window),("Open level",switch_level),("Clone level",clone_level),("Campaign / audio",lambda:config_window(True)),
                    ("Input bindings",lambda:config_window(False))):
        ttk.Button(left,text=text,command=fn).pack(fill="x",pady=3)
    def play():
        nonlocal child
        if child is not None and child.poll() is None:return
        if not save():return
        try:
            player=tools.binary("KairoPlayer","Runtime/KairoPlayer")
            command=[player,str(tools.project)]
            if renderer!="auto":command+=["--renderer",renderer]
            child=subprocess.Popen(command,cwd=tools.project.parent)
        except (ValueError,OSError) as error:report(error)
    def undo():doc.undo();refresh()
    def redo():doc.redo();refresh()
    for text,fn in (("Play",play),("Save",save),("Redo",redo),("Undo",undo)):
        ttk.Button(toolbar,text=text,command=fn).pack(side="right",padx=3)
    def close():
        if child is not None and child.poll() is None:
            report(ValueError("Close the running Player first"));return
        if doc.dirty and not messagebox.askyesno("Unsaved changes","Discard unsaved changes?",parent=root):return
        root.destroy()
    root.protocol("WM_DELETE_WINDOW",close)
    root.bind("<Control-s>",lambda _:save())
    refresh();root.mainloop()
    return 0


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project",type=Path)
    parser.add_argument("--build",type=Path,required=True)
    parser.add_argument("--renderer",choices=("auto","metal","vulkan","opengl","d3d12"),default="auto")
    args=parser.parse_args(argv)
    try:return run_editor(args.project,args.build,args.renderer)
    except (ValueError,OSError,subprocess.SubprocessError) as error:
        print(f"Kairo World2D: {error}",file=sys.stderr);return 1


if __name__=="__main__":
    sys.exit(main())
