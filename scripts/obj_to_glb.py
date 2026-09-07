#!/usr/bin/env python3
"""Minimal OBJ -> GLB for 3DS banner (node name COMMON)."""
import struct, json, sys
import numpy as np

obj_path, out_path = sys.argv[1], sys.argv[2]
verts, uvs, faces = [], [], []
with open(obj_path) as f:
    for line in f:
        if line.startswith('v '):
            verts.append(list(map(float, line.split()[1:4])))
        elif line.startswith('vt '):
            uvs.append(list(map(float, line.split()[1:3])))
        elif line.startswith('f '):
            idxs = []
            for p in line.split()[1:]:
                bits = p.split('/')
                vi = int(bits[0]) - 1
                ti = int(bits[1]) - 1 if len(bits) > 1 and bits[1] else 0
                idxs.append((vi, ti))
            for i in range(1, len(idxs)-1):
                faces.append([idxs[0], idxs[i], idxs[i+1]])

out_pos, out_uv, out_idx, key_map = [], [], [], {}
for tri in faces:
    for vi, ti in tri:
        key = (vi, ti)
        if key not in key_map:
            key_map[key] = len(out_pos)
            out_pos.append(verts[vi])
            out_uv.append(uvs[ti] if ti < len(uvs) else [0.0, 0.0])
        out_idx.append(key_map[key])

pos = np.array(out_pos, dtype=np.float32)
uv = np.array(out_uv, dtype=np.float32)
uv[:,1] = 1.0 - uv[:,1]
idx = np.array(out_idx, dtype=np.uint16)
center = pos.mean(axis=0)
pos -= center
scale = 1.5 / max(float(pos.max() - pos.min()), 0.001)
pos *= scale
pos[:,1] -= 0.2

def pad4_null(b):
    return b + b'\x00' * ((4 - len(b) % 4) % 4)
def pad4_spaces(b):
    return b + b' ' * ((4 - len(b) % 4) % 4)

pos_bin, uv_bin, idx_bin = pos.tobytes(), uv.tobytes(), idx.tobytes()
o_uv = len(pad4_null(pos_bin))
o_idx = o_uv + len(pad4_null(uv_bin))
bin_data = pad4_null(pos_bin) + pad4_null(uv_bin) + pad4_null(idx_bin)

gltf = {
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [{"mesh": 0, "name": "COMMON"}],
  "meshes": [{"name": "COMMON", "primitives": [{
      "attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "indices": 2, "mode": 4
  }]}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": len(pos), "type": "VEC3",
     "min": pos.min(axis=0).tolist(), "max": pos.max(axis=0).tolist()},
    {"bufferView": 1, "componentType": 5126, "count": len(uv), "type": "VEC2"},
    {"bufferView": 2, "componentType": 5123, "count": len(idx), "type": "SCALAR"}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": len(pos_bin), "target": 34962},
    {"buffer": 0, "byteOffset": o_uv, "byteLength": len(uv_bin), "target": 34962},
    {"buffer": 0, "byteOffset": o_idx, "byteLength": len(idx_bin), "target": 34963}
  ],
  "buffers": [{"byteLength": len(bin_data)}]
}
json_str = pad4_spaces(json.dumps(gltf, separators=(',', ':')).encode())
glb = b'glTF' + struct.pack('<II', 2, 12 + 8 + len(json_str) + 8 + len(bin_data))
glb += struct.pack('<I', len(json_str)) + b'JSON' + json_str
glb += struct.pack('<I', len(bin_data)) + b'BIN\x00' + bin_data
open(out_path, 'wb').write(glb)
print('Wrote', out_path, len(glb), 'bytes')
