# include <imgui.h>
# define IMGUI_DEFINE_MATH_OPERATORS
# include <imgui_internal.h>
# include <imgui_node_editor.h>
# include <application.h>
#include <string>
#include <vector>
#include <fstream>
#include <map>

using namespace std;

namespace ed = ax::NodeEditor;

static ed::EditorContext* g_Context = nullptr;



struct HNode
{
    HNode(const string& name)
    {
        this->name = name;
    };
    string name;
    std::vector<string> names;
};

vector<HNode*> nodes;

struct GraphNode
{
    int NodeID;
    int PinInID;
    int PinOutID;
    bool draw = false;
    vector< GraphNode*> in;
    vector< GraphNode*> out;
};


//name node id
map<string, GraphNode> GraphTable;
int gid = 1329;


vector<string> split(const string& str, const string& pattern)
{
    vector<string> ret;
    if (pattern.empty()) return ret;
    size_t start = 0, index = str.find_first_of(pattern, 0);
    while (index != str.npos)
    {
        if (start != index)
            ret.push_back(str.substr(start, index - start));
        start = index + 1;
        index = str.find_first_of(pattern, start);
    }
    if (!str.substr(start).empty())
        ret.push_back(str.substr(start));
    return ret;
}

void create_node(const string& filename)
{
    ifstream file(filename, ios::in);
    char line[1024];
    while (file.getline(line, 1024))
    {
        string lines = (line);
        auto ks = split(lines, " ");
        HNode* node = new HNode(ks[0]);
        for (int i = 1; i < ks.size(); ++i)
        {
            node->names.push_back(ks[i]);
        }
        nodes.push_back(node);
        GraphNode gnode;
        gnode.NodeID = gid++;
        gnode.PinInID = gid++;
        gnode.PinOutID = gid++;
        GraphTable.insert({ ks[0], gnode });
    }
    for (int i = 0; i < nodes.size(); ++i)
    {
        for (int j = 0; j < nodes[i]->names.size(); ++j)
        {
            if (GraphTable.find(nodes[i]->names[j]) != GraphTable.end())
            {
                string name = nodes[i]->name;
                string cname = nodes[i]->names[j];
                GraphTable[name].out.push_back(&GraphTable[cname]);
                GraphTable[cname].in.push_back(&GraphTable[name]);
            }
        }
    }
    for (auto& v : GraphTable)
    {
        if ((v.second.out.empty()&&v.second.in.size()>0)||(v.second.out.size()>0&&v.second.in.size()>0))
            v.second.draw = true;
        else
            v.second.draw = false;
    }
}

const char* Application_GetName()
{
    return "Simple";
}

void Application_Initialize()
{
    ed::Config config;
    config.SettingsFile = "Simple.json";
    g_Context = ed::CreateEditor(&config);

    create_node("D:/HeaderAna/HeaderAna/HeaderAna/a.txt");
    int jk = 0;
    jk++;
}

void Application_Finalize()
{
    ed::DestroyEditor(g_Context);
}


void DrawNode(int id, const string& name, int inid,int outid,bool draw=true)
{
    if (draw)
    {
        ed::BeginNode(id);
        ImGui::Text("%s %d", name.c_str(), id);
        ed::BeginPin(inid, ed::PinKind::Input);
        ImGui::Text("-> Included %d", inid);
        ed::EndPin();
        ImGui::SameLine();
        ed::BeginPin(outid, ed::PinKind::Output);
        ImGui::Text("Include -> %d", outid);
        ed::EndPin();
        ed::EndNode();
    }
}

void Application_Frame()
{
    auto& io = ImGui::GetIO();

    ImGui::Text("FPS: %.2f (%.2gms)", io.Framerate, io.Framerate ? 1000.0f / io.Framerate : 0.0f);

    ImGui::Separator();

    ed::SetCurrentEditor(g_Context);
    ed::Begin("My Editor", ImVec2(0.0, 0.0f));



    int uniqueId = 1;

    for (int i = 0; i < nodes.size(); ++i)
    {
        const string& name = nodes[i]->name;
        DrawNode(GraphTable[name].NodeID, name, GraphTable[name].PinInID, GraphTable[name].PinOutID, GraphTable[name].draw);
    }
    for (int i = 0; i < nodes.size(); ++i)
    {
        for (int j = 0; j < nodes[i]->names.size(); ++j)
        {
            if (GraphTable.find(nodes[i]->names[j]) != GraphTable.end())
            {
                string name = nodes[i]->name;
                string cname = nodes[i]->names[j];
                ed::Link(uniqueId++, GraphTable[name].PinOutID, GraphTable[cname].PinInID);
            }
        }
    }

    /*
    // Start drawing nodes.
    ed::BeginNode(uniqueId++);
        ImGui::Text("Node A");
        ed::BeginPin(1111, ed::PinKind::Input);
            ImGui::Text("-> In");
        ed::EndPin();
        ImGui::SameLine();
        ed::BeginPin(1112, ed::PinKind::Output);
            ImGui::Text("Out ->");
        ed::EndPin();
    ed::EndNode();
    
    ed::BeginNode(uniqueId++);
    ImGui::Text("Node AAA");
    ed::BeginPin(1113, ed::PinKind::Input);
    ImGui::Text("-> In");
    ed::EndPin();
    ImGui::SameLine();
    ed::BeginPin(1114, ed::PinKind::Output);
    ImGui::Text("Out ->");
    ed::EndPin();
    ed::EndNode();
    */
    
    ed::End();


    ed::SetCurrentEditor(nullptr);

	//ImGui::ShowMetricsWindow();
}

