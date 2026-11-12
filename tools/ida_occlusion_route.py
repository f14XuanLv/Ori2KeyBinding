"""Read selected Ori 2 rendering functions in a project-owned IDA DB copy."""
import json, re, traceback
from pathlib import Path
import ida_auto, ida_funcs, ida_hexrays, ida_lines, ida_name, ida_nalt, ida_pro, idautils, idc

ROOT=Path(r'D:\CodeT2\Ori2\build\occlusion-research')
OUT=ROOT/'native'; OUT.mkdir(exist_ok=True)
selected={
 'LayeredRenderSettings$$GetLayerIndexForRenderer',
 'Moon.Rendering.MoonRenderPipelineView$$getCullBounds',
 'Moon.Rendering.MoonRenderPipelineView$$getDrawRendererSettings',
 'Moon.Rendering.MoonRenderPipelineView$$render_Sliced',
 'Moon.Rendering.MoonRenderPipelineView$$OnCullingCategoryResultsReady',
 'Moon.Rendering.MoonRenderPipelineView$$finishMRT',
 'Moon.Rendering.MoonRenderPipelineView$$GetCurrentAlphaDepthThreshold',
 'Moon.Rendering.MoonRenderContext$$DrawRenderer',
 'Moon.Rendering.MoonRenderContext$$DrawRenderers',
 'Moon.Rendering.RuntimeSliceRenderSettings$$Update',
 'Moon.Rendering.RuntimeSliceRenderSettings$$GenerateSlice',
 'Moon.Rendering.SliceRenderSettings$$.ctor',
}
log=(OUT/'progress.log').open('w',encoding='utf-8',buffering=1)
try:
 base=ida_nalt.get_imagebase(); ida_auto.enable_auto(True); ida_hexrays.init_hexrays_plugin()
 data=json.loads(Path(r'D:\CodeT2\Ori2\build\local-evidence\il2cpp\script.json').read_text(encoding='utf-8-sig'))
 names={}
 for m in data['ScriptMethod']: names.setdefault(base+m['Address'],m['Name'])
 output=[]
 for m in data['ScriptMethod']:
  if m['Name'] not in selected:continue
  ea=base+m['Address'];log.write(m['Name']+' '+hex(m['Address'])+'\n')
  f=ida_funcs.get_func(ea)
  if not f:ida_funcs.add_func(ea);f=ida_funcs.get_func(ea)
  if not f:continue
  ida_auto.plan_and_wait(f.start_ea,f.end_ea)
  ida_name.set_name(ea,re.sub(r'\W','_',m['Name']),ida_name.SN_NOWARN)
  for head in idautils.FuncItems(ea):
   for target in idautils.CodeRefsFrom(head,False):
    if target in names:ida_name.set_name(target,re.sub(r'\W','_',names[target]),ida_name.SN_NOWARN)
  ida_hexrays.mark_cfunc_dirty(ea,False)
  file=OUT/(re.sub(r'\W','_',m['Name'])+'_'+hex(m['Address'])+'.txt')
  row={'name':m['Name'],'rva':hex(m['Address']),'signature':m['Signature'],'calls':[]}
  with file.open('w',encoding='utf-8') as out:
   out.write(m['Name']+' RVA '+hex(m['Address'])+'\n'+m['Signature']+'\n')
   try:out.write(str(ida_hexrays.decompile(ea))+'\n')
   except Exception as ex:out.write('DECOMPILATION ERROR '+str(ex)+'\n')
   out.write('\nASM\n')
   for head in idautils.FuncItems(ea):
    line=ida_lines.tag_remove(idc.generate_disasm_line(head,0))
    if idc.print_insn_mnem(head) in ('call','jmp'):
     target=idc.get_operand_value(head,0)
     if target in names:
      row['calls'].append({'at':hex(head-base),'target':names[target],'targetRva':hex(target-base)})
      line+=' ; '+names[target]
    out.write(hex(head-base)+' '+line+'\n')
  output.append(row)
 (OUT/'method-index.json').write_text(json.dumps(output,indent=2),encoding='utf-8')
 log.write('Done\n')
except BaseException:log.write(traceback.format_exc())
finally:log.close();ida_pro.qexit(0)
