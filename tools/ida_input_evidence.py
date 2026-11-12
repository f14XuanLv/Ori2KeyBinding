"""Headless IDA extractor for the installed Ori 2 PE. No game mutation."""
import json, traceback, re
from pathlib import Path
import ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_idaapi, ida_lines, ida_name, ida_nalt, ida_pro, ida_ua, idautils, idc

ROOT = Path(r'D:\CodeT2\Ori2\build\local-evidence')
OUT = ROOT / 'ida'
OUT.mkdir(exist_ok=True)
report = (OUT / 'progress.log').open('w', encoding='utf-8', buffering=1)

def note(s):
    report.write(str(s) + '\n')

try:
    note('Loaded: ' + ida_nalt.get_input_file_path())
    base = ida_nalt.get_imagebase()
    note('Imagebase: ' + hex(base))
    ida_auto.enable_auto(True)
    data = json.loads((ROOT / 'il2cpp/script.json').read_text(encoding='utf-8-sig'))
    names = {}
    for m in data['ScriptMethod']:
        names.setdefault(base + m['Address'], m['Name'])
    for group in ([] if ida_name.get_name(base + 74725368) == 'System_Type_TypeInfo' else ['ScriptMetadata', 'ScriptMetadataMethod', 'ScriptString']):
        for m in data[group]:
            ea = base + m['Address']
            name = m.get('Name') or 'Literal_' + re.sub(r'\W', '_', m.get('Value', ''))[:75]
            ida_name.set_name(ea, re.sub(r'[^a-zA-Z0-9_]', '_', name), ida_name.SN_NOWARN | ida_name.SN_NOCHECK)
            if 'Value' in m:
                ida_bytes.set_cmt(ea, m['Value'], False)
    note('Metadata named')
    # Only field declarations and offsets from this build's dump.cs are used here.
    # These types improve readability; all original instructions remain in the export.
    dump = (ROOT / 'il2cpp/dump.cs').read_text(encoding='utf-8-sig')
    def fields_for(name, instance=False, index=None):
        marker = r'public (?:static )?class '+re.escape(name)+r'(?: : [^\n]+)? // TypeDefIndex: '+(str(index) if index else r'\d+')
        hit = re.search(marker,dump)
        if not hit: return []
        at = hit.start()
        block = dump[at:dump.find('// Methods', at)].split('// Properties')[0]
        fields = []
        for line in block.splitlines():
            if instance and (' static ' in line or ' const ' in line): continue
            m = re.search(r'(\w+(?:\.\w+)*(?:\[\])?) (\w+); // 0x([0-9A-Fa-f]+)', line)
            if not m: continue
            t, n, off = m.groups(); off = int(off,16)
            c, size = ('unsigned char',1) if t=='bool' else ('float',4) if t=='float' else ('int',4) if t=='int' else ('float',8) if t=='Vector2' else ('void *',8)
            fields.append((off,c,n,size))
        return fields
    def decl(name, fields):
        s='typedef struct '+name+' {\n'; pos=0
        for off,c,n,size in sorted(fields):
            if off>pos: s+=f'unsigned char padding_{pos:x}[{off-pos}];\n'
            s+=f'{c} {n}'+('[2]' if size==8 and c=='float' else '')+';\n';pos=off+size
        return s+'} '+name+';\n'
    declarations = decl('OriPlayerInput', fields_for('PlayerInput',True))
    for src,dst,idx in [('Input','OriInputStatic',18851),('Input.Cmd','OriCmdStatic',None),('Input.Dbg','OriDbgStatic',None),('PlayerInputRebinding','OriRebindingStatic',None)]:
        declarations += decl(dst, fields_for(src,index=idx))
        declarations += f'typedef struct {dst}Class {{ unsigned char padding[184]; {dst} *static_fields; }} {dst}Class;\n'
    note('Parse local field types: '+str(idc.parse_decls(declarations,idc.PT_SIL)))
    (OUT/'local-field-types.h').write_text(declarations,encoding='utf-8')
    for symbol,kind in [('Core_Input_TypeInfo','OriInputStaticClass'),('Core_Input_Cmd_TypeInfo','OriCmdStaticClass'),('Core_Input_Dbg_TypeInfo','OriDbgStaticClass'),('PlayerInputRebinding_TypeInfo','OriRebindingStaticClass')]:
        addr=ida_name.get_name_ea(ida_idaapi.BADADDR,symbol)
        if addr!=ida_idaapi.BADADDR: idc.SetType(addr,kind+' *')
    prototypes = {
        'PlayerInput$$AddControllerControls':'void f(OriPlayerInput *self, void *method);',
        'PlayerInput$$FixedUpdate':'void f(OriPlayerInput *self, void *method);',
        'PlayerInput$$RefreshControls':'void f(OriPlayerInput *self, void *method);',
        'SmartInput.CompoundButtonInput$$Add':'void f(void *self, void *input, void *method);',
        'SmartInput.CompoundAxisInput$$Add':'void f(void *self, void *input, void *method);',
        'SmartInput.ControllerButtonInput$$.ctor':'void f(void *self, int button, void *method);',
        'SmartInput.ControllerAxisInput$$.ctor':'void f(void *self, int axis, void *method);',
        'SmartInput.XboxControllerInput$$GetButton':'bool f(int button, int userIndex, void *method);',
        'SmartInput.XboxControllerInput$$GetAxis':'float f(int axis, int userIndex, void *method);',
        'UnityEngine.Mathf$$RoundToInt':'int f(float value, void *method);',
        'PlayerInput$$SimplifyAxis':'float f(OriPlayerInput *self, float value, void *method);',
        'PlayerInput$$ApplyDeadzone':'void f(OriPlayerInput *self, float *x, float *y, float deadzone, void *method);',
    }
    for m in data['ScriptMethod']:
        if m['Name'] in prototypes:
            idc.SetType(base+m['Address'],prototypes[m['Name']])
    selected = [
        'PlayerInput$$AddControllerControls', 'PlayerInput$$FixedUpdate', 'PlayerInput$$RefreshControls',
        'PlayerInputRebinding$$.cctor', 'PlayerInputRebinding$$SetDefaultControllerButtonRemappings',
        'SmartInput.ControllerButtonInput$$.ctor', 'SmartInput.ControllerButtonInput$$GetValue',
        'SmartInput.CompoundButtonInput$$Add', 'SmartInput.CompoundButtonInput$$GetValue',
        'SmartInput.XboxControllerInput$$GetButton',
    ]
    extra = ROOT / 'ida-extra-methods.json'
    if extra.exists(): selected += json.loads(extra.read_text(encoding='utf-8'))
    methods = [m for m in data['ScriptMethod'] if m['Name'] in selected]
    hex_ok = ida_hexrays.init_hexrays_plugin()
    note('Hex-Rays available: ' + str(hex_ok))
    for m in methods:
        ea = base + m['Address']
        note('Analyzing ' + m['Name'] + ' ' + hex(ea))
        ida_ua.create_insn(ea)
        ida_funcs.add_func(ea)
        f = ida_funcs.get_func(ea)
        if not f:
            note('No function'); continue
        note('Range: ' + hex(f.start_ea) + '-' + hex(f.end_ea))
        ida_auto.plan_and_wait(f.start_ea, f.end_ea)
        ida_name.set_name(ea, re.sub(r'\W', '_', m['Name']), ida_name.SN_NOWARN)
        # AddControllerControls is large. Follow only its existing decoded function chunks.
        for head in idautils.FuncItems(ea):
            for target in idautils.CodeRefsFrom(head, False):
                if target in names:
                    ida_name.set_name(target, re.sub(r'\W', '_', names[target]), ida_name.SN_NOWARN)
        # Analysis creates call xrefs and stack frames; invalidate any preliminary microcode cache.
        if hex_ok: ida_hexrays.mark_cfunc_dirty(ea, False)
        file = OUT / (re.sub(r'\W', '_', m['Name']) + '.txt')
        with file.open('w', encoding='utf-8') as out:
            out.write(m['Name'] + '\nRVA: ' + hex(m['Address']) + '\nSignature: ' + m['Signature'] + '\n\n')
            if hex_ok:
                try:
                    cfunc = ida_hexrays.decompile(ea)
                    out.write(str(cfunc) + '\n\n')
                    calls = []
                    class CallVisitor(ida_hexrays.ctree_visitor_t):
                        def __init__(self): super().__init__(ida_hexrays.CV_FAST)
                        def visit_expr(self, expr):
                            if expr.op == ida_hexrays.cot_call and expr.x.op == ida_hexrays.cot_obj:
                                calls.append({'rva':hex(expr.ea-base), 'target_rva':hex(expr.x.obj_ea-base),
                                    'target':ida_name.get_name(expr.x.obj_ea),
                                    'args':[ida_lines.tag_remove(arg.print1(None)) for arg in expr.a]})
                            return 0
                    CallVisitor().apply_to(cfunc.body, None)
                    file.with_suffix('.calls.json').write_text(json.dumps(calls,indent=2),encoding='utf-8')
                except Exception as exc: out.write('DECOMPILE ERROR: ' + str(exc) + '\n')
            out.write('\nDISASSEMBLY\n')
            for head in idautils.FuncItems(ea):
                out.write(f'{head-base:08X}  ' + ida_lines.tag_remove(idc.generate_disasm_line(head, 0)) + '\n')
        note('Written ' + file.name)
    idc.save_database(str(ROOT / 'ori2-input.i64'))
    note('Saved database')
except BaseException:
    note(traceback.format_exc())
finally:
    report.close()
    ida_pro.qexit(0)
