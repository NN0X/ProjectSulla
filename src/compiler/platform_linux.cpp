#include "compiler.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <dlfcn.h>

static std::string collectStaticLinks(const std::string& cppCode)
{
        const std::string tag = "// SULLA_STATIC_LINK ";
        std::string extra;
        std::set<std::string> seen;
        std::istringstream iss(cppCode);
        std::string line;
        while (std::getline(iss, line))
        {
                if (line.rfind(tag, 0) != 0) continue;
                std::string path = line.substr(tag.size());
                while (!path.empty() && (path.back() == '\r' || path.back() == ' ')) path.pop_back();
                if (!path.empty() && seen.insert(path).second && std::filesystem::exists(path))
                        extra += " " + path;
        }
        return extra;
}

static std::map<std::string, void*> loadedHandles;

bool compileSharedLibrary(const std::string& cppCode, const std::string& moduleName)
{
        if (!std::filesystem::exists("parts")) std::filesystem::create_directory("parts");

        std::string srcFile = "parts/" + moduleName + ".cpp";
        std::string outFile = "parts/lib" + moduleName + ".so";

        std::ofstream out(srcFile);
        out << cppCode;
        out.close();

        std::string staticLibs = collectStaticLinks(cppCode);
        std::string command = "clang++ -O3 -shared -fPIC " + srcFile + staticLibs + " -o " + outFile + " -ldl";
        int result = std::system(command.c_str());

        std::filesystem::remove(srcFile);

        return (result == 0);
}

void unloadCompiledPart(const std::string& moduleName)
{
        std::map<std::string, void*>::iterator it = loadedHandles.find(moduleName);
        if (it != loadedHandles.end())
        {
                dlclose(it->second);
                loadedHandles.erase(it);
        }
}

bool compilePartLibrary(const std::string& cppCode, const std::string& label,
                        bool buildStatic, bool buildDynamic)
{
        std::string dir = sullaPartDir(label);
        std::filesystem::create_directories(dir);
        std::string staticLibs = collectStaticLinks(cppCode);
        bool ok = true;

        if (buildDynamic)
        {
                std::string srcFile = dir + "/" + label + ".dyn.cpp";
                std::string outFile = dir + "/lib" + label + ".so";
                std::ofstream(srcFile) << cppCode;
                std::string command = "clang++ -O3 -shared -fPIC " + srcFile + staticLibs + " -o " + outFile + " -ldl";
                ok = (std::system(command.c_str()) == 0) && ok;
                std::filesystem::remove(srcFile);
        }

        if (buildStatic)
        {
                std::string code = cppCode;
                const std::string from = "void executeTick(";
                std::string::size_type pos = code.find(from);
                if (pos != std::string::npos)
                        code.replace(pos, from.size(), "void " + sullaPartSymbol(label) + "(");

                std::string srcFile = dir + "/" + label + ".sta.cpp";
                std::string objFile = dir + "/" + label + ".o";
                std::string arFile  = dir + "/lib" + label + ".a";
                std::ofstream(srcFile) << code;
                int r1 = std::system(("clang++ -O3 -fPIC -c " + srcFile + " -o " + objFile).c_str());
                std::error_code ec; std::filesystem::remove(arFile, ec);
                int r2 = std::system(("ar rcs " + arFile + " " + objFile).c_str());
                std::filesystem::remove(srcFile);
                std::filesystem::remove(objFile);
                ok = (r1 == 0 && r2 == 0) && ok;
        }

        return ok;
}

Part loadCompiledPart(const std::string& moduleName, int outCount)
{
        unloadCompiledPart(moduleName);

        std::string resolved = sullaFindDynamic(moduleName);
        std::string libPath = !resolved.empty() ? ("./" + resolved)
                                                : ("./parts/lib" + moduleName + ".so");
        void* handle = dlopen(libPath.c_str(), RTLD_LAZY);
        if (!handle) return nullptr;

        loadedHandles[moduleName] = handle;

        void (*executeTick)(const uint8_t*, uint8_t*) = (void(*)(const uint8_t*, uint8_t*))dlsym(handle, "executeTick");

        if (!executeTick) return nullptr;

        return [executeTick, outCount](std::vector<State> inputs) -> std::vector<State> {
                std::vector<uint8_t> rawIn(inputs.size());
                for(size_t i = 0; i < inputs.size(); ++i) rawIn[i] = inputs[i] == STATE_HIGH ? 1 : 0;

                std::vector<uint8_t> rawOut(outCount); 
                executeTick(rawIn.data(), rawOut.data());

                std::vector<State> outputs(outCount);
                for(size_t i = 0; i < (size_t)outCount; ++i) outputs[i] = rawOut[i] == 1 ? STATE_HIGH : STATE_LOW;

                return outputs;
        };
}
