// stdafx.cpp : ֻ������׼�����ļ���Դ�ļ�
// FlashDBSimDll.pch ����ΪԤ����ͷ
// stdafx.obj ������Ԥ����������Ϣ

#include "stdafx.h"

unsigned int GUID_equals(IID iid1, IID iid2){
	if(iid1.Data1 != iid2.Data1)
		return 0;
	if(iid1.Data2 != iid2.Data2)
		return 0;
	if(iid1.Data3 != iid2.Data3)
		return 0;
	for(int i=0; i<8; i++){
		if(iid1.Data4[i] != iid2.Data4[i])
			return 0;
	}
	return 1;
}