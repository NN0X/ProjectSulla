#include "primitives.h"

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

std::vector<State> displayPart(std::vector<State> input)
{
        return input;
}

Part getClockPart()
{
        return [state = STATE_LOW](std::vector<State> input) mutable -> std::vector<State> {
                state = (state == STATE_HIGH) ? STATE_LOW : STATE_HIGH;
                return {state};
        };
}

bool parseRamLabel(const std::string& label, bool& sync, int& addrBits, int& dataBits)
{
        if (label.rfind("RAM_", 0) != 0) return false;
        std::string rest = label.substr(4);
        std::string::size_type u1 = rest.find('_');
        if (u1 == std::string::npos) return false;
        std::string mode = rest.substr(0, u1);
        if (mode == "SYNC") sync = true;
        else if (mode == "ASYNC") sync = false;
        else return false;
        std::string tail = rest.substr(u1 + 1);
        std::string::size_type u2 = tail.find('_');
        if (u2 == std::string::npos) return false;
        try {
                addrBits = std::stoi(tail.substr(0, u2));
                dataBits = std::stoi(tail.substr(u2 + 1));
        } catch (...) { return false; }
        return addrBits > 0 && addrBits <= 24 && dataBits > 0 && dataBits <= 32;
}

Part makeMemoryPart(bool sync, int addrBits, int dataBits)
{
        std::shared_ptr<std::vector<uint32_t> > mem = std::make_shared<std::vector<uint32_t> >((std::size_t)1 << addrBits, 0u);
        std::shared_ptr<uint32_t> dout = std::make_shared<uint32_t>(0u);
        int A = addrBits, W = dataBits;
        return [mem, dout, sync, A, W](const Input& in) -> std::vector<State> {
                int we = (!in.empty() && in[0] == STATE_HIGH) ? 1 : 0;
                uint32_t addr = 0;
                for (int k = 0; k < A; ++k)
                        if ((int)in.size() > 1 + k && in[1 + k] == STATE_HIGH) addr |= (1u << k);
                if ((std::size_t)addr >= mem->size()) addr = 0;
                uint32_t din = 0;
                for (int k = 0; k < W; ++k)
                        if ((int)in.size() > 1 + A + k && in[1 + A + k] == STATE_HIGH) din |= (1u << k);
                uint32_t rd = sync ? *dout : (*mem)[addr];
                std::vector<State> out((std::size_t)W, STATE_LOW);
                for (int k = 0; k < W; ++k) out[k] = ((rd >> k) & 1u) ? STATE_HIGH : STATE_LOW;
                if (sync) *dout = (*mem)[addr];
                if (we) (*mem)[addr] = din;
                return out;
        };
}

bool parseArithLabel(const std::string& label, bool& isMul, int& width)
{
        if (label.rfind("ADD_", 0) == 0) isMul = false;
        else if (label.rfind("MUL_", 0) == 0) isMul = true;
        else return false;
        std::string w = label.substr(4);
        if (w.empty()) return false;
        for (std::string::size_type i = 0; i < w.size(); ++i)
                if (w[i] < '0' || w[i] > '9') return false;
        width = std::stoi(w);
        return width > 0 && width <= 32;
}

Part makeArithPart(bool isMul, int width)
{
        int W = width;
        return [isMul, W](const Input& in) -> std::vector<State> {
                uint64_t a = 0, b = 0;
                for (int k = 0; k < W; ++k)
                        if ((int)in.size() > k && in[k] == STATE_HIGH) a |= (1ull << k);
                for (int k = 0; k < W; ++k)
                        if ((int)in.size() > W + k && in[W + k] == STATE_HIGH) b |= (1ull << k);
                uint64_t r = isMul ? (a * b) : (a + b);
                int nout = isMul ? 2 * W : W + 1;
                std::vector<State> out((std::size_t)nout, STATE_LOW);
                for (int p = 0; p < nout; ++p) out[p] = ((r >> p) & 1ull) ? STATE_HIGH : STATE_LOW;
                return out;
        };
}
