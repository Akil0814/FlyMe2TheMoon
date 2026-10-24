"""Normalize MSVC /showIncludes for Ninja on Chinese-only VS installations.

VSLANG cannot select English when only clui.dll/2052 is installed. Forward all
compiler arguments and exit status unchanged; translate only the dependency
marker, and emit UTF-8 paths for Ninja. This does not change compiled sources.
"""
import os,subprocess,sys,locale
environment=os.environ.copy();environment['VSLANG']='1033'
process=subprocess.Popen(sys.argv[1:],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=environment)
for line in iter(process.stdout.readline,b''):
    try:text=line.decode('utf-8')
    except UnicodeDecodeError:text=line.decode(locale.getpreferredencoding(False),errors='replace')
    if text.startswith('注意: 包含文件:'):
        text='Note: including file:'+text[len('注意: 包含文件:'):]
    sys.stdout.buffer.write(text.encode('utf-8'));sys.stdout.buffer.flush()
sys.exit(process.wait())
