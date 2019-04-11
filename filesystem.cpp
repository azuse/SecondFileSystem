

#include"filesystem.h"
#include<iostream>
#include"user.h"
#include"Buf.h"
#include"Kernel.h"
#include<string.h>
#include <stdio.h>
#include<stdlib.h>
using namespace std;



/*==============================class SuperBlock===================================*/
/* ϵͳȫ�ֳ�����SuperBlock���� */

mySuperBlock::mySuperBlock()
{
	//nothing to do here
}

mySuperBlock::~mySuperBlock()
{
	//nothing to do here
}



/*==============================class FileSystem===================================*/
myFileSystem::myFileSystem()
{
	//nothing to do here
}

myFileSystem::~myFileSystem()
{
	//nothing to do here
}

void myFileSystem::Initialize()
{
	this->m_BufferManager = &myKernel::Instance().GetBufferManager();

}

void myFileSystem::LoadSuperBlock()
{
//	cout << "FileSystem.LoadSuperBlock" << endl;
	myUser& u = myKernel::Instance().GetUser();
	myBufferManager& bufMgr = myKernel::Instance().GetBufferManager();
	myBuf* pBuf;

	for (int i = 0; i < 2; i++)
	{
		//ע��Ϊʲô����char*�Ӷ����Ƕ��٣�int*��Ҫ������//�о���Ϊ��������룿�ȽϿ죿
		int* p = (int *)&g_spb + i * 128;

		pBuf = bufMgr.Bread(myFileSystem::SUPER_BLOCK_SECTOR_NUMBER + i);

		memcpy(p, pBuf->b_addr, myBufferManager::BUFFER_SIZE);
		
		bufMgr.Brelse(pBuf);
	}
	

	g_spb.s_ronly = 0;
}

mySuperBlock* myFileSystem::GetFS()
{
	return &g_spb;

}


void myFileSystem::Update()
{
	int i;
	mySuperBlock* sb=&g_spb;
	myBuf* pBuf;

	/* ͬ��SuperBlock������ */

		if (1)	/* ��Mountװ����Ӧĳ���ļ�ϵͳ */
		{

			/* �����SuperBlock�ڴ渱��û�б��޸ģ�ֱ�ӹ���inode�Ϳ����̿鱻��������ļ�ϵͳ��ֻ���ļ�ϵͳ */
			if (sb->s_fmod == 0 || sb->s_ronly != 0)
			{
				return;
			}

			/* ��SuperBlock�޸ı�־ */
			sb->s_fmod = 0;

			/*
			* Ϊ��Ҫд�ص�������ȥ��SuperBlock����һ�黺�棬���ڻ�����СΪ512�ֽڣ�
			* SuperBlock��СΪ1024�ֽڣ�ռ��2��������������������Ҫ2��д�������
			*/
			for (int j = 0; j < 2; j++)
			{
				/* ��һ��pָ��SuperBlock�ĵ�0�ֽڣ��ڶ���pָ���512�ֽ� */
				int* p = (int *)sb + j * 128;

				/* ��Ҫд�뵽�豸dev�ϵ�SUPER_BLOCK_SECTOR_NUMBER + j������ȥ */
				//ע��ע���� ������get�����ʱ�����þ����blkno��
				pBuf = this->m_BufferManager->GetBlk(myFileSystem::SUPER_BLOCK_SECTOR_NUMBER + j);

				/* ��SuperBlock�е�0 - 511�ֽ�д�뻺���� */
				memcpy(pBuf->b_addr, p, 512);
				//Utility::DWordCopy(p, (int *)pBuf->b_addr, 128);

				/* ���������е�����д�������� */
				this->m_BufferManager->Bwrite(pBuf);
			}
		}


	/* ͬ���޸Ĺ����ڴ�Inode����Ӧ���Inode */
	g_InodeTable.UpdateInodeTable();



	/* ���ӳ�д�Ļ����д�������� */
	this->m_BufferManager->Bflush();
}



myInode* myFileSystem::IAlloc()
{
//	cout << "FileSystem.IAlloc" << endl;
	mySuperBlock* sb;
	myBuf* pBuf;
	myInode* pNode;
	myUser& u = myKernel::Instance().GetUser();
	int ino;	/* ���䵽�Ŀ������Inode��� */

				/* ��ȡ��Ӧ�豸��SuperBlock�ڴ渱�� */
	sb = this->GetFS();

	//ע����������в����ܴ��������ֶ�����
	/*
	* SuperBlockֱ�ӹ���Ŀ���Inode�������ѿգ�
	* ���뵽��������������Inode��
	* �ȶ�inode�б�������
	* ��Ϊ�����³����л���ж��̲������ܻᵼ�½����л���
	* ���������п��ܷ��ʸ����������ᵼ�²�һ���ԡ�
	*/
	if (sb->s_ninode <= 0)
	{
		/* ���Inode��Ŵ�0��ʼ���ⲻͬ��Unix V6�����Inode��1��ʼ��� */
		ino = -1;

		/* ���ζ������Inode���еĴ��̿飬�������п������Inode���������Inode������ */
		for (int i = 0; i < sb->s_isize; i++)
		{
			pBuf = this->m_BufferManager->Bread(myFileSystem::INODE_ZONE_START_SECTOR + i);

			/* ��ȡ��������ַ */
			//ע������ΪʲôҪ��int* ����4��˧��
			int* p = (int *)pBuf->b_addr;

			/* ���û�������ÿ�����Inode��i_mode != 0����ʾ�Ѿ���ռ�� */
			for (int j = 0; j < myFileSystem::INODE_NUMBER_PER_SECTOR; j++)
			{
				ino++;

				int mode = *(p + j * sizeof(DiskInode) / sizeof(int));

				/* �����Inode�ѱ�ռ�ã����ܼ������Inode������ */
				if (mode != 0)
				{
					continue;
				}

				/*
				* ������inode��i_mode==0����ʱ������ȷ��
				* ��inode�ǿ��еģ���Ϊ�п������ڴ�inodeû��д��
				* ������,����Ҫ���������ڴ�inode���Ƿ�����Ӧ����
				*/
				if (g_InodeTable.IsLoaded(ino) == -1)
				{
					/* �����Inodeû�ж�Ӧ���ڴ濽��������������Inode������ */
					sb->s_inode[sb->s_ninode++] = ino;

					/* ��������������Ѿ�װ�����򲻼������� */
					if (sb->s_ninode >= 100)
					{
						break;
					}
				}
			}

			/* �����Ѷ��굱ǰ���̿飬�ͷ���Ӧ�Ļ��� */
			this->m_BufferManager->Brelse(pBuf);

			/* ��������������Ѿ�װ�����򲻼������� */
			if (sb->s_ninode >= 100)
			{
				break;
			}
		}
		/* ����ڴ�����û���������κο������Inode������NULL */
		if (sb->s_ninode <= 0)
		{
			printf("cannot find aviliable diskInode\n");
			u.u_error = myUser::my_ENOSPC;
			return NULL;
		}
	}

	/*
	* ���沿���Ѿ���֤������ϵͳ��û�п������Inode��
	* �������Inode�������бض����¼�������Inode�ı�š�
	*/
	while (true)
	{
		/* ��������ջ������ȡ�������Inode��� */
		ino = sb->s_inode[--sb->s_ninode];
	//	cout << "��ÿ���Inode ���Ϊ ino=" << ino << endl;

		/* ������Inode�����ڴ� */
		pNode = g_InodeTable.IGet(ino);
		/* δ�ܷ��䵽�ڴ�inode */
		if (NULL == pNode)
		{
			return NULL;
		}

		/* �����Inode����,���Inode�е����� */
		if (0 == pNode->i_mode)
		{
	//		cout << "�������inode�����ݣ�IAlloc�ɹ�����inode��ָ��" << endl;
			pNode->Clean();
			/* ����SuperBlock���޸ı�־ */
			sb->s_fmod = 1;
			return pNode;
		}
		else	/* �����Inode�ѱ�ռ�� */
		{
			g_InodeTable.IPut(pNode);
			continue;	/* whileѭ�� */
		}
	}
	return NULL;	/* GCC likes it! */
}


myBuf* myFileSystem::Alloc()
{
//	cout << "FileSystem.Alloc" << endl;
	int blkno;	/* ���䵽�Ŀ��д��̿��� */
	mySuperBlock* sb;
	myBuf* pBuf;
	myUser& u = myKernel::Instance().GetUser();

	/* ��ȡSuperBlock������ڴ渱�� */
	sb = this->GetFS();

	/* ��������ջ������ȡ���д��̿��� */
	blkno = sb->s_free[--sb->s_nfree];

	/*
	* ����ȡ���̿���Ϊ�㣬���ʾ�ѷ��価���еĿ��д��̿顣
	* ���߷��䵽�Ŀ��д��̿��Ų����������̿�������(��BadBlock()���)��
	* ����ζ�ŷ�����д��̿����ʧ�ܡ�
	*/
	if (0 == blkno)
	{
		sb->s_nfree = 0;
		//Diagnose::Write("No Space On %d !\n", dev);
		printf("im sorry there is no free block aviliable\n");
		u.u_error = myUser::my_ENOSPC;
		return NULL;
	}
//	cout << "��ÿ��еĴ��̿飬���Ϊ blkno=" << blkno << endl;
	if (this->BadBlock(sb, blkno))
	{
		cout << "��õĿ��д��̿�Ϊbadblock��Alloc���󷵻�" << endl;
		return NULL;
	}

	/*
	* ջ�ѿգ��·��䵽���д��̿��м�¼����һ����д��̿�ı��,
	* ����һ����д��̿�ı�Ŷ���SuperBlock�Ŀ��д��̿�������s_free[100]�С�
	*/
	if (sb->s_nfree <= 0)
	{
		/* ����ÿ��д��̿� */
		pBuf = this->m_BufferManager->Bread( blkno);

		/* �Ӹô��̿��0�ֽڿ�ʼ��¼����ռ��4(s_nfree)+400(s_free[100])���ֽ� */
		int* p = (int *)pBuf->b_addr;

		/* ���ȶ��������̿���s_nfree */
		sb->s_nfree = *p++;

		/* ��ȡ�����к���λ�õ����ݣ�д�뵽SuperBlock�����̿�������s_free[100]�� */
		memcpy(sb->s_free, p, 400);
		//Utility::DWordCopy(p, sb->s_free, 100);

		/* ����ʹ����ϣ��ͷ��Ա㱻��������ʹ�� */
		this->m_BufferManager->Brelse(pBuf);

	}

	/* ��ͨ����³ɹ����䵽һ���д��̿� */
	pBuf = this->m_BufferManager->GetBlk( blkno);	/* Ϊ�ô��̿����뻺�� */
	this->m_BufferManager->ClrBuf(pBuf);	/* ��ջ����е����� */
	sb->s_fmod = 1;	/* ����SuperBlock���޸ı�־ */
//	cout << "��ÿ��д��̣�Alloc����" << endl;
	return pBuf;
}


void myFileSystem::IFree( int number)
{
	mySuperBlock* sb;

	sb = this->GetFS();	/* ��ȡ��Ӧ�豸��SuperBlock�ڴ渱�� */

	/*
	* ���������ֱ�ӹ���Ŀ������Inode����100����
	* ͬ�����ͷŵ����Inodeɢ���ڴ���Inode���С�
	*/
	if (sb->s_ninode >= 100)
	{
		return;
	}
	//ע������ֻ�����������get����ʱ��������õ�
	sb->s_inode[sb->s_ninode++] = number;

	/* ����SuperBlock���޸ı�־ */
	sb->s_fmod = 1;
}



void myFileSystem::Free( int blkno)
{
	mySuperBlock* sb;
	myBuf* pBuf;
	myUser& u = myKernel::Instance().GetUser();

	sb = this->GetFS();

	/*
	* ��������SuperBlock���޸ı�־���Է�ֹ���ͷ�
	* ���̿�Free()ִ�й����У���SuperBlock�ڴ渱��
	* ���޸Ľ�������һ�룬�͸��µ�����SuperBlockȥ
	*/
	sb->s_fmod = 1;	

	/*
	* �����ǰϵͳ���Ѿ�û�п����̿飬
	* �����ͷŵ���ϵͳ�е�1������̿�
	*/
	if (sb->s_nfree <= 0)
	{
		sb->s_nfree = 1;
		sb->s_free[0] = 0;	/* ʹ��0��ǿ����̿���������־ */
	}

	/* SuperBlock��ֱ�ӹ�����д��̿�ŵ�ջ���� */
	if (sb->s_nfree >= 100)
	{
		/*
		* ʹ�õ�ǰFree()������Ҫ�ͷŵĴ��̿飬���ǰһ��100������
		* ���̿��������
		*/
		
		pBuf = this->m_BufferManager->GetBlk(blkno);	/* Ϊ��ǰ��Ҫ�ͷŵĴ��̿���仺�� */
	
															/* �Ӹô��̿��0�ֽڿ�ʼ��¼����ռ��4(s_nfree)+400(s_free[100])���ֽ� */
		int* p = (int *)pBuf->b_addr;

		/* ����д������̿��������˵�һ��Ϊ99�飬����ÿ�鶼��100�� */
		*p++ = sb->s_nfree;
		/* ��SuperBlock�Ŀ����̿�������s_free[100]д�뻺���к���λ�� */
		memcpy(p, sb->s_free, 400);
		//Utility::DWordCopy(sb->s_free, p, 100);

		sb->s_nfree = 0;
		/* ����ſ����̿�������ġ���ǰ�ͷ��̿顱д����̣���ʵ���˿����̿��¼�����̿�ŵ�Ŀ�� */
		this->m_BufferManager->Bwrite(pBuf);

	}
	sb->s_free[sb->s_nfree++] = blkno;	/* SuperBlock�м�¼�µ�ǰ�ͷ��̿�� */
	sb->s_fmod = 1;
}

bool myFileSystem::BadBlock(mySuperBlock *spb, int blkno)
{
	return 0;
}

//=================================directoryEntry===================================
myDirectoryEntry::myDirectoryEntry()
{
	this->m_ino = 0;
	this->m_name[0] = '\0';
}

myDirectoryEntry::~myDirectoryEntry()
{
	//nothing to do here
}





/*============================file manager==============================*/
myFileManager::myFileManager()
{
	//nothing to do here
}

myFileManager::~myFileManager()
{
	//nothing to do here
}

void myFileManager::Initialize()
{
	this->m_FileSystem = &myKernel::Instance().GetFileSystem();

	this->m_InodeTable = &g_InodeTable;
	//cout << hex << "inodetable ��ַ�ڳ�ʼ��ʱΪ��" << this->m_InodeTable << endl;
	this->m_OpenFileTable = &g_OpenFileTable;
	this->m_gspb = &g_spb;

	this->m_InodeTable->Initialize();



}



/*
* ���ܣ����ļ�
* Ч�����������ļ��ṹ���ڴ�i�ڵ㿪�� ��i_count Ϊ������i_count ++��
* */
void myFileManager::Open()
{
//	cout << "FileManager.Open" << endl;
	myInode* pInode;
	myUser& u = myKernel::Instance().GetUser();

	pInode = this->NameI(NextChar, myFileManager::OPEN);	/* 0 = Open, not create */
														/* û���ҵ���Ӧ��Inode */
	if (NULL == pInode)
	{
		cout << "�ҵ��ļ���������" << endl;
		return;
	}
	this->Open1(pInode, u.u_arg[1], 0);
//	cout << "Open��������" << endl;
}


/*
* ���ܣ�����һ���µ��ļ�
* Ч�����������ļ��ṹ���ڴ�i�ڵ㿪�� ��i_count Ϊ������Ӧ���� 1��
* */
void myFileManager::Creat()
{
	myInode* pInode;
	myUser& u = myKernel::Instance().GetUser();
	unsigned int newACCMode = u.u_arg[1] & (myInode::IRWXU | myInode::IRWXG | myInode::IRWXO);

	/* ����Ŀ¼��ģʽΪ1����ʾ����������Ŀ¼����д�������� */
	pInode = this->NameI(NextChar, myFileManager::CREATE);
//	cout << "in creat namei�ոշ���" << endl;
	/* û���ҵ���Ӧ��Inode����NameI���� */
	if (NULL == pInode)
	{
		if (u.u_error)
		{
			cout << "��ô����"<<u.u_error << endl;
			return;
		}
		/* ����Inode */
		pInode = this->MakNode(newACCMode & (~myInode::ISVTX));
		/* ����ʧ�� */
		if (NULL == pInode)
		{
			printf("����ʧ��\n");
			exit(1);
			return;
		}

		/*
		* �����ϣ�������ֲ����ڣ�ʹ�ò���trf = 2������open1()��
		* ����Ҫ����Ȩ�޼�飬��Ϊ�ոս������ļ���Ȩ�޺ʹ������mode
		* ����ʾ��Ȩ��������һ���ġ�
		*/
		this->Open1(pInode, myFile::FWRITE, 2);
	}
	else
	{
		/* ���NameI()�������Ѿ�����Ҫ�������ļ�������ո��ļ������㷨ITrunc()����UIDû�иı�
		* ԭ��UNIX��������������ļ�����ȥ�����½����ļ�һ����Ȼ�������ļ������ߺ����Ȩ��ʽû�䡣
		* Ҳ����˵creatָ����RWX������Ч��
		* ������Ϊ���ǲ�����ģ�Ӧ�øı䡣
		* ���ڵ�ʵ�֣�creatָ����RWX������Ч */
//		printf("test 2\n");
		this->Open1(pInode, myFile::FWRITE, 1);
		pInode->i_mode |= newACCMode;
	}
}


/*
* trf == 0��open����
* trf == 1��creat���ã�creat�ļ���ʱ��������ͬ�ļ������ļ�
* trf == 2��creat���ã�creat�ļ���ʱ��δ������ͬ�ļ������ļ��������ļ�����ʱ��һ������
* mode���������ļ�ģʽ����ʾ�ļ������� ����д���Ƕ�д
*/
void myFileManager::Open1(myInode* pInode, int mode, int trf)
{
//	cout << "FileManager.Open1" << endl;
	myUser& u = myKernel::Instance().GetUser();

	/*
	* ����ϣ�����ļ��Ѵ��ڵ�����£���trf == 0��trf == 1����Ȩ�޼��
	* �����ϣ�������ֲ����ڣ���trf == 2������Ҫ����Ȩ�޼�飬��Ϊ�ս���
	* ���ļ���Ȩ�޺ʹ���Ĳ���mode������ʾ��Ȩ��������һ���ġ�
	*/
	if (trf != 2)
	{
		if (mode & myFile::FREAD)
		{
			/* ����Ȩ�� */
			this->Access(pInode, myInode::IREAD);
		}
		if (mode & myFile::FWRITE)
		{
			/* ���дȨ�� */
			this->Access(pInode, myInode::IWRITE);
			/* ϵͳ����ȥдĿ¼�ļ��ǲ������ */
			if ((pInode->i_mode & myInode::IFMT) == myInode::IFDIR)
			{
				cout << "������дĿ¼�ļ�����u.u_error��Ϊ��Ӧ������" << endl;
				u.u_error = myUser::my_EISDIR;
			}
		}
	}

	if (u.u_error)
	{
		cout << "��������Open1���󷵻�" << endl;
		this->m_InodeTable->IPut(pInode);
		return;
	}

	/* ��creat�ļ���ʱ��������ͬ�ļ������ļ����ͷŸ��ļ���ռ�ݵ������̿� */
	if (1 == trf)
	{
		cout << "��creat�ļ���ʱ��������ͬ�ļ������ļ����ͷŸ��ļ���ռ�ݵ������̿� " << endl;
		pInode->ITrunc();
	}

	/* ����inode!
	* ����Ŀ¼�����漰�����Ĵ��̶�д�������ڼ���̻���˯��
	* ��ˣ����̱������������漰��i�ڵ㡣�����NameI��ִ�е�IGet����������
	* �����ˣ����������п��ܻ���������л��Ĳ��������Խ���i�ڵ㡣
	*/
	//ע����������

	/* ������ļ����ƿ�File�ṹ */
	myFile* pFile = this->m_OpenFileTable->FAlloc();
	if (NULL == pFile)
	{
		cout << "δ���䵽File�飬�ͷ�inode,OpenI���󷵻�" << endl;
		this->m_InodeTable->IPut(pInode);
		return;
	}
	/* ���ô��ļ���ʽ������File�ṹ���ڴ�Inode�Ĺ�����ϵ */
	pFile->f_flag = mode & (myFile::FREAD | myFile::FWRITE);
	pFile->f_inode = pInode;



	/* Ϊ�򿪻��ߴ����ļ��ĸ�����Դ���ѳɹ����䣬�������� */
	if (u.u_error == 0)
	{
	//	cout << "OpenI��ȷ����" << endl;
		return;
	}
	//ע�������д��������漰��user�ṹ�б�����EAX�Ĵ��������ο�����û������
}




void myFileManager::Close()
{
//	cout << "FileManager.Close" << endl;
	myUser& u = myKernel::Instance().GetUser();
	int fd = u.u_arg[0];

	/* ��ȡ���ļ����ƿ�File�ṹ */
	myFile* pFile = u.u_ofiles.GetF(fd);
	if (NULL == pFile)
	{
		cout << "δ�ҵ���Ӧfd��File�ṹ��close���󷵻�" << endl;
		return;
	}

	/* �ͷŴ��ļ�������fd���ݼ�File�ṹ���ü��� */
	u.u_ofiles.SetF(fd, NULL);
	this->m_OpenFileTable->CloseF(pFile);
//	cout << "Close���ɹ�������" << endl;
}



void myFileManager::Seek()
{
//	cout << "FileManager.Seek" << endl;
	myFile* pFile;
	myUser& u = myKernel::Instance().GetUser();
	int fd = u.u_arg[0];

	pFile = u.u_ofiles.GetF(fd);
	if (NULL == pFile)
	{
		cout << "û�ҵ���Ӧ��file�ṹ seek���󷵻�" << endl;
		return;  /* ��FILE�����ڣ�GetF��������� */
	}

//ע��������ڹܵ��ļ������

	int offset = u.u_arg[1];

	/* ���u.u_arg[2]��3 ~ 5֮�䣬��ô���ȵ�λ���ֽڱ�Ϊ512�ֽ� */
	if (u.u_arg[2] > 2)
	{
		offset = offset << 9;
		u.u_arg[2] -= 3;
	}

	switch (u.u_arg[2])
	{
		/* ��дλ������Ϊoffset */
	case 0:
		pFile->f_offset = offset;
		break;
		/* ��дλ�ü�offset(�����ɸ�) */
	case 1:
		pFile->f_offset += offset;
		break;
		/* ��дλ�õ���Ϊ�ļ����ȼ�offset */
	case 2:
		pFile->f_offset = pFile->f_inode->i_size + offset;
		break;
	}
//	cout << "Seek�ɹ����� pFile->f_offset=" << pFile->f_offset<< endl;
}


void myFileManager::FStat()
{
	myFile* pFile;
	myUser& u = myKernel::Instance().GetUser();
	int fd = u.u_arg[0];

	pFile = u.u_ofiles.GetF(fd);
	if (NULL == pFile)
	{
		return;
	}

	/* u.u_arg[1] = pStatBuf */
	this->Stat1(pFile->f_inode, u.u_arg[1]);
}

void myFileManager::Stat()
{
	myInode* pInode;
	myUser& u = myKernel::Instance().GetUser();

	pInode = this->NameI(myFileManager::NextChar, myFileManager::OPEN);
	if (NULL == pInode)
	{
		return;
	}
	this->Stat1(pInode, u.u_arg[1]);
	this->m_InodeTable->IPut(pInode);
}

void myFileManager::Stat1(myInode* pInode, unsigned long statBuf)
{
	myBuf* pBuf;
	myBufferManager& bufMgr = myKernel::Instance().GetBufferManager();

	pInode->IUpdate();
	pBuf = bufMgr.Bread(myFileSystem::INODE_ZONE_START_SECTOR + pInode->i_number / myFileSystem::INODE_NUMBER_PER_SECTOR);

	/* ��pָ�򻺴����б��Ϊinumber���Inode��ƫ��λ�� */
	unsigned char* p = pBuf->b_addr + (pInode->i_number % myFileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
	memcpy(&statBuf, p, sizeof(DiskInode));
//	Utility::DWordCopy((int *)p, (int *)statBuf, sizeof(DiskInode) / sizeof(int));

	bufMgr.Brelse(pBuf);
}

void myFileManager::Read()
{
	/* ֱ�ӵ���Rdwr()�������� */
	this->Rdwr(myFile::FREAD);
}

void myFileManager::Write()
{
//	cout << "FileManager.write" << endl;
	/* ֱ�ӵ���Rdwr()�������� */
	this->Rdwr(myFile::FWRITE);
//	cout << "Write��������" << endl;
}

void myFileManager::Rdwr(enum myFile::FileFlags mode)
{
//	cout << "FileManager.Rdwr" << endl;
	myFile* pFile;
	myUser& u = myKernel::Instance().GetUser();

	/* ����Read()/Write()��ϵͳ���ò���fd��ȡ���ļ����ƿ�ṹ */
	pFile = u.u_ofiles.GetF(u.u_arg[0]);	/* fd */
	if (NULL == pFile)
	{
		/* �����ڸô��ļ���GetF�Ѿ����ù������룬�������ﲻ��Ҫ�������� */
		/*	u.u_error = myUser::EBADF;	*/
		return;
	}


	/* ��д��ģʽ����ȷ */
	if ((pFile->f_flag & mode) == 0)
	{
	//	cout << "�ļ���дģʽ����ȷ��Rdwr���󷵻�" << endl;
		u.u_error = myUser::my_EACCES;
		return;
	}

	u.u_IOParam.m_Base = (unsigned char *)u.u_arg[1];	/* Ŀ�껺������ַ */
	u.u_IOParam.m_Count = u.u_arg[2];		/* Ҫ���/д���ֽ��� */
//ע��Ӧ���ǲ���Ҫ��������������迼������������û�ж�������ж�
//	u.u_segflg = 0;		/* User Space I/O�����������Ҫ�����ݶλ��û�ջ�� */

						/* �ܵ���д */
	
/* ��ͨ�ļ���д �����д�����ļ������ļ�ʵʩ������ʣ���������ȣ�ÿ��ϵͳ���á�
Ϊ��Inode����Ҫ��������������NFlock()��NFrele()��
�ⲻ��V6����ơ�read��writeϵͳ���ö��ڴ�i�ڵ�������Ϊ�˸�ʵʩIO�Ľ����ṩһ�µ��ļ���ͼ��*/
	
	/* �����ļ���ʼ��λ�� */
	u.u_IOParam.m_Offset = pFile->f_offset;
	if (myFile::FREAD == mode)
	{
	//	cout << "�Զ���ʽ���� ��ִ��ReadI u.u_IOParam.m_Offset=" << u.u_IOParam.m_Offset<< endl;
		pFile->f_inode->ReadI();
	}
	else
	{
	//	cout << "��д��ʽ���� ��ִ��WriteI" << endl;
		pFile->f_inode->WriteI();
	}

		/* ���ݶ�д�������ƶ��ļ���дƫ��ָ�� */
	pFile->f_offset += (u.u_arg[2] - u.u_IOParam.m_Count);
		
	
	//ע�����ﺯ���ķ����Ƿ���user'�ṹ�У�ֱ�Ӵ���eax�Ĵ����ģ���Ҫ�������⴦��
	/* ����ʵ�ʶ�д���ֽ������޸Ĵ��ϵͳ���÷���ֵ�ĺ���ջ��Ԫ */
	u.u_ar0 = u.u_arg[2] - u.u_IOParam.m_Count;
	//cout << "Rwdr��������" << endl;
}


/* ����NULL��ʾĿ¼����ʧ�ܣ������Ǹ�ָ�룬ָ���ļ����ڴ��i�ڵ� ���������ڴ�i�ڵ�  */
myInode* myFileManager::NameI(char(*func)(), enum DirectorySearchMode mode)
{
	//cout << "FileManager.NameI" << endl;
	//ע�������func��Ϊnextchar
	myInode* pInode;
	myBuf* pBuf;
	char curchar;
	char* pChar;
	int freeEntryOffset;	/* �Դ����ļ�ģʽ����Ŀ¼ʱ����¼����Ŀ¼���ƫ���� */
	myUser& u = myKernel::Instance().GetUser();
	myBufferManager& bufMgr = myKernel::Instance().GetBufferManager();

	/*
	* �����·����'/'��ͷ�ģ��Ӹ�Ŀ¼��ʼ������
	* ����ӽ��̵�ǰ����Ŀ¼��ʼ������
	*/
	pInode = u.u_cdir;

	if ('/' == (curchar = (*func)()))
	{
		pInode = this->rootDirInode;
//		cout << "pInode������Ϊ��Ŀ¼,������Ӧ��Ŀ¼�ļ����ڵ��̿��Ϊ <rootDirInode->i_addr[0]=" <<rootDirInode->i_addr[0]<< endl;
	}

	/* ����Inode�Ƿ����ڱ�ʹ�ã��Լ���֤������Ŀ¼���������и�Inode�����ͷ� */
	this->m_InodeTable->IGet( pInode->i_number);

	/* �������////a//b ����·�� ����·���ȼ���/a/b */
	while ('/' == curchar)
	{
		curchar = (*func)();
	}
	/* �����ͼ���ĺ�ɾ����ǰĿ¼�ļ������ */
	//ע����Ӧ�����ɾ��Ŀ¼�أ�
	if ('\0' == curchar && mode != myFileManager::OPEN)
	{
//		cout << "��ͼ���ĺ�ɾ����ǰĿ¼�ļ������" << endl;
		u.u_error = myUser::my_ENOENT;
		goto out;
	}

	/* ���ѭ��ÿ�δ���pathname��һ��·������ */
	while (true)
	{
		//cout << "curchar=" << curchar << endl;
		/* ����������ͷŵ�ǰ��������Ŀ¼�ļ�Inode�����˳� */
		if (u.u_error != myUser::my_NOERROR)
		{
//			cout << "NameI��⵽u.u_error�д������󷵻�" << endl;
			break;	/* goto out; */
		}

		/* ����·��������ϣ�������ӦInodeָ�롣Ŀ¼�����ɹ����ء� */
		if ('\0' == curchar)
		{
	//		cout << "����·��������ϣ�������ӦInodeָ�롣Ŀ¼�����ɹ�����,NameI����" << endl;
			return pInode;
		}

		/* ���Ҫ���������Ĳ���Ŀ¼�ļ����ͷ����Inode��Դ���˳� */
		if ((pInode->i_mode & myInode::IFMT) != myInode::IFDIR)
		{
			cout << "���������Ĳ���Ŀ¼�ļ�,NameI���󷵻�   pInode->imode=" <<hex<<pInode->i_mode<< endl;
			u.u_error = myUser::my_ENOTDIR;
			break;	/* goto out; */
		}

		/* ����Ŀ¼����Ȩ�޼��,IEXEC��Ŀ¼�ļ��б�ʾ����Ȩ�� */
		if (this->Access(pInode, myInode::IEXEC))
		{
			cout << "NameIj����Ŀ¼����Ȩ�޼�飬δͨ���������󷵻�1" << endl;
			u.u_error = myUser::my_EACCES;
			break;	/* ���߱�Ŀ¼����Ȩ�ޣ�goto out; */
		}

		/*
		* ��Pathname�е�ǰ׼������ƥ���·������������u.u_dbuf[]�У�
		* ���ں�Ŀ¼����бȽϡ�
		*/
		pChar = &(u.u_dbuf[0]);
		while ('/' != curchar && '\0' != curchar && u.u_error == myUser::my_NOERROR)
		{
			if (pChar < &(u.u_dbuf[myDirectoryEntry::DIRSIZ]))
			{
				*pChar = curchar;
				pChar++;
			}
			curchar = (*func)();
		}
		/* ��u_dbufʣ��Ĳ������Ϊ'\0' */
		while (pChar < &(u.u_dbuf[myDirectoryEntry::DIRSIZ]))
		{
			*pChar = '\0';
			pChar++;
		}

		/* �������////a//b ����·�� ����·���ȼ���/a/b */
		while ('/' == curchar)
		{
			curchar = (*func)();
		}

		if (u.u_error != myUser::my_NOERROR)
		{
			cout << "NameIj����Ŀ¼����Ȩ�޼�飬δͨ���������󷵻�2" << endl;
			break; /* goto out; */
		}

		/* �ڲ�ѭ�����ֶ���u.u_dbuf[]�е�·���������������Ѱƥ���Ŀ¼�� */
		u.u_IOParam.m_Offset = 0;
		/* ����ΪĿ¼����� �����հ׵�Ŀ¼��*///ע�����pinodeָ����Ҫ������Ŀ¼�ļ�
		u.u_IOParam.m_Count = pInode->i_size / (myDirectoryEntry::DIRSIZ + 4);
		freeEntryOffset = 0;
		pBuf = NULL;

		while (true)
		{
			/* ��Ŀ¼���Ѿ�������� */
			if (0 == u.u_IOParam.m_Count)
			{
				if (NULL != pBuf)
				{
					bufMgr.Brelse(pBuf);
				}
				/* ����Ǵ������ļ� */
				if (myFileManager::CREATE == mode && curchar == '\0')
				{
					/* �жϸ�Ŀ¼�Ƿ��д */
					if (this->Access(pInode, myInode::IWRITE))
					{
						cout << "NameI�ж�Ŀ¼��дʧ�ܣ������󷵻�" << endl;
						u.u_error = myUser::my_EACCES;
						goto out;	/* Failed */
					}

					/* ����Ŀ¼Inodeָ�뱣���������Ժ�дĿ¼��WriteDir()�������õ� */
					u.u_pdir = pInode;

					if (freeEntryOffset)	/* �˱�������˿���Ŀ¼��λ��Ŀ¼�ļ��е�ƫ���� */
					{
						/* ������Ŀ¼��ƫ��������u���У�дĿ¼��WriteDir()���õ� */
						u.u_IOParam.m_Offset = freeEntryOffset - (myDirectoryEntry::DIRSIZ + 4);
					}
					else  /*���⣺Ϊ��if��֧û����IUPD��־��  ������Ϊ�ļ��ĳ���û�б�ѽ*/
					{
						pInode->i_flag |= myInode::IUPD;
					}
					/* �ҵ�����д��Ŀ���Ŀ¼��λ�ã�NameI()�������� */
				//	cout << "�ҵ�����д��Ŀ���Ŀ¼��λ��NameI����NULL" << endl;
					return NULL;
				}

				/* Ŀ¼��������϶�û���ҵ�ƥ����ͷ����Inode��Դ�����Ƴ� */
				u.u_error = myUser::my_ENOENT;
				goto out;
			}

			/* �Ѷ���Ŀ¼�ļ��ĵ�ǰ�̿飬��Ҫ������һĿ¼�������̿� */
			if (0 == u.u_IOParam.m_Offset % myInode::BLOCK_SIZE)
			{
				if (NULL != pBuf)
				{
					bufMgr.Brelse(pBuf);
				}
				/* ����Ҫ���������̿�� */
				int phyBlkno = pInode->Bmap(u.u_IOParam.m_Offset / myInode::BLOCK_SIZE);
				pBuf = bufMgr.Bread( phyBlkno);
			}

			/* û�ж��굱ǰĿ¼���̿飬���ȡ��һĿ¼����u.u_dent */
			int* src = (int *)(pBuf->b_addr + (u.u_IOParam.m_Offset % myInode::BLOCK_SIZE));
			memcpy(&u.u_dent, src, sizeof(myDirectoryEntry));
			//Utility::DWordCopy(src, (int *)&u.u_dent, sizeof(DirectoryEntry) / sizeof(int));

			u.u_IOParam.m_Offset += (myDirectoryEntry::DIRSIZ + 4);
			u.u_IOParam.m_Count--;

			/* ����ǿ���Ŀ¼���¼����λ��Ŀ¼�ļ���ƫ���� */
			if (0 == u.u_dent.m_ino)
			{
				if (0 == freeEntryOffset)
				{
					freeEntryOffset = u.u_IOParam.m_Offset;
				}
				/* ��������Ŀ¼������Ƚ���һĿ¼�� */
				continue;
			}

			int i;
			for (i = 0; i < myDirectoryEntry::DIRSIZ; i++)
			{
				if (u.u_dbuf[i] != u.u_dent.m_name[i])
				{
					break;	/* ƥ����ĳһ�ַ�����������forѭ�� */
				}
			}

			if (i < myDirectoryEntry::DIRSIZ)
			{
				/* ����Ҫ������Ŀ¼�����ƥ����һĿ¼�� */
				continue;
			}
			else
			{
				/* Ŀ¼��ƥ��ɹ����ص����While(true)ѭ�� */
				break;
			}
		}

		/*
		* ���ڲ�Ŀ¼��ƥ��ѭ�������˴���˵��pathname��
		* ��ǰ·������ƥ��ɹ��ˣ�����ƥ��pathname����һ·��
		* ������ֱ������'\0'������
		*/
		if (NULL != pBuf)
		{
			bufMgr.Brelse(pBuf);
		}

		/* �����ɾ���������򷵻ظ�Ŀ¼Inode����Ҫɾ���ļ���Inode����u.u_dent.m_ino�� */
		if (myFileManager::DELETE == mode && '\0' == curchar)
		{
			/* ����Ը�Ŀ¼û��д��Ȩ�� */
			if (this->Access(pInode, myInode::IWRITE))
			{
				u.u_error = myUser::my_EACCES;
				break;	/* goto out; */
			}
			return pInode;
		}

		/*
		* ƥ��Ŀ¼��ɹ������ͷŵ�ǰĿ¼Inode������ƥ��ɹ���
		* Ŀ¼��m_ino�ֶλ�ȡ��Ӧ��һ��Ŀ¼���ļ���Inode��
		*/
		short dev = pInode->i_dev;
		this->m_InodeTable->IPut(pInode);
		pInode = this->m_InodeTable->IGet( u.u_dent.m_ino);
		/* �ص����While(true)ѭ��������ƥ��Pathname����һ·������ */

		if (NULL == pInode)	/* ��ȡʧ�� */
		{
			cout << "��ȡʧ�� nameI����NULL" << endl;
			return NULL;
		}
	}
out:
	this->m_InodeTable->IPut(pInode);
	cout << "��������NameI����NULL" << endl;
	return NULL;
}

char myFileManager::NextChar()
{
	myUser& u = myKernel::Instance().GetUser();

	/* u.u_dirpָ��pathname�е��ַ� */
	return *u.u_dirp++;
}

/* ��creat���á�
* Ϊ�´������ļ�д�µ�i�ڵ���µ�Ŀ¼��
* ���ص�pInode�����������ڴ�i�ڵ㣬���е�i_count�� 1��
*
* �ڳ����������� WriteDir��������������Լ���Ŀ¼��д����Ŀ¼���޸ĸ�Ŀ¼�ļ���i�ڵ� ������д�ش��̡�
*
*/
myInode* myFileManager::MakNode(unsigned int mode)
{
	//cout << "FIleManager.MakNode" << endl;
	myInode* pInode;
	myUser& u = myKernel::Instance().GetUser();

	/* ����һ������DiskInode������������ȫ����� */

	pInode = this->m_FileSystem->IAlloc();
	if (NULL == pInode)
	{
		cout << "û�п��е�Inode�����䣬MakNode����NULL" << endl;
		return NULL;
	}

	pInode->i_flag |= (myInode::IACC | myInode::IUPD);
	pInode->i_mode = mode | myInode::IALLOC;
	pInode->i_nlink = 1;
	/* ��Ŀ¼��д��u.u_dent�����д��Ŀ¼�ļ� */
	this->WriteDir(pInode);
//	cout << "MakeNode����" << endl;
	return pInode;
}

void myFileManager::WriteDir(myInode* pInode)
{
	//cout << "FileManager.WriteDir" << endl;
	myUser& u = myKernel::Instance().GetUser();

	/* ����Ŀ¼����Inode��Ų��� */
	u.u_dent.m_ino = pInode->i_number;

	/* ����Ŀ¼����pathname�������� */
	for (int i = 0; i < myDirectoryEntry::DIRSIZ; i++)
	{
		u.u_dent.m_name[i] = u.u_dbuf[i];
	}
//	cout << "����д���Ŀ¼������֣�" << u.u_dent.m_name << endl;
	u.u_IOParam.m_Count = myDirectoryEntry::DIRSIZ + 4;
	u.u_IOParam.m_Base = (unsigned char *)&u.u_dent;
	//u.u_segflg = 1;

	/* ��Ŀ¼��д�븸Ŀ¼�ļ� */
	u.u_pdir->WriteI();
	this->m_InodeTable->IPut(u.u_pdir);
//	cout << "WriteDir ����" << endl;
}

void myFileManager::SetCurDir(char* pathname)
{
	myUser& u = myKernel::Instance().GetUser();

	/* ·�����ǴӸ�Ŀ¼'/'��ʼ����������u.u_curdir������ϵ�ǰ·������ */
	if (pathname[0] != '/')
	{
		int length = strlen(u.u_curdir);
		if (u.u_curdir[length - 1] != '/')
		{
			u.u_curdir[length] = '/';
			length++;
		}
		strcpy(u.u_curdir + length, pathname);
		//Utility::StringCopy(pathname, u.u_curdir + length);
	}
	else	/* ����ǴӸ�Ŀ¼'/'��ʼ����ȡ��ԭ�й���Ŀ¼ */
	{
	//	cout << "���ɹ�fanhui" << endl;
		strcpy(u.u_curdir, pathname);
		//Utility::StringCopy(pathname, u.u_curdir);
	}
}




/*
* ����ֵ��0����ʾӵ�д��ļ���Ȩ�ޣ�1��ʾû������ķ���Ȩ�ޡ��ļ�δ�ܴ򿪵�ԭ���¼��u.u_error�����С�
*/
int myFileManager::Access(myInode* pInode, unsigned int mode)
{
//	cout << "FileManager.Access" << endl;
	myUser& u = myKernel::Instance().GetUser();

	/* ����д��Ȩ�ޣ���������ļ�ϵͳ�Ƿ���ֻ���� */
	if (myInode::IWRITE == mode)
	{
		if (this->m_FileSystem->GetFS()->s_ronly != 0)
		{
			printf("Ȩ�޼�� ����д\n");
			u.u_error = myUser::my_EROFS;
			return 1;
		}
	}
	/*
	* ���ڳ����û�����д�κ��ļ����������
	* ��Ҫִ��ĳ�ļ�ʱ��������i_mode�п�ִ�б�־
	*/
	//ע��������ֻ�г����û�
	if (true)
	{
		if (myInode::IEXEC == mode && (pInode->i_mode & (myInode::IEXEC | (myInode::IEXEC >> 3) | (myInode::IEXEC >> 6))) == 0)
		{
			u.u_error = myUser::my_EACCES;
			printf("Ȩ�޼�� ����д\n");
			return 1;
		}
		return 0;	/* Permission Check Succeed! */
	}

	u.u_error = myUser::my_EACCES;
	return 1;
}



void myFileManager::ChDir()
{
	myInode* pInode;
	myUser& u = myKernel::Instance().GetUser();
	char * dirname = u.u_dirp;
	pInode = this->NameI(myFileManager::NextChar, myFileManager::OPEN);
	if (NULL == pInode)
	{
		cout << "cd: " << dirname << ": No such file or directory" << endl;
		return;
	}
	/* ���������ļ�����Ŀ¼�ļ� */
	if ((pInode->i_mode & myInode::IFMT) != myInode::IFDIR)
	{
		cout << "cd: " << dirname << ": Not a directory" << endl;
		u.u_error = myUser::my_ENOTDIR;
		this->m_InodeTable->IPut(pInode);
		return;
	}
	if (this->Access(pInode, myInode::IEXEC))
	{
		cout << "cd: "<< dirname << ": Permission denied" << endl;
		this->m_InodeTable->IPut(pInode);
		return;
	}
	this->m_InodeTable->IPut(u.u_cdir);
	u.u_cdir = pInode;

	this->SetCurDir((char *)u.u_arg[0] /* pathname */);
}



void myFileManager::UnLink()
{
//	cout << "FIleManager.UnLink" << endl;
	myInode* pInode;
	myInode* pDeleteInode;
	myUser& u = myKernel::Instance().GetUser();

	pDeleteInode = this->NameI(myFileManager::NextChar, myFileManager::DELETE);
	if (NULL == pDeleteInode)
	{
		cout << "û���ҵ���ɾ���ļ���inode��Unlink���󷵻�" << endl;
		return;
	}

	pInode = this->m_InodeTable->IGet( u.u_dent.m_ino);
	if (NULL == pInode)
	{
		printf("�޷���ȡ��Ӧ��inode unlink ----error\n");
		//Utility::Panic("unlink -- iget");
	}
	/* ֻ��root����unlinkĿ¼�ļ� */
	//ע���϶���root
	if ((pInode->i_mode & myInode::IFMT) == myInode::IFDIR )
	{
//		cout << "��ɾ��Ŀ¼�ļ�" << endl;
		this->m_InodeTable->IPut(pDeleteInode);
		this->m_InodeTable->IPut(pInode);
		return;
	}
//	cout << "д��������Ŀ¼��" << endl;
	/* д��������Ŀ¼�� */
	u.u_IOParam.m_Offset -= (myDirectoryEntry::DIRSIZ + 4);
	u.u_IOParam.m_Base = (unsigned char *)&u.u_dent;
	u.u_IOParam.m_Count = myDirectoryEntry::DIRSIZ + 4;

	u.u_dent.m_ino = 0;
	pDeleteInode->WriteI();

	/* �޸�inode�� */
	pInode->i_nlink--;
	pInode->i_flag |= myInode::IUPD;

	this->m_InodeTable->IPut(pDeleteInode);
	this->m_InodeTable->IPut(pInode);
//	cout << "unLink�ɹ�����" << endl;
}



void myFileManager::MkNod()
{
	//cout << "fileManager.MkNod" << endl;
	myInode* pInode;
	myUser& u = myKernel::Instance().GetUser();

	/* ���uid�Ƿ���root����ϵͳ����ֻ��uid==rootʱ�ſɱ����� */
	if (true)
	{
		pInode = this->NameI(myFileManager::NextChar, myFileManager::CREATE);
		/* Ҫ�������ļ��Ѿ�����,���ﲢ����ȥ���Ǵ��ļ� */
		if (pInode != NULL)
		{
			cout << "error: File exists." << endl;
			u.u_error = myUser::my_EEXIST;
			this->m_InodeTable->IPut(pInode);
			return;
		}
	}
	else
	{
		/* ��root�û�ִ��mknod()ϵͳ���÷���User::EPERM */
		u.u_error = myUser::my_EPERM;
		return;
	}
	/* û��ͨ��SUser()�ļ�� */
	if (myUser::my_NOERROR != u.u_error)
	{
		return;	/* û����Ҫ�ͷŵ���Դ��ֱ���˳� */
	}
	pInode = this->MakNode(u.u_arg[1]);
	if (NULL == pInode)
	{
		return;
	}
	/* ���������豸�ļ� */
	if ((pInode->i_mode & (myInode::IFBLK | myInode::IFCHR)) != 0)
	{
		pInode->i_addr[0] = u.u_arg[2];
	}
	this->m_InodeTable->IPut(pInode);
}