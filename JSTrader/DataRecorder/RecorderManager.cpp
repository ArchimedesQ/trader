//////�����ǽӿڹ���
#include"RecorderManager.h"
#include"CTPMD.h"
Recordermanager::Recordermanager(EventEngine *eventengine)
{
	m_ptr_eventengine = eventengine;
	Init();
}
Recordermanager::~Recordermanager()
{
	//����
}
void Recordermanager::Init()
{
	//��ʼ�����нӿ�
	//CTP
	std::shared_ptr<CTPMD>ctpgateway = std::make_shared<CTPMD>(m_ptr_eventengine, "CTP");
	m_gatewaymap.insert(std::pair<std::string, std::shared_ptr<JSRecordClass>>("CTP", ctpgateway));
	//LTS
	//.....
}
void Recordermanager::connect(std::string gatewayname)
{
	//���ӽӿ�
	m_gatewaymap[gatewayname]->connect();//����ָ���Ľӿ�
}
void Recordermanager::subscribe(SubscribeReq req, std::string gatewayname)
{
	//��������
	m_gatewaymap[gatewayname]->subscribe(req);//����ָ���Ľӿ�
}

void Recordermanager::close(std::string gatewayname)
{
	//�˳�ָ���ӿ�
	m_gatewaymap[gatewayname]->close();
}

void Recordermanager::exit()
{
	//�ر�ȫ���ӿ�
	for (std::map<std::string, std::shared_ptr<JSRecordClass>>::iterator it = m_gatewaymap.begin(); it != m_gatewaymap.end(); it++)
	{
		it->second->close();
	}
}