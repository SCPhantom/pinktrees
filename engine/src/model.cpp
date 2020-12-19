#include "model.h"

namespace engine {

	Model::Model()
	{
		sampler = new LinearMipmapLinearSampler();
		activeTextures[0] = new TextureInfo(GL_TEXTURE0, "texAlbedo", nullptr, sampler);
		activeTextures[1] = new TextureInfo(GL_TEXTURE1, "texNormal", nullptr, sampler);
		activeTextures[2] = new TextureInfo(GL_TEXTURE2, "texRoughness", nullptr, sampler);
		activeTextures[3] = new TextureInfo(GL_TEXTURE3, "texMetallic", nullptr, sampler);
		activeTextures[4] = new TextureInfo(GL_TEXTURE4, "texAO", nullptr, sampler);
	}

	Model::~Model()
	{
		delete sampler;
		delete[] loadedTextures;
		delete[] activeTextures;
	}

	void Model::load(std::string directoryName)
	{
		WavefrontLoader loaderGround;
		loaderGround.loadFile((directoryName + std::string("/mesh.obj")).c_str());

		mesh = loaderGround.getObjects()[0];
		mesh->calculateTangents();
		mesh->setup();

		loadedTextures[0] = new Texture2D();
		loadedTextures[0]->load(directoryName + "/albedo.png");

		loadedTextures[1] = new Texture2D();
		loadedTextures[1]->load(directoryName + "/normal.png");

		loadedTextures[2] = new Texture2D();
		loadedTextures[2]->load(directoryName + "/roughness.png");
		

		loadedTextures[3] = new Texture2D();
		loadedTextures[3]->load(directoryName + "/metallic.png");

		loadedTextures[4] = new Texture2D();
		loadedTextures[4]->load(directoryName + "/ao.png");

		useLoadedTextures();
	}

	void Model::useLoadedTextures()
	{
		for (int i = 0; i < 5; i++) {
			activeTextures[i]->texture = loadedTextures[i];
		}
	}

	void Model::draw(ShaderProgram* program)
	{
		for (TextureInfo* tInfo : activeTextures)
		{
			tInfo->updateShader(program);
		}

		mesh->draw();

		for (TextureInfo* tInfo : activeTextures) {
			tInfo->unbindSampler();
		}	
	}
}