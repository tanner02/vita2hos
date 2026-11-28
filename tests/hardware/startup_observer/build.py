"""Build the owned startup observer; never install or launch it."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def build(sdk, output):
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, VITASDK=str(sdk))
    env['PATH'] = str(sdk/'bin') + os.pathsep + env.get('PATH', '')
    elf, guest = output/'observer.elf', output/'observer.suprx'
    flags = ['-Os', '-Wall', '-Wextra', '-Werror', '-ffreestanding', '-fno-builtin',
             '-fno-stack-protector', '-fno-tree-loop-distribute-patterns',
             '-march=armv7-a', '-marm', '-mfloat-abi=softfp', '-mfpu=neon',
             '-nostdlib', '-Wl,-q,-e,module_start,-z,noexecstack']
    subprocess.run([str(sdk/'bin/arm-vita-eabi-gcc'), *flags, str(here/'observer.c'),
                    '-lSceLibKernel_stub', '-lSceIofilemgr_stub', '-lgcc', '-o', str(elf)], check=True, env=env)
    creation = subprocess.check_output([str(sdk/'bin/vita-elf-create'), '-vv', '-e',
                                       str(here/'exports.yml'), str(elf), str(guest)],
                                      stderr=subprocess.STDOUT, text=True, env=env)
    (output/'creation.txt').write_text(creation)
    guest_tool = load(repo/'tools/guest_elf.py', 'observer_guest')
    guest_tool.validate_guest(guest.read_bytes())
    source = '\n'.join(p.read_text(encoding='utf-8') for p in (repo/'source').rglob('*.c'))
    libraries = {n: int(v, 16) for n, v in re.findall(
        r'DECLARE_LIBRARY\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)', source)}
    exports = {(libraries[n], int(v, 16)) for n, v in re.findall(
        r'EXPORT(?:_FUNC)?\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)', source) if n in libraries}
    imports = re.findall(r'library_nid=(0x[0-9a-fA-F]+).*?target_nid=(0x[0-9a-fA-F]+) \((\w+)\)', creation)
    assert imports, 'No audited imports found'
    for library, nid, name in imports:
        assert (int(library, 16), int(nid, 16)) in exports, name
    manifest = {'version': 2, 'imports': sorted({name for _, _, name in imports}),
                'module_sha256': hashlib.sha256(guest.read_bytes()).hexdigest(),
                'source_sha256': hashlib.sha256((here/'observer.c').read_bytes()).hexdigest(),
                'hardware_execution': 'not performed by build script'}
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build(args.sdk, args.output)
