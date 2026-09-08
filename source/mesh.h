#pragma once
#include <3ds.h>
#include <citro3d.h>

typedef struct {
	float x, y, z;
	float r, g, b, a;
} MeshVertex;

typedef struct {
	MeshVertex* data;
	int count;
	void* vbo;
	bool loaded;
} Mesh;

bool meshLoadOBJ(const char* path, Mesh* mesh, float r, float g, float b);
bool meshLoadCube(Mesh* mesh, float r, float g, float b);
void meshFree(Mesh* mesh);
