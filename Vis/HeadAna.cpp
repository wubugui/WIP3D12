#include "FileSystem.h"
#include <iostream>
#include <fstream>

using namespace std;

struct HNode
{
	HNode(const string& name)
	{
		this->name = name;
	};
	string name;
	std::vector<string> names;
};

bool scan_include(const string& line,string& include_file)
{
	for (int i = 0; i < line.size(); ++i)
	{
		char a = line[i];
		if (a == '\t' || a == ' ')
		{
			continue;
		}
		if (a == '/')
			return false;
		if (a == '#')
			break;
	}
	size_t offset = line.find_first_of('#');
	if (offset == string::npos || line.substr(offset+1,7)!="include")
	{
		return false;
	}
	size_t offseta = line.find_last_of('\\');
	size_t offsetb = line.find_last_of('/');
	if (offseta == string::npos && offsetb==string::npos)
	{
		size_t offsetc = line.find_first_of("\"");
		size_t offsetd = line.find_last_of("\"");
		if (offsetc == string::npos)
		{
			offsetc = line.find_first_of("<");
			offsetd = line.find_last_of(">");
		}
		include_file = line.substr(offsetc+1, offsetd - offsetc -1);
		return true;
	}
	else
	{
		size_t offsety = line.find_last_of('\"');
		if (offsety == string::npos)
		{
			offsety = line.find_last_of('>');
		}
		if (offseta == string::npos)
		{
			
			include_file = line.substr(offsetb + 1, offsety- offsetb - 1 );
		}
		else if (offsetb == string::npos)
		{
			include_file = line.substr(offseta + 1, offsety- offsetb - 1 );
		}
	}
	
	return true;
}

void collect_include(HNode* node, const string& name)
{
	ifstream file(name, ios::in);
	string include_file;
	char line[4096];
	while (file.getline(line, 4096))
	{
		
		if (scan_include(line, include_file))
		{
			cout << (include_file);
			printf("\n");
			node->names.push_back(include_file);
		}
		
	}
	printf("\n\n\n");
}

string trim_name(const string& line)
{
	size_t offsetb = line.find_last_of('/');
	if (offsetb == string::npos)
	{
		return line;
	}
	else
	{
		return line.substr(offsetb + 1);
	}
}

int main(int argc,char** argv)
{
	WIPFileSystem* g_filesystem = WIPFileSystem::get_instance();
	std::vector<string> h_file_names;
	std::vector<string> cpp_file_names;
	std::vector<string> file_names;

	std::vector<HNode*> nodes;

	g_filesystem->scan_dir(h_file_names, argv[1], ".h", SCAN_FILES, true);
	g_filesystem->scan_dir(cpp_file_names, argv[1], ".cpp", SCAN_FILES, true);

	for (int i = 0; i < h_file_names.size(); ++i) {
		printf(h_file_names[i].c_str());
		printf("\n");

	}
	for (int i = 0; i < cpp_file_names.size(); ++i) {
		printf(cpp_file_names[i].c_str());
		printf("\n");
	}

	file_names.insert(file_names.begin(), h_file_names.begin(), h_file_names.end());
	file_names.insert(file_names.begin(), cpp_file_names.begin(), cpp_file_names.end());

	printf("======================\n");

	for (int i = 0; i < file_names.size(); ++i) {
		printf(file_names[i].c_str());
		printf("\n");
		string name = file_names[i];
		HNode* node = new HNode(trim_name(name));
		collect_include(node, string(argv[1])+string("\\") + file_names[i]);
		
		nodes.push_back(node);
	}


	ofstream ofs("a.txt",ios::out);
	for (int i=0;i<nodes.size();++i)
	{
		ofs<< nodes[i]->name<<" ";
		for (int j = 0; j < nodes[i]->names.size(); ++j)
		{
			ofs<<nodes[i]->names[j]<<" ";
		}
		ofs << endl;
	}
	ofs.close();
	return 0;
}