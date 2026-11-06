#!/usr/bin/env python3
import argparse
import json
import hashlib
import os
from pathlib import Path
import subprocess
import yaml

p = argparse.ArgumentParser()
p.add_argument('--workspace', type=Path, required=True)
p.add_argument('--pristine', action='store_true')
p.add_argument('--target', type=int, help='Zero-based build.yaml include index')
p.add_argument('--source-dir', type=Path, help='Source alias for Windows paths containing spaces')
p.add_argument('--manifest', default='build.yaml', help='Optional build matrix relative to repository')
a = p.parse_args()
repo = a.source_dir.absolute() if a.source_dir else Path(__file__).resolve().parents[1]
ws = a.workspace.resolve()
results = []
for i, target in enumerate(yaml.safe_load((repo / a.manifest).read_text())['include']):
    if a.target is not None and i != a.target:
        continue
    build = ws / 'builds' / (str(i) if a.manifest=='build.yaml' else f'd{i}')
    build.parent.mkdir(exist_ok=True)
    cmd = ['west', 'build', '-p', 'always' if a.pristine else 'auto', '-s', str(ws / 'zmk/app'), '-d', str(build), '-b', target['board']]
    if target.get('snippet'):
        for snippet in target['snippet'].split():
            cmd += ['-S', snippet]
    import shlex
    # CMake parses backslashes in Windows paths as escapes (for example, \U).
    cmd += ['--', f'-DZMK_CONFIG={repo.as_posix()}/config', f'-DZMK_EXTRA_MODULES={repo.as_posix()}', f'-DZEPHYR_BASE={ws.as_posix()}/zephyr', f'-DZephyr_DIR={ws.as_posix()}/zephyr/share/zephyr-package/cmake']
    if target.get('shield'):
        cmd += [f'-DSHIELD={target["shield"]}']
    if target.get('shield', '').split()[0].startswith('nano35_'):
        cmd += [f'-DKEYMAP_FILE={repo.as_posix()}/config/nano35.keymap']
    cmd += shlex.split(target.get('cmake-args', ''))
    log = build.with_suffix('.log')
    output = repo / 'artifacts' / (target['artifact-name'] + '.uf2')
    # A failed rebuild must not leave an older, apparently current release file.
    output.unlink(missing_ok=True)
    print(f'Nano35 target {i}: {target}', flush=True)
    with log.open('w') as out:
        status = subprocess.run(cmd, cwd=ws, env={**os.environ, 'ZEPHYR_BASE': (ws / 'zephyr').as_posix()}, stdout=out, stderr=subprocess.STDOUT).returncode
    artifact = build / 'zephyr/zmk.uf2'
    if status == 0 and artifact.is_file():
        import shutil
        (repo / 'artifacts').mkdir(exist_ok=True)
        shutil.copyfile(artifact, output)
    passed = status == 0 and artifact.is_file()
    results.append({'index': i, 'target': target, 'exit_code': status, 'passed': passed, 'log': str(log), 'artifact': str(artifact) if passed else None, 'sha256': hashlib.sha256(artifact.read_bytes()).hexdigest() if passed else None})
    print(f'  {"PASS" if passed else "FAIL"}: {log}', flush=True)
    if not passed:
        print('\n'.join(log.read_text().splitlines()[-30:]), flush=True)
(ws / 'results.json').write_text(json.dumps(results, indent=2))
raise SystemExit(0 if results and all(x['passed'] for x in results) else 1)
