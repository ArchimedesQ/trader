#ifndef GATEWAYMANAGER_H
#define GATEWAYMANAGER_H
#include "EventEngine.h"
#include "structs.h"
#include "ctpgateway.h"
#include "JSgateway.hpp"
class Gatewaymanager
{
public:
	Gatewaymanager(EventEngine *eventengine);
	~Gatewaymanager();
	void Init();
	void connect(std::string gatewayname);
	void subscribe(SubscribeReq req, std::string gatewayname);
	std::string sendOrder(OrderReq req, std::string gatewayname);
	void cancelOrder(CancelOrderReq req, std::string gatewayname);
	void qryAccont(std::string gatewayname);
	void qryPosition(std::string gatewayname);
	std::shared_ptr<Event_Contract>getContract(std::string symbol);
	std::shared_ptr<Event_Contract>Find_Contract(std::string symbol);
	std::shared_ptr<Event_Order>getorder(std::string gatewayname, std::string orderID);
	void close(std::string gatewayname);
	void exit();
private:
	void getallContract(std::shared_ptr<Event>e);
	EventEngine *m_eventengine;
	std::map<std::string, std::shared_ptr<JSGateway>>m_gatewaymap;			std::mutex gatewaymtx;                               //����gateway������gateway����Ķ�Ӧ��ϵ
	std::map<std::string, std::shared_ptr<Event_Contract>>m_contractmap;    std::mutex contractmutex;  std::condition_variable contractcv;//����symbol�ͺ�Լָ��
};
#endif