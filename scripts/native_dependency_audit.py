#!/usr/bin/env python3
"""Inventory direct native imports of trusted built executables, without running them.

This is evidence, not a license determination, transitive closure, or clean-host test.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def parse_elf(dynamic, program):
    require('Dynamic section at offset' in dynamic or 'There is no dynamic section' in dynamic,
            'Unrecognized readelf dynamic output')
    needed = re.findall(r'\(NEEDED\)\s+Shared library: \[([^\]\r\n]+)\]', dynamic)
    require(len(needed) == len(re.findall(r'\(NEEDED\)', dynamic)), 'Malformed ELF dependency')
    search = re.findall(r'\((?:RPATH|RUNPATH)\)\s+Library (?:rpath|runpath): \[([^\]\r\n]*)\]', dynamic)
    require(len(search) == len(re.findall(r'\((?:RPATH|RUNPATH)\)', dynamic)), 'Malformed ELF search path')
    require('Program Headers:' in program or 'There are no program headers' in program,
            'Unrecognized readelf program output')
    interpreter = re.findall(r'\[Requesting program interpreter: ([^\]\r\n]+)\]', program)
    require(len(interpreter) <= 1, 'Multiple ELF interpreters')
    return {'imports': sorted(set(needed)), 'search_paths': sorted(set(search)),
            'interpreter': interpreter[0] if interpreter else None}


def parse_macho(linked, commands):
    require(bool(linked.splitlines()) and linked.splitlines()[0].endswith(':'),
            'Unrecognized otool library output')
    imports = []
    for line in linked.splitlines():
        if not line.strip() or not line[0].isspace():
            continue
        match = re.fullmatch(r'\s+(.+?) \(compatibility version [^,]+, current version [^)]+\)(?:\s+.*)?', line)
        require(match, 'Malformed Mach-O dependency: ' + line)
        imports.append(match[1])
    require('Load command' in commands, 'Unrecognized otool load commands')
    paths = []
    active = False
    for line in commands.splitlines():
        stripped = line.strip()
        if stripped.startswith('Load command '):
            require(not active, 'Missing LC_RPATH path')
            active = False
        elif stripped == 'cmd LC_RPATH':
            active = True
        elif active and stripped.startswith('path '):
            match = re.fullmatch(r'path (.+) \(offset \d+\)', stripped)
            require(match, 'Malformed LC_RPATH')
            paths.append(match[1])
            active = False
    require(not active, 'Missing LC_RPATH path')
    return {'imports': sorted(set(imports)), 'search_paths': sorted(set(paths)), 'interpreter': None}


def parse_pe(output):
    require('Dump of file' in output and 'Summary' in output, 'Unrecognized dumpbin output')
    imports = []
    active = False
    for line in output.splitlines():
        stripped = line.strip()
        if re.fullmatch(r'Image has the following (?:delay load )?dependencies:', stripped, re.I):
            active = True
        elif active and stripped:
            if not re.fullmatch(r'[^\\/:*?"<>|\s]+\.dll', stripped, re.I):
                require(stripped == 'Summary', 'Malformed PE dependency: ' + stripped)
                active = False
            else:
                imports.append(stripped.lower())
    return {'imports': sorted(set(imports)), 'search_paths': [], 'interpreter': None}


def invoke(tool, *args):
    executable = shutil.which(tool)
    require(executable, 'Required native dependency tool unavailable: ' + tool)
    result = subprocess.run([executable, *map(str, args)], check=True, capture_output=True,
                            timeout=30, text=True, errors='replace',
                            env={**os.environ, 'LC_ALL': 'C', 'VSLANG': '1033'})
    require(len(result.stdout) <= 4 * 1024 * 1024, 'Dependency tool output exceeds budget')
    return result.stdout


def audit(binary, target):
    binary = Path(binary).resolve(strict=True)
    require(binary.is_file(), 'Expected native executable')
    if target.startswith('linux-') and sys.platform.startswith('linux'):
        tool = 'readelf'
        data = parse_elf(invoke(tool, '-dW', binary), invoke(tool, '-lW', binary))
    elif target.startswith('macos-') and sys.platform == 'darwin':
        tool = 'otool'
        data = parse_macho(invoke(tool, '-arch', 'all', '-L', binary),
                           invoke(tool, '-arch', 'all', '-l', binary))
    elif target.startswith('windows-') and sys.platform == 'win32':
        tool = 'dumpbin'
        data = parse_pe(invoke(tool, '/nologo', '/dependents', binary))
    else:
        raise ValueError('Dependency audit requires the matching native host')
    return {'schema': 1, 'target': target, 'tool': tool, **data,
            'scope': 'direct binary imports; no transitive or dynamic-load discovery',
            'dependency_closure_verified': False, 'third_party_licenses_verified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('target')
    args = parser.parse_args()
    print(json.dumps(audit(args.binary, args.target), sort_keys=True, indent=2))


if __name__ == '__main__':
    main()
