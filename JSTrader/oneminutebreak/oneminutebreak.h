#ifndef ONEMINUTEBREAK_H
#define ONEMINUTEBREAK_H
#ifdef ONEMINUTEBREAK_EXPORTS
#define ONEMINUTEBREAK_API __declspec(dllexport)
#else
#define ONEMINUTEBREAK_API __declspec(dllimport)
#endif
#include"StrategyTemplate.h"
extern "C" ONEMINUTEBREAK_API StrategyTemplate * CreateStrategy(CTAAPI *ctamanager);//��������
extern "C" ONEMINUTEBREAK_API int ReleaseStrategy();//�ͷŲ���
std::vector<StrategyTemplate*>g_ONEMINUTEBREAK_v;//ȫ��ָ��

#include<vector>
#include<algorithm>
#include<regex>
#include"talib.h"
#include"libbson-1.0\bson.h"
#include"libmongoc-1.0\mongoc.h"
class ONEMINUTEBREAK_API Oneminutebreak : public StrategyTemplate
{
public:
	Oneminutebreak(CTAAPI *ctamanager);
	~Oneminutebreak();

	void onInit();
	//TICK
	void onTick(TickData Tick);
	//BAR
	void onBar(BarData Bar);
	//�����ص�
	void onOrder(std::shared_ptr<Event_Order>e);
	//�ɽ��ص�
	void onTrade(std::shared_ptr<Event_Trade>e);

	void loadposfrommongo();

	void savepostomongo();

	//ָ��
	std::mutex m_HLCmtx;
	std::vector<double>close_vector;

	/*һ�ٸ�һ�ٵͣ�ֹ��λ ֹӯλ*/
	std::vector<double> high100;
	std::vector<double> low100;


	double high_100;
	double low_100;

	int supposlongpos = 0;
	int supposshortpos = 0;

	int longdirectionCount=0;
	int shortdirectionCount=0;

	bool long_direction_trading_flag = true;
	bool short_direction_trading_flag = true;
	 
	double long_holdingProfit;
	double long_holdingPrice;
	double short_holdingProfit;
	double short_holdingPrice;
 

	std::set<std::string> m_ninetoeleven;
	std::set<std::string> m_ninetohalfeleven;
	std::set<std::string> m_ninetoone;
	std::set<std::string> m_ninetohalftwo;
	//������
	double lastprice;				//���½���
	//���½���
	void putEvent();
};
#endif