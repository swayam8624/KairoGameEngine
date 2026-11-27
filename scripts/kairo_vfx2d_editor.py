#!/usr/bin/env python3
"""Native particle material preview and validated project authoring."""
from pathlib import Path
import os
import subprocess
import tempfile

LIMIT=8192
DEFAULT=b"""kairo-vfx2d 1
materials 1
spark fff4bd c27a2d 5 1 cross
emitters 1
main 1 collect spark 24 1 45 120 90
"""


def parse_preview(text):
    rows=text.splitlines()
    if not rows:raise ValueError("Missing native preview")
    tag,live,dropped,count=rows[0].split()
    live,dropped,count=map(int,(live,dropped,count))
    if tag!="vfx-preview" or not 0<=live<=512 or dropped<0 or not 0<=count<=1024 or len(rows)!=count+1:
        raise ValueError("Invalid native preview receipt")
    result=[]
    for row in rows[1:]:
        x,y,w,h,color=map(int,row.split())
        if not (0<=x<640 and 0<=y<360 and 0<w<=640-x and 0<h<=360-y and 0<=color<=0xffffff):
            raise ValueError("Invalid native preview rectangle")
        result.append((x,y,w,h,color))
    return live,dropped,result


def preview(project,tool,data,level,event,ticks):
    if not 0<len(data)<=LIMIT:raise ValueError("Invalid VFX document size")
    if event not in ("ambient","collect","death","complete") or not 0<=ticks<=600:raise ValueError("Invalid preview controls")
    fd,path=tempfile.mkstemp(prefix=".vfx-preview-",dir=Path(project)/"Config")
    stage=Path(path)
    try:
        with os.fdopen(fd,"wb") as stream:stream.write(data)
        result=subprocess.run([str(tool),"--vfx-preview",str(project),str(stage),level,event,str(ticks)],
                              capture_output=True,text=True,timeout=15)
        if result.returncode:raise ValueError(result.stderr.strip()[:1200] or "Native VFX preview failed")
        return parse_preview(result.stdout)
    finally:stage.unlink(missing_ok=True)


def open_vfx(root,project,tool,bounded_config,publish_config,report):
    import tkinter as tk
    from tkinter import ttk,messagebox,colorchooser
    project=Path(project);target=project/"Config/VFX.kvfx"
    if target.parent.is_symlink() or target.is_symlink():raise ValueError("Linked VFX document rejected")
    baseline=bounded_config(target,LIMIT)
    original=(baseline or DEFAULT).decode("ascii")
    window=tk.Toplevel(root);window.title("Kairo Studio / VFX Materials")
    window.geometry("1160x780");window.transient(root);window.grab_set()
    ttk.Label(window,text="Particle materials / emitters",font=("TkDefaultFont",13,"bold")).pack(anchor="w",padx=12,pady=8)
    ttk.Label(window,text="Material: ID startRGB endRGB startSize endSize square|cross. Emitter: level actorID event materialID count interval life speed gravity. Time in 60 Hz ticks; speed px/s; gravity px/s².",wraplength=1100).pack(fill="x",padx=12)
    body=ttk.Panedwindow(window,orient="horizontal");body.pack(fill="both",expand=True,padx=12,pady=8)
    left=ttk.Frame(body);right=ttk.Frame(body)
    body.add(left,weight=1);body.add(right,weight=1)
    editor=tk.Text(left,bg="#161a20",fg="#e4e8ef",insertbackground="white",undo=True,wrap="none")
    editor.pack(fill="both",expand=True);editor.insert("1.0",original)
    canvas=tk.Canvas(right,width=640,height=360,bg="#202832",highlightthickness=0)
    canvas.pack(anchor="center",fill="both",expand=True)
    status=tk.StringVar(value="Native preview: fixed authored actor positions, not a live physics scene.")
    ttk.Label(right,textvariable=status,wraplength=600).pack(fill="x",pady=4)
    controls=ttk.Frame(window);controls.pack(fill="x",padx=12,pady=6)
    level=tk.StringVar(value="main");event=tk.StringVar(value="collect");ticks=tk.IntVar(value=12)
    ttk.Label(controls,text="Level").pack(side="left")
    ttk.Entry(controls,textvariable=level,width=12).pack(side="left",padx=4)
    ttk.Combobox(controls,textvariable=event,values=("ambient","collect","death","complete"),state="readonly",width=12).pack(side="left",padx=4)
    ttk.Label(controls,text="Ticks").pack(side="left")
    ttk.Spinbox(controls,from_=0,to=600,textvariable=ticks,width=6).pack(side="left",padx=4)
    def document():return editor.get("1.0","end-1c")
    def show():
        try:
            live,dropped,rects=preview(project,tool,document().encode("ascii"),level.get(),event.get(),ticks.get())
            canvas.delete("all");w=max(1,canvas.winfo_width());h=max(1,canvas.winfo_height())
            scale=min(w/640,h/360);ox=(w-640*scale)/2;oy=(h-360*scale)/2
            canvas.create_rectangle(ox,oy,ox+640*scale,oy+360*scale,fill="#28313b",outline="")
            for x,y,bw,bh,rgb in rects:
                canvas.create_rectangle(ox+x*scale,oy+y*scale,ox+(x+bw)*scale,oy+(y+bh)*scale,fill=f"#{rgb:06x}",outline="")
            status.set(f"{live} live / {dropped} budget drops / {len(rects)} rectangles")
        except (OSError,ValueError,subprocess.SubprocessError,tk.TclError) as error:report(error)
    def pick():
        _,color=colorchooser.askcolor(parent=window)
        if color:
            # User selects a material RGB token before replacing it.
            try:editor.delete("sel.first","sel.last")
            except tk.TclError:pass
            editor.insert("insert",color[1:].lower())
    def save():
        nonlocal baseline,original
        try:
            data=document().encode("ascii")
            publish_config(target,baseline,data,LIMIT,[tool,"--vfx",project])
            baseline=data;original=document();status.set("Native validation passed; VFX saved.")
        except (OSError,ValueError,subprocess.SubprocessError) as error:report(error)
    def close():
        if document()!=original and not messagebox.askyesno("Unsaved VFX","Discard VFX edits?",parent=window):return
        window.destroy()
    for name,fn in (("Preview frame",show),("Replace selected RGB",pick),("Save VFX",save)):
        ttk.Button(controls,text=name,command=fn).pack(side="left",padx=4)
    window.protocol("WM_DELETE_WINDOW",close)
    window.after_idle(show)
