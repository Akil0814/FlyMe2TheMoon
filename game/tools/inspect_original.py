"""Read-only Mach-O/Objective-C metadata and disassembly for local research.

Never loads or executes the reference program. Capstone is an analysis-only
dependency installed in out/analysis/python, not a game runtime dependency.
"""
from pathlib import Path
import argparse, json, struct, sys
from analyze_reference import ROOT, original_file


class Original:
    def __init__(self):
        fat=original_file('Fly2theMoon').read_bytes()
        arch=struct.unpack_from('>5I',fat,8)
        self.data=fat[arch[2]:arch[2]+arch[3]]
        assert struct.unpack_from('<I',self.data)[0]==0xfeedface
        self.sections={};self.segments=[];self.names={};self.classes={}
        position=28;symtab=None;indirect=None
        for _ in range(struct.unpack_from('<I',self.data,16)[0]):
            command,size=struct.unpack_from('<II',self.data,position)
            if command==1:
                address,length,offset,filesize=struct.unpack_from('<4I',self.data,position+24)
                if filesize:self.segments.append((address,length,offset,filesize))
                for index in range(struct.unpack_from('<I',self.data,position+48)[0]):
                    at=position+56+68*index
                    name=self.data[at:at+16].split(b'\0')[0].decode()
                    self.sections[name]=struct.unpack_from('<9I',self.data,at+32)
            if command==0x21:assert struct.unpack_from('<I',self.data,position+16)[0]==0,'Encrypted reference cannot be inspected'
            if command==2:symtab=struct.unpack_from('<4I',self.data,position+8)
            if command==11:indirect=struct.unpack_from('<2I',self.data,position+56)
            position+=size
        if symtab and indirect:
            offset,count,strings,length=symtab
            symbols=[self.data[strings+struct.unpack_from('<I',self.data,offset+i*12)[0]:strings+length].split(b'\0',1)[0].decode('utf8','replace') for i in range(count)]
            for section in self.sections.values():
                address,length,_,_,_,_,flags,first,stride=section
                if flags&255==8:
                    for i in range(length//stride):
                        symbol=struct.unpack_from('<I',self.data,indirect[0]+(first+i)*4)[0]
                        if symbol<count:self.names[address+i*stride]=symbols[symbol]
        for name in ['__objc_methname','__objc_classname','__cstring']:
            address,length,offset,*_=self.sections[name];cursor=0
            while cursor<length:
                value=self.data[offset+cursor:offset+length].split(b'\0',1)[0]
                self.names[address+cursor]=value.decode('utf8','replace');cursor+=len(value)+1
        address,length,*_=self.sections['__objc_classlist']
        for at in range(address,address+length,4):
            cls=self.u32(at);ro=self.u32(cls+16)&~3;name=self.string(self.u32(ro+16))
            self.classes[name]={'address':cls,'methods':self.methods(self.u32(ro+20)),'ivars':[]}
            meta=self.u32(cls);meta_ro=self.u32(meta+16)&~3
            self.classes[name]['class_methods']=self.methods(self.u32(meta_ro+20))
            ivars=self.u32(ro+28)
            if ivars:
                stride,count=self.unpack('<2I',ivars)
                for n in range(count):
                    offset,name_ptr,type_ptr,alignment,size=self.unpack('<5I',ivars+8+n*stride)
                    field={'name':self.string(name_ptr),'offset':self.u32(offset),'type':self.string(type_ptr),'size':size,'offset_address':offset}
                    self.classes[name]['ivars'].append(field);self.names[offset]=name+'.'+field['name']
        for name,c in self.classes.items():
            for prefix,group in [('-',c['methods']),('+',c['class_methods'])]:
                for m in group:self.names[m['address']&~1]=f'{prefix}[{name} {m["name"]}]'

    def offset(self,address):
        for base,length,offset,size in self.segments:
            if base<=address<base+size:return offset+address-base
        raise ValueError(f'Unmapped address {address:#x}')

    def unpack(self,format,address):return struct.unpack_from(format,self.data,self.offset(address))
    def u32(self,address):return self.unpack('<I',address)[0]
    def string(self,address):return self.data[self.offset(address):].split(b'\0',1)[0].decode('utf8','replace')
    def describe(self,address):
        if address in self.names:return self.names[address]
        section=self.sections.get('__cfstring')
        if section and section[0]<=address<section[0]+section[1]:
            return repr(self.string(self.u32(address+8)))
        return ''
    def methods(self,address):
        if not address:return []
        stride,count=self.unpack('<2I',address);assert stride==12
        return [{'name':self.string(self.u32(address+8+i*12)),'type':self.string(self.u32(address+12+i*12)),'address':self.u32(address+16+i*12)} for i in range(count)]

    def disassemble(self,method):
        sys.path.insert(0,str(ROOT/'out/analysis/python'))
        from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM,CS_MODE_THUMB
        from capstone.arm import ARM_OP_MEM,ARM_OP_IMM,ARM_OP_REG,ARM_REG_PC
        address=method['address'];thumb=address&1;address&=~1
        addresses=sorted(m['address']&~1 for c in self.classes.values() for group in ['methods','class_methods'] for m in c[group])
        end=next((a for a in addresses if a>address),address+4096)
        decoder=Cs(CS_ARCH_ARM,CS_MODE_THUMB if thumb else CS_MODE_ARM);decoder.detail=True
        result=[];registers={}
        for instruction in decoder.disasm(self.data[self.offset(address):self.offset(address)+min(end-address,16384)],address):
            note='';loaded=None
            for operand in instruction.operands:
                if operand.type==ARM_OP_MEM and (operand.mem.base==ARM_REG_PC or operand.mem.base in registers):
                    pc=(instruction.address+4)&~3 if thumb else instruction.address+8
                    if operand.mem.index and operand.mem.index not in registers:continue
                    target=(pc if operand.mem.base==ARM_REG_PC else registers[operand.mem.base])+operand.mem.disp+(registers.get(operand.mem.index,0) if operand.mem.index else 0)
                    try:
                        value=self.u32(target);loaded=value;note+=f' [{target:#x}]={value:#x} float={self.unpack("<f",target)[0]:.8g} {self.describe(target) or self.describe(value)}'
                    except ValueError:pass
            if loaded is not None and instruction.mnemonic.startswith('ldr'):registers[instruction.operands[0].reg]=loaded
            elif instruction.mnemonic=='add' and len(instruction.operands)==3 and instruction.operands[1].reg==ARM_REG_PC and instruction.operands[2].type==ARM_OP_REG and instruction.operands[2].reg in registers:
                value=instruction.address+8+registers[instruction.operands[2].reg]
                registers[instruction.operands[0].reg]=value;note+=f' address={value:#x} {self.describe(value)}'
            elif instruction.mnemonic.startswith('bl'):
                if instruction.operands[0].type==ARM_OP_IMM:note+=' '+self.names.get(instruction.operands[0].imm,'')
                for name in ['r0','r1','r2','r3']:registers={r:v for r,v in registers.items() if decoder.reg_name(r)!=name}
            elif instruction.operands and instruction.operands[0].type==ARM_OP_REG and instruction.mnemonic not in ['cmp','tst','str','vstr']:
                registers.pop(instruction.operands[0].reg,None)
            result.append(f'{instruction.address:08x}  {instruction.mnemonic:9} {instruction.op_str:38} ;{note}')
        return '\n'.join(result)


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--class-name');parser.add_argument('--method');args=parser.parse_args()
    binary=Original();out=ROOT/'out/analysis';out.mkdir(parents=True,exist_ok=True)
    (out/'original-objc.json').write_text(json.dumps(binary.classes,ensure_ascii=False,indent=2),encoding='utf8')
    if args.class_name:
        cls=binary.classes[args.class_name]
        if args.method:
            method=next(m for group in ['methods','class_methods'] for m in cls[group] if m['name']==args.method)
            text=binary.disassemble(method);target=out/(args.class_name+'-'+args.method.replace(':','_')+'.asm');target.write_text(text,encoding='utf8');print(target);print(text)
        else:print(json.dumps(cls,ensure_ascii=False,indent=2))
    else:print('Parsed',len(binary.classes),'Objective-C classes')
