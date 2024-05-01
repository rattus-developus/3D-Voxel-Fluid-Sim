#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "shaderHelper.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <random>

/* Features/Plan:
	- Basic simulation functionality
	- GUI menu to customize and control simulation
	- Performance improvements (multi-threading/compute shaders, occlusion culling, etc.)
*/

/* Notes/To-do:
	
	Notes:

	To-do:
	- Optimize (notes on phone)
*/


const double PI = 3.14159265;

//Input and Controls
float camDistance = 100;
float camRotHorizontal = PI / 6;
float camRotVertical = PI / 2.5f;
bool pPressed = false;
bool oPressed = false;

//Simulation details
const int voxelCount = 2500;
const int xSimulationSize = 50;
const int ySimulationSize = 50;
const int zSimulationSize = 50;
const float voxelSpacing = 2;

glm::mat4 projection;

struct vec3Int
{
	int x = 0;
	int y = 0;
	int z = 0;
	vec3Int();
	vec3Int(int _x, int _y, int _z)
	{
		x = _x;
		y = _y;
		z = _z;
	}
};
vec3Int::vec3Int() {}

struct voxelPosition
{
	bool containsVoxel = false;

	glm::vec3 velocity;

	int rdm = 0;

	voxelPosition();
};

voxelPosition::voxelPosition() {}

voxelPosition voxelMatrix[xSimulationSize][ySimulationSize][zSimulationSize];
float offsetArray[voxelCount * 3];


int main()
{
	//Function prototypes:
	void framebuffer_size_callback(GLFWwindow * window, int width, int height);
	void mouseScrollCallback(GLFWwindow * window, double xOffset, double yOffset);
	void processInput(GLFWwindow * window);
	void fillOffsetsArray();
	void updateVoxelMatrixRandom();
	void updateVoxelMatrixVelocity();
	void printVoxelMatrixCount();
	void fillMatrixRandom();

	//Setup GLFW and glad:
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(800, 600, "Voxels", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}
	glViewport(0, 0, 800, 600);
	//Input call
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetScrollCallback(window, mouseScrollCallback);

	//Setup shader:
	ShaderHelper defaultShader("defaultVertexShader.vert", "defaultFragmentShader.frag");
	defaultShader.use();

#pragma region

	//Define common voxel data:

	//Vertices
	float cubeLocalVertices[] = {
		//Bottom Plane Positions
		 -1.0,  -1.0, -1.0, //Far Left
		 1.0,  -1.0, -1.0,  //Close Left
		 -1.0,  1.0, -1.0,  //Far Right
		 1.0,  1.0, -1.0,   //Close Right
		 //Top Plane Positions
		 -1.0,  -1.0, 1.0,  //Far Left
		 1.0,  -1.0, 1.0,   //Close Left
		 -1.0,  1.0, 1.0,   //Far Right
		 1.0,  1.0, 1.0,    //Close Right
	};

	//Tris
	unsigned int cubeTriIndices[] = {
		//Bottom
		0, 1, 2,
		1, 2, 3,

		//Top
		4, 5, 6,
		5, 6, 7,

		//Left
		0, 1, 4,
		4, 5, 1,

		//Right
		2, 3, 7,
		7, 6, 2,

		//Far
		0, 2, 4,
		2, 4, 6,

		//Near
		1, 3, 5,
		3, 5, 7,
	};

#pragma endregion Shared Voxel Data

	//Data in this array is tightly packed.

	//fillMatrixRandom();
	
	for (int i = 0; i < xSimulationSize; i++)
	{
		for (int j = 0; j < ySimulationSize; j++)
		{
			voxelMatrix[i][j][0].containsVoxel = true;
		}
	}
	

	//fillOffsetsArray(voxelMatrix, offsetArray);


#pragma region

	//Define matrices used by the vertex shader for rendering:

	//local-to-world identity matrix
	glm::mat4 modelToWorld = glm::mat4(1.0f);
	//Add rotation
	modelToWorld = glm::rotate(modelToWorld, glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));


	//View identity matrix
	glm::mat4 view = glm::mat4(1.0f);
	float matrixCenterX = (xSimulationSize * voxelSpacing) / 2.0f;
	float matrixCenterY = (ySimulationSize * voxelSpacing) / 2.0f;
	float matrixCenterZ = (zSimulationSize * voxelSpacing) / 2.0f;

	//perspective project matrix with fov 45, aspect ration 4:3, near and far plane 0.1 and 1000
	projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 1000.0f);

	//Assign matrices to vertex shader
	int modelUniformLoc = glGetUniformLocation(defaultShader.ID, "modelToWorld");
	glUniformMatrix4fv(modelUniformLoc, 1, GL_FALSE, glm::value_ptr(modelToWorld));

	int viewLoc = glGetUniformLocation(defaultShader.ID, "view");
	glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

	int projectionLoc = glGetUniformLocation(defaultShader.ID, "projection");
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

#pragma endregion Vertex Shader Matrices

#pragma region

	//Setup draw elements/data:

	//Generate and bind VAO
	unsigned int VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	//Generate and bind VBO for voxel
	unsigned int VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeLocalVertices), cubeLocalVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	//Generate and bind EBO for voxel
	unsigned int EBO;
	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeTriIndices), cubeTriIndices, GL_STATIC_DRAW);

	//Create VBO for instanced offsets array
	unsigned int offsetVBO;
	glGenBuffers(1, &offsetVBO);
	glBindBuffer(GL_ARRAY_BUFFER, offsetVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * voxelCount * 3, &offsetArray[0], GL_DYNAMIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glVertexAttribDivisor(1, 1);
	glEnableVertexAttribArray(1);

#pragma endregion Make Draw Elements

	//Render loop:

	//Depth Testing
	glEnable(GL_DEPTH_TEST);
	//Vsync
	glfwSwapInterval(1);
	while (!glfwWindowShouldClose(window))
	{
		//Input and Events:
		processInput(window);
		glfwPollEvents();

		//Update Camera matrix:
		float camPosX = (camDistance * sin(camRotHorizontal) * sin(camRotVertical)) + matrixCenterX;
		float camPosY = (camDistance * cos(camRotVertical)) + matrixCenterY;
		float camPosZ = (camDistance * cos(camRotHorizontal) * sin(camRotVertical)) + matrixCenterZ;

		view = glm::lookAt(glm::vec3(camPosX, camPosY, camPosZ),     //Position
						   glm::vec3(matrixCenterX, matrixCenterY, matrixCenterZ),   //Target Position
						   glm::vec3(0.0f, 1.0f, 0.0f));							 //Up 
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

		//Update Simulation
		if (pPressed)
		{
			updateVoxelMatrixVelocity();
		}
		else if (oPressed)
		{
			updateVoxelMatrixRandom();
		}

		//Update offset array (instanced array)
		fillOffsetsArray();
		glBindBuffer(GL_ARRAY_BUFFER, offsetVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * voxelCount * 3, &offsetArray[0]);

		//Clear Screen and depth buffer:
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		//Draw objects
		glBindVertexArray(VAO);
		glDrawElementsInstanced(GL_TRIANGLES, voxelCount * 36, GL_UNSIGNED_INT, 0, voxelCount);

		//Events and Buffers:
		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}


//Randomly fills the voxel matrix with a voxelCount/matrixSize chance to generate a voxel at each location.
void fillMatrixRandom()
{
	int randomIntInRange(int min, int max);
	int voxelsSpawned = 0;

	int chosenX;
	int chosenY;
	int chosenZ;

	while (voxelsSpawned < voxelCount)
	{
		chosenX = randomIntInRange(0, xSimulationSize - 1);
		chosenY = randomIntInRange(0, ySimulationSize - 1);
		chosenZ = randomIntInRange(0, zSimulationSize - 1);

		if (!voxelMatrix[chosenX][chosenY][chosenZ].containsVoxel)
		{
			voxelMatrix[chosenX][chosenY][chosenZ].containsVoxel = true;
			voxelsSpawned++;
		}
	}
}

//Min and max inclusive
int randomIntInRange(int min, int max) 
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<int> dis(min, max);
	return dis(gen);
}

void swapVoxelPosition(vec3Int from, vec3Int to)
{
	voxelMatrix[to.x][to.y][to.z].containsVoxel = true;
	voxelMatrix[to.x][to.y][to.z].velocity = voxelMatrix[from.x][from.y][from.z].velocity;

	voxelMatrix[from.x][from.y][from.z] = voxelPosition();
}

//Performs a single simulation step on the voxel matrix
void updateVoxelMatrixVelocity()
{
	int gravity = 1.0f;
	vec3Int desiredPos;
	glm::vec3 vel;

	for (int i = 0; i < xSimulationSize; i++)
	{
		for (int j = 0; j < ySimulationSize; j++)
		{
			for (int k = 0; k < zSimulationSize; k++)
			{
				if (voxelMatrix[i][j][k].containsVoxel)
				{
					voxelMatrix[i][j][k].velocity.z += gravity;

					vel = voxelMatrix[i][j][k].velocity;
					desiredPos = vec3Int(vel.x + i, vel.y + j, vel.z + k);

					//If desired position is empty, move there
					if (!voxelMatrix[desiredPos.x][desiredPos.y][desiredPos.z].containsVoxel)
					{
						swapVoxelPosition(vec3Int(i, j, k), vec3Int(desiredPos.x, desiredPos.y, desiredPos.z));
					}
					//Else half velocity and flip
					else
					{
						voxelMatrix[i][j][k].velocity.x = -(vel.x / 2);
						voxelMatrix[i][j][k].velocity.y = -(vel.y / 2);
						voxelMatrix[i][j][k].velocity.z = -(vel.z / 2);
					}
				}
			}
		}
	}
}

//Performs a single simulation step on the voxel matrix
void updateVoxelMatrixRandom()
{
	int rdm = 0;

	for (int i = 0; i < xSimulationSize; i++)
	{
		for (int j = 0; j < ySimulationSize; j++)
		{
			for (int k = 0; k < zSimulationSize; k++)
			{
				if (voxelMatrix[i][j][k].containsVoxel)
				{
					//Pick random direction to start sampling +x, -x, +y, -y
					rdm = randomIntInRange(0, 3);

					//Move down if none beneath and not at floor
					if (j > 0 && !voxelMatrix[i][j - 1][k].containsVoxel)
					{
						swapVoxelPosition(vec3Int(i, j, k), vec3Int(i, j - 1, k));
					}
					//+x
					else if (rdm == 0)
					{
						if(i < xSimulationSize - 1 && !voxelMatrix[i + 1][j][k].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i + 1, j, k));

						//-x
						else if(i > 0 && !voxelMatrix[i - 1][j][k].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i - 1, j, k));
						//+y
						else if(k < zSimulationSize - 1 && !voxelMatrix[i][j][k + 1].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i, j, k + 1));
						//-y
						else if(k > 0 && !voxelMatrix[i][j][k - 1].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i, j, k - 1));
					}
					//-x
					else if (rdm == 1)
					{
						if(i > 0 && !voxelMatrix[i - 1][j][k].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i - 1, j, k));

						//+y
						else if (k < zSimulationSize - 1 && !voxelMatrix[i][j][k + 1].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i, j, k + 1));
						//-y
						else if (k > 0 && !voxelMatrix[i][j][k - 1].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i, j, k - 1));
						//+x
						else if (i < xSimulationSize - 1 && !voxelMatrix[i + 1][j][k].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i + 1, j, k));
					}
					//+y
					else if (rdm == 2)
					{
						if(k < zSimulationSize - 1 && !voxelMatrix[i][j][k + 1].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i, j, k + 1));

						//-y
						else if (k > 0 && !voxelMatrix[i][j][k - 1].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i, j, k - 1));
						//+x
						else if (i < xSimulationSize - 1 && !voxelMatrix[i + 1][j][k].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i + 1, j, k));
						//-x
						else if (i > 0 && !voxelMatrix[i - 1][j][k].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i - 1, j, k));
					}
					//-y
					else if (rdm == 3)
					{
						if(k > 0 && !voxelMatrix[i][j][k - 1].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i, j, k - 1));
						
						//+x
						else if (i < xSimulationSize - 1 && !voxelMatrix[i + 1][j][k].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i + 1, j, k));
						//-x
						else if (i > 0 && !voxelMatrix[i - 1][j][k].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i - 1, j, k));
						//+y
						else if (k < zSimulationSize - 1 && !voxelMatrix[i][j][k + 1].containsVoxel)
							swapVoxelPosition(vec3Int(i, j, k), vec3Int(i, j, k + 1));
					}
				}
			}
		}
	}
}

//Fills the offset instanced array with the offset for each voxel
void fillOffsetsArray()
{
	int voxelsDrawn = 0;

	for (int i = 0; i < xSimulationSize; i++)
	{
		for (int j = 0; j < ySimulationSize; j++)
		{
			for (int k = 0; k < zSimulationSize; k++)
			{
				if (voxelMatrix[i][j][k].containsVoxel)
				{
					offsetArray[3 * voxelsDrawn] = i * voxelSpacing;
					offsetArray[3 * voxelsDrawn + 1] = j * voxelSpacing;
					offsetArray[3 * voxelsDrawn + 2] = k * voxelSpacing;
					voxelsDrawn++;
				}
			}
		}
	}
}

void mouseScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
	float zoomSensitivity = 5;
	float maxZoomClose = 5;
	float maxZoomFar = 1500;
	if (yOffset > 0 && camDistance > maxZoomClose)
	{
		//Scroll up (zoom in)
		camDistance -= zoomSensitivity;
	}
	else if (yOffset < 0 && camDistance < maxZoomFar)
	{
		//Scroll down (zoom out)
		camDistance += zoomSensitivity;
	}
}

void processInput(GLFWwindow* window)
{
	float rotationSensitivity = 0.0125;

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		camRotHorizontal -= rotationSensitivity;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		camRotHorizontal += rotationSensitivity;
	}

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && camRotVertical > rotationSensitivity)
	{
		camRotVertical -= rotationSensitivity;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && camRotVertical < PI - rotationSensitivity)
	{
		camRotVertical += rotationSensitivity;
	}

	if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
	{
		pPressed = true;
	}
	else pPressed = false;

	if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
	{
		oPressed = true;
	}
	else oPressed = false;
}

//A callback for whenever the window is resized
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	//projection = glm::perspective(glm::radians(45.0f), 800.0 / 600.0, 0.1f, 1000.0f);
}