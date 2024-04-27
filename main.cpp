#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "shaderHelper.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

/* Features/Plan:
	- Camera movement in sphere around center of matrix
	- Basic simulation functionality
	- Randomized starting positions and count of voxels
	- GUI menu to customize and control simulation
	- Performance improvements (multi-threading/compute shaders)
*/

/* Notes/To-do:
	- Camera needs to be centered and always looking at origin
	- GL_STATIC_DRAW is currently being used to draw voxels. This may need to change later when they start moving.
	- May need to reset offsets VBO after every simulation tick, this could get expensive quickly. (look into glBufferSubData())
*/

float cameraDistance = 20;
float cameraRotation = 0;

const int voxelCount = 8;
const int xSimulationSize = 5;
const int ySimulationSize = 5;
const int zSimulationSize = 5;

bool pPressed = false;

int main()
{
	//Function prototypes:
	void framebuffer_size_callback(GLFWwindow * window, int width, int height);
	void mouseScrollCallback(GLFWwindow * window, double xOffset, double yOffset);
	void processInput(GLFWwindow * window);
	void fillOffsetsArray(bool voxelMatrix[xSimulationSize][ySimulationSize][zSimulationSize], float(&offsetArray)[voxelCount * 3]);

	//Setup GLFW and glad:
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(800, 600, "HelloTriangle", NULL, NULL);
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

	//For more complex cellular automota, a struct/object may need to be used instead of bools
	//but for now, a bool will represent a voxel there
	bool voxelMatrix[xSimulationSize][ySimulationSize][zSimulationSize] = { false };

	//Top face corners
	voxelMatrix[0][0][0] = true;
	voxelMatrix[0][4][0] = true;
	voxelMatrix[0][0][4] = true;
	voxelMatrix[0][4][4] = true;
	//Bottom face corners
	voxelMatrix[4][0][0] = true;
	voxelMatrix[4][4][0] = true;
	voxelMatrix[4][0][4] = true;
	voxelMatrix[4][4][4] = true;

	//Eventually put this code in render loop, but for now it's static
	//In render loop, update voxel matrix with simulation logic just before this

	//Data in this array is tightly packed.
	float offsetArray[voxelCount * 3];
	fillOffsetsArray(voxelMatrix, offsetArray);


#pragma region

	//Define matrices used by the vertex shader for rendering:

	//local-to-world identity matrix
	glm::mat4 modelToWorld = glm::mat4(1.0f);
	//Add rotation
	modelToWorld = glm::rotate(modelToWorld, glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//view identity matrix
	glm::mat4 view = glm::mat4(1.0f);
	// Move the "camera" up 1 units on z axis
	view = glm::translate(view, glm::vec3(-5.0f, 0.0f, -20.0f));

	//perspective project matrix with fov 45, aspect ration 4:3, near and far plane 0.1 and 100 
	glm::mat4 projection;
	projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

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
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * voxelCount * 3, &offsetArray[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glVertexAttribDivisor(1, 1);
	glEnableVertexAttribArray(1);

#pragma endregion Make Draw Elements

	//Render loop:
	//Simulation variables
	float currentCameraZoom = 1;

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
		//Change camera z position
		view[3][2] = cos(-cameraRotation) * -cameraDistance;
		//Change camera y position
		view[3][0] = sin(-cameraRotation) * -cameraDistance;
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

		//Update Simulation
		if (pPressed)
		{

		}

		//Update offset array (instanced array)
		fillOffsetsArray(voxelMatrix, offsetArray);
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

void fillOffsetsArray(bool voxelMatrix[xSimulationSize][ySimulationSize][zSimulationSize], float(&offsetArray)[voxelCount * 3])
{
	const float voxelSpacing = 2;
	int voxelsDrawn = 0;

	for (int i = 0; i < xSimulationSize; i++)
	{
		for (int j = 0; j < ySimulationSize; j++)
		{
			for (int k = 0; k < zSimulationSize; k++)
			{
				if (voxelMatrix[i][j][k])
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
	float maxZoomFar = 100;
	if (yOffset > 0 && cameraDistance > maxZoomClose)
	{
		//Scroll up (zoom in)
		cameraDistance -= zoomSensitivity;
	}
	else if (yOffset < 0 && cameraDistance < maxZoomFar)
	{
		//Scroll down (zoom out)
		cameraDistance += zoomSensitivity;
	}
}

void processInput(GLFWwindow* window)
{
	float rotationSensitivity = 0.01;

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		cameraRotation += rotationSensitivity;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		cameraRotation -= rotationSensitivity;
	}

	if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
	{
		pPressed = true;
	}
	else pPressed = false;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}