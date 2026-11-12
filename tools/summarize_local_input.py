"""Summarize the local IDA exports; never uses web binding tables.

Requires tools/ida_input_evidence.py and the local AssetRipper JSON extracts.
The default branch RVA interval is specific to Steam Build 5845748 and was
checked in AddControllerControls disassembly (including its shared tail).
"""
from pathlib import Path
import json, re, struct

ROOT = Path(__file__).resolve().parents[1] / 'build/local-evidence'
code = (ROOT/'ida/PlayerInput__AddControllerControls.txt').read_text(encoding='utf-8').split('DISASSEMBLY')[0]
dump = (ROOT/'il2cpp/dump.cs').read_text(encoding='utf-8-sig')
# Hex-Rays reuses locals in the legacy else branch. Do not merge those aliases
# into the selected default branch: doing so would falsely merge L3 with Y.
prefix, remainder = code.split('if ( v181->static_fields->USE_NEW_BINDINGS_TEST )',1)
active = remainder.split('\n  else\n',1)[0]
state_code = prefix + active
parents = {}
def root(v):
    parents.setdefault(v,v)
    if parents[v] != v: parents[v] = root(parents[v])
    return parents[v]
def merge(a,b): parents[root(a)] = root(b)
for a,b in re.findall(r'^\s*(v\d+) = (?:\([^\n;]+\)\s*)?(v\d+);$',state_code,re.M): merge(a,b)

def enum(name):
    part = dump.split('public enum '+name+' //')[1].split('}\n',1)[0]
    return {int(value):key for key,value in re.findall(r'public const [\w.]+ (\w+) = (-?\d+);',part)}
button_names = enum('XboxControllerInput.Button')
axis_names = enum('XboxControllerInput.Axis')
kind = None
objects = {}
for line in state_code.splitlines():
    hit = re.search(r'= SmartInput_(ControllerAxisInput|ControllerButtonInput|AxisButtonInput)_TypeInfo;',line)
    if hit: kind = hit[1]
    hit = re.search(r'(v\d+)\[6\] = (\d+);',line) or re.search(r'\*\(_DWORD \*\)\((v\d+) \+ 24\) = (\d+);',line)
    if hit and kind in ['ControllerAxisInput','ControllerButtonInput']:
        objects[root(hit[1])] = {'kind':kind,'id':int(hit[2])}
    hit = re.search(r'\*\(\(_QWORD \*\)(v\d+) \+ 3\) = (v\d+);',line)
    if hit and kind == 'AxisButtonInput': objects.setdefault(root(hit[1]),{'kind':kind})['axis_object'] = root(hit[2])
    hit = re.search(r'(v\d+)\[([89])\] = (-?\d+);',line)
    if hit and kind == 'AxisButtonInput': objects.setdefault(root(hit[1]),{'kind':kind})['mode' if hit[2]=='8' else 'threshold_bits'] = int(hit[3])

def source(v):
    obj = objects[root(v)]
    if obj['kind'] == 'ControllerButtonInput': return {'button':button_names[obj['id']], 'enum':obj['id'], 'object_witness':v}
    if obj['kind'] == 'AxisButtonInput':
        axis = objects[obj['axis_object']]
        threshold = struct.unpack('<f',struct.pack('<I',obj['threshold_bits'] & 0xffffffff))[0]
        return {'axis':axis_names[axis['id']], 'axis_enum':axis['id'], 'mode_enum':obj['mode'], 'threshold':threshold, 'object_witness':v}
    raise ValueError(obj)

aliases = dict(re.findall(r'^\s*(\w+) = self->(\w+);',state_code,re.M))
calls = json.loads((ROOT/'ida/PlayerInput__AddControllerControls.calls.json').read_text())
awake = (ROOT/'ida/PlayerInput__Awake.txt').read_text(encoding='utf-8').split('DISASSEMBLY')[0]
awake_default = awake.split('USE_NEW_BINDINGS_TEST )',1)[1].split('\n  else\n',1)[0]
edges = []
for call in calls:
    if call['target'] != 'SmartInput_CompoundButtonInput__Add': continue
    ea = int(call['rva'],16)
    if not (0x14132ca <= ea <= 0x1413934): continue
    field, obj = call['args'][:2]
    try: physical = source(obj)
    except (KeyError, ValueError) as exc: raise RuntimeError((call,exc))
    name = aliases.get(field,field)
    edge = {'default_mode':'debug/raw' if ea < 0x1413458 else 'common command',
        'player_input_field':name, 'add_call_rva':call['rva'], 'source':physical}
    if ea >= 0x1413458:
        edge['found_in_default_awake_wiring'] = re.search(r'\b'+re.escape(name)+r'\b',awake_default) is not None
    edges.append(edge)
for field in ['MapFocusOri','MapFocusObjective']:
    assert next(e for e in edges if e['player_input_field']==field)['source']['enum']==8
(ROOT/'native-bindings.json').write_text(json.dumps({'build':'5845748','method_rva':'0x1411a70', 'edges':edges},indent=2),encoding='utf-8')
for edge in edges:
    print(edge['add_call_rva'],edge['player_input_field'],edge['source'])
