#include "httplib.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <codecvt>

#include <json/json.h>

using namespace std;

struct AREA
{
	AREA(string name, int confirmedCount, int deadCount, int curedCount)
		:name(name)
		, confirmedCount(confirmedCount)
		, deadCount(deadCount)
		, curedCount(curedCount)
	{}

	AREA()
		:name("查询失败,暂无此数据")
		, confirmedCount(0)
		, deadCount(0)
		, curedCount(0)
	{}

	string name;
	int confirmedCount;//确诊
	int deadCount;//死亡
	int curedCount;//治愈
};

bool AREAComp(const AREA& a, const AREA& b)
{
	return a.confirmedCount > b.confirmedCount;
}


class Plague
{

private:

	std::wstring UTF8ToUnicode(const std::string& str)
	{
		std::wstring ret;
		try {
			std::wstring_convert< std::codecvt_utf8<wchar_t> > wcv;
			ret = wcv.from_bytes(str);
		}
		catch (const std::exception & e) {
			//std::cerr << e.what() << std::endl;
		}
		return ret;
	}

	std::string UnicodeToANSI(const std::wstring& wstr)
	{
		string result;
		//获取缓冲区大小，并申请空间，缓冲区大小事按字节计算的  
		int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
		char* buffer = new char[len + 1];
		//宽字节编码转换成多字节编码  
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), buffer, len, NULL, NULL);
		buffer[len] = '\0';
		//删除缓冲区并返回值  
		result.append(buffer);
		delete[] buffer;
		return result;
	}

	std::string UTF8ToANSI(const std::string str)
	{
		return UnicodeToANSI(UTF8ToUnicode(str));
	}

	std::string substring(std::string buf, const std::string start, const std::string end)
	{
		int subStart = buf.find(start);
		if (subStart != buf.npos)
		{
			subStart += start.size();
		}
		else
		{
			throw exception("分割字符串头错误");
		}
		buf = buf.substr(subStart);


		int subEnd = buf.find(end);//- subStart;
		if (subEnd == buf.npos)
		{
			throw exception("分割字符串尾错误");
		}
		buf = buf.substr(0, subEnd);

		return buf;
	}

	//下载图片 并返回图片名称
	string downImg(string host, string path)
	{

		httplib::Client client(host, 80);

		auto ret = client.Get(path.c_str());
		if (!ret || ret->status != 200)
		{
			throw exception("图片下载失败 网络异常");
		}

		string downPath("Plague" + path.substr(path.rfind("/") + 1));


		ofstream file("data\\image\\" + downPath, ios::out | ios::binary);
		file << ret->body;
		file.close();

		return downPath;
	}

	//获取数据
	bool getData(string& error)
	{
		httplib::Client cli("3g.dxy.cn", 80);

		auto res = cli.Get("/newh5/view/pneumonia");

		if (!res || res->status != 200)
		{
			error = "获取数据失败 网络异常";
		}
		else
		{

			html = UTF8ToANSI(res->body);
			//string json = substring(buf, "window.getTimelineService =", "}catch(e){}");
			string json = substring(html, "<script id=\"getAreaStat\">try { window.getAreaStat = ", "}catch(e){}");

			Json::Reader reader;
			reader.parse(json, root);

			Json::Reader newReader;
			string newJson = substring(html, "<script id=\"getTimelineService\">try { window.getTimelineService = ", "}catch(e){}");
			newReader.parse(newJson, newsRoot);

			Json::Reader abroadReader;
			string abroadJson = substring(html, "<script id=\"getListByCountryTypeService2\">try { window.getListByCountryTypeService2 = ", "}catch(e){}");
			abroadReader.parse(abroadJson, abroadRoot);

			return true;
		}

		return false;
	}




public:

	Plague()
	{
		string error;
		for (int i = 0; i < 5; i++)
		{
			if (getData(error))
				break;

			if (i == 4)
			{
				//出错
				throw exception(error.c_str());
			}
			Sleep(200);
		}
	}

	//获取疫情地图
	string getMap()
	{
		//http://www.xiaoxiaoge.cn/PlagueMap.json
		httplib::Client cli("www.xiaoxiaoge.cn", 80);

		auto res = cli.Get("/PlagueMap.json");

		if (!res || res->status != 200)
		{
			throw exception("获取地图数据失败 网络异常");
		}
		else
		{
			string json = UTF8ToANSI(res->body);

			Json::Value root;
			Json::Reader reader;
			reader.parse(json, root);

			string host = root["host"].asString();
			string path = root["path"].asString();

			return downImg(host, path);
		}

		return "";
	}

	//获取疫情趋势图
	string getTrendMap()
	{
		string json = substring(html, "<script id=\"getStatisticsService\">try { window.getStatisticsService = ", "}catch(e){}");

		Json::Value root;
		Json::Reader reader;
		reader.parse(json, root);

		string url = root["dailyPic"].asString();

		string host = substring(html, "https://", "/");
		string path(url.substr(url.find(".com") + strlen(".com")));

		return downImg(host, path);
	}

	//获取新闻
	string getNews()
	{
		string str;
		int id = 0;

		for (auto temp : newsRoot)
		{
			if (id < temp["id"].asInt())
			{
				id = temp["id"].asInt();
				str = temp["title"].asString() + "(" + temp["pubDateStr"].asString() + ")\n";
				str += temp["summary"].asString() + "\n" + temp["provinceName"].asString() + "（" + temp["infoSource"].asString() + ")\n";

				str += "来自:" + temp["sourceUrl"].asString();
			}
		}

		return str;
	}

	//获取省份新闻
	string getNewsProvince(string name)
	{
		string str;
		int id = 0;

		for (auto temp : newsRoot)
		{
			if (temp["provinceName"].asString().find(name) == string::npos && temp["summary"].asString().find(name) == string::npos)
				continue;

			if (id < temp["id"].asInt())
			{
				id = temp["id"].asInt();
				str = temp["title"].asString() + "(" + temp["pubDateStr"].asString() + ")\n";
				str += temp["summary"].asString() + "\n" + temp["provinceName"].asString() + "（" + temp["infoSource"].asString() + ")\n";

				str += "来自:" + temp["sourceUrl"].asString();
			}

		}

		if (str.empty())
			str = "未找到有关最新新闻";

		return str;
	}

	//获取城市疫情
	string getCity(string name)
	{
		stringstream buf;

		for (auto province : root)
		{
			string provinceName = province["provinceName"].asString();

			if (provinceName.find(name) != string::npos)
			{
				int confirmedCount = province["confirmedCount"].asInt();
				int deadCount = province["deadCount"].asInt();
				int curedCount = province["curedCount"].asInt();
				buf << provinceName;
				buf << " 确诊" << confirmedCount;
				buf << " 死亡" << deadCount;
				buf << " 治愈" << curedCount;
				buf << endl << endl;

				for (auto city : province["cities"])
				{
					//AREA cityData(city["cityName"].asString(), city["confirmedCount"].asInt(), city["deadCount"].asInt(), city["curedCount"].asInt());
					buf << city["cityName"].asString();
					buf << " 确诊" << city["confirmedCount"].asInt();
					buf << " 死亡" << city["deadCount"].asInt();
					buf << " 治愈" << city["curedCount"].asInt();
					buf << endl;

				}


				return buf.str();
			}
		}


		for (auto province : root)
		{
			for (auto city : province["cities"])
			{
				if (city["cityName"].asString() == name)
				{
					AREA cityData(name, city["confirmedCount"].asInt(), city["deadCount"].asInt(), city["curedCount"].asInt());
					buf << cityData.name;
					buf << " 确诊" << cityData.confirmedCount;
					buf << " 死亡" << cityData.deadCount;
					buf << " 治愈" << cityData.curedCount;

					return buf.str();
				}
			}
		}

		buf << "未找到相关数据，可能此地区无疫情，请确定省份/城市名称是否正确";
		return buf.str();
	}

	//获取全部省份疫情
	vector<AREA> getProvince()
	{
		vector<AREA> temp;

		for (auto province : root)
		{
			string provinceName = province["provinceName"].asString();
			int confirmedCount = province["confirmedCount"].asInt();
			int deadCount = province["deadCount"].asInt();
			int curedCount = province["curedCount"].asInt();

			temp.push_back(AREA(provinceName, confirmedCount, deadCount, curedCount));
		}

		sort(temp.begin(), temp.end(), AREAComp);

		return temp;
	}

	//获取全部省份疫情
	vector<AREA> getAbroad()
	{
		vector<AREA> temp;

		for (auto province : abroadRoot)
		{
			string provinceName = province["provinceName"].asString();
			int confirmedCount = province["confirmedCount"].asInt();
			int deadCount = province["deadCount"].asInt();
			int curedCount = province["curedCount"].asInt();

			temp.push_back(AREA(provinceName, confirmedCount, deadCount, curedCount));
		}

		sort(temp.begin(), temp.end(), AREAComp);

		return temp;
	}

	////获取疫情重要内容
	//string getCountRemark()
	//{
	//	string json = substring(html, "<script id=\"getStatisticsService\">try { window.getStatisticsService = ", "}catch(e){}");

	//	Json::Value root;
	//	Json::Reader reader;
	//	reader.parse(json, root);

	//	return root["countRemark"].asString();
	//}


private:
	string html;
	Json::Value root;
	Json::Value newsRoot;
	Json::Value abroadRoot;
};


void MsgFun(string msg, std::function<void(string)> send)
{
	try
	{
		if (msg == "菜单" || msg == "指令" || msg == "命令" || msg == "功能")
		{
			stringstream buf;

			buf << "指令如下:" << endl;
			buf << "疫情查询/全国疫情" << endl;
			buf << "国外疫情" << endl;
			buf << "疫情地图" << endl;
			buf << "疫情趋势图" << endl;
			buf << "疫情新闻" << endl;
			buf << "省份(如山东)+新闻" << endl;
			buf << "省份(如山东)+疫情" << endl;
			buf << "城市(如潍坊)+疫情" << endl;

			buf << endl << "(数据来自 丁香园·丁香医生)";
			send(buf.str());
		}
		else if (msg == "疫情查询" || msg == "全国疫情")
		{
			Plague a;
			stringstream buf;
			vector<AREA> all = a.getProvince();
			//buf << a.getCountRemark() << endl << endl;

			for (auto temp : all)
			{
				buf << temp.name;
				buf << " 确诊" << temp.confirmedCount;
				buf << " 死亡" << temp.deadCount;
				buf << " 治愈" << temp.curedCount;

				buf << endl;
			}

			send(buf.str());
		}
		else if (msg == "国外疫情")
		{
			Plague a;
			stringstream buf;
			vector<AREA> all = a.getAbroad();
			//buf << a.getCountRemark() << endl << endl;

			for (auto temp : all)
			{
				buf << temp.name;
				buf << " 确诊" << temp.confirmedCount;
				buf << " 死亡" << temp.deadCount;
				buf << " 治愈" << temp.curedCount;

				buf << endl;
			}

			send(buf.str());
		}
		else if (msg == "疫情地图")
		{
			Plague a;
			stringstream buf;

			buf << "[CQ:image,file=";
			buf << a.getMap();
			buf << "]";

			send(buf.str());
		}
		else if (msg == "疫情趋势图")
		{
			Plague a;
			stringstream buf;

			buf << "[CQ:image,file=";
			buf << a.getTrendMap();
			buf << "]";

			send(buf.str());
		}
		else if (msg == "疫情新闻")
		{
			Plague a;
			stringstream buf;
			buf << a.getNews();

			send(buf.str());

		}
		else if (msg.length() != strlen("新闻") && msg.find("新闻") != msg.npos && msg.find("新闻") + strlen("新闻") == msg.size())
		{
			Plague a;
			stringstream buf;
			string provinceName(msg.substr(0, msg.find("新闻")));

			buf << a.getNewsProvince(provinceName);

			send(buf.str());

		}
		else if (msg.length() != strlen("疫情") && msg.find("疫情") != msg.npos && msg.find("疫情") + strlen("疫情") == msg.size())
		{
			Plague a;
			string cityName(msg.substr(0, msg.find("疫情")));
			auto Data = a.getCity(cityName);

			send(Data);
		}

	}
	catch (exception & e)
	{
		stringstream buf;
		buf << "异常 " << e.what();

		send(buf.str());
	}
	catch (...)
	{
		stringstream buf;
		cout << "其他异常";

		send(buf.str());
	}
}