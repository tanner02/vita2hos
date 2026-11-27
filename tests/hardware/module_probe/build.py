"""Build owned dependency-start/import probe; never execute or deploy it."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess


def load(path,name):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def tables(data,plan_tool):
    segments=plan_tool.segments(data)
    def read(address,size):
        for base,_,content in segments.values():
            if base<=address and size<=len(content)-(address-base):
                return content[address-base:address-base+size]
        raise ValueError('Table outside file-backed segments')
    entry=struct.unpack_from('<I',data,24)[0]
    info=segments[entry>>30][2][entry&0x3fffffff:]
    exports=[];imports=[]
    first,last=struct.unpack_from('<2I',info,0x24)
    base=segments[entry>>30][0]
    while first<last:
        assert last-first>=32
        size,_,_,nf,nv,nt,library,_,nids,addresses=struct.unpack('<4H6I',read(base+first,32))
        assert size==32 and first+size<=last
        count=nf+nv+nt
        values=struct.unpack('<'+'I'*count,read(nids,count*4))
        targets=struct.unpack('<'+'I'*count,read(addresses,count*4))
        exports.extend((library,'function' if i<nf else 'variable' if i<nf+nv else 'tls',nid,targets[i])
                       for i,nid in enumerate(values))
        first+=size
    first,last=struct.unpack_from('<2I',info,0x2c)
    while first<last:
        header=read(base+first,4);size=struct.unpack_from('<H',header)[0]
        assert size in (0x24,0x34) and first+size<=last
        raw=read(base+first,size);nf,nv,nt=struct.unpack_from('<3H',raw,6)
        library=struct.unpack_from('<I',raw,0x10 if size==0x34 else 0xc)[0]
        for kind,count,offset in [('function',nf,0x1c if size==0x34 else 0x14),
                                  ('variable',nv,0x24 if size==0x34 else 0x1c)]:
            if count:
                pointer=struct.unpack_from('<I',raw,offset)[0]
                imports.extend((library,kind,nid) for nid in struct.unpack('<'+'I'*count,read(pointer,count*4)))
        assert not nt
        first+=size
    return exports,imports


def build(sdk,output):
    here=Path(__file__).resolve().parent;repo=here.parents[2]
    output.mkdir(parents=True,exist_ok=True)
    environment=dict(os.environ,VITASDK=str(sdk))
    environment['PATH']=str(sdk/'bin')+os.pathsep+environment.get('PATH','')
    stubs=output/'stubs'
    subprocess.run([str(sdk/'bin/vita-libs-gen'),str(here/'imports.yml'),str(stubs)],check=True,env=environment)
    subprocess.run(['make','-C',str(stubs),'-j4'],check=True,env=environment)
    flags=['-Os','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin',
        '-fno-stack-protector','-fno-tree-loop-distribute-patterns','-march=armv7-a',
        '-marm','-mfloat-abi=softfp','-mfpu=neon','-nostdlib','-Wl,-q,-e,module_start,-z,noexecstack']
    created={}
    for name,config,sources,libraries in [
        ('dependency','dependency.yml',[here/'dependency.c'],[]),
        ('main','exports.yml',[here/'main.c',here/'../tls_probe/thread_pointer.S'],
            ['-L'+str(stubs),'-lowned_dependency_stub'])]:
        elf=output/(name+'.elf');guest=output/(name+'.velf')
        subprocess.run([str(sdk/'bin/arm-vita-eabi-gcc'),*flags,*map(str,sources),*libraries,
            '-lSceLibKernel_stub','-lSceKernelThreadMgr_stub','-lSceIofilemgr_stub','-lgcc','-o',str(elf)],
            check=True,env=environment)
        result=subprocess.run([str(sdk/'bin/vita-elf-create'),'-vv','-e',str(here/config),str(elf),str(guest)],
            check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=environment)
        (output/(name+'-creation.txt')).write_text(result.stdout,encoding='utf-8')
        created[name]=(guest,elf,result.stdout)
    helpers=load(here/'../tls_probe/build.py','owned_tls_builder')
    plan_tool=load(repo/'tools/make-tls-plan.py','owned_plan_tool')
    guest_tool=load(repo/'tools/guest_elf.py','owned_guest_tool')
    main,elf,_=created['main'];data=bytearray(main.read_bytes());segments=plan_tool.segments(data)
    symbols=helpers.symbols(data) or helpers.symbols(elf.read_bytes())
    sites=[]
    for name,mode,opcode,size in [('probe_read_tp_arm','arm',0xee1d0f70,8),
                                  ('probe_read_tp_thumb','thumb',0x0f70ee1d,6)]:
        address,actual=symbols[name];assert actual==size;address&=~1
        matches=[(i,address-base,mode) for i,(base,flags,content) in segments.items()
                 if flags&1 and base<=address and address-base+4<=len(content)]
        assert len(matches)==1;i,offset,_=matches[0]
        assert struct.unpack_from('<I',segments[i][2],offset)[0]==opcode
        sites.append(matches[0])
    entry=struct.unpack_from('<I',data,24)[0];index,relative=entry>>30,entry&0x3fffffff
    phoff=struct.unpack_from('<I',data,28)[0]
    file_offset=struct.unpack_from('<I',data,phoff+index*32+4)[0]
    info=file_offset+relative
    assert struct.unpack_from('<3I',data,info+0x38)==(0,0,0)
    seed,size=symbols['probe_tls_seed'];assert size==4
    seed-=segments[index][0]
    assert segments[index][2][seed:seed+4]==bytes.fromhex('df9b5713')
    struct.pack_into('<3I',data,info+0x38,seed,4,16)
    guest_tool.validate_guest(data)
    plan=plan_tool.make_plan(data,sites)
    # Source-audit native imports, excluding the two owned symbols provided by
    # the dependency. Structural export/variable checks are performed below.
    source='\n'.join(p.read_text(encoding='utf-8') for p in (repo/'source').rglob('*.c'))
    libraries={n:int(v,16) for n,v in re.findall(r'DECLARE_LIBRARY\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)',source)}
    exports={(libraries[n],int(v,16)) for n,v in re.findall(r'EXPORT(?:_FUNC)?\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)',source) if n in libraries}
    native=[]
    for _,_,creation in created.values():
        for lib,nid,name in re.findall(r'library_nid=(0x[0-9a-fA-F]+).*?target_nid=(0x[0-9a-fA-F]+) \((\w+)\)',creation):
            if int(lib,16)==0x6f776e64: continue
            assert (int(lib,16),int(nid,16)) in exports,name
            native.append(name)
    dependency=created['dependency'][0].read_bytes()
    guest_tool.validate_guest(dependency)
    provided,dependency_imports=tables(dependency,plan_tool)
    _,main_imports=tables(data,plan_tool)
    owned={(lib,kind,nid) for lib,kind,nid,_ in provided if lib}
    expected_owned={(0x6f776e64,'function',0xaabb0011),(0x6f776e64,'variable',0xaabb0022)}
    headers=[struct.unpack_from('<8I',dependency,struct.unpack_from('<I',dependency,28)[0]+i*32)
             for i in range(struct.unpack_from('<H',dependency,44)[0])]
    for library,kind,nid,address in provided:
        if not library: continue
        required=1 if kind=='function' else 2
        at=address&~1 if kind=='function' else address
        assert any(h[0]==1 and h[6]&required and h[2]<=at and at-h[2]<h[5] for h in headers)
    assert all(kind=='function' for _,kind,_ in dependency_imports)
    assert owned==expected_owned
    assert {key for key in main_imports if key[0]==0x6f776e64}==expected_owned
    assert all((lib,nid) in exports for lib,kind,nid in dependency_imports+main_imports
               if lib!=0x6f776e64 and kind=='function')
    assert all(kind=='function' or (lib,kind,nid) in owned for lib,kind,nid in main_imports)
    payload={'executable':bytes(data),'executable.tlsplan':plan,
        'modules.txt':b'sce_module/owned_dependency.suprx\n',
        'sce_module/owned_dependency.suprx':created['dependency'][0].read_bytes()}
    for name,content in payload.items():
        path=output/'sd/vita2hos'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(content)
    manifest={'probe':'module_probe','version':1,'native_imports':sorted(set(native)),
        'owned_library':'0x6f776e64','owned_function':'0xaabb0011','owned_variable':'0xaabb0022',
        'hardware_execution':'not performed','expected_guest_exit':0,
        'expected_report':'/vita2hos/module-probe.txt',
        'files':{name:hashlib.sha256(content).hexdigest() for name,content in payload.items()}}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print('Built owned main/dependency payload; native imports and two source-bound TLS patch sites audited.')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();build(args.sdk,args.output)
