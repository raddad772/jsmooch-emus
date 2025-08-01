# Thanks to EmuDev Discord user netcat

from lxml import html
from os import path
import re
import requests

URL = 'http://shared-ptr.com/sh_insns.html'
LOC = 'sh_insns.html'

ARCH = 'SH4'

def get_source():
    if path.isfile(LOC):
        print(f'reading cached source from {LOC}...')
        return open(LOC, 'rb').read()

    print(f'retrieving source from {URL}...')
    source = requests.get(URL).content
    open(LOC, 'wb').write(source)

    return source

def parse_source(source, arch):
    filt  = re.compile(r'(\s+|\n)')
    word  = re.compile(rf'\b{arch.lower()}\b')
    pcode = re.compile(r'([a-zA-Z0-9_]+) \(') 
    outstr = '';
    outstr2 = '';

    tree = html.fromstring(source)

    count = 0
    for inst in tree.xpath('//div[@class="col_cont"]'):
        compat = filt.sub(' ', inst[0].text).strip()
        disasm = filt.sub(' ', inst[1].text).strip()
        code   = filt.sub(' ', inst[3].text).strip()
        impl = inst[2].text.strip().replace('\n',', ')
        if not word.findall(compat.lower()):
            continue

        assert(len(code) == 16)

        pscode = inst.xpath('.//p[@class="precode"]/text()')[0]
        name = pcode.findall(pscode)[0]

        count += 1
        
        outstr += '\nOE("' + code + '", &SH4_' + name + ', "' + disasm + '"); // ' + impl
        outstr2 += "\nSH4ins(" + name + ") { // " + impl + "\n    BADOPCODE; // Crash on unimplemented opcode\n    PCinc;\n}\n";
        
    print(outstr)
    print(outstr2)
        

    print(f'{count} valid instructions for arch {arch}')

def main():
    source = get_source()
    parse_source(source, ARCH)

if __name__ == '__main__':
    main()
