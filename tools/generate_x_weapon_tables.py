"""Compile address-only weapon descriptors into the native ROM extractor."""
import json
from pathlib import Path
import sys

entries = json.loads(Path(sys.argv[1]).read_text())
lines = ['// Generated from address-only metadata; contains no game assets.',
         'static const WeaponSource weapon_sources[] = {']
for e in entries:
    groups = []
    for g in e['groups']:
        allowed = sum(1 << p for p in g.get('poses', range(g['frames'])))
        inherited = sum(1 << p for p in g.get('inherited_poses', []))
        setup = g.get('setup_poses', [])
        groups.append('{%d,%d,0x%s,%d,0x%xULL,0x%xULL,%d,0x%s,%d,%d,%d,%d,0x%s}' %
                      (g['group'], g['frames'], g['dma'], bool('poses' in g),
                       allowed if 'poses' in g else 0, inherited, setup[0] if setup else -1,
                       g.get('palette', e['weapon_palette']),g.get('source_group',g['group']),g.get('animated_frame_start',0),
                       g.get('animated_layout_start',0),g.get('animated_layout_count',0),g.get('animated_dma_table','0')))
    extra = e.get('additional_dma', ['0'])
    lines.append('  {%d,%d,0x%s,0x%s,0x%s,%d,%d,%d,{%s}},' %
                 (e['game'], e['weapon'], e['body_palette'], e['weapon_palette'],
                  extra[0], len(groups), e.get('static_resource', -1), e.get('static_offset', 0), ','.join(groups)))
lines.append('};')
Path(sys.argv[2]).write_text('\n'.join(lines) + '\n')
