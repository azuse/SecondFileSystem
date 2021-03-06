#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>  
#include <fcntl.h>  
#include <sys/mman.h>  
#include <sys/stat.h> 

#include <string.h>
#include<iostream>

//#include"BlockDevice.h"
#include"Buf.h"
#include"BufferManager.h"
#include"file.h"
#include"filesystem.h"
#include"inode.h"
#include"Kernel.h"
#include"openfilemanager.h"
#include"user.h"

using namespace std;
int MEMORY_INITIALIZED = 0;

void permissions(unsigned short mode)
{  //Directory or not: 
   if((mode&0x4000)==0)
      printf("-");
   else
      printf("d");

   // permissions of file owner
   if((mode&0x0100)==0)
      printf("-");
   else
      printf("r");

   if((mode&0x0080)==0)
      printf("-");
   else
      printf("w");
   if((mode&0x0040)==0)
      printf("-");
   else
      printf("x");

   //permissions of users in the same group
   if((mode&0x0020)==0)
      printf("-");
   else
      printf("r");
   if((mode&0x0010)==0)
      printf("-");
   else
      printf("w");
   if((mode&0x0008)==0)
      printf("-");
   else
      printf("x");

   //permissions of other users
   if((mode&0x0004)==0)
      printf("-");
   else
      printf("r");
   if((mode&0x0002)==0)
      printf("-");
   else
      printf("w");
   if((mode&0x0001)==0)
      printf("-");
   else
      printf("x");
}

void spb_init(mySuperBlock &sb)
{
	sb.s_isize = myFileSystem::INODE_ZONE_SIZE;
	sb.s_fsize = myFileSystem::DATA_ZONE_END_SECTOR+1;

	//第一组99块 其他都是一百块一组 剩下的被超级快直接管理
	sb.s_nfree = (myFileSystem::DATA_ZONE_SIZE - 99) % 100;

	//超级快直接管理的空闲盘块的第一个盘块的盘块号
	//成组链表法
	int start_last_datablk = myFileSystem::DATA_ZONE_START_SECTOR;
	for (;;)
		if ((start_last_datablk + 100 - 1) < myFileSystem::DATA_ZONE_END_SECTOR)//判断剩下盘块是否还有100个
			start_last_datablk += 100;
		else
			break;
	start_last_datablk--;
	for (int i = 0; i < sb.s_nfree; i++)
		sb.s_free[i] = start_last_datablk + i;

	sb.s_ninode = 100;
	for (int i = 0; i < sb.s_ninode; i++)
		sb.s_inode[i] = i ;//注：这里只是diskinode的编号，真正取用的时候要进行盘块的转换

	sb.s_fmod = 0;
	sb.s_ronly = 0;
}

void init_datablock(char *data)
{
	struct {
		int nfree;//本组空闲的个数
		int free[100];//本组空闲的索引表
	}tmp_table;

	int last_datablk_num = myFileSystem::DATA_ZONE_SIZE;//未加入索引的盘块的数量
	//注:成组连接法,必须的初始化索引表
	for (int i = 0;; i++)
	{
		if (last_datablk_num >= 100)
			tmp_table.nfree = 100;
		else
			tmp_table.nfree = last_datablk_num;
		last_datablk_num -= tmp_table.nfree;

		for (int j = 0; j < tmp_table.nfree; j++)
		{
			if (i == 0 && j == 0)
				tmp_table.free[j] = 0;
			else
			{
				tmp_table.free[j] = 100 * i + j + myFileSystem::DATA_ZONE_START_SECTOR - 1;
			}
		}
		memcpy(&data[i * 100 * 512], (void*)&tmp_table, sizeof(tmp_table));
		if (last_datablk_num == 0)
			break;
	}
}

int init_img(int fd)
{
	mySuperBlock spb;
	spb_init(spb);
	DiskInode *di = new DiskInode[myFileSystem::INODE_ZONE_SIZE*myFileSystem::INODE_NUMBER_PER_SECTOR];

	//设置rootDiskInode的初始值
	di[0].d_mode = myInode::IFDIR;
	di[0].d_mode |= myInode::IEXEC;
	//di[0].d_nlink = 888;

	char *datablock = new char[myFileSystem::DATA_ZONE_SIZE * 512];
	memset(datablock, 0, myFileSystem::DATA_ZONE_SIZE * 512);
	init_datablock(datablock);

	write(fd, &spb,  sizeof(mySuperBlock));
	write(fd, di, myFileSystem::INODE_ZONE_SIZE*myFileSystem::INODE_NUMBER_PER_SECTOR * sizeof(DiskInode));
	write(fd, datablock, myFileSystem::DATA_ZONE_SIZE * 512);

	cout << "格式化磁盘完毕" << endl;
//	exit(1);
}

void init_rootInode()
{

}

void sys_init()
{
	cout << "进入系统初始化：装填spb和根目录inode,设置user结构中的必要参数" << endl;
	myFileManager& fileMgr = myKernel::Instance().GetFileManager();
	fileMgr.rootDirInode = g_InodeTable.IGet(myFileSystem::ROOTINO);
	fileMgr.rootDirInode->i_flag &= (~myInode::ILOCK);

	myKernel::Instance().GetFileSystem().LoadSuperBlock();
	myUser& us = myKernel::Instance().GetUser();
	us.u_cdir = g_InodeTable.IGet(myFileSystem::ROOTINO);
	strcpy(us.u_curdir, "/");
//	Utility::StringCopy("/", us.u_curdir);
	cout << "系统初始化结束" << endl;
}



int fcreate(char *filename,int mode)
{
//	printf("\n\n\n--->fcreate ");
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	u.u_ar0 = 0;
	u.u_dirp = filename;
	u.u_arg[1] = myInode::IRWXU;
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
//	printf("文件名为%s\n",filename);
	fimanag.Creat();
//	cout << "按道理是创建成功了" << endl;
//	cout << u.u_ar0 << endl;
//	printf("fcreate成功返回\n");
	return u.u_ar0;
}

int fopen(char *pathname,int mode)
{
//	cout << "\n\n\n--->fopen" << endl;
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	u.u_ar0 = 0;
	u.u_dirp = pathname;
	u.u_arg[1] = mode;
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
//	printf("文件名为%s\n", pathname);
	fimanag.Open();
//	cout << "Open返回了 u.u_ar0=" << u.u_ar0 << endl;
	return u.u_ar0;
}

int fwrite(int fd,char *src,int len)
{
//	cout << "\n\n\n--->fwrite" << endl;
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	u.u_ar0 = 0;
	u.u_arg[0] = fd;
	u.u_arg[1] = int(src);
	u.u_arg[2] = len;
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
	fimanag.Write();
//	cout << "Write返回了 u.u_ar0=" << u.u_ar0 << endl;
	//delete temp;
	return u.u_ar0;
}


int fread(int fd,char *des,int len)
{
//	cout << "\n\n\n--->fread" << endl;
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	u.u_ar0 = 0;
	u.u_arg[0] = fd;
	u.u_arg[1] = int(des);
	u.u_arg[2] = len;
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
	fimanag.Read();
//	cout << "read返回了 u.u_ar0=" << u.u_ar0 << endl;
	return u.u_ar0;
}

void fdelete(char* name)
{
//	cout << "\n\n\n--->fdelete" << endl;
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	u.u_ar0 = 0;
	u.u_dirp = name;
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
	fimanag.UnLink();
//	cout << "delete返回了 u.u_ar0=" << u.u_ar0 << endl;
}

int flseek(int fd,int position,int ptrname)
{
//	cout << "\n\n\n--->fseek" << endl;
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	u.u_ar0 = 0;
	u.u_arg[0] = fd;
	u.u_arg[1] = position;
	u.u_arg[2] = ptrname;
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
	fimanag.Seek();
//	cout << "Seek返回了 u.u_ar0=" << u.u_ar0 << endl;
	return u.u_ar0;
}

void fclose(int fd)
{
//	cout << endl << endl << endl << "--->fclose" << endl;
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	u.u_ar0 = 0;
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
	u.u_arg[0] = fd;
	fimanag.Close();
//	cout << "delete返回了 u.u_ar0=" << u.u_ar0 << endl;
}


void ls()
{
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	int fd = fopen(u.u_curdir, (myFile::FREAD) );
	char temp_filename[32] = { 0 };
	for (;;)
	{
		if (fread(fd, temp_filename, 32) == 0) {
			return;
		}
		else
		{
			//for (int i = 0; i < 32; i++)
			//	cout << temp_filename[i] << "  " << int(temp_filename) << endl;
			myDirectoryEntry *mm = (myDirectoryEntry*)temp_filename;
			if (mm->m_ino == 0)
				continue;
			myInode * pInode = g_InodeTable.IGet(mm->m_ino);
			if(pInode->i_mode & pInode->IFDIR)
				cout << "\033[1;34m" << mm->m_name << "\033[0m" << endl;
			else
				cout << mm->m_name << endl;
			memset(temp_filename, 0, 32);
		}
	}
	fclose(fd);
}



void ll()
{
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	int fd = fopen(u.u_curdir, (myFile::FREAD) );
	char temp_filename[32] = { 0 };
	for (;;)
	{
		if (fread(fd, temp_filename, 32) == 0) {
			return;
		}
		else
		{
			myDirectoryEntry *mm = (myDirectoryEntry*)temp_filename;
			if (mm->m_ino == 0)
				continue;
			myInode * pInode = g_InodeTable.IGet(mm->m_ino);
			permissions(pInode->i_mode);
			printf("\t");
			printf("%d\t",pInode->i_nlink);
			printf("root\t");
			printf("root\t");
			printf("%d\t",pInode->i_size);
			if(pInode->i_mode & pInode->IFDIR)
				printf("\033[1;34m%s\033[0m\n",mm->m_name);
			else
				printf("%s\n",mm->m_name);
			memset(temp_filename, 0, 32);
		}
	}
	fclose(fd);
}


void mkdir(char *dirname)
{
	int defaultmode = 040755;
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	u.u_dirp = dirname;
	u.u_arg[1] = defaultmode;
	u.u_arg[2] = 0;
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
	fimanag.MkNod();

}

void cd(char *dirname)
{
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	u.u_dirp = dirname;
	u.u_arg[0] = int(dirname);
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
	fimanag.ChDir();
}

void backDir(){
	char temp_curdir[128] = { 0 };
	myUser &u = myKernel::Instance().GetUser();
	u.u_error = myUser::my_NOERROR;
	char *last=strrchr(u.u_curdir, '/');
	if (last == u.u_curdir&&u.u_curdir[1]==0)
	{
		return;
	}
	else if (last == u.u_curdir)
	{
		temp_curdir[0] = '/';
	}
	else {
		int i = 0;
		for (char *pc = u.u_curdir; pc != last; pc++)
		{
			temp_curdir[i] = u.u_curdir[i];
			i++;
		}
	}
	u.u_dirp = temp_curdir;
	u.u_arg[0] = int(temp_curdir);
	myFileManager &fimanag = myKernel::Instance().GetFileManager();
	fimanag.ChDir();
}

void quitOS(char *addr,int len)
{
	myBufferManager &bm = myKernel::Instance().GetBufferManager();
	bm.Bflush();
	msync(addr, len, MS_SYNC);
	myInodeTable *mit = myKernel::Instance().GetFileManager().m_InodeTable;
	mit->UpdateInodeTable();
}


int main()
{
	//int fd;
	/*打开文件*/
	int fd = open("c.img", O_RDWR);
	if (fd == -1) {//文件不存在  
		fd = open("c.img", O_RDWR | O_CREAT, 0666);
		if (fd == -1) {
			printf("打开或创建文件失败:%m\n");
			exit(-1);
		}
		init_img(fd);
	}
	struct stat st; //定义文件信息结构体  
					/*取得文件大小*/
	int r = fstat(fd, &st);
	if (r == -1) {
		printf("获取文件大小失败:%m\n");
		close(fd);
		exit(-1);
	}
	int len = st.st_size;
	/*把文件映射成虚拟内存地址*/
	void * addr=mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if ((void *) -1 == addr) {
        printf("Could not map memory: %s\n", strerror(errno));
		exit(-1);
    }
	myKernel::Instance().Initialize((char*)addr);

	sys_init();


	char nowDir[100] = "/";
	cout << "+++++++++++++++++++++++++++++" << endl;
	cout << "| welcome to my file system |" << endl;
	cout << "| use 'help' to display     |" << endl;
	cout << "| help message.             |" << endl;
	cout << "+++++++++++++++++++++++++++++" << endl;
	while (1)
	{

		cout << "root@Filesystem:" << nowDir << "# ";
		char buff[50] = "";
		char tmp[50] = "";
		char WhichToDo=-1;
		// cout << "===============================================================" << endl;
		// cout << "||请输入需要执行的API的对应编号，如下所示：                  ||" << endl;
		// cout << "||a   fopen(char *name, int mode)                            ||" << endl;
		// cout << "||b   fclose(int fd)                                         ||" << endl;
		// cout << "||c   fread(int fd, int length)                              ||" << endl;
		// cout << "||d   fwrite(int fd, char *buffer, int length)               ||" << endl;
		// cout << "||e   flseek(int fd, int position, int ptrname)              ||" << endl;
		// cout << "||f   fcreat(char *name, int mode)                           ||" << endl;
		// cout << "||g   fdelete(char *name)                                    ||" << endl;
		// cout << "||h   ls()                                                   ||" << endl;
		// cout << "||i   mkdir(char* dirname)                                   ||" << endl;
		// cout << "||j   cd(char* dirname)                                      ||" << endl;
		// cout << "||k   backDir()--返回上级目录                                ||" << endl;
		// cout << "||q  退出文件系统                                            ||" << endl << endl << endl;
		// cout << "===============================================================" << endl;
		// cout << "||SecondFileSystem@ 请输入编号>>";
		// cin >> WhichToDo;

		cin >> buff;

		string filename;
		string inBuf;
		string dirname;
		int temp_fd; 
		int outSeek;
		int inLen;
		int mode;
		int openfd;
		int temp_ptrname;
		int outLen;
		int temp_position;
		int readNum;
		int creatfd;
		int writeNum = 0;
		char c;
		char *temp_inBuf, *temp_des, *temp_filename,*temp_dirname;

		if(strcmp(buff,"help") == 0 or strcmp(buff,"h") == 0)
		{
			cout << "My file system beta" << endl << "(use 'help' or 'h' to display this message)" << endl;
			cout << "usage: [commands] [argments]" << endl;
			cout << "Commands: "<< endl;
			cout << "\t ls                    List all files and directories in current directory." << endl;
			cout << "\t ll                    List files with information in current directory." << endl;
			cout << "\t cd [dir]              Change working directory." << endl;
			cout << "\t pwd                   Print the name of the current working directory." << endl;
			cout << "\t mkdir [dirname]       Create a new directory in current directory." << endl;
			cout << "\t exit                  Save changes and exit this file system." << endl;
		}
		else if(strcmp(buff,"ls") == 0)
		{
			ls();
		}
		else if(strcmp(buff,"ll") == 0)
		{
			ll();
		}
		else if(strncmp(buff, "mkdir" , 5) == 0)
		{
			char newdirname[50] = "";
			cin >> newdirname;
			mkdir(newdirname);
		}
		else if(strncmp(buff, "cd", 2) == 0)
		{
			char path[50] = "";
			cin >> path;
			if(strcmp(path,".") == 0)
				continue;
			else if(strcmp(path,"..") == 0)
				backDir();
			else
				cd(path);
			myUser &u = myKernel::Instance().GetUser();
			strcpy(nowDir, u.u_curdir);
		}
		else if(strcmp(buff, "pwd") == 0)
		{
			myUser &u = myKernel::Instance().GetUser();
			cout <<  u.u_curdir << endl;
		}
		else if(strcmp(buff,"exit") == 0)
		{
			cout << "goodbye!" << endl;
			quitOS((char*)addr,len);
			return 0;
		}
		// switch (WhichToDo)
		// {
		// case 'a'://fopen
		// 	cout << "||SecondFileSystem@ 请输入第一个参数文件名>>";
		// 	cin >> filename;
		// 	temp_filename = new char[filename.length()+1];
		// 	strcpy(temp_filename, filename.c_str());
		// 	cout << "||SecondFileSystem@ 请输入第二个参数，打开方式>>";
		// 	cin >> mode;
		// 	openfd = fopen(temp_filename, mode);
		// 	if (openfd < 0)
		// 		cout << "||SecondFileSystem@ open失败" << endl;
		// 	else
		// 		cout << "||SecondFileSystem@ open 返回fd=" << openfd << endl;
		// 	delete temp_filename;
		// 	break;
		// case 'b'://fclose
		// 	cout << "||SecondFileSystem@ 请输入第一个参数文件句柄>>";
		// 	cin >> temp_fd;
		// 	fclose(temp_fd);
		// 	break;
		// case 'c'://fread
		// 	cout << "||SecondFileSystem@ 请输入第一个参数，文件句柄>>";
		// 	cin >> temp_fd;
		// 	cout << "||SecondFileSystem@ 请输入第二个参数，读出的数据长度:";
		// 	cin >>outLen;
		// 	temp_des = new char[1+outLen];
		// 	memset(temp_des, 0, outLen + 1);
		// 	readNum = fread(temp_fd, temp_des, outLen);
		// 	cout << "||SecondFileSystem@ read返回" << readNum << endl;
		// 	cout << "||SecondFileSystem@ 读出数据为:" << endl;
		// 	cout << temp_des << endl;
		// 	break;
		// case 'd'://fwrite
		// 	cout << "||SecondFileSystem@ 请输入第一个参数文件句柄>>";
		// 	cin >> temp_fd;
		// 	cout << "||SecondFileSystem@ 请输入第二个参数，写入数据>>";
		// 	cin >> inBuf;
		// 	temp_inBuf = new char[inBuf.length() + 1];
		// 	strcpy(temp_inBuf, inBuf.c_str());
		// 	cout << "||SecondFileSystem@ 请输入第三个参数，写入数据的长度>>";
		// 	cin >>inLen;
		// 	writeNum = fwrite(temp_fd, temp_inBuf, inLen);
		// 	cout << "||SecondFileSystem@ 写返回为" << writeNum << endl;
		// 	break;
		// case 'e'://flseek
		// 	cout << "||SecondFileSystem@ 请输入第一个参数文件句柄>>";
		// 	cin >> temp_fd;
		// 	cout << "||SecondFileSystem@ 请输入第二个参数，移动位置>>";
		// 	cin >> temp_position;
		// 	cout << "||SecondFileSystem@ 请输入第三个参数，移动方式>>";
		// 	cin >> temp_ptrname;
		// 	outSeek = flseek(temp_fd, temp_position, temp_ptrname);
		// 	cout << "||SecondFileSystem@ fseek函数返回" << outSeek << endl;
		// 	break;
		// case 'f'://fcreat
		// 	cout << "||SecondFileSystem@ 请输入第一个参数文件名>>";
		// 	cin >> filename;
		// 	temp_filename = new char[filename.length() + 1];
		// 	strcpy(temp_filename, filename.c_str());
		// 	cout << "||SecondFileSystem@ 请输入第二个参数，创建方式>>";
		// 	cin >> mode;
		// 	creatfd = fcreate(temp_filename, mode);
		// 	if (creatfd < 0)
		// 		cout << "||SecondFileSystem@ create失败" << endl;
		// 	else
		// 		cout << "create成功 返回fd=" << creatfd << endl;
		// 	delete temp_filename;
		// 	break;
		// case 'g'://fdelete
		// 	cout << "||SecondFileSystem@ 请输入第一个参数文件名>>";
		// 	cin >> filename;
		// 	temp_filename = new char[filename.length() + 1];
		// 	strcpy(temp_filename, filename.c_str());
		// 	fdelete(temp_filename);
		// 	delete temp_filename;
		// 	break;
		// case 'h':
		// 	cout << "||SecondFileSystem@ 输出如下>>" << endl;
		// 	ls();
		// 	break;
		// case 'i'://mkdir
		// 	cout << "||SecondFileSystem@ 请输入第一个创建目录名>>";
		// 	cin >> dirname;
		// 	temp_dirname = new char[dirname.length() + 1];
		// 	strcpy(temp_dirname, dirname.c_str());
		// 	mkdir(temp_dirname);
		// 	delete temp_dirname;
		// 	break;
		// case 'j'://cd
		// 	cout << "||SecondFileSystem@ 请输入第一个进入目录名>>";
		// 	cin >> dirname;
		// 	temp_dirname = new char[dirname.length() + 1];
		// 	strcpy(temp_dirname, dirname.c_str());
		// 	cd(temp_dirname);
		// 	delete temp_dirname;
		// 	break;
		// case 'k':
		// 	backDir();
		// 	break;
		// case 'q':
		// 	quitOS((char*)addr,len);
		// 	return 1;
		// 	break;
		// default:
		// 	cout << "||SecondFileSystem@ 输入不合法，请重新输入" << endl;
		// 	while ((c = getchar()) != EOF && c != '\n');
		// 	break;
		// }
	}



}
