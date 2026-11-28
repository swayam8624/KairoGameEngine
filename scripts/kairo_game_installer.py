#!/usr/bin/env python3
"""Kairo desktop game installer and launcher using the existing native tools."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import platform
import re
import tempfile
import subprocess
import sys

import kairo_release as release
from launch_private_game import RENDERERS, host_target, preflight
from verify_private_game_install import audit, record


def install_game(archive, destination, runtime, glfw_hash, vulkan_hash, ticket):
    """Install without replacing anything and save the receipt pin outside the game.

    Input hashes must come from a trusted source. The ticket is not a signature.
    If a post-install check or ticket write fails, retain the installation and
    report failure instead of deleting potentially valuable user files.
    """
    archive = Path(archive).resolve(strict=True)
    destination = Path(destination).absolute()
    runtime = Path(runtime).resolve(strict=True)
    ticket = Path(ticket).absolute()
    release.require(ticket.suffix == '.json' and ticket.parent.is_dir(),
                    'Choose an unused JSON ticket in an existing directory')
    release.require(not ticket.exists() and not ticket.is_symlink(), 'Ticket already exists')
    release.require(not ticket.resolve().is_relative_to(destination.resolve()),
                    'Keep the launch ticket outside the game installation')
    release.require(not ticket.resolve().is_relative_to(runtime),
                    'Keep the launch ticket outside the runtime source')
    target = release.verify(archive)['target']
    release.require(target == host_target(platform.system(), platform.machine()),
                    'Release archive does not match this native host')
    if target.startswith('windows-'):
        from install_local_windows_game import DLLS, install
        names = DLLS
    elif target == 'macos-arm64':
        from install_local_macos_game import LIBRARIES, install
        names = tuple(LIBRARIES.values())
    else:
        raise ValueError('Unsupported private installation target')
    # Reserve the external ticket exclusively before native installation.
    # A failed installer does not leave a usable launch ticket.
    with ticket.open('x', encoding='utf-8') as stream:
        try:
            install(archive, destination, runtime, dict(zip(names, (glfw_hash, vulkan_hash))))
            pin = record(destination / 'local-runtime.json')['sha256']
            result = audit(archive, destination, pin)
            data = {'schema': 1, 'archive': str(archive), 'installation': str(destination),
                    'receipt_sha256': pin}
            json.dump(data, stream, indent=2, sort_keys=True)
            stream.write('\n')
            stream.flush()
        except Exception as error:
            # Keep an incomplete ticket rather than racing to unlink a path
            # that may have been replaced externally. It cannot be launched.
            raise ValueError(f'Installation or ticket creation failed: {error}. '
                             f'Inspect {destination} and the incomplete ticket {ticket}.') from error
    return result


def read_ticket(ticket):
    """Strictly parse the separately retained launch pin; no path discovery."""
    ticket = Path(ticket)
    release.require(ticket.is_file() and not ticket.is_symlink(), 'Expected a regular launch ticket')
    with ticket.open('rb') as stream:
        raw = stream.read(65537)
    release.require(len(raw) <= 65536, 'Launch ticket too large')
    data = json.loads(raw)
    release.require(isinstance(data, dict) and set(data) == {'schema', 'archive', 'installation', 'receipt_sha256'}
                    and type(data['schema']) is int and data['schema'] == 1
                    and all(isinstance(data[k], str) and data[k] for k in
                            ('archive', 'installation', 'receipt_sha256')), 'Invalid launch ticket')
    release.require(re.fullmatch(r'[0-9a-f]{64}', data['receipt_sha256']), 'Invalid receipt SHA-256')
    archive, installation = Path(data['archive']), Path(data['installation'])
    release.require(archive.is_absolute() and installation.is_absolute(),
                    'Ticket paths must be absolute')
    release.require(not ticket.resolve().is_relative_to(installation.resolve()),
                    'Launch ticket must be stored outside the game installation')
    return archive, installation, data['receipt_sha256']


def use_ticket(ticket, renderer, action):
    release.require(action in ('check', 'validate', 'smoke', 'play'), 'Unknown action')
    archive, destination, pin = read_ticket(ticket)
    if action == 'check':
        return preflight(archive, destination, pin, renderer)
    # Recheck inside the existing launcher at execution time and propagate
    # native status. No shell, implicit downloads, or automatic launch.
    script = Path(__file__).resolve().with_name('launch_private_game.py')
    command = [sys.executable, '-B', str(script), str(archive), str(destination),
               '--receipt-sha256', pin, '--renderer', renderer, '--action', action]
    # Capture a bounded transcript tail from a real file: unlike an unread
    # pipe this cannot deadlock a noisy child during gameplay.
    with tempfile.TemporaryFile(mode='w+b') as output:
        result = subprocess.run(command, check=False, stdout=output, stderr=subprocess.STDOUT)
        output.seek(0, 2)
        output.seek(max(0, output.tell() - 16000))
        tail = output.read(16000).decode('utf-8', errors='replace')
    release.require(result.returncode == 0, f'Game action failed (exit {result.returncode})\n{tail}')
    return {'action': action, 'exit_code': result.returncode, 'output_tail': tail}


def gui(root=None, *, run_loop=True):
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    if root is None:
        root = tk.Tk()
    root.title('Kairo Studio / Game Installer')
    root.geometry('920x650')
    root.minsize(760, 560)
    bg, panel, field, text, muted = '#1a1c1f', '#25282c', '#202226', '#eef0f4', '#adb4bf'
    root.configure(background=bg)
    style = ttk.Style(root)
    style.theme_use('clam')
    style.configure('.', background=bg, foreground=text, font=('TkDefaultFont', 11))
    style.configure('TButton', background=panel, foreground=text, padding=7)
    style.configure('TEntry', fieldbackground=field, foreground=text, insertcolor=text)
    style.configure('TCombobox', fieldbackground=field, foreground=text)
    style.map('TCombobox', fieldbackground=[('readonly', field)], foreground=[('readonly', text)])
    style.configure('Primary.TButton', background='#245ac0', foreground=text)
    style.configure('Muted.TLabel', foreground=muted)
    frame = ttk.Frame(root, padding=16)
    frame.pack(fill='both', expand=True)
    frame.columnconfigure(1, weight=1)
    frame.rowconfigure(12, weight=1)
    ttk.Label(frame, text='KAIRO / GAME INSTALLER', font=('TkDefaultFont', 16, 'bold')).grid(row=0, column=0, columnspan=3, sticky='w')
    ttk.Label(frame, text='Private installation only. Supply trusted native libraries and their known SHA-256 hashes.',
              style='Muted.TLabel', wraplength=710).grid(row=1, column=0, columnspan=3, sticky='w', pady=(4, 12))
    values = {key: tk.StringVar() for key in ('archive', 'destination', 'runtime', 'glfw', 'vulkan', 'ticket')}
    renderer = tk.StringVar(value='auto')
    status = tk.StringVar(value='Choose a trusted game ZIP or an existing launch ticket.')
    controls = []
    worker = ThreadPoolExecutor(max_workers=1)
    pending = None

    def browse(key):
        if key == 'archive':
            chosen = filedialog.askopenfilename(parent=root, filetypes=[('Kairo game ZIP', '*.zip')])
        elif key == 'runtime':
            chosen = filedialog.askdirectory(parent=root)
        elif key == 'destination':
            # The final game directory must not yet exist; choose its parent.
            from tkinter import simpledialog
            parent = filedialog.askdirectory(parent=root)
            if not parent:
                return
            leaf = simpledialog.askstring('New installation folder', 'Folder name:', parent=root)
            if not leaf:
                return
            try:
                release.safe_name(leaf)
                release.require('/' not in leaf, 'Choose a single folder name')
            except ValueError as error:
                messagebox.showerror('Invalid folder name', str(error), parent=root)
                return
            chosen = str(Path(parent) / leaf)
        else:
            chosen = filedialog.asksaveasfilename(parent=root, defaultextension='.json',
                                                  filetypes=[('Launch ticket', '*.json')])
        if chosen:
            values[key].set(chosen)

    for row, (key, label) in enumerate((('archive', 'Game ZIP'), ('destination', 'New game folder'),
                                        ('runtime', 'Local runtime folder'), ('glfw', 'GLFW SHA-256'),
                                        ('vulkan', 'Vulkan SHA-256'), ('ticket', 'External launch ticket')), 2):
        ttk.Label(frame, text=label).grid(row=row, column=0, sticky='w', pady=4)
        entry = ttk.Entry(frame, textvariable=values[key])
        entry.grid(row=row, column=1, sticky='ew', padx=8, pady=4)
        controls.append(entry)
        if key not in ('glfw', 'vulkan'):
            button = ttk.Button(frame, text='Browse…', command=lambda k=key: browse(k))
            button.grid(row=row, column=2, sticky='ew')
            controls.append(button)
    ttk.Label(frame, text='Renderer').grid(row=8, column=0, sticky='w')
    combo = ttk.Combobox(frame, values=RENDERERS, textvariable=renderer, state='readonly')
    combo.grid(row=8, column=1, sticky='w', padx=8)
    bar = ttk.Frame(frame)
    bar.grid(row=9, column=0, columnspan=3, sticky='ew', pady=(12, 6))

    def open_ticket():
        chosen = filedialog.askopenfilename(parent=root, filetypes=[('Launch ticket', '*.json')])
        if chosen:
            values['ticket'].set(chosen)

    def busy(active):
        for item in controls:
            item.configure(state='disabled' if active else 'normal')
        combo.configure(state='disabled' if active else 'readonly')

    def poll():
        nonlocal pending
        if not pending.done():
            root.after(80, poll)
            return
        future, pending = pending, None
        busy(False)
        try:
            result = future.result()
            output = json.dumps(result, sort_keys=True, indent=2)
            status.set('Completed. Static checks do not certify GPU or driver readiness.')
        except Exception as error:
            output = str(error)
            status.set('Operation rejected; see details below.')
        log.configure(state='normal')
        log.delete('1.0', 'end')
        log.insert('end', output[:120000])
        log.configure(state='disabled')

    def start(action):
        nonlocal pending
        if pending is not None:
            return
        args = {key: value.get().strip() for key, value in values.items()}
        if action == 'install':
            if not all(args.values()):
                status.set('Complete every installation field before proceeding.')
                return
            if not messagebox.askyesno('Install trusted game',
                    'Install this verified ZIP using your trusted local runtime files? '
                    'No existing installation or launch ticket will be replaced.', parent=root):
                return
            job = (install_game, args['archive'], args['destination'], args['runtime'],
                   args['glfw'], args['vulkan'], args['ticket'])
        else:
            if not args['ticket']:
                status.set('Select an external launch ticket first.')
                return
            if action != 'check' and not messagebox.askyesno('Run trusted game',
                    'Execute the native game from this trusted launch ticket?', parent=root):
                return
            job = (use_ticket, args['ticket'], renderer.get(), action)
        busy(True)
        status.set('Running ' + action + '…')
        pending = worker.submit(*job)
        root.after(80, poll)

    for label, command in [('Install', lambda: start('install')), ('Open ticket…', open_ticket),
                           ('Check', lambda: start('check')), ('Validate', lambda: start('validate')),
                           ('Smoke', lambda: start('smoke')), ('Play', lambda: start('play'))]:
        button = ttk.Button(bar, text=label, command=command,
                            style='Primary.TButton' if label == 'Install' else 'TButton')
        button.pack(side='left', padx=(0, 5))
        controls.append(button)
    ttk.Label(frame, textvariable=status, wraplength=710).grid(row=10, column=0, columnspan=3, sticky='w')
    ttk.Label(frame, text='Operation details', style='Muted.TLabel').grid(row=11, column=0, columnspan=3, sticky='w', pady=(8, 2))
    log = tk.Text(frame, height=5, wrap='word', background=field, foreground=text, state='disabled')
    log.grid(row=12, column=0, columnspan=2, sticky='nsew')
    scroll = ttk.Scrollbar(frame, orient='vertical', command=log.yview)
    scroll.grid(row=12, column=2, sticky='ns')
    log.configure(yscrollcommand=scroll.set)

    def close():
        if pending is not None:
            status.set('Close the running game or finish the active operation first.')
            return
        worker.shutdown(wait=False)
        root.destroy()

    root.protocol('WM_DELETE_WINDOW', close)
    if run_loop:
        root.mainloop()
    return root


if __name__ == '__main__':
    gui()
