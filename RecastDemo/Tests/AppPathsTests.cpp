#include "AppPaths.h"
#include "InputGeom.h"
#include "SampleInterfaces.h"
#include "SDL.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
std::string baseDirectory;
std::string prefDirectory;
int failures = 0;

void expect(bool result, const char* message)
{
	if (!result)
	{
		fprintf(stderr, "FAIL: %s\n", message);
		++failures;
	}
}

void writeFile(const fs::path& path, const std::string& contents)
{
	std::ofstream file(path);
	file << contents;
	expect(file.good(), "create fixture file");
}
}

// Override only the two SDL path providers. Other SDL functions still use SDL2.
// This keeps the test isolated from the user's real preference directory.
extern "C" char* SDL_GetBasePath()
{
	return baseDirectory.empty() ? nullptr : SDL_strdup(baseDirectory.c_str());
}

extern "C" char* SDL_GetPrefPath(const char*, const char* app)
{
	expect(std::string(app) == "RecastDemo", "stable application save directory name");
	return prefDirectory.empty() ? nullptr : SDL_strdup(prefDirectory.c_str());
}

int main()
{
	std::error_code error;
	const fs::path originalDirectory = fs::current_path(error);
	const fs::path root = fs::temp_directory_path(error) / ("recast-path-tests-" + std::to_string(SDL_GetPerformanceCounter()));
	const fs::path app = root / "App with spaces.app";
	const fs::path resources = app / "Contents/Resources";
	const fs::path model = root / "model";
	const fs::path bin = root / "bin";
	const fs::path gset = root / "gset";
	const fs::path saves = root / "Application Support/RecastDemo";
	fs::create_directories(resources, error);
	expect(!error, "create resource fixture");
	fs::create_directories(root / "unrelated working directory", error);
	fs::current_path(root / "unrelated working directory", error);
	expect(!error, "switch to unrelated working directory");

	baseDirectory = resources.string();
	prefDirectory = saves.string();
	expect(AppPaths::initialize(), "initialize app paths");
	expect(!fs::exists(model) && !fs::exists(bin) && !fs::exists(gset), "startup does not create sibling directories");
	expect(AppPaths::resourcePath("DroidSans.ttf") == (resources / "DroidSans.ttf").string(), "font path uses bundle resources");
	expect(AppPaths::binPath((model / "triangle.obj").string()) == (bin / "triangle.bin").string(), "navmesh uses model basename");
	expect(AppPaths::gsetPath((model / "triangle.obj").string()) == (gset / "triangle.gset").string(), "geometry set uses model basename");
	expect(AppPaths::binPath("/models/模型 with.dots.obj") == (bin / "模型 with.dots.bin").string(), "export basename preserves spaces, Unicode, and inner dots");
	expect(AppPaths::prepareSavePath(AppPaths::binPath("triangle.obj")), "create bin directory when exporting");
	expect(AppPaths::savePath("imgui.ini") == (saves / "imgui.ini").string(), "interface settings stay in the user directory");

	const std::string triangle = "v 0 0 0\nv 1 0 0\nv 0 0 1\nf 1 2 3\n";
	fs::create_directories(model, error);
	writeFile(model / "triangle.obj", triangle);
	BuildContext context;
	InputGeom geometry;
	expect(geometry.load(&context, AppPaths::meshPath("triangle.obj")), "load model from sibling directory");
	BuildSettings settings;
	settings.cellSize = 0.3f;
	const float start[] = {0, 0, 0};
	const float end[] = {1, 0, 1};
	geometry.addOffMeshConnection(start, end, 0.5f, 1, 2, 3);
	expect(geometry.saveGeomSet(&settings), "save geometry set beside the app");
	expect(!fs::exists(model / "triangle.gset"), "model folder does not receive geometry sets");
	expect(!fs::exists(resources / "Meshes/triangle.gset"), "bundle resources remain unmodified");
	std::ifstream savedSet(gset / "triangle.gset");
	std::string firstLine;
	std::getline(savedSet, firstLine);
	expect(firstLine == "f ../model/triangle.obj", "geometry set points at the sibling model");

	InputGeom reloaded;
	expect(reloaded.load(&context, AppPaths::meshPath("triangle.gset")), "reload saved geometry set");
	expect(reloaded.mesh.getTriCount() == 1 && reloaded.offmeshConnRadius.size() == 1, "geometry and off-mesh connections survive round trip");
	expect(reloaded.getBuildSettings() && reloaded.getBuildSettings()->cellSize == settings.cellSize, "build settings survive round trip");
	writeFile(gset / "renamed-set.gset", "f ../model/triangle.obj\n");
	expect(reloaded.load(&context, AppPaths::meshPath("renamed-set.gset")), "load a set with a different basename");
	expect(AppPaths::binPath(reloaded.filename) == (bin / "triangle.bin").string(), "binary export uses referenced model rather than set name");
	expect(reloaded.saveGeomSet(&settings), "export renamed set using the model name");
	expect(fs::exists(gset / "triangle.gset"), "exported geometry set retains model name");

	const fs::path relocated = root / "relocated";
	fs::create_directories(relocated, error);
	fs::rename(app, relocated / "App with spaces.app", error);
	expect(!error, "move the app fixture");
	fs::rename(model, relocated / "model", error);
	expect(!error, "move the model folder with the app");
	fs::rename(bin, relocated / "bin", error);
	expect(!error, "move the bin folder with the app");
	fs::rename(gset, relocated / "gset", error);
	expect(!error, "move the gset folder with the app");
	baseDirectory = (relocated / "App with spaces.app/Contents/Resources").string();
	expect(AppPaths::initialize(), "reinitialize after relocation");
	expect(AppPaths::binPath("triangle.obj") == (relocated / "bin/triangle.bin").string(), "navmesh path follows the app");
	expect(AppPaths::meshPath("triangle.obj") == (relocated / "model/triangle.obj").string(), "model path follows the app");
	expect(reloaded.load(&context, AppPaths::meshPath("triangle.gset")), "reload saved geometry after moving app");

	writeFile(relocated / "gset/legacy.gset", "f Meshes/triangle.obj\n");
	expect(reloaded.load(&context, AppPaths::meshPath("legacy.gset")), "load legacy resource-relative mesh reference");
	writeFile(relocated / "gset/old-absolute.gset", "f " + (resources / "Meshes/triangle.obj").string() + "\n");
	expect(reloaded.load(&context, AppPaths::meshPath("old-absolute.gset")), "recover stale bundled absolute mesh reference");
	expect(reloaded.filename == (relocated / "model/triangle.obj").string(), "stale reference resolves to the sibling model");
	writeFile(relocated / "gset/missing-absolute.gset", "f " + (root / "external/triangle.obj").string() + "\n");
	expect(!reloaded.load(&context, AppPaths::meshPath("missing-absolute.gset")), "missing external absolute reference does not select a bundled namesake");
	writeFile(relocated / "gset/missing-relative.gset", "f ../external/triangle.obj\n");
	expect(!reloaded.load(&context, AppPaths::meshPath("missing-relative.gset")), "missing external relative reference does not select a bundled namesake");
	writeFile(relocated / "gset/missing-local.gset", "f triangle.obj\n");
	expect(!reloaded.load(&context, AppPaths::meshPath("missing-local.gset")), "missing local reference does not select a bundled namesake");
	std::vector<std::string> files;
	AppPaths::scanMeshes(files);
	expect(std::count(files.begin(), files.end(), "triangle.obj") == 1, "mesh list reads the sibling model folder");
	expect(std::count(files.begin(), files.end(), "triangle.gset") == 1, "saved set appears in mesh list");

	const fs::path plain = root / "plain";
	fs::create_directories(plain / "Meshes", error);
	baseDirectory = plain.string();
	prefDirectory = saves.string();
	expect(AppPaths::initialize(), "initialize paths outside an app bundle");
	expect(!fs::exists(saves / "Meshes"), "non-bundle startup does not create output directory");
	expect(AppPaths::binPath("triangle.obj") == (saves / "triangle.bin").string(), "non-bundle navmesh path uses model basename in user directory");
	expect(AppPaths::gsetPath("triangle.gset") == (saves / "Meshes/triangle.gset").string(), "non-bundle geometry set uses user directory");
	writeFile(plain / "Meshes/triangle.obj", triangle);
	expect(AppPaths::meshPath("triangle.obj") == (plain / "Meshes/triangle.obj").string(), "non-bundle mesh path uses resource directory");
	expect(geometry.load(&context, AppPaths::meshPath("triangle.obj")), "load non-bundle model");
	expect(geometry.saveGeomSet(&settings), "create non-bundle output directory when exporting");
	std::ifstream bundledSet(saves / "Meshes/triangle.gset");
	std::getline(bundledSet, firstLine);
	expect(firstLine == "f Meshes/triangle.obj", "bundled reference is distinguishable from a missing user model");
	expect(reloaded.load(&context, AppPaths::meshPath("triangle.gset")), "round-trip bundled mesh from user directory");
	writeFile(saves / "Meshes/triangle.obj", triangle);
	expect(geometry.load(&context, AppPaths::meshPath("triangle.obj")), "load user model over bundled namesake");
	expect(geometry.saveGeomSet(&settings), "save user mesh reference");
	fs::remove(saves / "Meshes/triangle.obj", error);
	expect(!reloaded.load(&context, AppPaths::meshPath("triangle.gset")), "removed user model does not fall back to bundled namesake");

	const fs::path readOnlyRoot = root / "read-only-install";
	fs::create_directories(readOnlyRoot / "Demo.app/Contents/Resources", error);
	fs::create_directories(readOnlyRoot / "model", error);
	writeFile(readOnlyRoot / "model/triangle.obj", triangle);
	fs::permissions(readOnlyRoot, fs::perms::owner_read | fs::perms::owner_exec, error);
	expect(!error, "make install directory read-only");
	baseDirectory = (readOnlyRoot / "Demo.app/Contents/Resources").string();
	expect(AppPaths::initialize(), "read-only install still initializes");
	expect(geometry.load(&context, AppPaths::meshPath("triangle.obj")), "read-only install still loads models");
	expect(!geometry.saveGeomSet(&settings), "read-only install reports geometry export failure");
	expect(!AppPaths::prepareSavePath(AppPaths::binPath("triangle.obj")), "read-only install reports binary export directory failure");
	expect(!fs::exists(readOnlyRoot / "bin") && !fs::exists(readOnlyRoot / "gset"), "read-only startup creates no output directories");
	fs::permissions(readOnlyRoot, fs::perms::owner_all, error);
	expect(!error, "restore install permissions for cleanup");

	const fs::path blockedRoot = root / "blocked-layout";
	fs::create_directories(blockedRoot / "blocked.app/Contents/Resources", error);
	writeFile(blockedRoot / "model", "not a directory");
	baseDirectory = (blockedRoot / "blocked.app/Contents/Resources").string();
	expect(AppPaths::initialize(), "unavailable model directory does not prevent startup");
	writeFile(root / "blocked", "not a directory");
	baseDirectory = plain.string();
	prefDirectory = (root / "blocked").string();
	expect(AppPaths::initialize(), "output creation is deferred until saving");
	expect(!AppPaths::prepareSavePath(AppPaths::gsetPath("triangle.obj")), "report failure to create save directory when exporting");
	prefDirectory.clear();
	expect(!AppPaths::initialize(), "report missing SDL preference path");
	baseDirectory.clear();
	expect(!AppPaths::initialize(), "report missing SDL resource path");

	fs::current_path(originalDirectory, error);
	fs::remove_all(root, error);
	expect(!error, "clean up fixtures");
	if (failures == 0)
	{
		printf("All app path and geometry round-trip checks passed.\n");
	}
	return failures == 0 ? 0 : 1;
}
