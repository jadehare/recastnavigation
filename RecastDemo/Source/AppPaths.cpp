#include "AppPaths.h"

#include "SDL.h"
#include "SampleInterfaces.h"

#include <algorithm>
#include <filesystem>

namespace
{
std::filesystem::path resourceDirectory;
std::filesystem::path saveDirectory;
std::filesystem::path layoutRoot;

std::filesystem::path bundleSiblingRoot(std::filesystem::path cursor)
{
	cursor = cursor.lexically_normal();
	for (int i = 0; i < 8 && !cursor.empty(); ++i)
	{
		if (cursor.extension() == ".app")
		{
			return cursor.parent_path();
		}
		const std::filesystem::path parent = cursor.parent_path();
		if (parent == cursor)
		{
			break;
		}
		cursor = parent;
	}
	return {};
}

bool createDirectory(const std::filesystem::path& directory)
{
	std::error_code error;
	std::filesystem::create_directories(directory, error);
	if (error)
	{
		SDL_SetError("Could not create directory %s: %s", directory.string().c_str(), error.message().c_str());
		return false;
	}
	return true;
}
}

bool AppPaths::initialize()
{
	char* basePath = SDL_GetBasePath();
	if (!basePath)
	{
		return false;
	}
	resourceDirectory = std::filesystem::u8path(basePath);
	SDL_free(basePath);

	char* prefPath = SDL_GetPrefPath(nullptr, "RecastDemo");
	if (!prefPath)
	{
		return false;
	}
	saveDirectory = std::filesystem::u8path(prefPath);
	SDL_free(prefPath);

	layoutRoot = bundleSiblingRoot(resourceDirectory);
	return true;
}

bool AppPaths::prepareSavePath(const std::string& filepath)
{
	return createDirectory(std::filesystem::u8path(filepath).parent_path());
}

std::string AppPaths::resourcePath(const std::string& relativePath)
{
	return (resourceDirectory / std::filesystem::u8path(relativePath)).string();
}

std::string AppPaths::savePath(const std::string& relativePath)
{
	return (saveDirectory / std::filesystem::u8path(relativePath)).string();
}

std::string AppPaths::binPath(const std::string& modelFilename)
{
	std::filesystem::path fileName = std::filesystem::u8path(modelFilename).filename();
	fileName.replace_extension(".bin");
	if (!layoutRoot.empty())
	{
		return (layoutRoot / "bin" / fileName).string();
	}
	return savePath(fileName.string());
}

std::string AppPaths::gsetPath(const std::string& modelFilename)
{
	std::filesystem::path fileName = std::filesystem::u8path(modelFilename).filename();
	fileName.replace_extension(".gset");
	if (!layoutRoot.empty())
	{
		return (layoutRoot / "gset" / fileName).string();
	}
	return savePath("Meshes/" + fileName.string());
}

std::string AppPaths::meshPath(const std::string& name)
{
	const std::filesystem::path fileName = std::filesystem::u8path(name).filename();
	if (!layoutRoot.empty())
	{
		const std::filesystem::path directory = fileName.extension() == ".gset" ? layoutRoot / "gset" : layoutRoot / "model";
		return (directory / fileName).string();
	}

	const std::string savedPath = savePath("Meshes/" + fileName.string());
	std::error_code error;
	if (std::filesystem::is_regular_file(std::filesystem::u8path(savedPath), error))
	{
		return savedPath;
	}
	return resourcePath("Meshes/" + fileName.string());
}

void AppPaths::scanMeshes(std::vector<std::string>& files)
{
	files.clear();
	if (!layoutRoot.empty())
	{
		FileIO::scanDirectory((layoutRoot / "model").string(), ".obj", files);
		FileIO::scanDirectory((layoutRoot / "gset").string(), ".gset", files);
	}
	else
	{
		for (const auto& directory : {resourcePath("Meshes"), savePath("Meshes")})
		{
			FileIO::scanDirectory(directory, ".obj", files);
			FileIO::scanDirectory(directory, ".gset", files);
		}
	}
	std::sort(files.begin(), files.end());
	files.erase(std::unique(files.begin(), files.end()), files.end());
}
