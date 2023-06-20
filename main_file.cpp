/*
Niniejszy program jest wolnym oprogramowaniem; możesz go
rozprowadzać dalej i / lub modyfikować na warunkach Powszechnej
Licencji Publicznej GNU, wydanej przez Fundację Wolnego
Oprogramowania - według wersji 2 tej Licencji lub(według twojego
wyboru) którejś z późniejszych wersji.

Niniejszy program rozpowszechniany jest z nadzieją, iż będzie on
użyteczny - jednak BEZ JAKIEJKOLWIEK GWARANCJI, nawet domyślnej
gwarancji PRZYDATNOŚCI HANDLOWEJ albo PRZYDATNOŚCI DO OKREŚLONYCH
ZASTOSOWAŃ.W celu uzyskania bliższych informacji sięgnij do
Powszechnej Licencji Publicznej GNU.

Z pewnością wraz z niniejszym programem otrzymałeś też egzemplarz
Powszechnej Licencji Publicznej GNU(GNU General Public License);
jeśli nie - napisz do Free Software Foundation, Inc., 59 Temple
Place, Fifth Floor, Boston, MA  02110 - 1301  USA
*/

#define GLM_FORCE_RADIANS

#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "lodepng.h"
#include "shaderprogram.h"

const float PI = 3.141592653589793f;

float speed_x = 0;//[radiany/s]
float speed_y = 0;//[radiany/s]
float aspectRatio = 1;
float speedup = 1;
bool flash = true;

ShaderProgram* sp;

enum textureNames
{
	metalTex, metalTexSpec, woodTex, woodTexSpec, marbleTex, marbleTexSpec, plasticTex, plasticTexSpec, faceTex, faceTexSpec, goldTex, goldTexSpec
};

GLuint texGLuints[goldTexSpec+1];

struct Mesh {
	std::vector<glm::vec4> vertices;
	std::vector<glm::vec4> normals;
	std::vector<glm::vec2> texCoords;
	std::vector<unsigned int> indices;
	glm::vec3 origin;
	glm::vec3 bottomCenter;
};
std::vector<Mesh> mainPart; //główna część z zegarem - ma 6 elementów
std::vector<Mesh> gearPart; //trybiki
std::vector<Mesh> facePart; //tarcza, wskazówki i wahadło

bool surprise = false;

void loadModel(std::string filePath) {
	using namespace std;
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_GenBoundingBoxes);
	//cout << importer.GetErrorString() << endl;

	int maxMeshes = scene->mNumMeshes;
	std::cout << maxMeshes;
	//dla każdego mesha
	for (int j = 0; j < maxMeshes; j++) {
		Mesh tmp;
		aiMesh* mesh = scene->mMeshes[j];

		aiVector3D maxAABB = mesh->mAABB.mMax;
		aiVector3D minAABB = mesh->mAABB.mMin;

		tmp.bottomCenter= glm::vec3((minAABB.x + maxAABB.x) / 2, minAABB.y, 0.0f);
		tmp.origin = glm::vec3((maxAABB.x + minAABB.x) / 2, (maxAABB.y + minAABB.y) / 2, 0.0f);

		for (int i = 0; i < mesh->mNumVertices; i++) {
			aiVector3D vertex = mesh->mVertices[i]; //aiVector3D podobny do glm::vec3
			tmp.vertices.push_back(glm::vec4(vertex.x, vertex.y, vertex.z, 1));

			aiVector3D normal = mesh->mNormals[i]; //Wektory znormalizowane
			tmp.normals.push_back(glm::vec4(normal.x, normal.y, normal.z, 0));

			//0 to numer zestawu współrzędnych teksturowania 
			aiVector3D texCoord = mesh->mTextureCoords[0][i];
			tmp.texCoords.push_back(glm::vec2(texCoord.x, texCoord.y));
		}

		//dla każdego wielokąta składowego
		for (int i = 0; i < mesh->mNumFaces; i++) {
			aiFace& face = mesh->mFaces[i]; //face to jeden z wielokątów siatki
			//dla każdego indeksu->wierzchołka tworzącego wielokąt
			//dla aiProcess_Triangulate to zawsze będzie 3
			for (int j = 0; j < face.mNumIndices; j++) {
				//cout << face.mIndices[j] << " ";
				tmp.indices.push_back(face.mIndices[j]);
			}
		}
		//std::cout << std::endl << tmp.origin.x << " " << tmp.origin.y << " " << tmp.origin.z << std::endl;
		/*tmp.origin = glm::vec4((max.x + min.x) / 2, (max.y + min.y) / 2, (max.z + min.z) / 2, 0);*/
		mainPart.push_back(tmp);
	}
}

//Procedura obsługi błędów
void error_callback(int error, const char* description) {
	fputs(description, stderr);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_LEFT) speed_x = -PI / 2;
		if (key == GLFW_KEY_RIGHT) speed_x = PI / 2;
		if (key == GLFW_KEY_UP) speed_y = PI / 2;
		if (key == GLFW_KEY_DOWN) speed_y = -PI / 2;
		if (key == GLFW_KEY_E) speedup = 2;
		if (key == GLFW_KEY_R) speedup = 0.5;
		if (key == GLFW_KEY_M) surprise = true;
	}
	if (action == GLFW_RELEASE) {
		if (key == GLFW_KEY_LEFT) speed_x = 0;
		if (key == GLFW_KEY_RIGHT) speed_x = 0;
		if (key == GLFW_KEY_UP) speed_y = 0;
		if (key == GLFW_KEY_DOWN) speed_y = 0;
		if (key == GLFW_KEY_E) speedup = 1;
		if (key == GLFW_KEY_R) speedup = 1;
		if (key == GLFW_KEY_M) surprise = false;
		if (key == GLFW_KEY_F) flash = !flash;
	}
}

GLuint readTexture(const char* filename) {
	GLuint tex;
	glActiveTexture(GL_TEXTURE0);

	//Wczytanie do pamięci komputera
	std::vector<unsigned char> image;   //Alokuj wektor do wczytania obrazka
	unsigned width, height;   //Zmienne do których wczytamy wymiary obrazka
	//Wczytaj obrazek
	unsigned error = lodepng::decode(image, width, height, filename);

	//Import do pamięci karty graficznej
	glGenTextures(1, &tex); //Zainicjuj jeden uchwyt
	glBindTexture(GL_TEXTURE_2D, tex); //Uaktywnij uchwyt
	//Wczytaj obrazek do pamięci KG skojarzonej z uchwytem
	glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, (unsigned char*)image.data());

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return tex;
}

void windowResizeCallback(GLFWwindow* window, int width, int height) {
	if (height == 0) return;
	aspectRatio = (float)width / (float)height;
	glViewport(0, 0, width, height);
}

//Procedura inicjująca
void initOpenGLProgram(GLFWwindow* window) {
	glClearColor(0, 0, 0, 1);
	glEnable(GL_DEPTH_TEST);
	glfwSetWindowSizeCallback(window, windowResizeCallback);
	glfwSetKeyCallback(window, keyCallback);

	sp = new ShaderProgram("v_simplest.glsl", NULL, "f_simplest.glsl");

	texGLuints[metalTex] = readTexture("res/metal.png");
	texGLuints[metalTexSpec] = readTexture("res/metal_spec.png");
	texGLuints[woodTex] = readTexture("res/french_wood.png");
	texGLuints[woodTexSpec] = readTexture("res/french_wood_specular.png");
	texGLuints[marbleTex] = readTexture("res/marbles.png");
	texGLuints[marbleTexSpec] = readTexture("res/marbles_specular.png");
	texGLuints[plasticTex] = readTexture("res/plastic.png");
	texGLuints[plasticTexSpec] = readTexture("res/plastic_spec.png");
	texGLuints[faceTex] = readTexture("res/face.png");
	texGLuints[faceTexSpec] = readTexture("res/face_spec.png");
	texGLuints[goldTex] = readTexture("res/gold.png");
	texGLuints[goldTexSpec] = readTexture("res/gold_spec.png");
	loadModel("res/ZegarFinal.obj");
}


//Zwolnienie zasobów zajętych przez program
void freeOpenGLProgram(GLFWwindow* window) {
	delete sp;
	//************Tutaj umieszczaj kod, który należy wykonać po zakończeniu pętli głównej************
	glDeleteTextures(faceTexSpec+1, texGLuints);
}

//aka texKostka
void drawMesh(
	glm::mat4 P, glm::mat4 V, glm::mat4 M, int meshNum, textureNames texClear, textureNames texSpec, float exponent, float specular) {
	sp->use();//Aktywacja programu cieniującego
	glUniformMatrix4fv(sp->u("P"), 1, false, glm::value_ptr(P)); //Załaduj do programu cieniującego macierz rzutowania
	glUniformMatrix4fv(sp->u("V"), 1, false, glm::value_ptr(V)); //Załaduj do programu cieniującego macierz widoku
	glUniformMatrix4fv(sp->u("M"), 1, false, glm::value_ptr(M)); //Załaduj do programu cieniującego macierz modelu

	glEnableVertexAttribArray(sp->a("vertex"));
	glVertexAttribPointer(sp->a("vertex"), 4, GL_FLOAT, false, 0, mainPart[meshNum].vertices.data()); //Współrzędne wierzchołków bierz z tablicy myCubeVertices

	glEnableVertexAttribArray(sp->a("texCoord0"));
	glVertexAttribPointer(sp->a("texCoord0"), 2, GL_FLOAT, false, 0, mainPart[meshNum].texCoords.data()); //Współrzędne teksturowania bierz z tablicy myCubeTexCoords

	glEnableVertexAttribArray(sp->a("normal"));
	glVertexAttribPointer(sp->a("normal"), 4, GL_FLOAT, false, 0, mainPart[meshNum].normals.data()); //Wektory normalne bierz z tablicy myCubeNormals

	std::vector<float> params;
	params.push_back(exponent);
	params.push_back(specular);

	glUniform1i(sp->u("textureMap0"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texGLuints[texClear]);

	glUniform1i(sp->u("textureMap1"), 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, texGLuints[texSpec]);

	glUniform1f(sp->u("exponent"), exponent);
	glUniform1f(sp->u("specular"), specular);
	if (flash) glUniform4f(sp->u("col2"), 0.9, 1.0, 1.0, 1.0);
	else glUniform4f(sp->u("col2"), 0.0, 0.0, 0.0, 0.0);
	if (!surprise) glUniform4f(sp->u("col1"), 1.0, 1.0, 0.95, 1.0);
	else glUniform4f(sp->u("col1"), 1.0, 0.3, 0.7, 1.0);

	//glDrawArrays(GL_TRIANGLES, 0, myCubeVertexCount);
	glDrawElements(GL_TRIANGLES, mainPart[meshNum].indices.size(), GL_UNSIGNED_INT, mainPart[meshNum].indices.data());

	glDisableVertexAttribArray(sp->a("vertex"));
	glDisableVertexAttribArray(sp->a("texCoord0"));
	glDisableVertexAttribArray(sp->a("normal"));

}

void drawMainPart(glm::mat4 P, glm::mat4 V, glm::mat4 M) {
	drawMesh(P, V, M, 0, marbleTex, marbleTexSpec, 70, 0.7);
	drawMesh(P, V, M, 1, woodTex, woodTexSpec, 70, 0.3);
	drawMesh(P, V, M, 2, woodTex, woodTexSpec, 70, 0.3);
	drawMesh(P, V, M, 3, woodTex, woodTexSpec, 70, 0.3);
	drawMesh(P, V, M, 4, woodTex, woodTexSpec, 70, 0.3);
	drawMesh(P, V, M, 5, woodTex, woodTexSpec, 70, 0.3);
}

void drawAndRotateGears(glm::mat4 P, glm::mat4 V, glm::mat4 M, float angle) {
	glm::mat4 Mc = glm::translate(M, mainPart[6].origin);
	Mc = glm::rotate(Mc, angle, glm::vec3(0.0f, 0.0f, 1.0f));
	Mc = glm::translate(Mc, -mainPart[6].origin);
	drawMesh(P, V, Mc, 6, metalTex, metalTexSpec, 50, 0.9);

	Mc = glm::translate(M, mainPart[7].origin);
	Mc = glm::rotate(Mc, angle * 2, glm::vec3(0.0f, 0.0f, 1.0f));
	Mc = glm::translate(Mc, -mainPart[7].origin);
	drawMesh(P, V, Mc, 7, metalTex, metalTexSpec, 50, 0.9);

	Mc = glm::translate(M, mainPart[8].origin);
	Mc = glm::rotate(Mc, -angle * 4, glm::vec3(0.0f, 0.0f, 1.0f));
	Mc = glm::translate(Mc, -mainPart[8].origin);
	drawMesh(P, V, Mc, 8, metalTex, metalTexSpec, 50, 0.9);

	Mc = glm::translate(M, mainPart[9].origin);
	Mc = glm::rotate(Mc, angle, glm::vec3(0.0f, 0.0f, 1.0f));
	Mc = glm::translate(Mc, -mainPart[9].origin);
	drawMesh(P, V, Mc, 9, metalTex, metalTexSpec, 50, 0.9);

	Mc = glm::translate(M, mainPart[10].origin);
	Mc = glm::rotate(Mc, angle * 4, glm::vec3(0.0f, 0.0f, 1.0f));
	Mc = glm::translate(Mc, -mainPart[10].origin);
	drawMesh(P, V, Mc, 10, metalTex, metalTexSpec, 50, 0.9);

	Mc = glm::translate(M, mainPart[11].origin);
	Mc = glm::rotate(Mc, angle * 8, glm::vec3(0.0f, 0.0f, 1.0f));
	Mc = glm::translate(Mc, -mainPart[11].origin);
	drawMesh(P, V, Mc, 11, metalTex, metalTexSpec, 50, 0.9);

	Mc = glm::translate(M, mainPart[12].origin);
	Mc = glm::rotate(Mc, -angle * 4, glm::vec3(0.0f, 0.0f, 1.0f));
	Mc = glm::translate(Mc, -mainPart[12].origin);
	drawMesh(P, V, Mc, 12, metalTex, metalTexSpec, 50, 0.9);

}

void drawAndRotateFaceElements(glm::mat4 P, glm::mat4 V, glm::mat4 M, float angle) {
	drawMesh(P, V, M, 13, goldTex, goldTexSpec, 50, 0.9);	// Pendulum2
	drawMesh(P, V, M, 16, plasticTex, plasticTexSpec, 70, 0.8);	// Frame
	drawMesh(P, V, M, 17, faceTex, faceTexSpec, 70, 0.3); // Face
	drawMesh(P, V, M, 18, plasticTex, plasticTexSpec, 70, 0.8); // cylinder
	drawMesh(P, V, M, 19, goldTex, goldTexSpec, 50, 0.9);  // Pendulum1

	// prawa godzin
	glm::mat4 Mf = glm::translate(M, mainPart[14].bottomCenter);
	Mf = glm::rotate(Mf, angle/2, glm::vec3(0.0f, 0.0f, 1.0f));
	Mf = glm::translate(Mf, -mainPart[14].bottomCenter);
	drawMesh(P, V, Mf, 14, plasticTex, plasticTexSpec, 70, 0.8);	// Minute Hand (left)

	Mf = glm::translate(M, mainPart[15].bottomCenter);
	Mf = glm::rotate(Mf, angle/24, glm::vec3(0.0f, 0.0f, 1.0f));
	Mf = glm::translate(Mf, -mainPart[15].bottomCenter);
	drawMesh(P, V, Mf, 15, plasticTex, plasticTexSpec, 70, 0.8);	// Minute Hand (left)
}

//Procedura rysująca zawartość sceny
void drawScene(GLFWwindow* window, float angle_x, float angle_y, float cogAngle) {
	//************Tutaj umieszczaj kod rysujący obraz******************l
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 V = glm::lookAt(
		glm::vec3(0, 0, -5.0),
		glm::vec3(0, 0, 0),
		glm::vec3(0.0f, 1.0f, 0.0f)); //Wylicz macierz widoku

	glm::mat4 P = glm::perspective(50.0f * PI / 180.0f, aspectRatio, 0.01f, 50.0f); //Wylicz macierz rzutowania

	glm::mat4 M = glm::mat4(1.0f);
	M = glm::rotate(M, angle_y, glm::vec3(1.0f, 0.0f, 0.0f)); //Wylicz macierz modelu
	M = glm::rotate(M, angle_x, glm::vec3(0.0f, 1.0f, 0.0f)); //Wylicz macierz modelu

	//drawMesh(P, V, M, 0, metalTex, metalTexSpec);
	drawMainPart(P, V, M);
	drawAndRotateGears(P, V, M, cogAngle);
	drawAndRotateFaceElements(P, V, M, cogAngle);

	glfwSwapBuffers(window); //Skopiuj bufor tylny do bufora przedniego
}


int main(void)
{
	GLFWwindow* window; //Wskaźnik na obiekt reprezentujący okno

	glfwSetErrorCallback(error_callback);//Zarejestruj procedurę obsługi błędów

	if (!glfwInit()) { //Zainicjuj bibliotekę GLFW
		fprintf(stderr, "Nie można zainicjować GLFW.\n");
		exit(EXIT_FAILURE);
	}

	window = glfwCreateWindow(1000, 1000, "OpenGL", NULL, NULL);  //Utwórz okno 500x500 o tytule "OpenGL" i kontekst OpenGL.

	if (!window) //Jeżeli okna nie udało się utworzyć, to zamknij program
	{
		fprintf(stderr, "Nie można utworzyć okna.\n");
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window); //Od tego momentu kontekst okna staje się aktywny i polecenia OpenGL będą dotyczyć właśnie jego.
	glfwSwapInterval(1); //Czekaj na 1 powrót plamki przed pokazaniem ukrytego bufora

	if (glewInit() != GLEW_OK) { //Zainicjuj bibliotekę GLEW
		fprintf(stderr, "Nie można zainicjować GLEW.\n");
		exit(EXIT_FAILURE);
	}

	initOpenGLProgram(window); //Operacje inicjujące

	//Główna pętla
	float angle_x = 0; //zadeklaruj zmienną przechowującą aktualny kąt obrotu
	float angle_y = 0; //zadeklaruj zmienną przechowującą aktualny kąt obrotu
	float cogAngle = 0;
	glfwSetTime(0); //Wyzeruj licznik czasu
	while (!glfwWindowShouldClose(window)) //Tak długo jak okno nie powinno zostać zamknięte
	{	
		cogAngle += -PI / 6 * glfwGetTime() * speedup;
		angle_x += speed_x * glfwGetTime(); //Oblicz kąt o jaki obiekt obrócił się podczas poprzedniej klatki
		angle_y += speed_y * glfwGetTime(); //Oblicz kąt o jaki obiekt obrócił się podczas poprzedniej klatki
		glfwSetTime(0); //Wyzeruj licznik czasu
		drawScene(window, angle_x, angle_y, cogAngle); //Wykonaj procedurę rysującą
		glfwPollEvents(); //Wykonaj procedury callback w zalezności od zdarzeń jakie zaszły.
	}

	freeOpenGLProgram(window);

	glfwDestroyWindow(window); //Usuń kontekst OpenGL i okno
	glfwTerminate(); //Zwolnij zasoby zajęte przez GLFW
	exit(EXIT_SUCCESS);
}
