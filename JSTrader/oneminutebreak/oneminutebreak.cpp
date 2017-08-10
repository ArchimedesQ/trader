#include"oneminutebreak.h"

/*

*/
StrategyTemplate* CreateStrategy(CTAAPI *ctamanager)
{
	//��������
	StrategyTemplate *strategy = new Oneminutebreak(ctamanager);
	g_ONEMINUTEBREAK_v.push_back(strategy);
	return strategy;
}

int ReleaseStrategy()//��Ʒ��Ҫɾ����ζ���
{
	//�ͷŲ��Զ���
	for (std::vector<StrategyTemplate*>::iterator it = g_ONEMINUTEBREAK_v.begin(); it != g_ONEMINUTEBREAK_v.end(); it++)
	{
		if ((*it) != nullptr)
		{
			//ɾ��ָ��
			delete *it;
			*it = nullptr;
		}
	}
	return 0;
}

Oneminutebreak::Oneminutebreak(CTAAPI *ctamanager) :StrategyTemplate(ctamanager)
{
	//��������
	m_ctamanager = ctamanager;
	trademode = BAR_MODE;
	tickDbName = "CTPTickDb";
	BarDbName = "CTPMinuteDb";
	gatewayname = "CTP";
	initDays = 10;
	unitLimit = 2;

	/*****************ҹ������ƽ��*************************/
	std::string ninetoeleven[] = { "bu", "rb", "hc", "ru" };//9�㵽11��ĺ�Լ�б�
	std::string ninetohalfeleven[] = { "p", "j", "m", "y", "a", "b", "jm", "i", "SR", "CF", "RM", "MA", "ZC", "FG", "OI" };//9�㵽11���ĺ�Լ
	std::string ninetoone[] = { "cu", "al", "zn", "pb", "sn", "ni" };//9�㵽1��ĺ�Լ�б�
	std::string ninetohalftwo[] = { "ag", "au" };//9�㵽2���ĺ�Լ
	for (int i = 0; i < sizeof(ninetoeleven) / sizeof(ninetoeleven[0]); ++i)
	{
		m_ninetoeleven.insert(ninetoeleven[i]);
	}
	for (int i = 0; i < sizeof(ninetohalfeleven) / sizeof(ninetohalfeleven[0]); ++i)
	{
		m_ninetohalfeleven.insert(ninetohalfeleven[i]);
	}
	for (int i = 0; i < sizeof(ninetoone) / sizeof(ninetoone[0]); ++i)
	{
		m_ninetoone.insert(ninetoone[i]);
	}
	for (int i = 0; i < sizeof(ninetohalftwo) / sizeof(ninetohalftwo[0]); ++i)
	{
		m_ninetohalftwo.insert(ninetohalftwo[i]);
	}
}
//TICK
void Oneminutebreak::onInit()
{
	StrategyTemplate::onInit();
	/*************************************************/

	putEvent();
}

void Oneminutebreak::onTick(TickData Tick)
{

	putEvent();

	int tickMinute = Tick.getminute();
	int tickHour = Tick.gethour();
	m_algorithm->checkPositions_Tick(&Tick);
	m_hourminutemtx.lock();
	if ((tickMinute != m_minute) || tickHour != m_hour)
	{
		if (m_bar.close != 0)
		{
			if (!((m_hour == 11 && m_minute == 30) || (m_hour == 15 && m_minute == 00) || (m_hour == 10 && m_minute == 15)))
			{
				onBar(m_bar);
			}
		}
		BarData bar;
		bar.symbol = Tick.symbol;
		bar.exchange = Tick.exchange;
		bar.open = Tick.lastprice;
		bar.high = Tick.lastprice;
		bar.low = Tick.lastprice;
		bar.close = Tick.lastprice;
		bar.highPrice = Tick.highPrice;
		bar.lowPrice = Tick.lowPrice;
		bar.upperLimit = Tick.upperLimit;
		bar.lowerLimit = Tick.lowerLimit;
		bar.openInterest = Tick.openInterest;
		bar.openPrice = Tick.openPrice;
		bar.volume = Tick.volume;
		bar.date = Tick.date;
		bar.time = Tick.time;
		bar.unixdatetime = Tick.unixdatetime;

		m_bar = bar;
		m_minute = tickMinute;
		m_hour = tickHour;
	}
	else
	{
		m_bar.high = std::max(m_bar.high, Tick.lastprice);
		m_bar.low = std::min(m_bar.low, Tick.lastprice);
		m_bar.close = Tick.lastprice;
		m_bar.volume = Tick.volume;
		m_bar.highPrice = Tick.highPrice;
		m_bar.lowPrice = Tick.lowPrice;
	}
	lastprice = Tick.lastprice;
	m_hourminutemtx.unlock();
	//��λ����

}
//BAR
void Oneminutebreak::onBar(BarData Bar)
{
	
	/*������������*/
	std::unique_lock<std::mutex>lck(m_HLCmtx);
	if (close_vector.size() >= 100)
	{
		close_vector.erase(close_vector.begin());
	}
	close_vector.push_back(Bar.close);
	if (close_vector.size() == 100)
	{
		std::vector<double>::iterator biggest_100 = std::max_element(std::begin(close_vector), std::end(close_vector));
		std::vector<double>::iterator smallest_100 = std::min_element(std::begin(close_vector), std::end(close_vector));

		if (high100.size() > 100)
		{
			high100.erase(high100.begin());
			low100.erase(low100.begin());
		}

		high100.push_back(*biggest_100);
		low100.push_back(*smallest_100);

		if (high100.size() <= 3)
		{
			return;
		}
		high_100 = high100.back();
		low_100 = low100.back();

		double D = high100.back() - low100.back();

		if (longdirectionCount >= 30)
		{
			longdirectionCount = 0;
		}

		if (shortdirectionCount >= 30)
		{
			shortdirectionCount = 0;
		}

		if (longdirectionCount >= 1)
		{
			longdirectionCount++;
		}

		if (shortdirectionCount >= 1)
		{
			shortdirectionCount++;
		}

		if (supposlongpos == 0 && supposshortpos == 0)
		{
			if (long_direction_trading_flag == false || short_direction_trading_flag == false){
				m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��տ�������", "oneminutebreak");
			}
			long_direction_trading_flag = true;
			short_direction_trading_flag = true;
		}

		if ((Bar.gethour() == 9 && Bar.getminute() == 0) || (Bar.gethour() == 21 && Bar.getminute() == 0))
		{
			if (D >= 135)
			{
				if (supposlongpos > 0)
				{
					if (long_direction_trading_flag == true)
					{
						supposlongpos = 0;
						long_holdingPrice = Bar.close;
						m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��ͷ100��-100�ʹ���135��ƽ��", "oneminutebreak");
					}
				}
				if (supposshortpos)
				{
					if (short_direction_trading_flag == true)
					{
						supposshortpos = 0;
						short_holdingPrice = Bar.close;
						m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��ͷ100��-100�ʹ���135��ƽ��", "oneminutebreak");
					}
				}
			}
		}

		/*�߼��ж�*/
		if (supposlongpos == 0 && D >= 45 && D <= 85)
		{
			if (Bar.close >= high100.back())
			{
				if (long_direction_trading_flag == true)
				{
					m_ctamanager->writeCtaLog(Bar.date + Bar.time + "���࿪��", "oneminutebreak");
					supposlongpos = 1;
					long_holdingPrice = ((abs(supposlongpos) - 1)*long_holdingPrice + 1 * Bar.close) / abs(1);
					short_direction_trading_flag = false;
					m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��ͷ���عر�", "oneminutebreak");
				}
			}
		}

		if (supposshortpos == 0 && D >= 45 && D <= 85)
		{
			if (Bar.close <= low100.back())
			{
				if (short_direction_trading_flag == true)
				{
					m_ctamanager->writeCtaLog(Bar.date + Bar.time + "���տ���", "oneminutebreak");
					supposshortpos = 1;
					short_holdingPrice = ((abs(supposshortpos) -1)*short_holdingPrice +1 * Bar.close) / abs(1);
					long_direction_trading_flag = false;
					m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��ͷ���عر�", "oneminutebreak");
				}
			}
		}
		/*�Ӳ��ж�*/
		if (supposlongpos >=1)
		{
			if (low100[low100.size() - 1] >= low100[low100.size() - 2]){
				if ((Bar.close < low100.back() + 5) && (Bar.close > low100.back()) && (longdirectionCount == 0) && D >= 30)
				{
					if (long_direction_trading_flag == true)
					{
						supposlongpos+=1;
						longdirectionCount++;
						long_holdingPrice = ((abs(supposlongpos) - 1)*long_holdingPrice + 1 * Bar.close) / abs(supposlongpos);
						m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��100low����㼰��������Ӳ�", "oneminutebreak");
					}
				}
			}
		}
		if (supposshortpos >= 1)
		{
			if (high100[high100.size() - 1] <= high100[high100.size() - 2])
			{
				if ((Bar.close > high100.back() - 5) && (Bar.close <= high100.back()) && (shortdirectionCount == 0) && D >= 30)
				{
					if (short_direction_trading_flag == true)
					{
						supposshortpos+=1;
						shortdirectionCount++;
						short_holdingPrice = ((abs(supposshortpos) - 1)*short_holdingPrice + 1* Bar.close) / abs(supposshortpos);
						m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��100high����㼰�������ռӲ�", "oneminutebreak");
					}
				}
			}
		}


		/*ֹ���ж�*/
		if (long_holdingPrice == 0)
		{
			long_holdingPrice = Bar.close;
		}
		long_holdingProfit = (Bar.close - long_holdingPrice)* supposlongpos * 10;
		if (long_holdingProfit < -1000)
		{
			if (supposlongpos > 0)
			{
				if (long_direction_trading_flag == true)
				{
					m_ctamanager->writeCtaLog(Bar.date + Bar.time + "TICK��ͷֹ�� ����:" + std::to_string(supposlongpos), "oneminutebreak");
					supposlongpos = 0;
					long_holdingPrice = Bar.close;
				}
			}
		}

		if (short_holdingPrice == 0)
		{
			short_holdingPrice = Bar.close;
		}
		short_holdingProfit = (short_holdingPrice - Bar.close)* supposshortpos * 10;
		if (short_holdingProfit < -1000)
		{
			if (supposshortpos > 0)
			{
				if (short_direction_trading_flag == true)
				{
					m_ctamanager->writeCtaLog(Bar.date + Bar.time + "TICK��ͷֹ�� ����:" + std::to_string(supposshortpos), "oneminutebreak");
					supposshortpos = 0;
					short_holdingPrice = Bar.close;
				}
			}
		}
		/*ֹӯ�ж�*/
		if (long_holdingPrice == 0)
		{
			long_holdingPrice = Bar.close;
		}
		long_holdingProfit = (Bar.close - long_holdingPrice)* supposlongpos * 10;
		if (long_holdingProfit > 10000)
		{
			if (supposlongpos > 0)
			{
				if (long_direction_trading_flag == true)
				{
					supposlongpos = 0;
					long_holdingPrice = Bar.close;
					m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��ͷ���󳬹�1��ƽ��", "oneminutebreak");
				}
			}
		}

		if (short_holdingPrice == 0)
		{
			short_holdingPrice = Bar.close;
		}
		short_holdingProfit = (short_holdingPrice - Bar.close)* supposshortpos * 10;
		if (short_holdingProfit > 10000)
		{
			if (supposshortpos > 0)
			{
				if (short_direction_trading_flag == true)
				{
					supposshortpos = 0;
					short_holdingPrice = Bar.close;
					m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��ͷ���󳬹�1��ƽ��", "oneminutebreak");
				}
			}
		}

		if (supposshortpos > 6)
		{
			if (short_direction_trading_flag == true)
			{
				supposshortpos = 0;
				m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��ͷ�Ӳֳ���6�� ƽ��", "oneminutebreak");
			}
		}

		if (supposlongpos > 6)
		{
			if (long_direction_trading_flag == true)
			{
				supposlongpos = 0;
				m_ctamanager->writeCtaLog(Bar.date + Bar.time + "��ͷ�Ӳֳ���6�� ƽ��", "oneminutebreak");
			}
		}

		m_algorithm->set_supposedPos(Bar.symbol, supposlongpos - supposshortpos);

	}
	/*�ز�*/
	if (TradingMode == BacktestMode)
	{
		if (high100.size() <= 3)
		{
			return;
		}
		std::unique_lock<std::mutex>lck(m_VarPlotmtx);
		m_VarPlot["high100"] = Utils::doubletostring(high100.back());
		m_VarPlot["low100"] = Utils::doubletostring(low100.back());

		m_VarPlot["short_holdingPrice"] = Utils::doubletostring(short_holdingPrice);
		m_VarPlot["long_holdingPrice"] = Utils::doubletostring(long_holdingPrice);

		m_indicatorPlot["pos"] = Utils::doubletostring(getpos(Bar.symbol));
		m_indicatorPlot["pos_long"] = Utils::doubletostring(supposlongpos);
		m_indicatorPlot["pos_short"] = Utils::doubletostring(supposshortpos);
		m_algorithm->setTradingMode(BacktestMode);
		m_algorithm->checkPositions_Bar(&Bar);
	}
	putEvent();
}
//�����ص�
void Oneminutebreak::onOrder(std::shared_ptr<Event_Order>e)
{


}
//�ɽ��ص�
void Oneminutebreak::onTrade(std::shared_ptr<Event_Trade>e)
{

}
//���²���������
void Oneminutebreak::savepostomongo()
{
	//��Ҫupdate
	bson_t *query;
	bson_t *update;
	query = BCON_NEW("strategyname", BCON_UTF8(m_strategydata->getparam("name").c_str()));
	std::map<std::string, double>map = getposmap();
	for (std::map<std::string, double>::iterator it = map.begin(); it != map.end(); it++)
	{
		update = BCON_NEW("$set", "{", BCON_UTF8(it->first.c_str()), BCON_DOUBLE(it->second),
			BCON_UTF8("long_holdingPrice"), BCON_DOUBLE(long_holdingPrice),
			BCON_UTF8("short_holdingPrice"), BCON_DOUBLE(short_holdingPrice),
			BCON_UTF8("supposlongpos"), BCON_DOUBLE(supposlongpos),
			BCON_UTF8("supposshortpos"), BCON_DOUBLE(supposshortpos),
			BCON_UTF8("short_direction_trading_flag"), BCON_DOUBLE(short_direction_trading_flag),
			BCON_UTF8("long_direction_trading_flag"), BCON_DOUBLE(long_direction_trading_flag),
			"}");
		char * str = bson_as_json(update, NULL);
		m_MongoCxx->updateData(query, update, "StrategyPos", "pos");
	}
}

void Oneminutebreak::loadposfrommongo()
{
	std::vector<std::string>result;

	bson_t query;

	bson_init(&query);

	m_MongoCxx->append_utf8(&query, "strategyname", m_strategydata->getparam("name").c_str());

	result = m_MongoCxx->findData(&query, "StrategyPos", "pos");

	for (std::vector<std::string>::iterator iter = result.begin(); iter != result.end(); iter++)
	{
		std::string s = (*iter);
		std::string err;


		auto json_parsed = json11::Json::parse(s, err);
		if (!err.empty())
		{
			return;
		}
		auto amap = json_parsed.object_items();

		std::map<std::string, double>temp_posmap = getposmap();
		for (std::map<std::string, json11::Json>::iterator it = amap.begin(); it != amap.end(); it++)
		{
			if (temp_posmap.find(it->first) != temp_posmap.end())
			{
				changeposmap(it->first, it->second.number_value());
			}

			if (it->first == "long_holdingPrice")
			{
				long_holdingPrice = it->second.number_value();
			}

			if (it->first == "short_holdingPrice")
			{
				short_holdingPrice = it->second.number_value();
			}

			if (it->first == "supposlongpos")
			{
				supposlongpos = it->second.number_value();
			}

			if (it->first == "supposshortpos")
			{
				supposshortpos = it->second.number_value();
			}

			if (it->first == "long_direction_trading_flag")
			{
				long_direction_trading_flag = it->second.bool_value();
			}

			if (it->first == "short_direction_trading_flag")
			{
				short_direction_trading_flag = it->second.bool_value();
			}

		}
	}
}
//���²���������
void Oneminutebreak::putEvent()
{
	m_strategydata->insertvar("inited", Utils::booltostring(inited));
	m_strategydata->insertvar("trading", Utils::booltostring(trading));
	m_strategydata->insertvar("high100", Utils::doubletostring(high_100));
	m_strategydata->insertvar("low100", Utils::doubletostring(low_100));
	m_strategydata->insertvar("short_hold", Utils::doubletostring(short_holdingPrice));
	m_strategydata->insertvar("long_hold", Utils::doubletostring(long_holdingPrice));
	//���²�λ
	std::map<std::string, double>map = getposmap();
	for (std::map<std::string, double>::iterator iter = map.begin(); iter != map.end(); iter++)
	{
		m_strategydata->insertvar(("pos_" + iter->first), Utils::doubletostring(iter->second));
	}
	m_strategydata->insertvar("longpos", Utils::doubletostring(supposlongpos));
	m_strategydata->insertvar("shortpos", Utils::doubletostring(supposshortpos));
	m_strategydata->insertvar("lastprice", Utils::doubletostring(lastprice));

	//�������ͱ������ݵ�������ȥ
	std::shared_ptr<Event_UpdateStrategy>e = std::make_shared<Event_UpdateStrategy>();
	e->parammap = m_strategydata->getallparam();
	e->varmap = m_strategydata->getallvar();
	e->strategyname = m_strategydata->getparam("name");
	m_ctamanager->PutEvent(e);
}
