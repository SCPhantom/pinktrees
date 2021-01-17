#include <sstream>

#include "engine.h"
#include "model.h"
#include "skybox.h"
#include "light.h"

#include "postprocess.h"
#include "meshfactory.h"

using namespace engine;

class MyApp : public App
{
	const float CAMERA_SPEED = 5.0f;
	const float CAMERA_ROTATE_SPEED = 0.01f;

	Vector2 mouseStartingPos;
	Vector2 mouseCurrentPos;

	Matrix4 oldCameraViewMatrix;
	Matrix3 oldCameraViewMatrixInversed;

	SceneGraph* sceneGraph;

	Camera* camera;
	Mesh* quad;

	Skybox* skybox;

	// for debugging output
	TextureCubemap* environmentMap, *irradianceMap, *prefilterMap;

	ShaderProgram* geoProgram;
	ShaderProgram* lightProgram;
	ShaderProgram* bloomSeparationProgram;
	ShaderProgram* dofProgram;
	ShaderProgram* horizontalBlurProgram;
	ShaderProgram* vertikalBlurProgram;
	ShaderProgram* bloomProgram;
	ShaderProgram* debugProgram;
	ShaderProgram* reflectionsProgram;
	ShaderProgram* fastBoxBlurProgram;

	//bool useTextures = true;
	float roughness = 0.5;
	float metallic = 0.4;
	float ao = 1;

	float bloomExposure = 0.2f;
	bool useBloom = false;

	bool useDOF = false;
	float focalDepth = 2.0f;

	bool showDemoWindow = false;
	bool showGbufferContent = false;

	Model* models[6];

	GBuffer gbuffer;
	ShadedBuffer shadedBuffer;
	BloomBuffer bloomBuffer;
	PingPongBuffer pingPongBuffer;
	ReflectionsBuffer reflectionsBuffer;
	BlurBuffer blurBuffer;

	std::vector<Light> lights;

	void updateProjection()
	{
		float aspectRatio = engine.windowWidth / (float)engine.windowHeight;
		// M_PI / 3 is aproximately 60 degrees FOV
		camera->setPerspective(M_PI / 3, aspectRatio, 0.1, 50);
	}

	#pragma region Callbacks
	void windowCloseCallback() override
	{
		engine.stop();
		delete sceneGraph;
		delete quad;
		delete skybox;
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
		glViewport(0, 0, newWidth, newHeight);
		updateProjection();
	}

	void keyCallback(int key, int scancode, int action, int mods) override
	{
		// escape key to close window
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) windowCloseCallback();

		// show gbuffer content
		if (key == GLFW_KEY_C && action == GLFW_PRESS) showGbufferContent = !showGbufferContent;

		// model switching
		if (key == GLFW_KEY_1 && action == GLFW_PRESS) sceneGraph->getRoot()->setDrawable(models[0]);
		if (key == GLFW_KEY_2 && action == GLFW_PRESS) sceneGraph->getRoot()->setDrawable(models[1]);
		if (key == GLFW_KEY_3 && action == GLFW_PRESS) sceneGraph->getRoot()->setDrawable(models[2]);
		if (key == GLFW_KEY_4 && action == GLFW_PRESS) sceneGraph->getRoot()->setDrawable(models[3]);
		if (key == GLFW_KEY_5 && action == GLFW_PRESS) sceneGraph->getRoot()->setDrawable(models[4]);

		if (key == GLFW_KEY_T && action == GLFW_PRESS) {
		/*	useTextures = !useTextures;
			std::cout << "Use Textures: " << (useTextures ? "Yes" : "No") << std::endl;
			if (useTextures) {
				for (PBRModel* m : models)
				{
					m->useLoadedTextures();
				}
			}*/
		}
		if (key == GLFW_KEY_B && action == GLFW_PRESS) useBloom = !useBloom;
		if (key == GLFW_KEY_D && action == GLFW_PRESS) useDOF = !useDOF;
		if (key == GLFW_KEY_I && action == GLFW_PRESS) showDemoWindow = !showDemoWindow; // I to toggle the ImGui debug window
	}

	void mouseCallback(Vector2 mousePosition) override { mouseCurrentPos = mousePosition; }

	void mouseButtonCallback(int button, int action, int mods) override
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		{
			mouseStartingPos = mouseCurrentPos;

			oldCameraViewMatrix = camera->getViewMatrix();
			oldCameraViewMatrixInversed = camera->getViewMatrixInversed();
		}
	}

	void mouseScrollCallback(double xoffset, double yoffset) override
	{
		focalDepth += 0.5 * yoffset;
	}
#pragma endregion

	void start() override
	{
		models[0] = new Model("assets/models/lantern/lantern.obj");
		models[1] = new Model("assets/models/sphere/sphere.obj");
		models[2] = new Model("assets/models/teapot/teapot.obj");
		models[3] = new Model("assets/models/car/car.obj");
		models[4] = new Model("assets/models/tree/tree.obj");
		models[5] = new Model("assets/models/ground/ground.obj");

		lights.push_back(Light(Vector3(2.f, 3.f, 2.f), Vector3(1.f, 1.f, 1.f), 15.f));
		lights.push_back(Light(Vector3(-2.f, 3.f, -2.f), Vector3(1.f, 1.f, 1.f), 15.f));

		sceneGraph = new SceneGraph();
		SceneNode* root = sceneGraph->getRoot();
		root->setDrawable(models[0]);
		SceneNode* groundNode = root->createNode();
		groundNode->setDrawable(models[5]);
		groundNode->setMatrix(Matrix4::CreateTranslation(0, -1, 0));

		quad = MeshFactory::createQuad();

		camera = new Camera(0);

		camera->lookAt(
			Vector3(0, 0, 5),
			Vector3(0, 0, 1),
			Vector3(0, 1, 0)
		);

		updateProjection();

		skybox = new Skybox(camera);
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
		blurBuffer.initialize(engine.windowWidth, engine.windowHeight);

		try
		{
			geoProgram = new ShaderProgram();
			geoProgram->init("shaders/GEO_vertex.vert", "shaders/GEO_fragment.frag");
			geoProgram->link();
			geoProgram->setUniformBlockBinding("SharedMatrices", camera->getUboBP());
			sceneGraph->getRoot()->setShaderProgram(geoProgram);

			lightProgram = new ShaderProgram();
			lightProgram->init("shaders/LIGHT_vertex.vert", "shaders/LIGHT_fragment.frag");
			lightProgram->link();

			lightProgram->use();
			lightProgram->setUniform("gScreenSize", Vector2(engine.windowWidth, engine.windowHeight));
			lightProgram->setUniform("gPosition", GBuffer::GB_POSITION);
			lightProgram->setUniform("gAlbedo", GBuffer::GB_ALBEDO);
			lightProgram->setUniform("gNormal", GBuffer::GB_NORMAL);
			lightProgram->setUniform("gMetallicRoughnessAO", GBuffer::GB_METALLIC_ROUGHNESS_AO);
			lightProgram->setUniform("gTexCoord", GBuffer::GB_TEXCOORD);
			irradianceMapInfo->updateShader(lightProgram);
			prefilterMapInfo->updateShader(lightProgram);
			brdfLUTinfo->updateShader(lightProgram);
			lightProgram->unuse();

			dofProgram = new ShaderProgram();
			dofProgram->init("shaders/LIGHT_vertex.vert", "shaders/DOF.frag");
			dofProgram->link();
			dofProgram->setUniformBlockBinding("SharedMatrices", camera->getUboBP());

			dofProgram->use();
			dofProgram->setUniform("gScreenSize", Vector2(engine.windowWidth, engine.windowHeight));
			dofProgram->setUniform("gPosiion", GBuffer::GB_POSITION);
			dofProgram->setUniform("gAlbedo", GBuffer::GB_ALBEDO);
			dofProgram->setUniform("gNormal", GBuffer::GB_NORMAL);
			dofProgram->setUniform("gMetallicRoughnessAO", GBuffer::GB_METALLIC_ROUGHNESS_AO);
			dofProgram->setUniform("gTexCoord", GBuffer::GB_TEXCOORD);
			dofProgram->setUniform("gShaded", GBuffer::GB_NUMBER_OF_TEXTURES + 0);
			dofProgram->setUniform("gBloom", GBuffer::GB_NUMBER_OF_TEXTURES + 1);
			dofProgram->unuse();

			horizontalBlurProgram = new ShaderProgram();
			horizontalBlurProgram->init("shaders/LIGHT_vertex.vert", "shaders/blur_horizontal.frag");
			horizontalBlurProgram->link();

			vertikalBlurProgram = new ShaderProgram();
			vertikalBlurProgram->init("shaders/LIGHT_vertex.vert", "shaders/blur_vertikal.frag");
			vertikalBlurProgram->link();

			bloomSeparationProgram = new ShaderProgram();
			bloomSeparationProgram->init("shaders/LIGHT_vertex.vert", "shaders/bloom_separation.frag");
			bloomSeparationProgram->link();
			bloomSeparationProgram->use();
			bloomSeparationProgram->setUniform("gShaded", 0);
			bloomSeparationProgram->unuse();

			bloomProgram = new ShaderProgram();
			bloomProgram->init("shaders/LIGHT_vertex.vert", "shaders/bloom_blend.frag");
			bloomProgram->link();

			reflectionsProgram = new ShaderProgram();
			reflectionsProgram->init("shaders/LIGHT_vertex.vert", "shaders/ScreenSpaceReflections.frag");
			reflectionsProgram->link();
			reflectionsProgram->use();
			reflectionsProgram->setUniform("gPosition", 0);
			reflectionsProgram->setUniform("gNormal", 1);
			reflectionsProgram->setUniform("gShaded", 2);
			reflectionsProgram->setUniform("gBlur", 3);
			reflectionsProgram->setUniform("gMetallicRoughnessAO", 4);
			//reflectionsProgram->setUniform("gDepth", 3);
			reflectionsProgram->setUniformBlockBinding("SharedMatrices", camera->getUboBP());
			reflectionsProgram->unuse();

			fastBoxBlurProgram = new ShaderProgram();
			fastBoxBlurProgram->init("shaders/LIGHT_vertex.vert", "shaders/fastBoxBlur.frag");
			fastBoxBlurProgram->link();
			fastBoxBlurProgram->use();
			fastBoxBlurProgram->setUniform("gShaded", 0);
			fastBoxBlurProgram->unuse();

			debugProgram = new ShaderProgram();
			debugProgram->init("shaders/LIGHT_vertex.vert", "shaders/debug.frag");
			debugProgram->link();
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
		if (engine.getMouseButton(GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) // camera rotation using mouse
		{
			Vector2 diff = mouseCurrentPos - mouseStartingPos;
			diff *= CAMERA_ROTATE_SPEED;

			Vector3 oldCameraSide = oldCameraViewMatrixInversed * Vector3(1, 0, 0);

			Matrix4 rotationSide = Matrix4::CreateRotationY(diff.x);
			Matrix4 rotationUp = Matrix4::CreateRotation(-diff.y, oldCameraSide);

			camera->setViewMatrix(oldCameraViewMatrix * rotationUp * rotationSide);
		}
		else // camera translation using keys
		{
			Vector3 cameraVelocity = Vector3(0, 0, 0);

			bool upArrowPressed = engine.getKey(GLFW_KEY_UP) == GLFW_PRESS;
			bool downArrowPressed = engine.getKey(GLFW_KEY_DOWN) == GLFW_PRESS;

			if (upArrowPressed && !downArrowPressed) cameraVelocity.z = CAMERA_SPEED;
			if (!upArrowPressed && downArrowPressed) cameraVelocity.z = -CAMERA_SPEED;

			Matrix4 translationMatrix = Matrix4::CreateTranslation(camera->getViewMatrixInversed() * cameraVelocity * elapsedSecs);
			Matrix4 out = camera->getViewMatrix() * translationMatrix;
			camera->setViewMatrix(out);
		}

		camera->bind();

		if (useBloom) {
			int multiplier = engine.getKey(GLFW_KEY_LEFT_ALT) == GLFW_PRESS ? -1 : 1;
			if (engine.getKey(GLFW_KEY_E) == GLFW_PRESS)
			{	
				bloomExposure += 0.02 * multiplier;
				if (bloomExposure > 1) bloomExposure = 1;
				if (bloomExposure < 0) bloomExposure = 0;
				std::cout << "Bloom Exposure: " << bloomExposure << std::endl;
			}	
		}
	
		/*if (!useTextures) {
			int multiplier = engine.getKey(GLFW_KEY_LEFT_ALT) == GLFW_PRESS ? -1 : 1;

			if (engine.getKey(GLFW_KEY_R) == GLFW_PRESS)
			{
				roughness += 0.02 * multiplier;
				if (roughness > 1) roughness = 1;
				if (roughness < 0) roughness = 0;
				std::cout << "Roughness: " << roughness << std::endl;
			}

			if (engine.getKey(GLFW_KEY_M) == GLFW_PRESS)
			{
				metallic += 0.02 * multiplier;
				if (metallic > 1) metallic = 1;
				if (metallic < 0) metallic = 0;
				std::cout << "Metallic: " << metallic << std::endl;
			}

			if (engine.getKey(GLFW_KEY_A) == GLFW_PRESS)
			{
				ao += 0.02 * multiplier;
				if (ao > 1) ao = 1;
				if (ao < 0) ao = 0;
				std::cout << "Ambient Occlusion: " << ao << std::endl;
			}*/

			/*if (!useTextures) {
				for (PBRModel* m : models)
				{
					Texture2D* texAlbedo = new Texture2D();
					texAlbedo->createFromColorRGB(Vector3(1, 0, 0));
					m->activeTextures[0]->texture = texAlbedo;

					Texture2D* texNormal = new Texture2D();
					texNormal->createFromColorRGB(Vector3(0.5, 0.5, 1));
					m->activeTextures[1]->texture = texNormal;

					Texture2D* texRoughness = new Texture2D();
					texRoughness->createFromColorGrayscale(roughness);
					m->activeTextures[2]->texture = texRoughness;

					Texture2D* texMetallic = new Texture2D();
					texMetallic->createFromColorGrayscale(metallic);
					m->activeTextures[3]->texture = texMetallic;

					Texture2D* texAO = new Texture2D();
					texAO->createFromColorGrayscale(ao);
					m->activeTextures[4]->texture = texAO;
				}
			}*/
		//}

		Matrix4 viewMatrix = camera->getViewMatrix();
		Matrix3 viewMatrixInversed = camera->getViewMatrixInversed();
		Vector3 translation = viewMatrixInversed * Vector3(viewMatrix * Vector4(0, 0, 0, 1)) * -1;

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
			// lighting pass
			glBindFramebuffer(GL_FRAMEBUFFER, shadedBuffer.fbo);
			for (unsigned int i = 0; i < GBuffer::GB_NUMBER_OF_TEXTURES; i++) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_POSITION + i]);
			}

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
			quad->draw();
			lightProgram->unuse();

			// draw Skybox
			glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer.fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shadedBuffer.fbo);
			glBlitFramebuffer(0, 0, engine.windowWidth, engine.windowHeight, 0, 0, engine.windowWidth, engine.windowHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_FRAMEBUFFER, shadedBuffer.fbo);

			skybox->draw();

			glBindFramebuffer(GL_FRAMEBUFFER, blurBuffer.fbo);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, shadedBuffer.texture);
			fastBoxBlurProgram->use();
			quad->draw();
			fastBoxBlurProgram->unuse();

			glBindFramebuffer(GL_FRAMEBUFFER, reflectionsBuffer.fbo);
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
			reflectionsProgram->setUniform("gScreenSize", Vector2(engine.windowWidth, engine.windowHeight));
			reflectionsProgram->setUniform("viewPos", translation);
			quad->draw();
			reflectionsProgram->unuse();

			//separate bright regions of shaded image and save into Pong FBO
			if (useBloom) {
				glBindFramebuffer(GL_FRAMEBUFFER, pingPongBuffer.fbo[1]);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, reflectionsBuffer.texture);
				bloomSeparationProgram->use();
				quad->draw();
				bloomSeparationProgram->unuse();
			
				// bloom: apply blur to bright regions
				bool firstBlurIteration = true;
				int numBlurIterations = 20;
				for (int i = 0; i < numBlurIterations; i++) {
			
					// horizontal blur kernel: Read from Pong Texture, Write into Ping FBO (Texture)
					glBindFramebuffer(GL_FRAMEBUFFER, pingPongBuffer.fbo[0]);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, pingPongBuffer.texture[1]);
					horizontalBlurProgram->use();
					quad->draw();
					horizontalBlurProgram->unuse();
			
					// vertikal blur kernel: Read from Ping Texture, Write into Pong FBO (Texture)
					glBindFramebuffer(GL_FRAMEBUFFER, pingPongBuffer.fbo[1]);
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, pingPongBuffer.texture[0]);
					vertikalBlurProgram->use();
					quad->draw();
					vertikalBlurProgram->unuse();
				}
			}
			// add blurred regions (currently in Pong FBO) to original image and save result in Bloom FBO
			glBindFramebuffer(GL_FRAMEBUFFER, bloomBuffer.fbo);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, reflectionsBuffer.texture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D,pingPongBuffer.texture[1]);
			bloomProgram->use();
			
			if (useBloom)
				bloomProgram->setUniform("exposure", bloomExposure);
			else
				bloomProgram->setUniform("exposure", 0.0f);
			
			quad->draw();
			bloomProgram->unuse();
			
			// post process: DOF 
			dofProgram->use();
			dofProgram->setUniform("useDOF", useDOF);
			dofProgram->setUniform("viewPos", translation);
			dofProgram->setUniform("focalDepth", focalDepth);
			
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			for (unsigned int i = 0; i < GBuffer::GB_NUMBER_OF_TEXTURES; i++) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, gbuffer.texture[GBuffer::GB_POSITION + i]);
			}
			glActiveTexture(GL_TEXTURE0 + GBuffer::GB_NUMBER_OF_TEXTURES + 0);
			glBindTexture(GL_TEXTURE_2D, reflectionsBuffer.texture);
			glActiveTexture(GL_TEXTURE0 + GBuffer::GB_NUMBER_OF_TEXTURES + 1);
			glBindTexture(GL_TEXTURE_2D, bloomBuffer.texture);
			
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			glBlendFunc(GL_ONE, GL_ONE);
			
			quad->draw();
			dofProgram->unuse();

			glDisable(GL_BLEND);
		}

		handleImGui();

		/*if (!useTextures) {
			for (PBRModel* m : models)
			{
				for (int i = 0; i < 5; i++)
				{
					delete m->activeTextures[i]->texture;
				}
			}
		}*/
	}

	int selectedLight;

	void handleImGui()
	{
		// ImGui demo window (toggle with I)
		if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);

		{
			ImGui::Begin("Post Processing Effects");

			ImGui::Checkbox("Enable Bloom", &useBloom);
			ImGui::Checkbox("Enable DOF", &useDOF);
			ImGui::SliderFloat("Bloom Exposure", &bloomExposure, 0.0f, 1.0f);
			ImGui::SliderFloat("Focal Depth", &focalDepth, -50.0f, 50.0f);
			ImGui::End();
		}

		{
			ImGui::Begin("Physically Based Rendering Options");

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

			ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f);
			ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f);
			ImGui::SliderFloat("AO", &ao, 0.0f, 1.0f);


			ImGui::Text("Direct Lights:");
			static int selectedLight = 0;			
			ImGui::DragFloat3("Light Position", (float*)&lights[selectedLight].position, 0.1, -15, 15);
			ImGui::ColorEdit3("Color", (float*)&lights[selectedLight].color);
			ImGui::DragFloat("Brightness", (float*)&lights[selectedLight].brightness, 0.1, 0, 100);
			
			if (ImGui::Button("Add Light")) { lights.push_back(Light(Vector3(), Vector3(1.f, 1.f, 1.f), 15.f)); }

			ImGui::SameLine();

			if (ImGui::Button("Remove Light") && lights.size() > 0) {
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

		// render windows
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
};

int main(int argc, char* argv[])
{
	Engine& engine = Engine::getInstance();

	MyApp* myApp = new MyApp();
	engine.setApp(myApp);
	engine.start();
	engine.run();
	delete myApp;

	exit(EXIT_SUCCESS);
}
