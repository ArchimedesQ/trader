#ifndef CTPGATEWAY_H
#define CTPGATEWAY_H
#ifdef  CTPGATEWAY_EXPORTS
#define CTPGATEWAY_API __declspec(dllexport)
#else
#define CTPGATEWAY_API __declspec(dllimport)
#endif
#include<map>
#include<atomic>
#include<io.h>
#include<direct.h>
#include<fstream>
#include<memory>
#include"CTPAPI/ThostFtdcMdApi.h"
#include"CTPAPI/ThostFtdcTraderApi.h"
#include"JSgateway.hpp"
#include"eventengine.h"
#include"md.h"
#include"td.h"
class CTPTD;										//��ǰ����
class CTPMD;										//��ǰ����
class CTPGATEWAY_API CTPGateway : public JSGateway//�̳г�����JSGATEWAY
{
public:
	explicit CTPGateway(EventEngine *eventengine, std::string gatewayname = "CTP");
	~CTPGateway();
	//����״̬
	std::atomic_bool ctpmdconnected;
	std::atomic_bool ctptdconnected;
	//��������
	void connect();											//����
	void subscribe(SubscribeReq& subscribeReq);				//����
	std::string sendOrder(OrderReq & req);					//����
	void cancelOrder(CancelOrderReq & req);					//����
	void qryAccount();										//���˻�
	void qryPosition();										//��ֲ�
	void close();											//�˳�
	void initQuery();										//��ʼ����ѯѭ��
	void query();											//��ѯ
	std::shared_ptr<Event_Order>getorder(std::string orderID);		//��ȡorder
	//����ά��
	std::map<std::string, std::shared_ptr<Event_Order>>m_ordermap; std::mutex m_ordermapmtx;//���ί�е�map

private:
	//�¼�����
	EventEngine * m_eventengine = nullptr;
	//����ͽ���
	CTPMD *m_MDSPI;
	CTPTD *m_TDSPI;
	//�ӿ���
	std::string m_gatewayname;
	//��ѯ��ر���
	std::atomic_int m_qrycount;
	const int m_maxqry =4;									//����4���ٴ�0��ʼ
	std::atomic_bool m_qryEnabled;
};
#endif