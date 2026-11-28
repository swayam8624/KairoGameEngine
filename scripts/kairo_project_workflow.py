#!/usr/bin/env python3
"""Shared CLI and desktop workflow for an existing authored Kairo game."""
import argparse
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ACTIONS = ('editor', 'menu', 'gameplay', 'validate', 'smoke', 'play', 'package')


class ProjectTools:
    """Resolve only built engine tools; do not run a shell or auto-build.

    All actions use the same descriptor and project working directory. The
    compiler and Player remain the authorities on project validity and output.
    """
    def __init__(self, project, build, root=ROOT):
        project = Path(project).absolute()
        if project.is_symlink() or project.suffix != '.kproject' or not project.is_file():
            raise ValueError('Select an existing regular .kproject descriptor')
        self.project = project.resolve(strict=True)
        self.build = Path(build).resolve(strict=True)
        self.root = Path(root).resolve(strict=True)
        if not self.build.is_dir():
            raise ValueError('Build path must be a directory')

    def binary(self, name, *directories):
        suffix = '.exe' if os.name == 'nt' else ''
        for directory in directories:
            path = self.build / directory / (name + suffix)
            if path.is_file() and os.access(path, os.X_OK):
                return str(path)
        raise ValueError(f'Built {name} not found in {self.build}')

    def profiles(self):
        # Native descriptor parsing remains authoritative. This projection only
        # resolves named output folders for the subsequent archive command.
        with self.project.open('rb') as stream:data=stream.read(1024*1024+1)
        if len(data)>1024*1024:raise ValueError('Project descriptor exceeds size limit')
        profiles={}
        for line in data.decode('utf-8').splitlines():
            if not line.lstrip().startswith('build-profile '):continue
            tokens=shlex.split(line)
            if len(tokens)!=4 or tokens[1] in profiles:raise ValueError('Invalid build profile')
            relative=Path(tokens[3])
            if relative.is_absolute() or len(relative.parts)<2 or relative.parts[0]!='Build' or '..' in relative.parts:
                raise ValueError('Build profiles must output under Build')
            profiles[tokens[1]]=relative
        return profiles or {'Development':Path('Build/Development'),'Release':Path('Build/Release')}

    def commands(self, action, renderer='auto', profile='Release', archive=None, replace=False):
        if action not in ACTIONS or renderer not in ('auto','metal','vulkan','opengl','d3d12'):
            raise ValueError('Unknown action or renderer')
        p = str(self.project)
        if action == 'editor':
            tile = self.project.parent / 'Config/Platformer.k2d'
            # The canonical descriptor marker preserves 2D dispatch even if
            # the tile file is missing. The dedicated editor reports the error.
            with self.project.open('rb') as stream:
                header = stream.read(1024 * 1024 + 1)
            if len(header) > 1024 * 1024:
                raise ValueError('Project descriptor exceeds size limit')
            world = self.project.parent / 'Config/World.k2d'
            world2d = any(line.strip() == b'startup-scene "Scenes/World2D.kscene"'
                          for line in header.splitlines())
            if world2d or world.exists() or world.is_symlink():
                script = self.root / 'scripts/kairo_world2d_editor.py'
                if not script.is_file():
                    raise ValueError('World2D editor script missing')
                return [[sys.executable, str(script), p, '--build', str(self.build), '--renderer', renderer]]
            platformer = any(line.strip() == b'startup-scene "Scenes/Platformer.kscene"'
                             for line in header.splitlines())
            if platformer or tile.exists() or tile.is_symlink():
                script = self.root / 'scripts/kairo_platformer_editor.py'
                if not script.is_file():
                    raise ValueError('2D editor script missing')
                return [[sys.executable, str(script), p, '--build', str(self.build), '--renderer', renderer]]
            command = [self.binary('KairoEditorApp','components/KairoEditor','KairoEditor'), '--project', p]
            if renderer != 'auto':
                command += ['--renderer',renderer]
            return [command]
        if action == 'gameplay':
            if self.project.name != 'Project.kproject':
                raise ValueError('Gameplay editor requires canonical Project.kproject')
            script = self.root / 'scripts/kairo_gameplay_editor.py'
            if not script.is_file():
                raise ValueError('Gameplay editor script missing')
            return [[sys.executable, str(script), p]]
        if action == 'menu':
            if self.project.name != 'Project.kproject':
                raise ValueError('Menu designer requires canonical Project.kproject')
            validator = self.binary('KairoMenuDocumentTool','Runtime/KairoPlayer')
            script = self.root / 'scripts/kairo_menu_designer.py'
            if not script.is_file():
                raise ValueError('Menu designer script missing')
            return [[sys.executable, str(script), str(self.project.parent), '--validator', validator]]
        compiler = self.binary('KairoProjectCompiler','components/KairoEditor','KairoEditor')
        player = self.binary('KairoPlayer','Runtime/KairoPlayer')
        commands = [[compiler,p], [player,p,'--validate']]
        if action in ('play','smoke'):
            launch = [player,p]
            if renderer != 'auto':
                launch += ['--renderer',renderer]
            if action == 'smoke':
                launch.append('--smoke')
            commands.append(launch)
        elif action == 'package':
            # Preserve existing releases; never pass --replace implicitly.
            profiles=self.profiles()
            if profile not in profiles:raise ValueError('Unknown build profile: '+profile)
            command=[player,p,'--package',profile]
            if replace:command.append('--replace')
            if archive is not None:
                archive=Path(archive).absolute()
                if archive.suffix!='.zip' or archive.exists() or archive.is_symlink():
                    raise ValueError('Choose an unused ZIP output path')
                if not archive.parent.is_dir():raise ValueError('ZIP output parent does not exist')
                bundle=self.project.parent/profiles[profile]
                if archive.resolve().is_relative_to(bundle.resolve()):raise ValueError('ZIP must be outside package folder')
            commands.append(command)
            if archive is not None:
                if (self.root / 'studio-inventory.json').is_file():
                    # Installed Studio exports with notices bound to its actual Player.
                    commands.append([sys.executable, str(self.root/'scripts/studio_game_export.py'),
                                     str(bundle), str(archive), str(self.root)])
                else:
                    # Source-checkout workflows keep the original native export behavior.
                    commands.append([sys.executable,str(self.root/'scripts/kairo_release.py'),'export',str(bundle),str(archive)])
        return commands


def run_cli(tools, action, renderer='auto', profile='Release', archive=None, replace=False):
    """Fail fast: no Player execution after compiler/validation failure."""
    for command in tools.commands(action,renderer,profile,archive,replace):
        print(f'Running {Path(command[0]).name}',flush=True)
        result = subprocess.run(command,cwd=tools.project.parent)
        if result.returncode:
            return result.returncode if result.returncode > 0 else 1
    return 0


def gui(project, build, renderer):
    # Import Tk only for the GUI so headless CLI remains dependency-free.
    import tkinter as tk
    from tkinter import filedialog, ttk

    class Workbench(ttk.Frame):
        def __init__(self,parent):
            super().__init__(parent,padding=12)
            self.parent=parent
            self.project=tk.StringVar(value=str(project or ''))
            self.build=tk.StringVar(value=str(build))
            self.renderer=tk.StringVar(value=renderer)
            self.status=tk.StringVar(value='Choose a project and built engine')
            self.process=None
            self.pending=[]
            self.log=None
            self.tools=None
            self.pack(fill='both',expand=True)
            self.columnconfigure(1,weight=1)
            self.rowconfigure(4,weight=1)
            self.inputs=[]
            for row,label,value,choose in ((0,'Project',self.project,self.choose_project),(1,'Build',self.build,self.choose_build)):
                ttk.Label(self,text=label).grid(row=row,column=0,sticky='w')
                entry=ttk.Entry(self,textvariable=value)
                entry.grid(row=row,column=1,sticky='ew',padx=6,pady=3)
                button=ttk.Button(self,text='Browse',command=choose)
                button.grid(row=row,column=2)
                self.inputs.extend((entry,button))
            ttk.Label(self,text='Renderer').grid(row=2,column=0,sticky='w')
            self.backend=ttk.Combobox(self,textvariable=self.renderer,values=('auto','metal','vulkan','opengl','d3d12'),state='readonly')
            self.backend.grid(row=2,column=1,sticky='w',padx=6,pady=3)
            bar=ttk.Frame(self);bar.grid(row=3,column=0,columnspan=3,sticky='ew',pady=8)
            for action,label in zip(ACTIONS,('Open Editor','Design Menu','Edit Gameplay','Validate','Native Smoke','Play','Package Release')):
                button=ttk.Button(bar,text=label,command=lambda a=action:self.start(a))
                button.pack(side='left',padx=2)
                self.inputs.append(button)
            self.output=tk.Text(self,height=18,wrap='word',state='disabled')
            self.output.grid(row=4,column=0,columnspan=3,sticky='nsew')
            ttk.Label(self,textvariable=self.status,wraplength=850).grid(row=5,column=0,columnspan=3,sticky='w',pady=8)
            self.parent.protocol('WM_DELETE_WINDOW',self.close)

        def choose_project(self):
            path=filedialog.askopenfilename(parent=self.parent,filetypes=[('Kairo project','*.kproject')])
            if path:self.project.set(path)

        def choose_build(self):
            path=filedialog.askdirectory(parent=self.parent)
            if path:self.build.set(path)

        def append(self,text):
            self.output.configure(state='normal')
            self.output.insert('end',text)
            # Bound the displayed transcript, including very long lines.
            count=self.output.count('1.0','end-1c','chars')
            if count and count[0]>120000:
                self.output.delete('1.0',f'1.0+{count[0]-120000}c')
            self.output.see('end');self.output.configure(state='disabled')

        def busy(self,value):
            for widget in self.inputs:widget.configure(state='disabled' if value else 'normal')
            self.backend.configure(state='disabled' if value else 'readonly')

        def start(self,action):
            if self.process is not None or self.pending:return
            try:
                self.tools=ProjectTools(self.project.get(),self.build.get())
                profile='Release';archive=None
                if action=='package':
                    from tkinter import simpledialog,messagebox
                    available=self.tools.profiles()
                    profile=simpledialog.askstring('Build profile','Choose profile: '+', '.join(available),
                        initialvalue='Release' if 'Release' in available else next(iter(available)),parent=self.parent)
                    if profile is None:return
                    if messagebox.askyesno('Export ZIP','Also export a verified game ZIP?',parent=self.parent):
                        archive=filedialog.asksaveasfilename(parent=self.parent,defaultextension='.zip',
                            filetypes=[('Kairo game archive','*.zip')])
                        if not archive:return
                self.pending=self.tools.commands(action,self.renderer.get(),profile,archive)
                self.busy(True)
                self.status.set(f'Running {action}…')
                self.advance()
            except (OSError,ValueError) as error:self.finish(f'Cannot start: {error}')

        def advance(self):
            if not self.pending:
                self.finish('Workflow completed successfully');return
            command=self.pending.pop(0)
            self.append(f'\n> {Path(command[0]).name}\n')
            # A real file prevents unread-pipe deadlocks while a GUI child
            # runs. Poll bounded chunks; child output never executes code.
            self.log=tempfile.TemporaryFile(mode='w+b')
            self.offset=0
            try:
                self.process=subprocess.Popen(command,cwd=self.tools.project.parent,stdout=self.log,stderr=subprocess.STDOUT)
            except OSError as error:
                self.finish(f'Launch failed: {error}');return
            self.after(100,self.poll)

        def poll(self):
            # Popen and the parent share a file offset on POSIX. pread is
            # unavailable on Windows, so avoid seeking while a child writes:
            # only read the bounded tail after the child exits.
            code=self.process.poll()
            if code is None:
                self.after(100,self.poll);return
            self.log.seek(0,os.SEEK_END)
            length=self.log.tell()
            self.log.seek(max(0,length-120000))
            self.append(self.log.read(120000).decode('utf-8',errors='replace'))
            self.log.close();self.log=None;self.process=None
            if code:
                self.finish(f'Workflow failed (exit {code}); later steps not run')
            else:self.advance()

        def finish(self,message):
            if self.log is not None:self.log.close();self.log=None
            self.process=None;self.pending=[]
            self.status.set(message);self.busy(False)

        def close(self):
            if self.process is not None or self.pending:
                self.status.set('Close the running Editor/Player or let the current command finish first.');return
            self.parent.destroy()

    root=tk.Tk();root.title('Kairo Project Workbench');root.geometry('1020x460')
    Workbench(root);root.mainloop()
    return 0


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('project',nargs='?',type=Path)
    parser.add_argument('--build',type=Path,default=ROOT/'build/game-clang')
    parser.add_argument('--renderer',choices=('auto','metal','vulkan','opengl','d3d12'),default='auto')
    parser.add_argument('--action',choices=ACTIONS)
    parser.add_argument('--profile',default='Release',help='Authored package build profile')
    parser.add_argument('--archive',type=Path,help='Optional verified ZIP export for package action')
    parser.add_argument('--replace',action='store_true',help='Explicitly replace a recognized native package, not a ZIP')
    args=parser.parse_args(argv)
    try:
        if args.action:
            if args.project is None:raise ValueError('A project descriptor is required in CLI mode')
            if args.action!='package' and (args.archive is not None or args.replace):raise ValueError('Archive and replacement require package action')
            return run_cli(ProjectTools(args.project,args.build),args.action,args.renderer,args.profile,args.archive,args.replace)
        return gui(args.project,args.build,args.renderer)
    except (ValueError,OSError) as error:
        print(f'Kairo workflow: {error}',file=sys.stderr);return 1


if __name__=='__main__':
    sys.exit(main())
