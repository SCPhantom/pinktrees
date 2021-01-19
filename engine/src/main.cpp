#include <sstream>

#include "engine.h"
#include "skybox.h"

using namespace engine;

class MyApp : public App
{
	SceneGraph* sceneGraph = new SceneGraph();

	SceneNode* showcaseObjectNode;

	Mesh* quad = MeshFactory::createQuad();

	// camera
	Camera* camera = new Camera(0);
	bool catchCursor = false;
	Vector2 lastCursorPos;

	Skybox* skybox = new Skybox(camera);

	// for debugging
	TextureCubemap* environmentMap, *irradianceMap, *prefilterMap;

	ShaderProgram* geoProgram;
	ShaderProgram* lightProgram;
	ShaderProgram* bloomSeparationProgram;
	ShaderProgram* dofProgram;
	ShaderProgram* horizontalBlurProgram;
	ShaderProgram* vertikalBlurProgram;
	ShaderProgram* bloomProgram;
	ShaderProgram* reflectionsProgram;
	ShaderProgram* fastBoxBlurProgram;
	ShaderProgram* ssaoProgram;
	ShaderProgram* reflectionBlendProgram;

	float bloomExposure = 0.2f;
	bool useBloom = false;
	int bloomBlur = 5;
	float bloomThreshold = 0.2f;

	bool useDOF = false;
	float focalDepth = 2.0f;
	int dofSamples = 25;

	bool useSsr = false;
	bool useSsao = false;

	float maxRayDistance = 25;
	float stepResolution = 0.4f;
	int stepIterations = 400;
	float tolerance = 0.1f;

	float ambientRadius = 0.5f;
	float ambientBias = 0.025f;

	bool showDemoWindow = false;
	bool showGbufferContent = false;

	unsigned int selectedModel = 0;
	Model* models[7];

	GBuffer gbuffer;
	ShadedBuffer shadedBuffer;
	BloomBuffer bloomBuffer;
	PingPongBuffer pingPongBuffer;
	ReflectionsBuffer reflectionsBuffer;
	BlurBuffer blurBuffer;
	SsaoBuffer ssaoBuffer;
	ReflectionsBlendBuffer reflectionsBlendBuffer;
	std::vector<Light> lights;

	void updateProjection()
	{
		float aspectRatio = engine.windowWidth / (float)engine.windowHeight;
		// M_PI / 3 is aproximately 60 degrees FOV
		camera->setPerspective((float)M_PI / 3.f, aspectRatio, 0.1f, 100.f);
	}

	#pragma region Callbacks
	void windowCloseCallback() override
	{
		engine.stop();

		delete sceneGraph;
		delete quad;
		delete skybox;
		delete camera;
	}

	void windowSizeCallback(int newWidth, int newHeight)
	{
		engine.windowWidth = newWidth;
		engine.windowHeight = newHeight;
		gbuffer.deleteBufferData();
		gbuffer.initialize(newWidth, newHeight);
		shadedBuffer.deleteBufferData();
		shadedBuffer.initialize(newWidth, newHeight);
		bloomBuffer.deleteBufferData();
		bloomBuffer.initialize(newWidth, newHeight);
		pingPongBuffer.deleteBufferData();
		pingPongBuffer.initialize(newWidth, newHeight);
		reflectionsBuffer.deleteBufferData();
		reflectionsBuffer.initialize(newWidth, newHeight);
		blurBuffer.deleteBufferData();
		blurBuffer.initialize(newWidth, newHeight);
		ssaoBuffer.deleteBufferData();
		ssaoBuffer.initialize(newWidth, newHeight);
		reflectionsBlendBuffer.deleteBufferData();
		reflectionsBlendBuffer.initialize(newWidth, newHeight);
		glViewport(0, 0, newWidth, newHeight);
		updateProjection();
	}

	void keyCallback(int key, int scancode, int action, int mods) override
	{
		// escape key to close window
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) windowCloseCallback();

		// show gbuffer content
		if (key == GLFW_KEY_X && action == GLFW_PRESS) showGbufferContent = !showGbufferContent;

		// model switching
		if (key == GLFW_KEY_1 && action == GLFW_PRESS) selectedModel = 0;
		if (key == GLFW_KEY_2 && action == GLFW_PRESS) selectedModel = 1;
		if (key == GLFW_KEY_3 && action == GLFW_PRESS) selectedModel = 2;
		if (key == GLFW_KEY_4 && action == GLFW_PRESS) selectedModel = 3;
		showcaseObjectNode->setDrawable(models[selectedModel]);

		// press I to toggle the ImGui debug window
		if (key == GLFW_KEY_I && action == GLFW_PRESS) showDemoWindow = !showDemoWindow; 
	}

	void mouseButtonCallback(int button, int action, int mods) override
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		{
			catchCursor = !catchCursor;
			glfwSetInputMode(window, GLFW_CURSOR, catchCursor ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
			lastCursorPos = engine.getCursorPos();
		}
	}

	void mouseScrollCallback(double xoffset, double yoffset) override
	{
		focalDepth += 0.5f * (float)yoffset;
	}
#pragma endregion

	void start() override
	{
		models[0] = new Model("assets/models/lantern/lantern.obj");
		models[1] = new Model("assets/models/sphere/sphere.obj");
		models[2] = new Model("assets/models/teapot/teapot.obj");
		models[3] = new Model("assets/models/car/car.obj");
		models[4] = new Model("assets/models/tree/tree.obj");
		//models[5] = new Model("assets/models/tree2/tree2.obj");
		models[6] = new Model("assets/models/ground/ground.obj");

		lights.push_back(Light(Vector3(2.f, 3.f, 2.f), Vector3(1.f, 1.f, 1.f), 15.f));
		lights.push_back(Light(Vector3(-2.f, 3.f, -2.f), Vector3(1.f, 1.f, 1.f), 15.f));

		SceneNode* root = sceneGraph->getRoot();
		root->setDrawable(models[6]); // assign ground to root

		SceneNode* tree1 = root->createNode();
		tree1->setDrawable(models[4]);
		tree1->setMatrix(Matrix4::CreateTranslation(-7, 0, 12));
		SceneNode* tree2 = root->createNode();
		tree2->setDrawable(models[4]);
		tree2->setMatrix(Matrix4::CreateTranslation(-5, 0, -9));
		SceneNode* tree3 = root->createNode();
		tree3->setDrawable(models[4]);
		tree3->setMatrix(Matrix4::CreateTranslation(7, 0, 16));
		SceneNode* tree4 = root->createNode();
		tree4->setDrawable(models[4]);
		tree4->setMatrix(Matrix4::CreateTranslation(5.123, 0, -3.456));
		SceneNode* tree5 = root->createNode();
		tree5->setDrawable(models[4]);
		tree5->setMatrix(Matrix4::CreateTranslation(-20, 0, -15));
		SceneNode* tree6 = root->createNode();
		tree6->setDrawable(models[4]);
		tree6->setMatrix(Matrix4::CreateTranslation(-18, 0, 10));
		SceneNode* tree7 = root->createNode();
		tree7->setDrawable(models[4]);
		tree7->setMatrix(Matrix4::CreateTranslation(15, 0, 13));
		SceneNode* tree8 = root->createNode();
		tree8->setDrawable(models[4]);
		tree8->setMatrix(Matrix4::CreateTranslation(17, 0, -12));

		showcaseObjectNode= root->createNode();
		showcaseObjectNode->setDrawable(models[selectedModel]);
		showcaseObjectNode->setMatrix(Matrix4::CreateTranslation(0, 2, 0));

		camera->lookAt(Vector3(5, 2, 0), Vector3(0, 2, 0));

		updateProjection();

		//skybox->loadCubemapFromDiskSingleFiles("assets/cubemaps/palermo");
		skybox->loadCubemapFromDiskHDR("assets/hdris/kiaradawn4k.hdr");
		environmentMap = skybox->getCubemap();

		irradianceMap = new TextureCubemap();
		irradianceMap->convoluteIrradianceMapFromCubemap(skybox->getCubemap());
		TextureInfo* irradianceMapInfo = new TextureInfo(GL_TEXTURE8, "irradianceMap", irradianceMap, nullptr);

		prefilterMap = new TextureCubemap();
		prefilterMap->convolutePrefilterMapFromCubemap(skybox->getCubemap());
		TextureInfo* prefilterMapInfo = new TextureInfo(GL_TEXTURE9, "prefilterMap", prefilterMap, nullptr);

		Texture2D* brdfLUT = new Texture2D();
		brdfLUT->createBRDFLookupTexture();
		TextureInfo* brdfLUTinfo = new TextureInfo(GL_TEXTURE10, "brdfLUT", brdfLUT, nullptr);

		gbuffer.initialize(engine.windowWidth, engine.windowHeight);
		shadedBuffer.initialize(engine.windowWidth, engine.windowHeight);
		bloomBuffer.initialize(engine.windowWidth, engine.windowHeight);
		pingPongBuffer.initialize(engine.windowWidth, engine.windowHeight);
		reflectionsBuffer.initialize(engine.windowWidth, engine.windowHeight);
		reflectionsBlendBuffer.initialize(engine.windowWidth, engine.windowHeight);
		blurBuffer.initialize(engine.windowWidth, engine.windowHeight);
		ssaoBuffer.initialize(engine.windowWidth, engine.windowHeight);
		ssaoBuffer.generateSampleKernel();
		ssaoBuffer.generateNoiseTexture();

		try
		{
			geoProgram = new ShaderProgram();
			geoProgram->init("shaders/general/GBUFFER.vert", "shaders/general/GBUFFER.frag");
			geoProgram->link();
			geoProgram->setUniformBlockBinding("SharedMatrices", camera->getUboBP());
			sceneGraph->getRoot()->setShaderProgram(geoProgram);

			lightProgram = new ShaderProgram();
			lightProgram->init("shaders/general/quad2D.vert", "shaders/general/PBR.frag");
			lightProgram->link();

			lightProgram->use();
			lightProgram->setUniform("gScreenSize", Vector2((float)engine.windowWidth, (float)engine.windowHeight));
			lightProgram->setUniform("gPosition", GBuffer::GB_POSITION);
			lightProgram->setUniform("gAlbedo", GBuffer::GB_ALBEDO);
			lightProgram->setUniform("gNormal", GBuffer::GB_NORMAL);
			lightProgram->setUniform("gMetallicRoughnessAO", GBuffer::GB_METALLIC_ROUGHNESS_AO);
			lightProgram->setUniform("gTexCoord", GBuffer::GB_TEXCOORD);
			lightProgram->setUniform("gSsao", GBuffer::GB_TEXCOORD + 1);
			irradianceMapInfo->updateShader(lightProgram);
			prefilterMapInfo->updateShader(lightProgram);
			brdfLUTinfo->updateShader(lightProgram);
			lightProgram->unuse();

			dofProgram = new ShaderProgram();
			dofProgram->init("shaders/general/quad2D.vert", "shaders/postprocessing/DOF.frag");
			dofProgram->link();
			dofProgram->setUniformBlockBinding("SharedMatrices", camera->getUboBP());

			dofProgram->use();
			dofProgram->setUniform("gScreenSize", Vector2((float)engine.windowWidth, (float)engine.windowHeight));
			dofProgram->setUniform("gPosiion", GBuffer::GB_POSITION);
			dofProgram->setUniform("gAlbedo", GBuffer::GB_ALBEDO);
			dofProgram->setUniform("gNormal", GBuffer::GB_NORMAL);
			dofProgram->setUniform("gMetallicRoughnessAO", GBuffer::GB_METALLIC_ROUGHNESS_AO);
			dofProgram->setUniform("gTexCoord", GBuffer::GB_TEXCOORD);
			dofProgram->setUniform("gBloom", GBuffer::GB_NUMBER_OF_TEXTURES);
			dofProgram->unuse();

			horizontalBlurProgram = new ShaderProgram();
			horizontalBlurProgram->init("shaders/general/quad2D.vert", "shaders/postprocessing/blur_horizontal.frag");
			horizontalBlurProgram->link();

			vertikalBlurProgram = new ShaderProgram();
			vertikalBlurProgram->init("shaders/general/quad2D.vert", "shaders/postprocessing/blur_vertikal.frag");
			vertikalBlurProgram->link();

			bloomSeparationProgram = new ShaderProgram();
			bloomSeparationProgram->init("shaders/general/quad2D.vert", "shaders/postprocessing/bloom_separation.frag");
			bloomSeparationProgram->link();
			bloomSeparationProgram->use();
			bloomSeparationProgram->setUniform("gShaded", 0);
			bloomSeparationProgram->unuse();

			bloomProgram = new ShaderProgram();
			bloomProgram->init("shaders/general/quad2D.vert", "shaders/postprocessing/bloom_blend.frag");
			bloomProgram->link();

			//reflectionsProgram = new ShaderProgram();
			//reflectionsProgram->init("shaders/general/quad2D.vert", "shaders/postprocessing/SSR.frag");
			//reflectionsProgram->link();
			//reflectionsProgram->use();
			//reflectionsProgram->setUniform("gPosition", 0);
			//reflectionsProgram->setUniform("gNormal", 1);
			//reflectionsProgram->setUniform("gShaded", 2);
			//reflectionsProgram->setUniform("gBlur", 3);
			//reflectionsProgram->setUniform("gMetallicRoughnessAO", 4);
			//reflectionsProgram->setUniformBlockBinding("SharedMatrices", camera->getUboBP());
			//reflectionsProgram->unuse();

			fastBoxBlurProgram = new ShaderProgram();
			fastBoxBlurProgram->init("shaders/general/quad2D.vert", "shaders/postprocessing/blur_fastBox.frag");
			fastBoxBlurProgram->link();
			fastBoxBlurProgram->use();
			fastBoxBlurProgram->setUniform("gShaded", 0);
			fastBoxBlurProgram->unuse();

			ssaoProgram = new ShaderProgram();
			ssaoProgram->init("shaders/general/quad2D.vert", "shaders/postprocessing/SSAO.frag");
			ssaoProgram->link();
			ssaoProgram->use();
			ssaoProgram->setUniform("gPosition", 0);
			ssaoProgram->setUniform("gNormal", 1);
			ssaoProgram->setUniform("texNoise", 2);
			ssaoProgram->setUniform("gDepth", 3);
			ssaoProgram->setUniform("samples", ssaoBuffer.ssaoKernel);
			ssaoProgram->setUniformBlockBinding("SharedMatrices", camera->getUboBP());
			ssaoProgram->unuse();
			
			reflectionBlendProgram = new ShaderProgram;
			reflectionBlendProgram->init("shaders/general/quad2D.vert", "shaders/postprocessing/reflection_blend.frag");
			reflectionBlendProgram->link();
			reflectionBlendProgram->use();
			reflectionBlendProgram->setUniform("gReflection", 0);
			reflectionBlendProgram->setUniform("gReflectionBlur", 1);
			reflectionBlendProgram->setUniform("gMetallicRoughnessAO", 2);
			reflectionBlendProgram->setUniform("gShaded", 3);
			reflectionBlendProgram->unuse();
		}
		catch (Exception e)
		{
			std::cout << e.message << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	void showGbuffer() {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		GLsizei halfWidth = (GLsizei)(engine.windowWidth / 2.0f);
		GLsizei halfHeight = (GLsizei)(engine.windowHeight / 2.0f);

		gbuffer.setBufferToRead(GBuffer::GB_POSITION);
		glBlitFramebuffer(0, 0, engine.windowWidth, engine.windowHeight, 0, 0, halfWidth, halfHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		gbuffer.setBufferToRead(GBuffer::GB_ALBEDO);
		glBlitFramebuffer(0, 0, engine.windowWidth, engine.windowHeight, 0, halfHeight, halfWidth, engine.windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		gbuffer.setBufferToRead(GBuffer::GB_METALLIC_ROUGHNESS_AO);
		glBlitFramebuffer(0, 0, engine.windowWidth, engine.windowHeight, halfWidth, halfHeight, engine.windowWidth, engine.windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		gbuffer.setBufferToRead(GBuffer::GB_NORMAL);
		glBlitFramebuffer(0, 0, engine.windowWidth, engine.windowHeight, halfWidth, 0, engine.windowWidth, halfHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	void update(double elapsedSecs) override
	{
		// update camera
		Vector2 cursorPos = engine.getCursorPos();
		Vector2 cursorDiff = cursorPos - lastCursorPos;
		lastCursorPos = cursorPos;

		if (!catchCursor) cursorDiff = Vector2(0.0, 0.0);
		camera->update((float)elapsedSecs, cursorDiff);

		Vector3 translation = camera->getPosition();

		// geometry pass
		glBindFramebuffer(GL_FRAMEBUFFER, gbuffer.fbo);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		sceneGraph->getRoot()->setShaderProgram(geoProgram);
		sceneGraph->draw();

		// debug view of geometry buffer
		if (showGbufferContent)
		{
			showGbuffer();
		}
		else {

			// SSAO Pass 
			if (useSsao) {
				glBindFramebuffer(GL_FRAMEBUFFER, ssaoBuffer.fbo);
				glClear(GL_COLOR_BUFFER_BIT);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_POSITION]);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_NORMAL]);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, ssaoBuffer.noiseTexture);

				ssaoProgram->use();
				ssaoProgram->setUniform("viewPos", translation);
				ssaoProgram->setUniform("radius", ambientRadius);
				ssaoProgram->setUniform("bias", ambientBias);
				quad->draw();
				ssaoProgram->unuse();

				// Blur SSAO Image 
				glBindFramebuffer(GL_FRAMEBUFFER, blurBuffer.fbo);
				glClear(GL_COLOR_BUFFER_BIT);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, ssaoBuffer.texture);
				fastBoxBlurProgram->use();
				fastBoxBlurProgram->setUniform("kernelSize", 1);
				fastBoxBlurProgram->setUniform("kernelSeparation", 1);
				quad->draw();					 
				fastBoxBlurProgram->unuse();
			}

			// lighting pass
			glBindFramebuffer(GL_FRAMEBUFFER, shadedBuffer.fbo);
			glClear(GL_COLOR_BUFFER_BIT);
			for (unsigned int i = 0; i < GBuffer::GB_NUMBER_OF_TEXTURES; i++) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_POSITION + i]);
			}
			glActiveTexture(GL_TEXTURE0 + GBuffer::GB_NUMBER_OF_TEXTURES);
			glBindTexture(GL_TEXTURE_2D, blurBuffer.texture);
			
			// draw objects
			lightProgram->use();
			lightProgram->setUniform("lightCount", (int) lights.size());
			
			// direct light sources
			std::vector<Vector3> lightPositions;
			std::vector<Vector3> lightColors;
			for (int i = 0; i < lights.size(); i++)
			{
				lightPositions.push_back(lights[i].position);
				lightColors.push_back(lights[i].color * lights[i].brightness);
			}
			lightProgram->setUniform("lightPositions", lightPositions);
			lightProgram->setUniform("lightColors", lightColors);
			lightProgram->setUniform("viewPos", translation);
			lightProgram->setUniform("useSsao", useSsao);
			quad->draw();
			lightProgram->unuse();
			
			// copy depth buffer
			glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer.fbo);
			glBlitFramebuffer(0, 0, engine.windowWidth, engine.windowHeight, 0, 0, engine.windowWidth, engine.windowHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			
			// draw Skybox
			skybox->draw();
			
			// Calculate Screen Space Reflections
			if (useSsr) {
				glBindFramebuffer(GL_FRAMEBUFFER, reflectionsBuffer.fbo);
				glClear(GL_COLOR_BUFFER_BIT);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_POSITION]);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_NORMAL]);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, shadedBuffer.texture);
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, blurBuffer.texture);
				glActiveTexture(GL_TEXTURE4);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_METALLIC_ROUGHNESS_AO]);

				reflectionsProgram->use();
				reflectionsProgram->setUniform("gScreenSize", Vector2((float)engine.windowWidth, (float)engine.windowHeight));
				reflectionsProgram->setUniform("viewPos", translation);

				reflectionsProgram->setUniform("maxRayDistance", maxRayDistance);
				reflectionsProgram->setUniform("stepResolution", stepResolution);
				reflectionsProgram->setUniform("stepIterations", stepIterations);
				reflectionsProgram->setUniform("tolerance", tolerance);
				quad->draw();
				reflectionsProgram->unuse();

				// Blur Shaded Image (for rough reflections)
				glBindFramebuffer(GL_FRAMEBUFFER,blurBuffer.fbo);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, reflectionsBuffer.texture);
				fastBoxBlurProgram->use();
				fastBoxBlurProgram->setUniform("kernelSize", 3);
				fastBoxBlurProgram->setUniform("kernelSeparation", 2);
				quad->draw();
				fastBoxBlurProgram->unuse();

				glBindFramebuffer(GL_FRAMEBUFFER, reflectionsBlendBuffer.fbo);
				glClear(GL_COLOR_BUFFER_BIT);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, reflectionsBuffer.texture);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, reflectionsBuffer.texture);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_METALLIC_ROUGHNESS_AO]);
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, shadedBuffer.texture);
				reflectionBlendProgram->use();
				quad->draw();
				reflectionBlendProgram->unuse();
			}

			//separate bright regions of shaded image and save into Pong FBO
			if (useBloom) {
				glBindFramebuffer(GL_FRAMEBUFFER, pingPongBuffer.fbo[1]);
				glClear(GL_COLOR_BUFFER_BIT);
				glActiveTexture(GL_TEXTURE0);
				
				glBindTexture(GL_TEXTURE_2D, useSsr? reflectionsBlendBuffer.texture : shadedBuffer.texture);
				bloomSeparationProgram->use();
				bloomSeparationProgram->setUniform("bloomThreshold", bloomThreshold);
				quad->draw();
				bloomSeparationProgram->unuse();
				
				// bloom: apply blur to bright regions
				bool firstBlurIteration = true;
				for (int i = 0; i < bloomBlur; i++) {
				
					// horizontal blur kernel: Read from Pong Texture, Write into Ping FBO (Texture)
					glBindFramebuffer(GL_FRAMEBUFFER, pingPongBuffer.fbo[0]);
					glClear(GL_COLOR_BUFFER_BIT);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, pingPongBuffer.texture[1]);
					horizontalBlurProgram->use();
					quad->draw();
					horizontalBlurProgram->unuse();
				
					// vertikal blur kernel: Read from Ping Texture, Write into Pong FBO (Texture)
					glBindFramebuffer(GL_FRAMEBUFFER, pingPongBuffer.fbo[1]);
					glClear(GL_COLOR_BUFFER_BIT);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, pingPongBuffer.texture[0]);
					vertikalBlurProgram->use();
					quad->draw();
					vertikalBlurProgram->unuse();
				}

				// add blurred regions (currently in Pong FBO) to original image and save result in Bloom FBO
				glBindFramebuffer(GL_FRAMEBUFFER, bloomBuffer.fbo);
				glClear(GL_COLOR_BUFFER_BIT);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, useSsr ? reflectionsBlendBuffer.texture : shadedBuffer.texture);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, pingPongBuffer.texture[1]);
				bloomProgram->use();
				bloomProgram->setUniform("exposure", bloomExposure);
				quad->draw();
				bloomProgram->unuse();
			}
			
			// DOF 
			dofProgram->use();
			dofProgram->setUniform("useDOF", useDOF);
			dofProgram->setUniform("viewPos", translation);
			dofProgram->setUniform("focalDepth", focalDepth);
			dofProgram->setUniform("dofSamples", dofSamples);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glClear(GL_COLOR_BUFFER_BIT);
			for (unsigned int i = 0; i < GBuffer::GB_NUMBER_OF_TEXTURES; i++) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_POSITION + i]);
			}
			glActiveTexture(GL_TEXTURE0 + GBuffer::GB_NUMBER_OF_TEXTURES);
			
			if (useBloom) {
				glBindTexture(GL_TEXTURE_2D, bloomBuffer.texture);
			}
			else if (useSsr & !useBloom) {
				glBindTexture(GL_TEXTURE_2D, reflectionsBlendBuffer.texture);
			}
			else if (!useSsr & !useBloom){
				glBindTexture(GL_TEXTURE_2D, shadedBuffer.texture);
			}
			
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_ONE, GL_ONE);
			
			quad->draw();
			dofProgram->unuse();
			
			glDisable(GL_BLEND);
		}

		if (!catchCursor) handleImGui();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	void handleImGui()
	{
		const ImVec4 accentColor(0.5f, 1.f, 1.f, 1.f);

		// ImGui demo window (toggle with I)
		if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);

		{
			ImGui::Begin("Post Processing Effects");

			ImGui::Checkbox("Enable Bloom", &useBloom);
			ImGui::Checkbox("Enable DOF", &useDOF);
			ImGui::Checkbox("Enable Reflections", &useSsr);
			ImGui::Checkbox("Enable Ambient Occlusion", &useSsao);

			ImGui::SliderFloat("SSAO Radius", &ambientRadius, 0.1f, 1.0f);
			ImGui::SliderFloat("SSAO Bias", &ambientBias, 0.01f, 0.05f);

			ImGui::SliderFloat("BLOOM Exposure", &bloomExposure, 0.0f, 1.0f);
			ImGui::SliderInt("BLOOM Blur Amount", &bloomBlur, 0, 20);
			ImGui::SliderFloat("BLOOM Threshold", &bloomThreshold, 0.01f, 1.f);

			ImGui::SliderFloat("DOF Focal Depth", &focalDepth, -25.0f, 25.0f);
			ImGui::SliderInt("DOF #Samples", &dofSamples, 1, 150);

			ImGui::SliderFloat("SSR Max Ray Length", &maxRayDistance, 1.0f, 100.0f);
			ImGui::SliderFloat("SSR Resolution", &stepResolution, 0.1f, 1.0f);
			ImGui::SliderInt("SSR Iterations", &stepIterations, 50, 800);
			ImGui::SliderFloat("SSR Hit Tolerance", &tolerance, 0.025f, 0.9f);

			ImGui::End();
		}

		{
			ImGui::Begin("Physically Based Rendering Options");

			ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

			// material properties (only applies to debug objects)
			ImGui::TextColored(accentColor, "Material Properties (only debug objects)");
			
			Material* material = models[6]->getMeshes()[0]->getMaterial();

			ImGui::Checkbox("Use albedo from texture", &material->useAlbedoMap);
			if (!material->useAlbedoMap) ImGui::ColorEdit3("Albedo", (float*)&material->albedo);

			ImGui::Checkbox("Use normals from texture", &material->useNormalMap);
			if (!material->useNormalMap) ImGui::DragFloat3("Normal", (float*)&material->normal, 0.1f, -1.f, 1.f);

			ImGui::Checkbox("Use metallic from texture", &material->useMetallicMap);
			if (!material->useMetallicMap) ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);

			ImGui::Checkbox("Use roughness from texture", &material->useRoughnessMap);
			if (!material->useRoughnessMap) ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);

			ImGui::Checkbox("Use ambient occlusion from texture", &material->useAoMap);
			if (!material->useAoMap) ImGui::SliderFloat("AO", &material->ao, 0.0f, 1.0f);

			// indirect lighting section
			ImGui::TextColored(accentColor, "Indirect lighting");
			ImGui::Text("Displayed Cube Map:");

			static int map = 0;
			ImGui::RadioButton("Environment map (default)", &map, 0);
			ImGui::RadioButton("Irradiance map (for debugging)", &map, 1);
			ImGui::RadioButton("Prefilter map (max mipmap level, for debugging)", &map, 2);

			switch (map)
			{
			case 0: skybox->setCubemap(environmentMap); break;
			case 1: skybox->setCubemap(irradianceMap); break;
			case 2: skybox->setCubemap(prefilterMap); break;
			default: break;
			}

			// direct lighting
			ImGui::TextColored(accentColor, "Direct Lighting:");
			static int selectedLight = 0;
			ImGui::DragFloat3("Light Position", (float*)&lights[selectedLight].position, 0.1f, -15.f, 15.f);
			ImGui::ColorEdit3("Color", (float*)&lights[selectedLight].color);
			ImGui::DragFloat("Brightness", (float*)&lights[selectedLight].brightness, 0.1f, 0.f, 100.f);

			if (ImGui::Button("Add Light")) { lights.push_back(Light(Vector3(), Vector3(1.f, 1.f, 1.f), 15.f)); }
			ImGui::SameLine();
			if (ImGui::Button("Remove Light") && lights.size() > 0)
			{
				lights.erase(lights.begin() + selectedLight);
				selectedLight = 0;
			}

			ImGui::BeginChild("Scrolling");
			for (int i = 0; i < lights.size(); i++)
			{
				std::stringstream ss;
				ss << "Light " << i << std::endl;
				if (ImGui::Selectable(ss.str().c_str(), selectedLight == i))
					selectedLight = i;
			}
			ImGui::EndChild();

			ImGui::End();
		}
	}
};

int main(int argc, char* argv[])
{
	Engine& engine = Engine::getInstance();
	engine.setup();

	MyApp* myApp = new MyApp();
	engine.setApp(myApp);
	engine.start();
	engine.run();
	delete myApp;

	exit(EXIT_SUCCESS);
}
