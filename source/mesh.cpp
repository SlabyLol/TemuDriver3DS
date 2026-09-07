#include "mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

struct V3 { float x,y,z; };

bool meshLoadOBJ(const char* path, Mesh* mesh, float cr, float cg, float cb) {
	memset(mesh, 0, sizeof(Mesh));
	FILE* f = fopen(path, "rb");
	if (!f) return false;

	std::vector<V3> pos;
	std::vector<MeshVertex> tris;
	float r = cr, g = cg, b = cb, a = 1.0f;

	char line[512];
	while (fgets(line, sizeof(line), f)) {
		if (line[0]=='v' && line[1]==' ') {
			V3 p; if (sscanf(line+2,"%f %f %f",&p.x,&p.y,&p.z)==3) pos.push_back(p);
		} else if (strncmp(line,"usemtl",6)==0) {
			if (strstr(line,"glass")) { r=0.6f; g=0.8f; b=1.0f; a=0.7f; }
			else if (strstr(line,"light")) { r=1.0f; g=1.0f; b=0.7f; a=1.0f; }
			else if (strstr(line,"metallic")) { r=0.55f; g=0.55f; b=0.6f; a=1.0f; }
			else { r=cr; g=cg; b=cb; a=1.0f; }
		} else if (line[0]=='f' && line[1]==' ') {
			int vi[16]; int n=0;
			char* p = line+2;
			while (*p && n<16) {
				while (*p==' ') p++;
				if (!*p || *p=='\n') break;
				int v=0,t=0,nrm=0;
				if (sscanf(p,"%d/%d/%d",&v,&t,&nrm)>=1) {}
				else if (sscanf(p,"%d//%d",&v,&nrm)>=1) {}
				else if (sscanf(p,"%d/%d",&v,&t)>=1) {}
				else if (sscanf(p,"%d",&v)!=1) break;
				vi[n++] = v;
				while (*p && *p!=' ' && *p!='\n') p++;
			}
			for (int i=1; i+1<n; i++) {
				int ids[3] = {vi[0], vi[i], vi[i+1]};
				for (int k=0; k<3; k++) {
					int id = ids[k];
					if (id < 0) id = (int)pos.size() + id + 1;
					if (id < 1 || id > (int)pos.size()) continue;
					V3& pp = pos[id-1];
					MeshVertex mv;
					mv.x = pp.x; mv.y = pp.y; mv.z = pp.z;
					mv.r = r; mv.g = g; mv.b = b; mv.a = a;
					tris.push_back(mv);
				}
			}
		}
	}
	fclose(f);
	if (tris.empty()) return false;

	mesh->count = (int)tris.size();
	size_t bytes = mesh->count * sizeof(MeshVertex);
	mesh->vbo = linearAlloc(bytes);
	if (!mesh->vbo) return false;
	memcpy(mesh->vbo, tris.data(), bytes);
	mesh->data = (MeshVertex*)mesh->vbo;
	mesh->loaded = true;
	return true;
}

void meshFree(Mesh* mesh) {
	if (mesh && mesh->vbo) { linearFree(mesh->vbo); mesh->vbo = nullptr; }
	mesh->loaded = false; mesh->count = 0;
}
