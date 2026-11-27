#!/usr/bin/env python3
"""Visual World2D UI authoring; native Player parser remains authoritative."""
from copy import deepcopy
from dataclasses import dataclass
import math
import re
import shlex

LIMIT=16384
PAGES=("play","pause","complete")
KINDS=("panel","label","bar","button","counter")
ANCHORS=("tl","t","tr","l","c","r","bl","b","br")
BINDINGS=("none","coins","deaths","level","title")
ACTIONS=("none","pause","resume","restart","newgame","quit")
DEFAULT=b'''kairo-ui2d 1
widgets 5
coins play counter tl 16 16 250 50 202832 ffffff coins none "COINS "
pause play button tr -16 16 175 65 303d50 ffffff none pause "PAUSE"
resume pause button c 0 -70 380 80 304d46 ffffff none resume "RESUME"
quit_pause pause button c 0 35 380 80 463d47 ffffff none quit "QUIT"
new_game complete button c 0 0 400 85 304d46 ffffff none newgame "NEW GAME"
'''


@dataclass
class Widget:
    id:str
    page:str
    kind:str
    anchor:str
    x:int
    y:int
    w:int
    h:int
    bg:str
    fg:str
    binding:str
    action:str
    text:str


def parse(data):
    if len(data)>LIMIT:raise ValueError("UI document too large")
    tokens=shlex.split(data.decode("ascii"),comments=False)
    if len(tokens)<4 or tokens[:3]!=["kairo-ui2d","1","widgets"]:raise ValueError("Invalid UI document")
    count=int(tokens[3])
    if not 1<=count<=24 or len(tokens)!=4+13*count:raise ValueError("Invalid UI widget count")
    result=[]
    for i in range(count):
        row=tokens[4+i*13:4+(i+1)*13]
        w=Widget(*row[:4],*map(int,row[4:8]),*row[8:])
        if not re.fullmatch(r"[a-z0-9_]{1,24}",w.id) or any(v.id==w.id for v in result):raise ValueError("Invalid or duplicate widget ID")
        if w.page not in PAGES or w.kind not in KINDS or w.anchor not in ANCHORS or w.binding not in BINDINGS or w.action not in ACTIONS:raise ValueError("Invalid widget enum")
        if not (-1000<=w.x<=1000 and -1000<=w.y<=1000 and 1<=w.w<=1000 and 1<=w.h<=1000):raise ValueError("Invalid bounds")
        if not all(re.fullmatch(r"[0-9a-fA-F]{6}",c) for c in (w.bg,w.fg)):raise ValueError("Invalid color")
        if len(w.text)>32 or not all(32<=ord(c)<=126 for c in w.text):raise ValueError("Invalid label")
        result.append(w)
    return result


def encode(widgets):
    lines=["kairo-ui2d 1",f"widgets {len(widgets)}"]
    for w in widgets:
        label='"'+w.text.replace("\\","\\\\").replace('"','\\"')+'"'
        lines.append(f"{w.id} {w.page} {w.kind} {w.anchor} {w.x} {w.y} {w.w} {w.h} {w.bg} {w.fg} {w.binding} {w.action} {label}")
    data=("\n".join(lines)+"\n").encode("ascii")
    parse(data)
    return data


def layout(widget,width,height):
    # C++ lround rounds ties away from zero, unlike Python round.
    def nearest(x):return math.floor(x+.5) if x>=0 else math.ceil(x-.5)
    unit=min(width,height)/1000
    bw,bh=nearest(widget.w*unit),nearest(widget.h*unit)
    i=ANCHORS.index(widget.anchor)
    return int((width-bw)*(i%3)/2)+nearest(widget.x*unit),int((height-bh)*(i//3)/2)+nearest(widget.y*unit),bw,bh


def open_designer(root,target,validator,bounded_config,publish_config,report):
    import tkinter as tk
    from tkinter import ttk,messagebox,colorchooser
    baseline=bounded_config(target,LIMIT)
    widgets=parse(baseline if baseline is not None else DEFAULT)
    original=encode(widgets);undo=[];redo=[];selected=None;drag=None
    win=tk.Toplevel(root);win.title("Kairo Studio / Game UI Designer")
    win.geometry("1250x760");win.transient(root);win.grab_set()
    top=ttk.Frame(win);top.pack(fill="x",padx=8,pady=6)
    page=tk.StringVar(value="play");aspect=tk.StringVar(value="16:9")
    ttk.Label(top,text="Screen").pack(side="left")
    ttk.Combobox(top,textvariable=page,values=PAGES,state="readonly",width=12).pack(side="left",padx=6)
    ttk.Label(top,text="Viewport").pack(side="left")
    ttk.Combobox(top,textvariable=aspect,values=("16:9","4:3","9:16","21:9"),state="readonly",width=9).pack(side="left",padx=6)
    body=ttk.Panedwindow(win,orient="horizontal");body.pack(fill="both",expand=True,padx=8,pady=6)
    left=ttk.Frame(body,width=190);center=ttk.Frame(body);right=ttk.Frame(body,width=270)
    body.add(left,weight=0);body.add(center,weight=1);body.add(right,weight=0)
    ttk.Label(left,text="Widgets / paint order").pack(anchor="w")
    items=tk.Listbox(left,bg="#161a20",fg="#e4e8ef",selectbackground="#365e91",exportselection=False)
    items.pack(fill="both",expand=True)
    canvas=tk.Canvas(center,bg="#10141b",highlightthickness=0);canvas.pack(fill="both",expand=True)
    ttk.Label(center,text="Drag to position. Preview typography is approximate; Player uses its bitmap font.").pack(anchor="w")
    ttk.Label(right,text="Widget properties").pack(anchor="w")
    fields={}
    for name in Widget.__dataclass_fields__:
        row=ttk.Frame(right);row.pack(fill="x",pady=2)
        ttk.Label(row,text=name,width=9).pack(side="left")
        value=tk.StringVar();fields[name]=value
        choices={"page":PAGES,"kind":KINDS,"anchor":ANCHORS,"binding":BINDINGS,"action":ACTIONS}.get(name)
        if choices:ttk.Combobox(row,textvariable=value,values=choices,state="readonly",width=14).pack(side="left",fill="x",expand=True)
        else:ttk.Entry(row,textvariable=value,width=15).pack(side="left",fill="x",expand=True)
        if name in ("bg","fg"):
            def pick(v=value):
                _,color=colorchooser.askcolor("#"+v.get(),parent=win)
                if color:v.set(color[1:])
            ttk.Button(row,text="…",width=3,command=pick).pack(side="right")
    status=tk.StringVar();ttk.Label(win,textvariable=status).pack(fill="x",padx=8,pady=4)
    def viewport():
        a,b=map(int,aspect.get().split(":"))
        width=max(1,canvas.winfo_width()-32);height=max(1,canvas.winfo_height()-32)
        scale=min(width/a,height/b);w=max(1,int(a*scale));h=max(1,int(b*scale))
        return (canvas.winfo_width()-w)//2,(canvas.winfo_height()-h)//2,w,h
    def paint():
        canvas.delete("all");ox,oy,width,height=viewport()
        canvas.create_rectangle(ox,oy,ox+width,oy+height,fill="#28313b" if page.get()=="play" else "#10161f",outline="#505c6b")
        for i,w in enumerate(widgets):
            if w.page!=page.get():continue
            x,y,bw,bh=layout(w,width,height)
            l,t=max(0,x),max(0,y);r,d=min(width,x+bw),min(height,y+bh)
            if r<=l or d<=t:continue
            if w.kind not in ("label","counter"):canvas.create_rectangle(ox+l,oy+t,ox+r,oy+d,fill="#"+w.bg,outline="")
            if w.kind=="bar":canvas.create_rectangle(ox+l,oy+t,ox+min(r,max(l,x+int(bw*.6))),oy+d,fill="#"+w.fg,outline="")
            elif w.kind in ("label","counter","button"):
                label={"coins":w.text+"3/5","deaths":w.text+"1","level":w.text+"1/2","title":"KAIRO ADVENTURE"}.get(w.binding,w.text)
                size=max(1,min(bh//3,bw//max(1,len(label))))
                canvas.create_text(ox+(l+r)/2,oy+(t+d)/2,text=label.upper(),fill="#"+w.fg,font=("TkDefaultFont",size),width=max(1,r-l))
            if selected==i:canvas.create_rectangle(ox+l,oy+t,ox+r,oy+d,outline="#79baff",width=2)
    def refresh():
        items.delete(0,"end")
        for i,w in enumerate(widgets):
            items.insert("end",f"{w.page} / {w.id}")
            if i==selected:items.selection_set(i)
        if selected is not None and selected<len(widgets):
            for name,var in fields.items():var.set(str(getattr(widgets[selected],name)))
        status.set(f"{len(widgets)} / 24 widgets | {'Unsaved' if encode(widgets)!=original else 'Saved state'}")
        paint()
    def commit(candidate):
        nonlocal widgets
        encode(candidate)
        if candidate==widgets:return
        undo.append(deepcopy(widgets));del undo[:-64];redo.clear();widgets=candidate;refresh()
    def apply():
        try:
            if selected is None:return
            values={name:var.get() for name,var in fields.items()}
            for key in ("x","y","w","h"):values[key]=int(values[key])
            candidate=deepcopy(widgets);candidate[selected]=Widget(**values);page.set(candidate[selected].page);commit(candidate)
        except (ValueError,UnicodeError) as e:report(e)
    def add():
        nonlocal selected
        try:
            id=next(f"widget_{i}" for i in range(1,1000) if all(w.id!=f"widget_{i}" for w in widgets))
            candidate=deepcopy(widgets);candidate.append(Widget(id,page.get(),"label","c",0,0,300,70,"202832","ffffff","none","none","LABEL"))
            encode(candidate);selected=len(candidate)-1;commit(candidate)
        except ValueError as e:report(e)
    def delete():
        nonlocal selected
        if selected is None:return
        try:
            candidate=deepcopy(widgets);candidate.pop(selected);encode(candidate);selected=None;commit(candidate)
        except ValueError as e:report(e)
    def move_order(delta):
        nonlocal selected
        if selected is None or not 0<=selected+delta<len(widgets):return
        candidate=deepcopy(widgets);other=selected+delta
        candidate[selected],candidate[other]=candidate[other],candidate[selected];selected=other;commit(candidate)
    def history(back):
        nonlocal widgets,selected
        src,dst=(undo,redo) if back else (redo,undo)
        if src:dst.append(deepcopy(widgets));widgets=src.pop();selected=None;refresh()
    def select(_):
        nonlocal selected
        if items.curselection():
            selected=int(items.curselection()[0]);page.set(widgets[selected].page);refresh()
    def press(event):
        nonlocal selected,drag
        ox,oy,w,h=viewport();selected=None;drag=None
        for i in range(len(widgets)-1,-1,-1):
            widget=widgets[i]
            if widget.page!=page.get():continue
            x,y,bw,bh=layout(widget,w,h)
            if max(0,x)<=event.x-ox<min(w,x+bw) and max(0,y)<=event.y-oy<min(h,y+bh):
                selected=i;drag=(event.x,event.y,widget.x,widget.y,deepcopy(widgets));break
        refresh()
    def motion(event):
        if drag is None or selected is None:return
        _,_,w,h=viewport();unit=min(w,h)/1000
        widget=widgets[selected];widget.x=max(-1000,min(1000,drag[2]+round((event.x-drag[0])/unit)))
        widget.y=max(-1000,min(1000,drag[3]+round((event.y-drag[1])/unit)));refresh()
    def release(_):
        nonlocal drag
        if drag is not None and drag[4]!=widgets:undo.append(drag[4]);del undo[:-64];redo.clear()
        drag=None
    def save():
        nonlocal baseline,original
        try:
            data=encode(widgets);publish_config(target,baseline,data,LIMIT,[validator,"--ui"])
            baseline=original=data;refresh()
        except (OSError,ValueError,UnicodeError) as e:report(e)
    def close():
        if encode(widgets)!=original and not messagebox.askyesno("Unsaved UI","Discard unsaved UI changes?",parent=win):return
        win.destroy()
    for name,fn in (("Add widget",add),("Delete",delete),("Move earlier",lambda:move_order(-1)),("Move later",lambda:move_order(1))):
        ttk.Button(left,text=name,command=fn).pack(fill="x",pady=2)
    ttk.Button(right,text="Apply properties",command=apply).pack(fill="x",pady=8)
    for name,fn in (("Save UI",save),("Redo",lambda:history(False)),("Undo",lambda:history(True))):
        ttk.Button(top,text=name,command=fn).pack(side="right",padx=3)
    page.trace_add("write",lambda *_:paint());aspect.trace_add("write",lambda *_:paint())
    items.bind("<<ListboxSelect>>",select);canvas.bind("<Configure>",lambda _:paint())
    canvas.bind("<ButtonPress-1>",press);canvas.bind("<B1-Motion>",motion);canvas.bind("<ButtonRelease-1>",release)
    win.protocol("WM_DELETE_WINDOW",close);refresh()
