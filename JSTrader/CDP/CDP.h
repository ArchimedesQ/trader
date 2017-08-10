#ifndef CDP_H
#define CDP_H
#ifdef  CDP_EXPORTS
#define CDP_API __declspec(dllexport)
#else
#define CDP_API __declspec(dllimport)
#endif
#include"StrategyTemplate.h"
extern "C" CDP_API StrategyTemplate * CreateStrategy(CTAAPI *ctamanager);//��������
extern "C" CDP_API int ReleaseStrategy();//�ͷŲ���
std::vector<StrategyTemplate*>g_CDP_v;//ȫ��ָ��

#include<vector>
#include<algorithm>
#include<regex>
#include"talib.h"
#include"libbson-1.0\bson.h"
#include"libmongoc-1.0\mongoc.h"
class CDP_API CDP : public StrategyTemplate
{
public:
	CDP(CTAAPI *ctamanager);
	~CDP();

	void onInit();
	//TICK
	void onTick(TickData Tick);
	//BAR
	void onBar(BarData Bar);

	//10Bar
	void on10Bar(BarData Bar);
	//�����ص�
	void onOrder(std::shared_ptr<Event_Order>e);
	//�ɽ��ص�
	void onTrade(std::shared_ptr<Event_Trade>e);


	//����
	double unit_volume;


	//ָ��
	std::mutex m_HLCmtx;
	std::vector<double>close_vector;
	std::vector<double>high_vector;
	std::vector<double>low_vector;

	std::vector<double>close_10min_vector;
	double dayHIGH;
	double dayLOW;
	double dayClose;

	double cdp;
	double ah;
	double nh;
	double nl;
	double al;

	double todayEntry;

	std::set<std::string> m_ninetoeleven;
	std::set<std::string> m_ninetohalfeleven;
	std::set<std::string> m_ninetoone;
	std::set<std::string> m_ninetohalftwo;
	BarData Bar_10min;

	//������
	double lastprice;				//���½���
	//���½���
	void putEvent();
};
#endif