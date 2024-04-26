#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "shaderHelper.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

int main()
{
	//Function prototypes:
	void framebuffer_size_callback(GLFWwindow * window, int width, int height);
	void processInput(GLFWwindow * window);

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

	#pragma region

	//Define matrices used by the vertex shader for rendering:

	//local-to-world identity matrix
	glm::mat4 modelToWorld = glm::mat4(1.0f);
	//Add rotation
	modelToWorld = glm::rotate(modelToWorld, glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//view identity matrix
	glm::mat4 view = glm::mat4(1.0f);
	// Move the "camera" up 20 units on z axis
	view = glm::translate(view, glm::vec3(0.0f, 0.0f, -20.0f));

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

	//Generate and bind VBO
	unsigned int VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	//Assign vertex data to VBO
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeLocalVertices), cubeLocalVertices, GL_STATIC_DRAW);
	//Tell OpenGL how to read vertex data
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	//Enable the VBO with the vertex attribute location as its argument
	glEnableVertexAttribArray(0);
	//Generate and bind EBO
	unsigned int EBO;
	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	//Copy our indices to the EBO
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeTriIndices), cubeTriIndices, GL_STATIC_DRAW);

	//Unbind our objects
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	#pragma endregion Draw Elements

	//Render loop:

	//Depth Testing
	glEnable(GL_DEPTH_TEST);
	//Vsync
	glfwSwapInterval(1);
	while (!glfwWindowShouldClose(window))
	{
		//Input:
		processInput(window);

		//Add rotation to cube matrix
		modelToWorld = glm::rotate(modelToWorld, 0.01f, glm::vec3(0.5f, 1.0f, 0.0f));
		//Reassign cube matrix uniform
		glUniformMatrix4fv(modelUniformLoc, 1, GL_FALSE, glm::value_ptr(modelToWorld));

		//Clear Screen and depth buffer:
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		//Draw objects
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 999, GL_UNSIGNED_INT, 0);
		glBindVertexArray(VAO);

		//Events and Buffers:
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}