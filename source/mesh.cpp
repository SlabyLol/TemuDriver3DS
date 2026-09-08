#include "mesh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

struct V3 { float x, y, z; float r, g, b, a; bool hasCol; };

static bool meshFromTris(Mesh* mesh, const std::vector<MeshVertex>& tris) {
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

bool meshLoadCube(Mesh* mesh, float cr, float cg, float cb) {
	memset(mesh, 0, sizeof(Mesh));
	static const float V[8][3] = {
		{-1,-0.5f,-2},{1,-0.5f,-2},{1,0.8f,-2},{-1,0.8f,-2},
		{-1,-0.5f, 2},{1,-0.5f, 2},{1,0.8f, 2},{-1,0.8f, 2}
	};
	static const int F[12][3] = {
		{0,1,2},{0,2,3},{1,5,6},{1,6,2},{5,4,7},{5,7,6},
		{4,0,3},{4,3,7},{3,2,6},{3,6,7},{4,5,1},{4,1,0}
	};
	std::vector<MeshVertex> tris; tris.reserve(36);
	for (int i = 0; i < 12; i++) for (int k = 0; k < 3; k++) {
		const float* p = V[F[i][k]];
		MeshVertex mv; mv.x=p[0]; mv.y=p[1]; mv.z=p[2];
		mv.r=cr; mv.g=cg; mv.b=cb; mv.a=1.f; tris.push_back(mv);
	}
	return meshFromTris(mesh, tris);
}

bool meshLoadOBJ(const char* path, Mesh* mesh, float cr, float cg, float cb) {
	memset(mesh, 0, sizeof(Mesh));
	FILE* f = fopen(path, "rb");
	if (!f) {
		const char* slash = strrchr(path, '/');
		if (slash) f = fopen(slash + 1, "rb");
	}
	if (!f) return meshLoadCube(mesh, cr, cg, cb);

	std::vector<V3> pos; pos.reserve(2048);
	std::vector<MeshVertex> tris; tris.reserve(8192);
	float r = cr, g = cg, b = cb, a = 1.0f;

	char line[1024];
	while (fgets(line, sizeof(line), f)) {
		if (line[0]=='v' && line[1]==' ') {
			V3 p; p.hasCol=false; p.r=cr; p.g=cg; p.b=cb; p.a=1.f;
			float c0=0,c1=0,c2=0;
			int n = sscanf(line+2, "%f %f %f %f %f %f", &p.x,&p.y,&p.z,&c0,&c1,&c2);
			if (n >= 3) {
				if (n >= 6) { p.r=c0; p.g=c1; p.b=c2; p.hasCol=true; }
				pos.push_back(p);
			}
		} else if (strncmp(line,"usemtl",6)==0) {
			if (strstr(line,"glass")) { r=0.6f;g=0.8f;b=1.0f;a=0.85f; }
			else { r=cr;g=cg;b=cb;a=1.f; }
		} else if (line[0]=='f' && line[1]==' ') {
			int vi[32]; int n=0;
			char* p = line+2;
			while (*p && n<32) {
				while (*p==' ') p++;
				if (!*p || *p=='\n' || *p=='\r') break;
				int v=0,t=0,nrm=0;
				if (sscanf(p,"%d/%d/%d",&v,&t,&nrm)>=1) {}
				else if (sscanf(p,"%d//%d",&v,&nrm)>=1) {}
				else if (sscanf(p,"%d/%d",&v,&t)>=1) {}
				else if (sscanf(p,"%d",&v)!=1) break;
				vi[n++]=v;
				while (*p && *p!=' ' && *p!='\n' && *p!='\r') p++;
			}
			for (int i=1; i+1<n; i++) {
				int ids[3]={vi[0],vi[i],vi[i+1]};
				for (int k=0;k<3;k++) {
					int id=ids[k];
					if (id<0) id=(int)pos.size()+id+1;
					if (id<1||id>(int)pos.size()) continue;
					V3& pp=pos[id-1];
					MeshVertex mv;
					mv.x=pp.x; mv.y=pp.y; mv.z=pp.z;
					if (pp.hasCol) { mv.r=pp.r; mv.g=pp.g; mv.b=pp.b; mv.a=1.f; }
					else { mv.r=r; mv.g=g; mv.b=b; mv.a=a; }
					tris.push_back(mv);
				}
			}
		}
	}
	fclose(f);
	if (tris.empty()) return meshLoadCube(mesh, cr, cg, cb);
	return meshFromTris(mesh, tris);
}

void meshFree(Mesh* mesh) {
	if (mesh && mesh->vbo) { linearFree(mesh->vbo); mesh->vbo=nullptr; }
	if (mesh) { mesh->loaded=false; mesh->count=0; mesh->data=nullptr; }
}
