/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fsys/filesystem.h"
#include <iostream>
#ifndef __linux__
	#include <Windows.h>
#endif

#define COMPILE_EXECUTABLE 0

#if COMPILE_EXECUTABLE == 0
#ifdef __linux__
class InitAndRelease
{
public:
	InitAndRelease()
	{

	}
	~InitAndRelease()
	{
		FileManager::Close();
	}
} initAndRelease __attribute__((visibility("default")));
#else
BOOLEAN WINAPI DllMain(IN HINSTANCE,IN DWORD nReason,IN LPVOID)
{
	switch(nReason)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		FileManager::Close();
		break;
	}
	return TRUE;
}
#endif
#else
void PrintFileSystem(VDirectory *root,std::string tab="")
{
	std::cout<<tab<<root->GetName()<<":"<<std::endl;
	std::vector<VData*> &files = root->GetFiles();
	for(unsigned int i=0;i<files.size();i++)
	{
		VData *f = files[i];
		if(f->IsDirectory())
			PrintFileSystem(static_cast<VDirectory*>(f),tab +"\t");
		else
			std::cout<<(tab +"\t")<<f->GetName()<<std::endl;
	}
}
int main(int argc,char *argv[])
{
	//VFilePtr *f = FileManager::OpenFile("C:\\Users\\Florian\\Documents\\Visual Studio 2010\\Projects\\vfilesystem\\vfilesystem\\test.txt","rb");
	//if(true)
	//	return 0;
	char *data = "Test";
	FileManager::AddVirtualFile("Users\\Florian\\Documents\\Visual Studio 2010\\Projects\\vfilesystem\\vfilesystem\\sex.txt",data,4);
	VFilePtr *f = FileManager::OpenFile("Users\\Florian\\Documents\\Visual Studio 2010\\Projects\\vfilesystem\\vfilesystem\\sex.txt","rb");
	if(f == NULL)
		std::cout<<"UNABLE TO OPEN FILE!"<<std::endl;
	else
		std::cout<<"GOT FILE!"<<std::endl;
	/*FILE *f = fopen("C:\\Users\\Florian\\Documents\\Visual Studio 2010\\Projects\\weave\\Debug\\arch.txt","r");
	if(f == NULL)
		return 0;
	f = freopen("C:\\Users\\Florian\\Documents\\Visual Studio 2010\\Projects\\weave\\Debug\\arch.txt","w",f);
	if(f == NULL)
	{
		std::cout<<"DERP"<<std::endl;
		std::cout<<"errno "<<errno<<std::endl;
		while(true);
		return 0;
	}
	const char *data = "TEST";
	fwrite(data,1,4,f);
	std::cout<<"GOT FILE"<<std::endl;
	fclose(f);*/
	/*VDirectory *dir = FileManager::GetRootDirectory();
	VDirectory *subdir = dir->AddDirectory("sounds");
	char *data = new char[1024];
	for(unsigned int i=0;i<512;i++)
		data[i] = 'F';
	for(unsigned int i=512;i<1024;i++)
		data[i] = 'K';
	VFile *file = new VFile("testfile.txt",data,1024);
	VFile *fileB = new VFile("testfile.txt",data,1024);
	delete data;
	subdir->Add(file);
	dir->GetDirectory("sounds")->Add(fileB);
	PrintFileSystem(dir);
	std::vector<std::string> files;
	std::vector<std::string> dirs;
	FileManager::LoadPackage("archivelist");
	FileManager::FindFiles("sounds/npc/dragon/*",&files,&dirs);
	std::cout<<"Found "<<files.size()<<" files:"<<std::endl;
	for(unsigned int i=0;i<files.size();i++)
		std::cout<<"\t"<<files[i]<<std::endl;
	std::cout<<"Found "<<dirs.size()<<" directories:"<<std::endl;
	for(unsigned int i=0;i<dirs.size();i++)
		std::cout<<"\t"<<dirs[i]<<std::endl;*/
	/*VFile *f = dir->GetFile("this\\is/a/path/n\\shit\\testfile.txt");
	if(f == NULL)
		std::cout<<"File is NULL!"<<std::endl;
	else
		std::cout<<"Got file"<<std::endl;
	FileManager::LoadPackage("archivelist");
	std::string files[3] = {
		"test.txt",
		"sounds/./././../sounds/bp_MUS_LastStand_mono.mp3",
		"this/is\\a/path/n/shit/testfile.txt"
	};
	for(unsigned int i=0;i<3;i++)
	{
		std::string file = files[i];
		std::cout<<"File: "<<FileManager::GetNormalizedPath(file)<<std::endl;
		std::cout<<"\tSize: "<<FileManager::GetFileSize(file)<<std::endl;
		std::cout<<"\tFlags: "<<FileManager::GetFileFlags(file)<<std::endl;
	}*/
	while(true);
	return 0;
}
#endif