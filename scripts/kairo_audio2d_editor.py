#!/usr/bin/env python3
"""World2D audio authoring with native source/bank validation and audition."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

LIMIT=8192
WAVE_LIMIT=32*1024*1024
DEFAULT=b"""kairo-audio2d 1
mix 70 80 35 35
clips 3
pickup tone 660 90
damage tone 160 160
victory tone 880 250
collect pickup 75
death damage 85
complete victory 80
music - 70
emitters 0
"""


def add_wave(document,name):
    if not re.fullmatch(r"[a-z0-9_]{1,32}",name) or re.fullmatch(r"con|prn|aux|nul|com[1-9]|lpt[1-9]",name):
        raise ValueError("Use a portable lowercase clip ID")
    lines=[line.strip() for line in document.splitlines() if line.strip()]
    if len(lines)<4 or lines[0]!="kairo-audio2d 1" or not lines[2].startswith("clips "):
        raise ValueError("Expected the audio document header and clips count")
    count=int(lines[2].split()[1])
    if not 1<=count<32 or len(lines)<3+count:raise ValueError("Invalid clip bank count")
    if any(line.split()[0]==name for line in lines[3:3+count]):raise ValueError("Clip already exists")
    lines[2]=f"clips {count+1}";lines.insert(3+count,f"{name} wav")
    result="\n".join(lines)+"\n"
    if len(result.encode("ascii"))>LIMIT:raise ValueError("Audio document exceeds budget")
    return result


def native(command):
    result=subprocess.run([str(x) for x in command],capture_output=True,text=True,timeout=30)
    if result.returncode:raise ValueError(result.stderr.strip()[:1200] or "Native audio validation failed")
    return result.stdout.strip()


def import_wave(source,project,name,document,tool):
    edited=add_wave(document,name)
    source=Path(source)
    if source.is_symlink() or not source.is_file():raise ValueError("Select a regular WAV file")
    project=Path(project).resolve(strict=True)
    directory=project
    for component in ("Assets","Audio"):
        directory=directory/component
        if directory.is_symlink():raise ValueError("Linked audio directory rejected")
        directory.mkdir(exist_ok=True)
    target=directory/(name+".wav")
    if target.exists() or target.is_symlink():raise ValueError("Audio asset already exists")
    with source.open("rb") as stream:data=stream.read(WAVE_LIMIT+1)
    if not data or len(data)>WAVE_LIMIT:raise ValueError("WAV exceeds 32 MiB")
    fd,path=tempfile.mkstemp(prefix=".audio-import-",dir=directory)
    stage=Path(path)
    try:
        with os.fdopen(fd,"wb") as stream:stream.write(data);stream.flush();os.fsync(stream.fileno())
        native([tool,"--wave",stage])
        # Exclusive publication: no silently replaced existing asset.
        os.link(stage,target)
    finally:stage.unlink(missing_ok=True)
    return edited


def open_audio(root,project,tool,bounded_config,publish_config,report):
    import tkinter as tk
    from tkinter import ttk,filedialog,simpledialog,messagebox
    project=Path(project);target=project/"Config/Audio.kaudio"
    if target.parent.is_symlink() or target.is_symlink():raise ValueError("Linked audio configuration rejected")
    baseline=bounded_config(target,LIMIT)
    original=(baseline if baseline is not None else DEFAULT).decode("ascii")
    window=tk.Toplevel(root);window.title("Kairo Studio / Audio")
    window.geometry("900x640");window.transient(root);window.grab_set()
    ttk.Label(window,text="Sound bank / event routing / spatial emitters",font=("TkDefaultFont",13,"bold")).pack(anchor="w",padx=12,pady=8)
    ttk.Label(window,text="clips: ID wav, or ID tone Hz milliseconds. Events and music: ID gain (use - to disable). Emitters: level actorID clipID gain radius.",wraplength=850).pack(fill="x",padx=12,pady=4)
    sliders=ttk.Frame(window);sliders.pack(fill="x",padx=12)
    variables=[]
    editor=tk.Text(window,bg="#161a20",fg="#e4e8ef",insertbackground="white",wrap="none",undo=True)
    def document():return editor.get("1.0","end-1c")
    def replace(data):
        editor.delete("1.0","end");editor.insert("1.0",data)
    def sync():
        lines=document().splitlines()
        if len(lines)>1:
            values=lines[1].split()
            if len(values)==5 and values[0]=="mix":
                try:
                    for variable,value in zip(variables,values[1:]):variable.set(int(value))
                except ValueError:pass
    def set_levels():
        lines=document().splitlines()
        if len(lines)<2 or not lines[1].startswith("mix "):raise ValueError("Missing mix row")
        lines[1]="mix "+" ".join(str(v.get()) for v in variables)
        replace("\n".join(lines)+"\n")
    for name,initial in zip(("Master","Effects","Music","Ambient"),(70,80,35,35)):
        frame=ttk.Frame(sliders);frame.pack(side="left",fill="x",expand=True)
        ttk.Label(frame,text=name).pack();v=tk.IntVar(value=initial);variables.append(v)
        tk.Scale(frame,from_=0,to=100,orient="horizontal",variable=v,bg="#202329",fg="#e4e8ef",highlightthickness=0).pack(fill="x")
    def apply_levels():
        try:set_levels()
        except ValueError as e:report(e)
    ttk.Button(window,text="Apply bus gains to document",command=apply_levels).pack(anchor="w",padx=12)
    editor.pack(fill="both",expand=True,padx=12,pady=8);replace(original);sync()
    status=tk.StringVar(value="Save validates the entire bank. Preview uses the native audio device.")
    ttk.Label(window,textvariable=status,wraplength=850).pack(fill="x",padx=12)
    toolbar=ttk.Frame(window);toolbar.pack(fill="x",padx=12,pady=8)
    def import_clip():
        path=filedialog.askopenfilename(parent=window,filetypes=[("WAV audio","*.wav")])
        if not path:return
        name=simpledialog.askstring("Import WAV","Portable lowercase clip ID",parent=window)
        if not name:return
        try:
            replace(import_wave(path,project,name,document(),tool))
            status.set("WAV copied. Map the clip to an event/music/emitter, then save. Imported files remain if you discard this document.")
        except (OSError,ValueError,subprocess.SubprocessError) as e:report(e)
    child=None;preview_path=None;deadline=0.
    def poll():
        nonlocal child,preview_path
        if child is None:return
        if child.poll() is None:
            if time.monotonic()>deadline:child.kill()
            window.after(100,poll);return
        _,error=child.communicate()
        if child.returncode:report(ValueError(error.strip()[:1200] or "Audio preview failed"))
        else:status.set("Preview finished.")
        child=None
        if preview_path:preview_path.unlink(missing_ok=True);preview_path=None
    def preview():
        nonlocal child,preview_path,deadline
        if child is not None:return
        name=simpledialog.askstring("Preview","Clip ID to audition (up to three seconds)",parent=window)
        if not name:return
        try:
            data=document().encode("ascii")
            if not 0<len(data)<=LIMIT:raise ValueError("Invalid audio document size")
            fd,path=tempfile.mkstemp(prefix=".audio-preview-",dir=target.parent)
            preview_path=Path(path)
            with os.fdopen(fd,"wb") as stream:stream.write(data)
            child=subprocess.Popen([str(tool),"--preview",str(project),str(preview_path),name],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            deadline=time.monotonic()+30
            status.set("Preview running…");window.after(100,poll)
        except (OSError,ValueError,subprocess.SubprocessError) as e:
            if preview_path:preview_path.unlink(missing_ok=True);preview_path=None
            report(e)
    def save():
        nonlocal baseline,original
        try:
            data=document().encode("ascii")
            publish_config(target,baseline,data,LIMIT,[tool,"--validate",project])
            baseline=data;original=document();status.set("Audio bank validated and saved.")
        except (OSError,ValueError,subprocess.SubprocessError) as e:report(e)
    def close():
        if child is not None:report(ValueError("Preview is still running"));return
        if document()!=original and not messagebox.askyesno("Unsaved audio","Discard audio document changes?",parent=window):return
        window.destroy()
    for label,fn in (("Import WAV",import_clip),("Preview clip",preview),("Read bus gains",sync),("Save audio",save)):
        ttk.Button(toolbar,text=label,command=fn).pack(side="left",padx=3)
    window.protocol("WM_DELETE_WINDOW",close)
