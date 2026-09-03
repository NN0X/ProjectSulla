#include "compiler.h"

#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <windows.h>

static std::map<std::string, HINSTANCE> loadedHandles;

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

bool compileSharedLibrary(const std::string& cppCode, const std::string& moduleName)
{
        if (!std::filesystem::exists("parts")) std::filesystem::create_directory("parts");

        std::string srcFile = "parts/" + moduleName + ".cpp";
        std::string outFile = "parts/lib" + moduleName + ".dll";

        std::ofstream out(srcFile);
        out << cppCode;
        out.close();

        std::string staticLibs = collectStaticLinks(cppCode);
        std::string command = "clang++ -O3 -shared " + srcFile + staticLibs + " -o " + outFile;
        int result = std::system(command.c_str());

        std::filesystem::remove(srcFile);

        return (result == 0);
}

void unloadCompiledPart(const std::string& moduleName)
{
        std::map<std::string, HINSTANCE>::iterator it = loadedHandles.find(moduleName);
        if (it != loadedHandles.end())
        {
                FreeLibrary(it->second);
                loadedHandles.erase(it);
        }
}

Part loadCompiledPart(const std::string& moduleName, int outCount)
{
        unloadCompiledPart(moduleName);

        std::string libPath = "./parts/lib" + moduleName + ".dll";
        HINSTANCE handle = LoadLibraryA(libPath.c_str());
        if (!handle) return nullptr;

        loadedHandles[moduleName] = handle;

        void (*executeTick)(const uint8_t*, uint8_t*) = (void(*)(const uint8_t*, uint8_t*))GetProcAddress(handle, "executeTick");

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
