#include"dualthrust.h"

/*
���ڽ���
*/
StrategyTemplate* CreateStrategy(CTAAPI *ctamanager)
{
	//��������
	StrategyTemplate *strategy = new dualthrust(ctamanager);
	g_DUALTHRUST_v.push_back(strategy);
	return strategy;
}

int ReleaseStrategy()//��Ʒ��Ҫɾ����ζ���
{
	//�ͷŲ��Զ���
	for (std::vector<StrategyTemplate*>::iterator it = g_DUALTHRUST_v.begin(); it != g_DUALTHRUST_v.end(); it++)
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


dualthrust::dualthrust(CTAAPI *ctamanager) :StrategyTemplate(ctamanager)
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

void dualthrust::onInit()
{
	StrategyTemplate::onInit();
	/*************************************************/
	unitLimit = 2;

	putEvent();
}


void dualthrust::onTick(TickData Tick)
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

}
//BAR
void dualthrust::onBar(BarData Bar)
{

	if (TradingMode == BacktestMode)
	{
		//��ǰ���ٱ䶯���Է�return�ض�
		m_VarPlotmtx.lock();

		m_VarPlot["buy"] = std::to_string(BuyPosition);
		m_VarPlot["sell"] = std::to_string(SellPosition);
		m_indicatorPlot["pos"] = Utils::doubletostring(getpos(Bar.symbol));
		m_VarPlotmtx.unlock();
	}

	m_hour = Bar.gethour();
	m_minute = Bar.getminute();

	//����5������

	if (m_ninetoeleven.find(Utils::regMySymbol(Bar.symbol)) != m_ninetoeleven.end())
	{
		if (m_hour == 22 && m_minute == 59)
		{
			return;
		}
	}
	else if (m_ninetohalfeleven.find(Utils::regMySymbol(Bar.symbol)) != m_ninetohalfeleven.end())
	{
		if (m_hour == 23 && m_minute == 29)
		{
			return;

		}
	}
	else if ((m_ninetohalfeleven.find(Utils::regMySymbol(Bar.symbol)) == m_ninetohalfeleven.end()) &&
		(m_ninetoeleven.find(Utils::regMySymbol(Bar.symbol)) == m_ninetoeleven.end()) &&
		(m_ninetoone.find(Utils::regMySymbol(Bar.symbol)) == m_ninetoone.end()) &&
		(m_ninetohalftwo.find(Utils::regMySymbol(Bar.symbol)) == m_ninetohalftwo.end()))
	{
		if (m_hour == 14 && m_minute == 59)
		{
			return;
		}
	}
	if (m_ninetoone.find(Utils::regMySymbol(Bar.symbol)) != m_ninetoone.end())
	{
		if (m_hour == 00 && m_minute == 59)
		{
			return;
		}
	}
	else if (m_ninetohalftwo.find(Utils::regMySymbol(Bar.symbol)) != m_ninetohalftwo.end())
	{
		if (m_hour == 2 && m_minute == 29)
		{
			return;
		}
	}



	if (m_minute % 5 == 0)
	{
		if (m_5Bar.close != 0)
		{
			if (!((m_hour == 11 && m_minute == 30) || (m_hour == 15 && m_minute == 00) || (m_hour == 10 && m_minute == 15)))
			{
				on5Bar(m_5Bar);
			}
		}
		BarData bar_5min;
		bar_5min.symbol = Bar.symbol;
		bar_5min.exchange = Bar.exchange;
		bar_5min.open = Bar.open;
		bar_5min.high = Bar.high;
		bar_5min.low = Bar.low;
		bar_5min.close = Bar.close;
		bar_5min.highPrice = Bar.highPrice;
		bar_5min.lowPrice = Bar.lowPrice;
		bar_5min.upperLimit = Bar.upperLimit;
		bar_5min.lowerLimit = Bar.lowerLimit;
		bar_5min.openInterest = Bar.openInterest;
		bar_5min.openPrice = Bar.openPrice;
		bar_5min.volume = Bar.volume;
		bar_5min.date = Bar.date;
		bar_5min.time = Bar.time;
		bar_5min.unixdatetime = Bar.unixdatetime;

		m_5Bar = bar_5min;
	}
	else
	{
		m_5Bar.high = std::max(m_5Bar.high, Bar.high);
		m_5Bar.low = std::min(m_5Bar.low, Bar.low);
		m_5Bar.close = Bar.close;
		m_5Bar.volume += Bar.volume;
		m_5Bar.highPrice = Bar.highPrice;
		m_5Bar.lowPrice = Bar.lowPrice;
	}

	if (lastHour >= 375)
	{
		if (m_dailyBar.close != 0)
		{
			if (!((m_hour == 11 && m_minute == 30) || (m_hour == 15 && m_minute == 00) || (m_hour == 10 && m_minute == 15)))
			{
				ondailyBar(m_dailyBar);
			}
		}
		BarData bar_5min;
		bar_5min.symbol = Bar.symbol;
		bar_5min.exchange = Bar.exchange;
		bar_5min.open = Bar.open;
		bar_5min.high = Bar.high;
		bar_5min.low = Bar.low;
		bar_5min.close = Bar.close;
		bar_5min.highPrice = Bar.highPrice;
		bar_5min.lowPrice = Bar.lowPrice;
		bar_5min.upperLimit = Bar.upperLimit;
		bar_5min.lowerLimit = Bar.lowerLimit;
		bar_5min.openInterest = Bar.openInterest;
		bar_5min.openPrice = Bar.openPrice;
		bar_5min.volume = Bar.volume;
		bar_5min.date = Bar.date;
		bar_5min.time = Bar.time;
		bar_5min.unixdatetime = Bar.unixdatetime;

		m_dailyBar = bar_5min;
		m_minute = m_minute;
		m_hour = m_hour;
		lastHour = 0;
	}
	else
	{
		m_dailyBar.high = std::max(m_dailyBar.high, Bar.high);
		m_dailyBar.low = std::min(m_dailyBar.low, Bar.low);
		m_dailyBar.close = Bar.close;
		m_dailyBar.volume += Bar.volume;
		m_dailyBar.highPrice = Bar.highPrice;
		m_dailyBar.lowPrice = Bar.lowPrice;
		lastHour += 1;
	}


	if (TradingMode == BacktestMode)
	{
		m_algorithm->setTradingMode(BacktestMode);
		m_algorithm->checkPositions_Bar(&Bar);
	}
	putEvent();
}

void dualthrust::on5Bar(BarData Bar)
{
	Bars += 1;
	if (Bars > 44)
	{
		if (high_vector.size()> 75)
		{
			high_vector.erase(high_vector.begin());
			close_vector.erase(close_vector.begin());
			low_vector.erase(low_vector.begin());
		}
		high_vector.push_back(Bar.high);
		close_vector.push_back(Bar.close);
		low_vector.push_back(Bar.low);

		if (close_vector.size() < 75)
		{
			return;
		}

		double ma10 = TALIB::MA(close_vector, TA_MAType_SMA, 5).back();

		
		if (Bar.close > MA100)
		{
			K1 = 0.5;
			K2 = 0.7;
		}
		else
		{
			K1 = 0.7;
			K2 = 0.5;
		}


		if (m_hour == 9 && m_minute == 00)
		{
			std::vector<double>::iterator highd = std::max_element(std::begin(high_vector), std::end(high_vector));
			std::vector<double>::iterator lowd = std::min_element(std::begin(low_vector), std::end(low_vector));
			HH = *highd;
			HC = close_vector[close_vector.size()-2];
			LL = *lowd;
			LC = close_vector[close_vector.size() - 2];
			if ((HH - HC) >= (HC - LL))
			{
				SellRange = HH - LC;
			}
			else
			{
				SellRange = HC - LL;
			}
			if ((HH - LC) >= (HC - LL))
			{
				BuyRange = HH - LC;
			}
			else
			{
				BuyRange = HC - LL;
			}

			BuyTrig = K1*BuyRange;
			SellTrig = K2 *SellTrig;

			BuyPosition = Bar.open + BuyTrig;
			SellPosition = Bar.open - SellTrig;

			TimesToday = 0;
		}

		if (TimesToday < TimesMaxToday &&Bars>90)
		{
			if (getpos(Bar.symbol) ==0)
			{
				if (ma10 >= BuyPosition)
				{
					m_algorithm->set_supposedPos(Bar.symbol, 1);
					TimesToday += 1;
				}
			}
			if (getpos(Bar.symbol)==0)
			{
				if (ma10 <= SellPosition)
				{
					m_algorithm->set_supposedPos(Bar.symbol, -1);
				}
			}
		}
		if (getpos(Bar.symbol) == 1 && Bar.low <= SellPosition)
		{
			m_algorithm->set_supposedPos(Bar.symbol, 0);
			TimesToday += 1;
		}
		if (getpos(Bar.symbol) == -1 && Bar.high >= BuyPosition)
		{
			m_algorithm->set_supposedPos(Bar.symbol, 0);
			TimesToday += 1;
		}

	}
}

void dualthrust::ondailyBar(BarData Bar)
{
	if (close_daily.size()> 5)
	{
		close_daily.erase(close_daily.begin());

	}
	close_daily.push_back(Bar.close);


	if (close_vector.size() < 5)
	{
		return;
	}

	MA100 = TALIB::MA(close_vector, TA_MAType_SMA, 5).back();
}

//�����ص�
void dualthrust::onOrder(std::shared_ptr<Event_Order>e)
{


}
//�ɽ��ص�
void dualthrust::onTrade(std::shared_ptr<Event_Trade>e)
{

}

void dualthrust::savepostomongo()
{

	////��Ҫupdate
	//bson_t *query;
	//bson_t *update;
	//query = BCON_NEW("strategyname", BCON_UTF8(m_strategydata->getparam("name").c_str()));
	//std::map<std::string, double>map = getposmap();
	//for (std::map<std::string, double>::iterator it = map.begin(); it != map.end(); it++)
	//{
	//	update = BCON_NEW("$set", "{", BCON_UTF8(it->first.c_str()), BCON_DOUBLE(it->second), BCON_UTF8("holdingPrice"), BCON_DOUBLE(holdingPrice), "}");
	//	m_MongoCxx->updateData(query, update, "StrategyPos", "pos");
	//}
}

void dualthrust::loadposfrommongo()
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
			if (it->first == "holdingPrice")
			{
				//	holdingPrice = it->second.number_value();
			}
		}
	}
}

//���²���������
void dualthrust::putEvent()
{
	m_strategydata->insertvar("inited", Utils::booltostring(inited));
	m_strategydata->insertvar("trading", Utils::booltostring(trading));
	//���²�λ
	std::map<std::string, double>map = getposmap();
	for (std::map<std::string, double>::iterator iter = map.begin(); iter != map.end(); iter++)
	{
		m_strategydata->insertvar(("pos_" + iter->first), Utils::doubletostring(iter->second));
	}

	m_strategydata->insertvar("lastprice", Utils::doubletostring(lastprice));

	//�������ͱ������ݵ�������ȥ
	std::shared_ptr<Event_UpdateStrategy>e = std::make_shared<Event_UpdateStrategy>();
	e->parammap = m_strategydata->getallparam();
	e->varmap = m_strategydata->getallvar();
	e->strategyname = m_strategydata->getparam("name");
	m_ctamanager->PutEvent(e);
}