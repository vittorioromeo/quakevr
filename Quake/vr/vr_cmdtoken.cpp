// vr_cmdtoken.cpp -- the console's tokenizer for arguments of any length (cmd.c's Cmd_TokenizeString calls
// VR_ParseToken): COM_Parse's rules (whitespace, // and block comments, quoted strings, the single-character tokens)
// into a string that grows. COM_Parse cut every token at 1023 characters (com_token), silently, so a long cvar value
// (vr_menu_positions, vr_bodycal_undo...) set from the config or the console lost its end.

#include "vr_engine.hpp"
#include "vr_mem.hpp"

#include <string>
#include <tuple>

namespace
{

// The token read last, valid until the next VR_ParseToken (Cmd_TokenizeString copies it at once: Cmd_AddArg).
struct CmdTokenReadouts
{
    std::string token;
    auto members() { return qvr::mem::list(token); }
};
qvr::mem::Scratch<CmdTokenReadouts> readouts{"cmd token"};

bool singleChar(int c)
{
    return c == '{' || c == '}' || c == '(' || c == ')' || c == '\'' || c == ':';
}

// The rest of `data` after the token read into `t` (empty: none), or nullptr at the end of the text.
const char* parse(const char* data, std::string& t)
{
    if(!data)
    {
        return nullptr;
    }

    int c;
    for(;;)
    {
        // whitespace
        while((c = *data) <= ' ')
        {
            if(c == 0)
            {
                return nullptr; // end of the text
            }
            data++;
        }
        if(c == '/' && data[1] == '/') // a // comment
        {
            while(*data && *data != '\n')
            {
                data++;
            }
            continue;
        }
        if(c == '/' && data[1] == '*') // a /*..*/ comment
        {
            data += 2;
            while(*data && !(*data == '*' && data[1] == '/'))
            {
                data++;
            }
            if(*data)
            {
                data += 2;
            }
            continue;
        }
        break;
    }

    if(c == '\"') // a quoted string
    {
        data++;
        for(;;)
        {
            if((c = *data) != 0)
            {
                ++data;
            }
            if(c == '\"' || !c)
            {
                return data;
            }
            t += static_cast<char>(c);
        }
    }

    if(singleChar(c))
    {
        t += static_cast<char>(c);
        return data + 1;
    }

    // a word (':' does not end one, so that ip:port works)
    do
    {
        t += static_cast<char>(c);
        data++;
        c = *data;
        if(c != ':' && singleChar(c))
        {
            break;
        }
    } while(c > 32);
    return data;
}

} // namespace

// Cmd_TokenizeString: the next argument of `data` into *token (valid until the next call); the text after it, or
// nullptr at the end (then *token is empty).
extern "C" const char* VR_ParseToken(const char* data, const char** token)
{
    std::string& t = readouts.token;
    t.clear();
    data = parse(data, t);
    *token = t.c_str();
    return data;
}
