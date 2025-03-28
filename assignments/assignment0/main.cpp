#include <ew/external/glad.h>

#include <stdio.h>
#include <math.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <ew/shader.h>
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/cameraController.h>
#include <ew/transform.h>
#include <ew/texture.h>

#include "anim.h"
#include "kinematics.h"

GLFWwindow* initWindow(const char* title, int width, int height);
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void resetCamera(ew::Camera* camera, ew::CameraController* controller);
void drawUI();

void setupPlane(unsigned int& planeVBO, unsigned int& planeVAO);
void setupDepthMap(unsigned int& depthMapFBO, unsigned int& depthMap);
void renderQuad();
void renderScene(const ew::Shader& shader, ew::Model& monkeyModel);

void InitSkeleton(Skeleton& hierarchy);

struct Material {
	float Ka = 1.0;
	float Kd = 0.5;
	float Ks = 0.5;
	float Shininess = 128;
};

// Global state //
int screenWidth = 1080, screenHeight = 720;
float prevFrameTime, deltaTime;

ew::Camera camera;
ew::CameraController cameraController;
Material material;

const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
unsigned int planeVAO;

float lightPos[3] = { 1.0f, 5.0f, 1.0f };
float lightColor[3] = { 1.0f, 1.0f, 1.0f };
float maxBias = 0.05, minBias = 0.005;

//Animator animator;

const unsigned int NUM_OBJS = 8;
ew::Transform transforms[NUM_OBJS];
Skeleton skeleton;

int main() {
	GLFWwindow* window = initWindow("Assignment 4", screenWidth, screenHeight);
	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// OpenGL setup
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK); // Back face culling
	glEnable(GL_DEPTH_TEST); // Depth testing

	// camera setup
	camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.target = glm::vec3(0.0f, 0.0f, 0.0f); //Look at the center of the scene
	camera.aspectRatio = (float)screenWidth / screenHeight;
	camera.fov = 60.0f; // Vertical field of view, in degrees

	// shaders setup
	ew::Shader mainShader = ew::Shader("assets/shaders/lit.vert", "assets/shaders/lit.frag");
	ew::Shader simpleDepthShader = ew::Shader("assets/shaders/shadow.vert", "assets/shaders/shadow.frag");

	mainShader.use();
	mainShader.setInt("_DiffuseTexture", 0);
	mainShader.setInt("_ShadowMap", 1);

	// shadow map setup
	unsigned int depthMapFBO, depthMap;
	setupDepthMap(depthMapFBO, depthMap);

	// objects setup
	unsigned int planeVBO;
	setupPlane(planeVBO, planeVAO);

	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
	GLuint monkeyTexture = ew::loadTexture("assets/PavingStones143_1K-JPG_Color.jpg");
	glBindTextureUnit(0, monkeyTexture); // Bind brick texture to texture unit 0

	InitSkeleton(skeleton);

	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		// update camera (aspect ratio & position)
		camera.aspectRatio = (float)screenWidth / screenHeight; // it's not inside framebufferSizeCallback, but it'll do
		cameraController.move(window, &camera, deltaTime); // cam control before actually using camera for rendering

		// clear scene
		glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// update transforms based on skeleton
		SolveFK(skeleton);
		for(int i = 0; i < NUM_OBJS; i++) {
			JointPose pose = UndoTransformMatrix(skeleton.a_globalPoses[i]);
			transforms[i].scale = pose.scale;
			transforms[i].rotation = glm::quat(glm::radians(pose.rotation));
			transforms[i].position = pose.position;
		}

		// ----- 1) render depth of scene to texture (from light's perspective) ----- //

		/* code taken from the LearnOpenGL tutorial on Shadow Mapping
		* https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping */

		glm::vec3 lightPos(lightPos[0], lightPos[1], lightPos[2]);
		glm::mat4 lightProjection, lightView, lightSpaceMatrix;
		float near_plane = 1.0f, far_plane = 20.0f;
		lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
		lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
		lightSpaceMatrix = lightProjection * lightView;

		// render scene from light's point of view
		simpleDepthShader.use();
		simpleDepthShader.setMat4("_LightSpaceMatrix", lightSpaceMatrix);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, monkeyTexture);
		//glCullFace(GL_FRONT); // peter panning solution doesn't work for monkey model
		renderScene(simpleDepthShader, monkeyModel);
		//glCullFace(GL_BACK); // peter panning solution doesn't work for monkey model
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// reset viewport
		glViewport(0, 0, screenWidth, screenHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// ----- 2) render scene as normal using the generated depth/shadow map ----- //
		mainShader.use();
		mainShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

		// set light uniforms
		mainShader.setVec3("_EyePos", camera.position);
		mainShader.setVec3("_LightColor", glm::vec3(lightColor[0], lightColor[1], lightColor[2]));
		mainShader.setVec3("_LightPos", lightPos);
		mainShader.setMat4("_LightSpaceMatrix", lightSpaceMatrix);
		mainShader.setFloat("_MaxBias", maxBias);
		mainShader.setFloat("_MinBias", minBias);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, monkeyTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		renderScene(mainShader, monkeyModel);

		drawUI();

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	printf("\nShutting down...");
	return 0;
}

//--------------- assignment 6 functions --------------- //

void InitSkeleton(Skeleton& hierarchy) {
	// ---------- create arrays ---------- //
	hierarchy.numJoints = NUM_OBJS;
	hierarchy.a_joints = new Joint[NUM_OBJS];
	hierarchy.a_localPoses = new JointPose[NUM_OBJS];
	hierarchy.a_globalPoses = new glm::mat4[NUM_OBJS];

	// ---------- names & parents ---------- //
	hierarchy.a_joints[0].name = "Torso";
	hierarchy.a_joints[1].name = "Head";
	hierarchy.a_joints[2].name = "Left Shoulder";
	hierarchy.a_joints[3].name = "Left Arm";
	hierarchy.a_joints[4].name = "Left Hand";
	hierarchy.a_joints[5].name = "Right Shoulder";
	hierarchy.a_joints[6].name = "Right Arm";
	hierarchy.a_joints[7].name = "Right Hand";

	hierarchy.a_joints[1].parentIndex = 0; // Head parent: Torso
	hierarchy.a_joints[2].parentIndex = 0; // Left Shoulder parent: Torso
	hierarchy.a_joints[3].parentIndex = 2; // Left Arm parent: Left Shoulder
	hierarchy.a_joints[4].parentIndex = 3; // Left Hand parent: Left Arm
	hierarchy.a_joints[5].parentIndex = 0; // Right Shoulder parent: Torso
	hierarchy.a_joints[6].parentIndex = 5; // Right Arm parent: Right Shoulder
	hierarchy.a_joints[7].parentIndex = 6; // Right Hand parent: Right Arm
	
	// ---------- local poses ---------- //
	hierarchy.a_localPoses[0].scale = glm::vec3(1.0f, 1.0f, 1.0f);
	hierarchy.a_localPoses[0].rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	hierarchy.a_localPoses[0].position = glm::vec3(0.0f, 0.0f, 0.0f);

	hierarchy.a_localPoses[1].scale = glm::vec3(0.5f, 0.5f, 0.5f);
	hierarchy.a_localPoses[1].rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	hierarchy.a_localPoses[1].position = glm::vec3(0.0f, 2.0f, 0.0f);

	hierarchy.a_localPoses[2].scale = glm::vec3(0.5f, 0.5f, 0.5f);
	hierarchy.a_localPoses[2].rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	hierarchy.a_localPoses[2].position = glm::vec3(-2.0f, 0.0f, 0.0f);

	hierarchy.a_localPoses[3].scale = glm::vec3(0.5f, 0.5f, 0.5f);
	hierarchy.a_localPoses[3].rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	hierarchy.a_localPoses[3].position = glm::vec3(-1.0f, 0.0f, 0.0f);

	hierarchy.a_localPoses[4].scale = glm::vec3(0.5f, 0.5f, 0.5f);
	hierarchy.a_localPoses[4].rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	hierarchy.a_localPoses[4].position = glm::vec3(-0.5f, 0.0f, 0.0f);

	hierarchy.a_localPoses[5].scale = glm::vec3(0.5f, 0.5f, 0.5f);
	hierarchy.a_localPoses[5].rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	hierarchy.a_localPoses[5].position = glm::vec3(2.0f, 0.0f, 0.0f);

	hierarchy.a_localPoses[6].scale = glm::vec3(0.5f, 0.5f, 0.5f);
	hierarchy.a_localPoses[6].rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	hierarchy.a_localPoses[6].position = glm::vec3(1.0f, 0.0f, 0.0f);

	hierarchy.a_localPoses[7].scale = glm::vec3(0.5f, 0.5f, 0.5f);
	hierarchy.a_localPoses[7].rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	hierarchy.a_localPoses[7].position = glm::vec3(0.5f, 0.0f, 0.0f);
}

void renderScene(const ew::Shader& shader, ew::Model& monkeyModel) {
	// floor //
	glm::mat4 model = glm::mat4(1.0f);
	shader.setMat4("_Model", model);

	shader.setInt("_MainTex", 0);
	shader.setFloat("_Material.Ka", material.Ka);
	shader.setFloat("_Material.Kd", 0.5f);
	shader.setFloat("_Material.Ks", 0.0f);
	shader.setFloat("_Material.Shininess", 0.0f);

	glBindVertexArray(planeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// monkeys //
	//shader.setInt("_MainTex", 0);
	//shader.setFloat("_Material.Ka", material.Ka);
	shader.setFloat("_Material.Kd", material.Kd);
	shader.setFloat("_Material.Ks", material.Ks);
	shader.setFloat("_Material.Shininess", material.Shininess);

	for(int i = 0; i < NUM_OBJS; i++) {
		// transform.modelMatrix() combines translation, rotation, and scale into a 4x4 model matrix
		shader.setMat4("_Model", transforms[i].modelMatrix());
		monkeyModel.draw();
	}
}

void drawUI() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Skeleton");

	if(ImGui::CollapsingHeader("Torso")) {
		ImGui::DragFloat3("Scale##0", &skeleton.a_localPoses[0].scale[0], 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat3("Rotation##0", &skeleton.a_localPoses[0].rotation[0], 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat3("Position##0", &skeleton.a_localPoses[0].position[0], 0.01f);
	}

	if(ImGui::CollapsingHeader("Head")) {
		ImGui::DragFloat3("Scale##1", &skeleton.a_localPoses[1].scale[0], 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat3("Rotation##1", &skeleton.a_localPoses[1].rotation[0], 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat3("Position##1", &skeleton.a_localPoses[1].position[0], 0.01f);
	}

	if(ImGui::CollapsingHeader("Left Shoulder")) {
		ImGui::DragFloat3("Scale##2", &skeleton.a_localPoses[2].scale[0], 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat3("Rotation##2", &skeleton.a_localPoses[2].rotation[0], 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat3("Position##2", &skeleton.a_localPoses[2].position[0], 0.01f);
	}

	if(ImGui::CollapsingHeader("Left Arm")) {
		ImGui::DragFloat3("Scale##3", &skeleton.a_localPoses[3].scale[0], 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat3("Rotation##3", &skeleton.a_localPoses[3].rotation[0], 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat3("Position##3", &skeleton.a_localPoses[3].position[0], 0.01f);
	}

	if(ImGui::CollapsingHeader("Left Hand")) {
		ImGui::DragFloat3("Scale##4", &skeleton.a_localPoses[4].scale[0], 0.01f);
		ImGui::DragFloat3("Rotation##4", &skeleton.a_localPoses[4].rotation[0], 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat3("Position##4", &skeleton.a_localPoses[4].position[0], 0.01f);
	}

	if(ImGui::CollapsingHeader("Right Shoulder")) {
		ImGui::DragFloat3("Scale##5", &skeleton.a_localPoses[5].scale[0], 0.01f);
		ImGui::DragFloat3("Rotation##5", &skeleton.a_localPoses[5].rotation[0], 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat3("Position##5", &skeleton.a_localPoses[5].position[0], 0.01f);
	}

	if(ImGui::CollapsingHeader("Right Arm")) {
		ImGui::DragFloat3("Scale##6", &skeleton.a_localPoses[6].scale[0], 0.01f);
		ImGui::DragFloat3("Rotation##6", &skeleton.a_localPoses[6].rotation[0], 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat3("Position##6", &skeleton.a_localPoses[6].position[0], 0.01f);
	}

	if(ImGui::CollapsingHeader("Right Hand")) {
		ImGui::DragFloat3("Scale##7", &skeleton.a_localPoses[7].scale[0], 0.01f);
		ImGui::DragFloat3("Rotation##7", &skeleton.a_localPoses[7].rotation[0], 0.1f, -180.0f, 180.0f);
		ImGui::DragFloat3("Position##7", &skeleton.a_localPoses[7].position[0], 0.01f);
	}

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// --------------- assignment 2 functions --------------- //

/* code taken from the LearnOpenGL tutorial on Shadow Mapping
* https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping */
void setupPlane(unsigned int& planeVBO, unsigned int& planeVAO) {
	const float planeVertices[] = {
		// positions          // normals         // texcoords
		 5.0f, -1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 0.0f,
		-5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,  0.0f, 5.0f,
		-5.0f, -1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f,

		 5.0f, -1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 0.0f,
		 5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f,
		-5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,  0.0f, 5.0f
	};

	// plane VAO
	glGenVertexArrays(1, &planeVAO);
	glGenBuffers(1, &planeVBO);
	glBindVertexArray(planeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glBindVertexArray(0);
}

/* code taken from the LearnOpenGL tutorial on Shadow Mapping
* https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping */
void setupDepthMap(unsigned int& depthMapFBO, unsigned int& depthMap) {
	glGenFramebuffers(1, &depthMapFBO);
	glGenTextures(1, &depthMap);

	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
		SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* code taken from the LearnOpenGL tutorial on Shadow Mapping
* https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping */
void renderQuad() { // renders a 1x1 XY quad in NDC
	unsigned int quadVAO = 0, quadVBO;

	if(quadVAO == 0)
	{
		const float quadVertices[] = {
			// positions        // texture Coords
			-1.0f, 0.25f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			0.25f, 0.25f, 0.0f, 1.0f, 1.0f,
			0.25f, -1.0f, 0.0f, 1.0f, 0.0f,
		};

		// setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

// --------------- assignment 0 functions --------------- //

/// <summary>
/// Initializes GLFW, GLAD, and IMGUI
/// </summary>
/// <param name="title">Window title</param>
/// <param name="width">Window width</param>
/// <param name="height">Window height</param>
/// <returns>Returns window handle on success or null on fail</returns>
GLFWwindow* initWindow(const char* title, int width, int height) {
	printf("Initializing...");
	if(!glfwInit()) {
		printf("GLFW failed to init!");
		return nullptr;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
	if(window == NULL) {
		printf("GLFW failed to create window");
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	if(!gladLoadGL(glfwGetProcAddress)) {
		printf("GLAD Failed to load GL headers");
		return nullptr;
	}

	//Initialize ImGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	return window;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	screenWidth = width;
	screenHeight = height;
}

void resetCamera(ew::Camera* camera, ew::CameraController* controller) {
	camera->position = glm::vec3(0, 0, 5.0f);
	camera->target = glm::vec3(0);
	controller->yaw = controller->pitch = 0;
}
