#pragma once

#include <string>
#include <vector>

// Resource paths never depend on the process working directory. A macOS .app
// reads models from a sibling "model" folder, writes navmesh binaries to a
// sibling "bin" folder, and writes geometry sets to a sibling "gset" folder.
// Other platforms keep writable files in SDL's per-user application directory.
namespace AppPaths
{
bool initialize();
// Create the output directory only when saving, so read-only installs can run.
bool prepareSavePath(const std::string& filepath);
std::string resourcePath(const std::string& relativePath);
std::string savePath(const std::string& relativePath);
// Output names use the model's basename, with the requested export extension.
std::string binPath(const std::string& modelFilename);
std::string gsetPath(const std::string& modelFilename);
std::string meshPath(const std::string& name);
void scanMeshes(std::vector<std::string>& files);
}
