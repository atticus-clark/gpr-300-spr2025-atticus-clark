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

GLFWwindow* initWindow(const char* title, int width, int height);
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void resetCamera(ew::Camera* camera, ew::CameraController* controller);
void drawUI();

void setupPlane(unsigned int& planeVBO, unsigned int& planeVAO);
void setupDepthMap(unsigned int& depthMapFBO, unsigned int& depthMap);
void renderQuad();
void renderScene(const ew::Shader& shader, ew::Model& monkeyModel);

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
ew::Transform monkeyTransform;

float lightPos[3] = { 1.0f, 5.0f, 1.0f };
float lightColor[3] = { 1.0f, 1.0f, 1.0f };
float maxBias = 0.05, minBias = 0.005;

Animator animator;

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
	ew::Shader debugDepthQuad = ew::Shader("assets/shaders/debugDepthQuad.vert", "assets/shaders/debugDepthQuad.frag");

	mainShader.use();
	mainShader.setInt("_DiffuseTexture", 0);
	mainShader.setInt("_ShadowMap", 1);
	debugDepthQuad.use();
	debugDepthQuad.setInt("_DepthMap", 0);

	// shadow map setup
	unsigned int depthMapFBO, depthMap;
	setupDepthMap(depthMapFBO, depthMap);

	// objects setup
	unsigned int planeVBO;
	setupPlane(planeVBO, planeVAO);

	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
	// Handles to OpenGL object are unsigned integers
	GLuint monkeyTexture = ew::loadTexture("assets/PavingStones143_1K-JPG_Color.jpg");
		//GLuint brickTexture = ew::loadTexture("assets/brick_color.jpg");

	// animation setup
	animator.clip = new AnimationClip;
	animator.target = &monkeyTransform;

	// render loop //
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		// update camera (aspect ratio & position)
		camera.aspectRatio = (float)screenWidth / screenHeight; // it's not inside framebufferSizeCallback, but it'll do
		cameraController.move(window, &camera, deltaTime); // cam control before actually using camera for anything

		// play animation clip
		animator.PlayClip(deltaTime);
		// Rotate model around Y axis
		//monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));

		// Bind brick texture to texture unit 0
		glBindTextureUnit(0, monkeyTexture);

		// clear scene
		glClearColor(0.6f, 0.8f, 0.92f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

		/*
		// render depth map to quad for visual debugging
		debugDepthQuad.use();
		debugDepthQuad.setFloat("_NearPlane", near_plane);
		debugDepthQuad.setFloat("_FarPlane", far_plane);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		renderQuad();
		*/

		drawUI();

		glfwSwapBuffers(window);
	}

	delete animator.clip;
	//delete animator.target; // can't delete monkeyTransform (not created with new)
	glfwTerminate();
	printf("\nShutting down...");
	return 0;
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

	// monkey //
	// transform.modelMatrix() combines translation, rotation, and scale into a 4x4 model matrix
	shader.setMat4("_Model", monkeyTransform.modelMatrix());

	shader.setInt("_MainTex", 0);
	shader.setFloat("_Material.Ka", material.Ka);
	shader.setFloat("_Material.Kd", material.Kd);
	shader.setFloat("_Material.Ks", material.Ks);
	shader.setFloat("_Material.Shininess", material.Shininess);

	monkeyModel.draw();
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

void drawUI() {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");

	if(ImGui::Button("Reset Camera")) { resetCamera(&camera, &cameraController); }
	if(ImGui::CollapsingHeader("Material")) {
		ImGui::SliderFloat("AmbientK", &material.Ka, 0.0f, 1.0f);
		ImGui::SliderFloat("DiffuseK", &material.Kd, 0.0f, 1.0f);
		ImGui::SliderFloat("SpecularK", &material.Ks, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 1024.0f);
	}
	if(ImGui::CollapsingHeader("Directional Light")) {
		ImGui::ColorEdit3("Color", lightColor);
		ImGui::SliderFloat("Light Pos X", &lightPos[0], -5.0f, 5.0f);
		ImGui::SliderFloat("Light Pos Y", &lightPos[1], 3.0f, 10.0f);
		ImGui::SliderFloat("Light Pos Z", &lightPos[2], -5.0f, 5.0f);
	}
	if(ImGui::CollapsingHeader("Shadow")) {
		ImGui::SliderFloat("Max Bias", &maxBias, 0.05, 0.2);
		ImGui::SliderFloat("Min Bias", &minBias, 0.001, maxBias);
	}

	// animation stuff (assignment 4)
	animator.clip->EnsureAscendingTimes();

	if(ImGui::CollapsingHeader("Animation")) {
		ImGui::Checkbox("Playing", &animator.isPlaying);
		ImGui::Checkbox("Looping", &animator.isLooping);
		ImGui::DragFloat("Playback Speed", &animator.playbackSpeed, 0.01f);
		ImGui::SliderFloat("Playback Time", &animator.playbackTime, 0.0f, animator.clip->duration);
		ImGui::DragFloat("Clip Duration", &animator.clip->duration, 0.01f, 0.0f, 60.0f);
			// 1 minute max clip duration for no particular reason except
			// that DragFloat min doesn't work if i don't specify a max
	}

	int id = 0;
	if(ImGui::CollapsingHeader("Position Keyframes")) {
		for(int i = 0; i < animator.clip->posKeys.size(); i++) {
			ImGui::PushID(id);
			id++;

			std::string text = "Position " + std::to_string(i + 1);
			ImGui::Text(text.data());
			ImGui::SliderFloat("Time", &animator.clip->posKeys[i].time, 0.0f, animator.clip->duration);
			ImGui::DragFloat3("Values", &animator.clip->posKeys[i].values.x, 0.01f);
			// TODO: dropdown (ImGui::Combo) for easing function (extra credit)

			ImGui::PopID();
		}

		if(ImGui::Button("Add keyframe##-1")) { animator.clip->AddKeyframe(POS); }
		if(ImGui::Button("Remove last keyframe##-1")) { animator.clip->RemoveLastKeyframe(POS); }
	}
	if(ImGui::CollapsingHeader("Rotation Keyframes")) {
		for(int i = 0; i < animator.clip->rotKeys.size(); i++) {
			ImGui::PushID(id);
			id++;

			std::string text = "Rotation " + std::to_string(i + 1);
			ImGui::Text(text.data());
			ImGui::SliderFloat("Time", &animator.clip->rotKeys[i].time, 0.0f, animator.clip->duration);
			ImGui::DragFloat3("Values", &animator.clip->rotKeys[i].values.x, 0.01f);
			// TODO: dropdown (ImGui::Combo) for easing function (extra credit)

			ImGui::PopID();
		}

		if(ImGui::Button("Add keyframe##-2")) { animator.clip->AddKeyframe(ROT); }
		if(ImGui::Button("Remove last keyframe##-2")) { animator.clip->RemoveLastKeyframe(ROT); }
	}
	if(ImGui::CollapsingHeader("Scale Keyframes")) {
		for(int i = 0; i < animator.clip->scaKeys.size(); i++) {
			ImGui::PushID(id);
			id++;

			std::string text = "Scale " + std::to_string(i + 1);
			ImGui::Text(text.data());
			ImGui::SliderFloat("Time", &animator.clip->scaKeys[i].time, 0.0f, animator.clip->duration);
			ImGui::DragFloat3("Values", &animator.clip->scaKeys[i].values.x, 0.01f);
			// TODO: dropdown (ImGui::Combo) for easing function (extra credit)

			ImGui::PopID();
		}

		if(ImGui::Button("Add keyframe##-3")) { animator.clip->AddKeyframe(SCA); }
		if(ImGui::Button("Remove last keyframe##-3")) { animator.clip->RemoveLastKeyframe(SCA); }
	}

	ImGui::Begin("Shadow Map");
	//Using a Child allow to fill all the space of the window.
	ImGui::BeginChild("Shadow Map");
	//Stretch image to be window size
	ImVec2 windowSize = ImGui::GetWindowSize();
	//Invert 0-1 V to flip vertically for ImGui display
	//shadowMap is the texture2D handle
	ImGui::Image((ImTextureID)planeVAO, windowSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::EndChild();
	ImGui::End();

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}