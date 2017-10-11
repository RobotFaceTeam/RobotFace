//standard C++
#include <stdio.h>
#include <stdlib.h>

//assimp
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/version.h>

//OpenGL
#include <GL/glut.h>

#define MIN(x, y) x < y ? x : y
#define MAX(x, y) x > y ? x : y

const aiScene* scene = NULL;
GLuint scene_list = 0;
aiVector3D scene_min, scene_max, scene_center;

float axisRot[3] = { 0, -90, 0 };//model rotation offset
float axisVel[3] = { 0 };

void versions() {
	printf("\tC/C++ %s\n", __VERSION__);
	printf("\tOpenGL %s\n", glGetString(GL_VERSION));
	printf("\tassimp %d.%d.%d\n", aiGetVersionMajor(),
		aiGetVersionMinor(),
		aiGetVersionRevision());
}

/*
 * HELPERS
 */
void doTiming() {
	//fps
	static GLint prev_time = 0;
	static int frames = 0;

	int time = glutGet(GLUT_ELAPSED_TIME);

	if ((time - prev_time) > 1000) {
		printf("%d fps\n", frames);
		frames = 0;
		prev_time = time;
	}

	//non-normalized movement
	axisRot[0] += axisVel[0] * .5;
	axisRot[1] += axisVel[1] * .5;
	axisRot[2] += axisVel[2] * .5;

	//finalize
	glutPostRedisplay();

	frames++;
}

void color4_to_float4(aiColor4D* c, float f[4]) {
	f[0] = c->r;
	f[1] = c->g;
	f[2] = c->b;
	f[3] = c->a;
}

void setFloat4(float f[4], float a, float b, float c, float d) {
	f[0] = a;
	f[1] = b;
	f[2] = c;
	f[3] = d;
}

void drawAxis() {
	bool restore = glIsEnabled(GL_LIGHTING);
	glDisable(GL_LIGHTING);

	glLineWidth(3);
	glBegin(GL_LINES);

	glColor3f(1, 0, 0);
	glVertex3f(-10, 0, 0);
	glVertex3f(10, 0, 0);

	glColor3f(0, 1, 0);
	glVertex3f(0, -10, 0);
	glVertex3f(0, 10, 0);

	glColor3f(0, 0, 1);
	glVertex3f(0, 0, -10);
	glVertex3f(0, 0, 10);

	glEnd();

	if (restore) {
		glEnable(GL_LIGHTING);
	}
}

void rotateAxis() {
	float sum = axisRot[0] + axisRot[1] + axisRot[2];
	float norm[3] = { axisRot[0] / sum, axisRot[1] / sum, axisRot[2] / sum };
	glRotatef(axisRot[0], 1, 0, 0);
	glRotatef(axisRot[1], 0, 1, 0);
	glRotatef(axisRot[2], 0, 0, 1);
}

/*
 * RECURSIVELY LOADING MODEL
 */
void get_bbox_node(aiNode* node, aiVector3D* min, aiVector3D* max, aiMatrix4x4* trafo) {
	aiMatrix4x4 prev;

	prev = *trafo;
	aiMultiplyMatrix4(trafo, &node->mTransformation);

	unsigned int n;
	for (n = 0; n < node->mNumMeshes; n++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[n]];
		for (unsigned int t = 0; t < mesh->mNumVertices; t++) {
			aiVector3D tmp = mesh->mVertices[t];
			aiTransformVecByMatrix4(&tmp, trafo);

			min->x = MIN(min->x, tmp.x);
			min->y = MIN(min->y, tmp.y);
			min->z = MIN(min->z, tmp.z);

			max->x = MAX(max->x, tmp.x);
			max->y = MAX(max->y, tmp.y);
			max->z = MAX(max->z, tmp.z);
		}
	}

	for (n = 0; n < node->mNumChildren; n++) {
		get_bbox_node(node->mChildren[n], min, max, trafo);
	}
	*trafo = prev;
}

void get_bb(aiVector3D* min, aiVector3D* max) {
	aiMatrix4x4 trafo;
	aiIdentityMatrix4(&trafo);

	min->x = min->y = min->z = 1e10f;
	max->x = max->y = max->z = -1e10f;
	get_bbox_node(scene->mRootNode, min, max, &trafo);
}

void applyMaterial(aiMaterial* mat) {
	float c[4];

	//properties
	aiColor4D diffuse, specular, ambient, emission;
	ai_real shininess, strength;

	setFloat4(c, 0.8f, 0.8f, 0.8f, 1.0f);
	if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse)
		== AI_SUCCESS) {
		color4_to_float4(&diffuse, c);
	}
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, c);

	setFloat4(c, 0.0f, 0.0f, 0.0f, 1.0f);
	if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &specular)
		== AI_SUCCESS) {
		color4_to_float4(&specular, c);
	}
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);

	setFloat4(c, 0.2f, 0.2f, 0.2f, 1.0f);
	if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_AMBIENT, &ambient)
		== AI_SUCCESS) {
		color4_to_float4(&ambient, c);
	}
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, c);

	setFloat4(c, 0.0f, 0.0f, 0.0f, 1.0f);
	if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &emission)
		== AI_SUCCESS) {
		color4_to_float4(&emission, c);
	}
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, c);

	//advanced properties
	unsigned int max;
	if (aiGetMaterialFloatArray(mat, AI_MATKEY_SHININESS, &shininess, &max)
		== AI_SUCCESS) {
		max = 1;
		if (aiGetMaterialFloatArray(mat, AI_MATKEY_SHININESS_STRENGTH, &strength, &max)
			== AI_SUCCESS) {
			glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess * shininess);
		} else {
			glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
		}
	} else {
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0);
		setFloat4(c, 0, 0, 0, 0);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);
	}

	//wireframe
	GLenum fillMode;
	int wireframe;
	max = 1;
	if (aiGetMaterialIntegerArray(mat, AI_MATKEY_ENABLE_WIREFRAME, &wireframe, &max)
		== AI_SUCCESS) {
		fillMode = wireframe ? GL_LINE : GL_FILL;
	} else {
		fillMode = GL_FILL;
	}
	glPolygonMode(GL_FRONT_AND_BACK, fillMode);

	//two sided rendering
	int twoSided;
	max = 1;
	if (aiGetMaterialIntegerArray(mat, AI_MATKEY_TWOSIDED, &twoSided, &max)
		== AI_SUCCESS && twoSided) {
		glDisable(GL_CULL_FACE);
	} else {
		glEnable(GL_CULL_FACE);
	}
}

void r_render(const aiScene* scene, const aiNode* node) {
	aiMatrix4x4 m = node->mTransformation;

	aiTransposeMatrix4(&m);
	glPushMatrix();
	glMultMatrixf((float*) &m);

	unsigned int n;
	for (n = 0; n < node->mNumMeshes; n++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[n]];

		applyMaterial(scene->mMaterials[mesh->mMaterialIndex]);

		if (mesh->mNormals == NULL) {
			glDisable(GL_LIGHTING);
		} else {
			glEnable(GL_LIGHTING);
		}

		for (unsigned int t = 0; t < mesh->mNumFaces; t++) {
			aiFace* face = &mesh->mFaces[t];
			GLenum faceMode;

			switch (face->mNumIndices) {
				case 1: faceMode = GL_POINTS; break;
				case 2: faceMode = GL_LINES; break;
				case 3: faceMode = GL_TRIANGLES; break;
				default: faceMode = GL_POLYGON; break;
			
			}

			glBegin(faceMode);

			for (unsigned int i = 0; i < face->mNumIndices; i++) {
				int idx = face->mIndices[i];
				if (mesh->mColors[0] != NULL) {
					glColor4fv((GLfloat*) &mesh->mColors[0][idx]);
				}
				if (mesh->mNormals != NULL) {
					glNormal3fv(&mesh->mNormals[idx].x);
				}
				glVertex3fv(&mesh->mVertices[idx].x);
			}

			glEnd();
		}
	}

	for (n = 0; n < node->mNumChildren; n++) {
		r_render(scene, node->mChildren[n]);
	}

	glPopMatrix();
}

/*
 * CALLBACKS
 */
void display_callback() {
	float tmp;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0, 0, 3, 0, 0, 0, 0, 1, 2);

	//rotate dat model
	rotateAxis();

	//scale to fit camera
	tmp = scene_max.x - scene_min.x;
	tmp = MAX(scene_max.y - scene_min.y, tmp);
	tmp = MAX(scene_max.z - scene_min.z, tmp);
	tmp = 1 / tmp;
	glScalef(tmp, tmp, tmp);

	//center
	glTranslatef(-scene_center.x, -scene_center.y, -scene_center.z);

	if (scene_list == 0) {
		scene_list = glGenLists(1);
		glNewList(scene_list, GL_COMPILE);
		r_render(scene, scene->mRootNode);
		glEndList();
	}

	//render model + axis
	drawAxis();
	glCallList(scene_list);

	//finalize
	glutSwapBuffers();

	doTiming();
}

void reshape_callback(int width, int height) {
	double aspectRatio = (float) width / height;
	double FOV = 45;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(FOV, aspectRatio, 1, 1000);
	glViewport(0, 0, width, height);
}

void key_callback(unsigned char key, int, int) {
	switch (key) {
		case 'a':
		case 'A':
			axisVel[1] = -1;
			break;
		case 'd':
		case 'D':
			axisVel[1] = 1;
			break;
		case 'w':
		case 'W':
			axisVel[0] = -1;
			break;
		case 's':
		case 'S':
			axisVel[0] = 1;
			break;
		case 'q':
		case 'Q':
			axisVel[2] = 1;
			break;
		case 'e':
		case 'E':
			axisVel[2] = -1;
			break;
	}
}

void key_spec_callback(int key, int, int) {
	switch (key) {
		case GLUT_KEY_LEFT:
			axisVel[1] = -1;
			break;
		case GLUT_KEY_RIGHT:
			axisVel[1] = 1;
			break;
		case GLUT_KEY_UP:
			axisVel[0] = -1;
			break;
		case GLUT_KEY_DOWN:
			axisVel[0] = 1;
			break;
		case GLUT_KEY_PAGE_UP:
			axisVel[2] = 1;
			break;
		case GLUT_KEY_PAGE_DOWN:
			axisVel[2] = -1;
			break;
	}
}

void key_up_callback(unsigned char key, int, int) {
	switch (key) {
		case 'a':
		case 'A':
		case 'd':
		case 'D':
			axisVel[1] = 0;
			break;
		case 'w':
		case 'W':
		case 's':
		case 'S':
			axisVel[0] = 0;
			break;
		case 'q':
		case 'Q':
		case 'e':
		case 'E':
			axisVel[2] = 0;
			break;
	}
}

void key_spec_up_callback(int key, int, int) {
	switch (key) {
		case GLUT_KEY_LEFT:
		case GLUT_KEY_RIGHT:
			axisVel[1] = 0;
			break;
		case GLUT_KEY_UP:
		case GLUT_KEY_DOWN:
			axisVel[0] = 0;
			break;
		case GLUT_KEY_PAGE_UP:
		case GLUT_KEY_PAGE_DOWN:
			axisVel[2] = 0;
			break;
	}
}

/*
 * ASSIMP STUFF
 */
bool load(const char* path) {
	scene = aiImportFile(path,
		aiProcess_CalcTangentSpace       |
		aiProcess_Triangulate            |
		aiProcess_JoinIdenticalVertices  |
		aiProcess_SortByPType);

	if (scene) {
		get_bb(&scene_min, &scene_max);
		return true;
	}
	return false;
}

int main(int argc, char** argv) {
	setbuf(stdout, NULL);//flush after print, instead of newline

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInit(&argc, argv);

	glutSetKeyRepeat(0);
	glutInitWindowSize(1024, 768);
	glutInitWindowPosition((glutGet(GLUT_SCREEN_WIDTH) - 1024) / 2,
				(glutGet(GLUT_SCREEN_HEIGHT) - 768) / 2);
	glutCreateWindow("what a face");
	glutDisplayFunc(display_callback);
	glutReshapeFunc(reshape_callback);
	glutKeyboardFunc(key_callback);
	glutSpecialFunc(key_spec_callback);
	glutKeyboardUpFunc(key_up_callback);
	glutSpecialUpFunc(key_spec_up_callback);

	if (!load("res/pjanic.dae")) {
		printf("could load the goddamn file\n");
		return 0;
	}

	versions();

	glClearColor(0.2, 0.2, 0.2, 0.2);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);

	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	glEnable(GL_NORMALIZE);

	if (getenv("MODEL_IS_BROKEN")) {
		glFrontFace(GL_CW);
	}

	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);

	glutGet(GLUT_ELAPSED_TIME);
	glutMainLoop();

	aiReleaseImport(scene);
}
