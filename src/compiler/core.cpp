#include "compiler.h"

#include <sstream>
#include <vector>
#include <algorithm>

std::string transpileToCpp(const AppState& state)
{
        std::ostringstream code;
        code << "#include <stdint.h>\n\n";

        for (std::map<int, Part>::const_iterator it = state.parts.begin(); it != state.parts.end(); ++it)
        {
                int id = it->first;
                int outC = state.outputCounts.count(id) ? state.outputCounts.at(id) : 0;
                for (int p = 0; p < outC; ++p)
                {
                        code << "static uint8_t n_" << id << "_out_" << p << " = 0;\n";
                }

                PartType type = PART_TYPE_CUSTOM;
                if (state.partTypes.count(id)) type = state.partTypes.at(id);
                if (type == PART_TYPE_CLOCK)
                {
                        code << "static uint8_t clk_" << id << " = 0;\n";
                }
        }

        code << "\nextern \"C\" {\n";
        code << "void executeTick(const uint8_t* in, uint8_t* out) {\n";

        std::vector<int> sources;
        std::vector<int> outputs;
        std::map<int, std::vector<int>> adj;
        std::map<int, int> inDegree;

        for (std::map<int, Part>::const_iterator it = state.parts.begin(); it != state.parts.end(); ++it)
        {
                int id = it->first;
                inDegree[id] = 0;
                PartType type = PART_TYPE_CUSTOM;
                if (state.partTypes.count(id)) type = state.partTypes.at(id);

                if (type == PART_TYPE_SOURCE) sources.push_back(id);
                if (type == PART_TYPE_OUTPUT) outputs.push_back(id);
        }

        std::sort(sources.begin(), sources.end());
        std::sort(outputs.begin(), outputs.end());

        for (std::map<PartPin, PartPin>::const_iterator it = state.connections.begin(); it != state.connections.end(); ++it)
        {
                int toID = it->first.first;
                int fromID = it->second.first;
                adj[fromID].push_back(toID);
                inDegree[toID]++;
        }

        std::vector<int> q;
        for (std::map<int, int>::iterator it = inDegree.begin(); it != inDegree.end(); ++it)
        {
                if (it->second == 0) q.push_back(it->first);
        }

        std::vector<int> order;
        std::map<int, bool> emitted;
        while (!q.empty())
        {
                int u = q.front();
                q.erase(q.begin());
                order.push_back(u);
                emitted[u] = true;
                for (size_t i = 0; i < adj[u].size(); ++i)
                {
                        int v = adj[u][i];
                        inDegree[v]--;
                        if (inDegree[v] == 0) q.push_back(v);
                }
        }

        for (std::map<int, Part>::const_iterator it = state.parts.begin(); it != state.parts.end(); ++it)
        {
                int id = it->first;
                PartType type = PART_TYPE_CUSTOM;
                if (state.partTypes.count(id)) type = state.partTypes.at(id);
                if (type != PART_TYPE_SOURCE && type != PART_TYPE_OUTPUT && !emitted[id])
                {
                        order.push_back(id);
                }
        }

        int inIdx = 0;
        for (size_t i = 0; i < sources.size(); ++i)
        {
                int id = sources[i];
                int outC = state.outputCounts.count(id) ? state.outputCounts.at(id) : 0;
                for (int p = 0; p < outC; ++p)
                {
                        code << "        n_" << id << "_out_" << p << " = in[" << inIdx++ << "];\n";
                }
        }

        for (size_t i = 0; i < order.size(); ++i)
        {
                int u = order[i];
                PartType type = PART_TYPE_CUSTOM;
                if (state.partTypes.count(u)) type = state.partTypes.at(u);
                if (type == PART_TYPE_SOURCE || type == PART_TYPE_OUTPUT) continue;

                int inC = state.inputCounts.count(u) ? state.inputCounts.at(u) : 0;
                std::vector<std::string> inVars(inC, "0");
                for (int p = 0; p < inC; ++p)
                {
                        std::map<PartPin, PartPin>::const_iterator cit = state.connections.find({u, p});
                        if (cit != state.connections.end())
                        {
                                inVars[p] = "n_" + std::to_string(cit->second.first) + "_out_" + std::to_string(cit->second.second);
                        }
                }

                if (inC == 0 && type != PART_TYPE_CUSTOM && type != PART_TYPE_CLOCK) continue;

                if (type == PART_TYPE_AND)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " &= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = t_" << u << ";\n";
                }
                else if (type == PART_TYPE_OR)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " |= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = t_" << u << ";\n";
                }
                else if (type == PART_TYPE_NOT)
                {
                        code << "        n_" << u << "_out_0 = !" << inVars[0] << ";\n";
                }
                else if (type == PART_TYPE_NAND)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " &= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = !t_" << u << ";\n";
                }
                else if (type == PART_TYPE_NOR)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " |= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = !t_" << u << ";\n";
                }
                else if (type == PART_TYPE_XOR)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " ^= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = t_" << u << ";\n";
                }
                else if (type == PART_TYPE_XNOR)
                {
                        code << "        uint8_t t_" << u << " = " << inVars[0] << ";\n";
                        for (int p = 1; p < inC; ++p) code << "        t_" << u << " ^= " << inVars[p] << ";\n";
                        code << "        n_" << u << "_out_0 = !t_" << u << ";\n";
                }
                else if (type == PART_TYPE_CLOCK)
                {
                        code << "        clk_" << u << " = !clk_" << u << ";\n";
                        code << "        n_" << u << "_out_0 = clk_" << u << ";\n";
                }
        }

        int outIdx = 0;
        for (size_t i = 0; i < outputs.size(); ++i)
        {
                int id = outputs[i];
                int inC = state.inputCounts.count(id) ? state.inputCounts.at(id) : 0;
                for (int p = 0; p < inC; ++p)
                {
                        std::map<PartPin, PartPin>::const_iterator cit = state.connections.find({id, p});
                        if (cit != state.connections.end())
                        {
                                code << "        out[" << outIdx++ << "] = n_" << cit->second.first << "_out_" << cit->second.second << ";\n";
                        }
                        else
                        {
                                code << "        out[" << outIdx++ << "] = 0;\n";
                        }
                }
        }

        code << "}\n}\n";
        return code.str();
}
