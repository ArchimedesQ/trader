#ifndef JSGATEWAY_H
#define JSGATEWAY_H
//�����࣬�����нӿ���̳�
#include<string>
#include"structs.h"
#include"../JSTrader/eventengine/eventengine.h"
class JSGateway
{
public:
	JSGateway::JSGateway(EventEngine *eventengine, std::string gatewayname)
	{
		m_eventengine = eventengine;
		m_gatewayname = gatewayname;
	}
	void JSGateway::onTick(std::shared_ptr<Event_Tick>e)
	{
		m_eventengine->Put(e);
	}
	void JSGateway::onTrade(std::shared_ptr<Event_Trade>e)
	{
		m_eventengine->Put(e);
	}
	void JSGateway::onOrder(std::shared_ptr<Event_Order>e)
	{
		m_eventengine->Put(e);
	}
	void JSGateway::onPosition(std::shared_ptr<Event_Position>e)
	{
		m_eventengine->Put(e);
	}
	void JSGateway::onAccount(std::shared_ptr<Event_Account>e)
	{
		m_eventengine->Put(e);
	}
	void JSGateway::onError(std::shared_ptr<Event_Error>e)
	{
		m_eventengine->Put(e);
	}
	void JSGateway::onLog(std::shared_ptr<Event_Log>e)
	{
		m_eventengine->Put(e);
	}

	void JSGateway::onContract(std::shared_ptr<Event_Contract>e)
	{
		m_eventengine->Put(e);
	}
	virtual void connect() = 0;																					//����
	virtual void subscribe(SubscribeReq& subscribeReq) = 0;														//����
	virtual std::string sendOrder(OrderReq & req) = 0;															//����
	virtual void cancelOrder(CancelOrderReq & req) = 0;															//����
	virtual void qryAccount() = 0;																				//��ѯ�˻��ʽ�
	virtual void qryPosition() = 0;																				//��ѯ�ֲ�
	virtual void close() = 0;																					//�Ͽ�API
private:
	EventEngine *m_eventengine;
	std::string m_gatewayname;
};
#endif