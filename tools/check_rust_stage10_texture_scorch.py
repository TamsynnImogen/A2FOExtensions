#!/usr/bin/env python3
"""Build frozen/current full-source scorch fixtures and compare supplied native logs.

This checker never launches Wine. The lead runs the two binaries with
``scorch-matrix`` and places stdout/stderr beside them for --compare-existing.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/render-ui-batch'
BEFORE = SLICE / 'before'
ORIGINAL = ROOT / 'build/rust-migration-stage10/squadrons/checkpoint/liba2fo_core-win.a'
ORIGINAL_SHA = '08c75d5764c7d7f7094b4dcbcf32da99205f51b93f673291984cfc399ba8fe9a'
CANDIDATE = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
FIXTURE = Path('tests/rust_stage9_texture_visuals_native.cpp')
MODULE = Path('modules/A2FOTextureVariants/module.cpp')
HELPER = Path('modules/A2FOTextureVariants/texture_variants.cpp')
FLAGS = ['-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-Wno-mismatched-new-delete',
         '-UNDEBUG', '-DWIN32_LEAN_AND_MEAN', '-DNOMINMAX',
         '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
         '-static', '-static-libgcc', '-static-libstdc++']
LIBS = ['-lntdll', '-luserenv', '-lws2_32', '-lbcrypt', '-ladvapi32',
        '-lgdi32', '-luser32', '-lole32', '-lshell32', '-lshlwapi', '-lcomctl32']

EXTENSION = r'''
static unsigned short scorch_cw() {
    unsigned short value=0; __asm__ __volatile__("fnstcw %0":"=m"(value)); return value;
}
static unsigned short scorch_sw() {
    unsigned short value=0; __asm__ __volatile__("fnstsw %0":"=m"(value)); return value;
}
static void scorch_set_cw(unsigned short value) {
    __asm__ __volatile__("fldcw %0"::"m"(value));
}
static void scorch_clear() { __asm__ __volatile__("fnclex"); }
static void scorch_matrix() {
    std::puts("ready:scorch-matrix"); std::fflush(stdout);
    std::array<std::uint8_t,0x300> local_class{};
    std::array<std::uint8_t,0x100> local_geometry{}, a{}, b{}, c{};
    put(local_class.data(),kCraftClassGeometryOffset,static_cast<void*>(local_geometry.data()));
    put(local_geometry.data(),kGeometryRootNodeOffset,static_cast<void*>(a.data()));
    put(a.data(),kNodeNameOffset,"a"); put(b.data(),kNodeNameOffset,"b");
    put(c.data(),kNodeNameOffset,"c");
    put(a.data(),kNodeSiblingOffset,static_cast<void*>(b.data()));
    put(b.data(),kNodeSiblingOffset,static_cast<void*>(c.data()));
    g_node_particle_add=nullptr;
    struct Case {const char* name; double hp; int max; float threshold;
        unsigned published; bool null_craft, null_class, empty_target, empty_effects;};
    const Case cases[]={
        {"full",100,100,0.2f,0,false,false,false,false},
        {"band-one",70,100,0.2f,0,false,false,false,false},
        {"band-two",50,100,0.2f,0,false,false,false,false},
        {"band-three",30,100,0.2f,0,false,false,false,false},
        {"band-four",10,100,0.2f,0,false,false,false,false},
        {"clamp-low",-10,100,0.2f,0,false,false,false,false},
        {"clamp-high",200,100,0.2f,0,false,false,false,false},
        {"near-80-low",79.9999,100,0.2f,0,false,false,false,false},
        {"at-80",80,100,0.2f,0,false,false,false,false},
        {"near-80-high",80.0001,100,0.2f,0,false,false,false,false},
        {"at-60",60,100,0.2f,0,false,false,false,false},
        {"unchanged-published",0,100,0.2f,5,false,false,false,false},
        {"null-craft",30,100,0.2f,0,true,false,false,false},
        {"null-class",30,100,0.2f,0,false,true,false,false},
        {"zero-threshold",30,100,0.0f,0,false,false,false,false},
        {"negative-threshold",30,100,-0.2f,0,false,false,false,false},
        {"empty-target",30,100,0.2f,0,false,false,true,false},
        {"empty-effects",30,100,0.2f,0,false,false,false,true},
        {"zero-max",30,0,0.2f,0,false,false,false,false},
    };
    const unsigned short original=scorch_cw();
    const unsigned short pc[]={0x0000,0x0200,0x0300};
    const unsigned short rc[]={0x0000,0x0400,0x0800,0x0c00};
    unsigned total=0;
    for(unsigned p=0;p<3;++p)for(unsigned r=0;r<4;++r)for(const auto& item:cases){
        ClassDamageVisualPolicy policy{}; SubsystemDamageVisualState visual{};
        policy.damage_threshold=item.threshold;
        if(!item.empty_target)policy.target_hardpoints[0]={"a","b","c"};
        if(!item.empty_effects)policy.scorch_effects={"burnA","burnB"};
        visual.scorch_count=item.published;
        trace_rows.clear();
        const unsigned short requested=static_cast<unsigned short>(
            (original&~0x0f00u)|pc[p]|rc[r]);
        scorch_clear(); scorch_set_cw(requested);
        const unsigned short before_cw=scorch_cw(),before_sw=scorch_sw();
        add_scorch_marker(item.null_craft?nullptr:craft.data(),
            item.null_class?nullptr:local_class.data(),policy,visual,7,0,item.hp,item.max);
        const unsigned short after_cw=scorch_cw(),after_sw=scorch_sw();
        scorch_set_cw(original); scorch_clear();
        check(before_cw==requested&&after_cw==requested,"scorch control word");
        check((before_sw&0x3800)==(after_sw&0x3800),"scorch x87 TOP");
        check(visual.scorch_count>=item.published&&visual.scorch_count<=5,
              "scorch published count");
        if(visual.scorch_count>item.published){
            check(trace_rows.size()==1,"scorch published log");
            const char* wanted[]={"a","b","c"};
            const char* hardpoint=wanted[(visual.scorch_count-1)%3];
            check(trace_rows[0].find(std::string("hardpoint '")+hardpoint+"'")!=
                  std::string::npos,"scorch chosen hardpoint");
        }else check(trace_rows.empty(),"scorch rejected without log");
        std::printf("scorch:%u:%u:%s:%u:cw=%04x,%04x:sw=%04x,%04x\n",
            p,r,item.name,visual.scorch_count,before_cw,after_cw,
            before_sw&0x3a3f,after_sw&0x3a3f);
        for(const auto& row:trace_rows)std::printf("trace:%s\n",row.c_str());
        ++total;
    }
    scorch_set_cw(original); scorch_clear();
    std::printf("completed:scorch-matrix:%u\n",total);
}
'''


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def generated_fixture(source):
    marker = 'int main(int argc, char** argv) {'
    if source.count(marker) != 1:
        raise RuntimeError('original fixture main marker drift')
    before, main = source.split(marker, 1)
    branch = '    if (mode == "shutdown") {'
    if main.count(branch) != 1:
        raise RuntimeError('original fixture mode marker drift')
    main = main.replace(branch, '    if (mode == "scorch-matrix") {\n'
                        '        scorch_matrix(); return 0;\n'
                        '    }\n' + branch, 1)
    return before + EXTENSION + '\n' + marker + main


def dependencies(path, macro=None):
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM',
               '-I', str(path.parent)]
    if macro is not None:
        command.append(f'-DMODULE_SOURCE="{macro}"')
    command.append(str(path))
    output = subprocess.run(command, cwd=ROOT, capture_output=True,
                            check=True, timeout=60).stdout.decode()
    words = output.replace('\\\n', ' ').split(':', 1)[1].split()
    return sorted({str(p.relative_to(ROOT)) for word in words
                   if (p := Path(word).resolve()).is_relative_to(ROOT)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--build-receipt', type=Path,
                        default=SLICE / 'build-receipt-v1.json')
    parser.add_argument('--compare-existing', action='store_true')
    args = parser.parse_args()
    work = args.work_dir.resolve()
    if work.parent != SLICE or not work.name.startswith('scorch-'):
        parser.error('--work-dir must be a scorch-* child of the batch slice')
    if not args.compare_existing and work.exists():
        parser.error('build requires a fresh work directory')
    receipt_path = work / 'receipt.json'
    if args.compare_existing:
        receipt = json.loads(receipt_path.read_text())
        if receipt.get('status') != 'built':
            parser.error('comparison requires a built fixture receipt')
        for version in ('before', 'current'):
            exe = work / f'scorch-{version}.exe'
            if sha(exe) != receipt['binaries'][version]:
                raise RuntimeError(f'{version} binary drift')
            stdout = work / f'scorch-{version}.stdout'
            stderr = work / f'scorch-{version}.stderr'
            data = stdout.read_bytes().replace(b"\r\n", b"\n")
            if data.count(b'ready:scorch-matrix\n') != 1 or \
                    data.count(b'completed:scorch-matrix:228\n') != 1 or \
                    sum(line.startswith(b'scorch:') for line in data.splitlines()) != 228:
                raise RuntimeError(f'{version} incomplete native trace')
            receipt.setdefault('logs', {})[version] = {
                'stdout_sha256': sha(stdout), 'stderr_sha256': sha(stderr)}
        for suffix in ('stdout', 'stderr'):
            if (work / f'scorch-before.{suffix}').read_bytes().replace(b'\r\n', b'\n') != \
                    (work / f'scorch-current.{suffix}').read_bytes().replace(b'\r\n', b'\n'):
                raise RuntimeError(f'frozen/current {suffix} mismatch')
        receipt['status'] = 'passed'
        receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
        return

    build_receipt = args.build_receipt.resolve(strict=True)
    build = json.loads(build_receipt.read_text())
    candidate_key = CANDIDATE.relative_to(ROOT).as_posix()
    if build.get('status') != 'passed' or \
            build.get('artifacts', {}).get(candidate_key) != sha(CANDIDATE):
        parser.error('passed batch receipt must pin the candidate archive')
    if sha(ORIGINAL) != ORIGINAL_SHA:
        parser.error('original archive drift')
    frozen_hashes = json.loads((SLICE / 'before-hashes.json').read_text())
    work.mkdir()
    receipt = {'status': 'building', 'build_receipt_sha256': sha(build_receipt),
               'original_archive_sha256': sha(ORIGINAL),
               'candidate_archive_sha256': sha(CANDIDATE), 'binaries': {},
               'dependencies': {}, 'commands': []}
    try:
        for version, archive in (('before', ORIGINAL), ('current', CANDIDATE)):
            root = BEFORE if version == 'before' else ROOT
            original_fixture = root / FIXTURE
            module = root / MODULE
            helper = root / HELPER
            for path in (original_fixture, module, helper):
                if version == 'before':
                    logical = path.relative_to(BEFORE).as_posix()
                    if frozen_hashes.get(logical) != sha(path):
                        raise RuntimeError(f'frozen source drift: {logical}')
            generated = work / f'scorch-{version}.cpp'
            generated.write_text(generated_fixture(original_fixture.read_text()))
            macro = module.as_posix()
            deps = sorted(set(dependencies(generated, macro)) |
                          set(dependencies(helper)))
            receipt['dependencies'][version] = {name: sha(ROOT / name) for name in deps}
            for name in deps:
                path = ROOT / name
                if version == 'before' and path.is_relative_to(BEFORE):
                    logical = path.relative_to(BEFORE).as_posix()
                    if frozen_hashes.get(logical) != sha(path):
                        raise RuntimeError(f'frozen dependency drift: {logical}')
            snapshot = work / 'source-used' / version
            snapshot.mkdir(parents=True)
            for name in deps:
                source = ROOT / name
                dest = snapshot / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, dest)
            shutil.copy2(helper, snapshot / 'texture_variants.cpp')
            exe = work / f'scorch-{version}.exe'
            command = ['i686-w64-mingw32-g++', *FLAGS,
                       f'-DMODULE_SOURCE="{module}"', str(generated), str(helper),
                       str(archive), *LIBS, '-o', str(exe)]
            stdout = work / f'compile-{version}.stdout'
            stderr = work / f'compile-{version}.stderr'
            with stdout.open('wb') as out, stderr.open('wb') as err:
                code = subprocess.run(command, cwd=ROOT, stdout=out, stderr=err,
                                      timeout=300, check=False).returncode
            receipt['commands'].append({'version': version, 'command': command,
                                        'exit_code': code, 'stdout_sha256': sha(stdout),
                                        'stderr_sha256': sha(stderr)})
            if code:
                raise RuntimeError(f'{version} compilation failed: {stderr}')
            receipt['binaries'][version] = sha(exe)
        receipt['status'] = 'built'
    except Exception as error:
        receipt['status'] = 'failed'
        receipt['error'] = str(error)
        raise
    finally:
        receipt_path.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')


if __name__ == '__main__':
    main()
