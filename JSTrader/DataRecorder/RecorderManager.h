//�����������ӿڵ����ݻر�ͨ����
#include"eventengine.h"
#include"JSRecordClass.h"
#include"structs.h"
class Recordermanager
{
public:
	//ͨ�ýӿ�����
	Recordermanager(EventEngine *eventengine);
	~Recordermanager();
	void Init();
	void connect(std::string gatewayname);
	void subscribe(SubscribeReq req, std::string gatewayname);
	void close(std::string gatewayname);
	void exit();
private:
	EventEngine *m_ptr_eventengine;
	std::map<std::string, std::shared_ptr<JSRecordClass>>m_gatewaymap;//����gateway������gateway����Ķ�Ӧ��ϵ
};