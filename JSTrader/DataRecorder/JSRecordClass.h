#ifndef __JSGATEWAY_H__
#define __JSGATEWAY_H__
//�����࣬�����нӿ���̳�
#include<string>
#include"structs.h"
#include"eventengine.h"
class JSRecordClass
{
public:
	JSRecordClass(EventEngine *eventengine, std::string gatewayname);
	void onTick(std::shared_ptr<Event_Tick>e);
	void onContract(std::shared_ptr<Event_Contract>e);
	void onError(std::shared_ptr<Event_Error>e);
	void onLog(std::shared_ptr<Event_Log>e);
	virtual void connect()=0;//����
	virtual void subscribe(SubscribeReq& subscribeReq) = 0;//����
	virtual void close() = 0;//�Ͽ�API
private:
	EventEngine *m_ptr_eventengine;
	std::string m_gatewayname;
};

#endif