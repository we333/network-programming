
#ifndef MYSQL_H_INCLUDED
#define MYSQL_H_INCLUDED

#include <iostream>
#include <sstream>
#include <mysql/mysql.h>

#include <mysql_connection.h>  
#include <mysql_driver.h>  
#include <cppconn/driver.h>  
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

using namespace std;
using namespace sql;

#define SQL_ADDRESS			("tcp://localhost:3306")
#define SQL_USER			("root")
#define SQL_PASSWORD		("333333")
#define SQL_INIT_ADDR		("-1")

typedef struct
{
	string name;
	string pwd;
}login_info;

typedef struct
{
	string name;
	string pwd;
	string email;
	string sex;
	string age;
}register_info;

typedef struct
{
	string date;
	string start;
	string end;
}search_info;

typedef struct
{
	string name;
	string date;
	string start;
	string end;
}booking_info;

typedef struct
{
	string name;
	string date;
	string start;
	string end;
	string price;
	string seat;
	string comment;
}carpool_info;

class WeSQL
{
private:
	Driver *driver;
	Connection *conn;
	Statement *stmt;
	ResultSet *res;
	PreparedStatement *pstmt;
public:	
	WeSQL()
	{
		driver = get_driver_instance();
		conn = driver->connect(SQL_ADDRESS, SQL_USER, SQL_PASSWORD); 
		conn->setSchema("carpool");
		stmt = conn->createStatement();
		ClearAllAddr();
	}
	~WeSQL()
	{	
		ClearAllAddr();
	};
/*	bool Create_table()
	{
		stmt = conn->createStatement();
		stmt->execute("DROP TABLE IF EXISTS userinfo");
		stmt->execute("CREATE TABLE userinfo(id int primary key, name varchar(20), pwd varchar(20), email varchar(50), sex varchar(2), age varchar(2))");
		stmt->execute("alter table userinfo change id id int auto_increment;");
		return true;
	}	*/
	bool ClearAddr(int addr)					// init chat_addr as -1 when sign out
	{
		pstmt = conn->prepareStatement("UPDATE userinfo set addr=(?) where addr=(?)");
		char c_addr[10];
		snprintf(c_addr, 10, "%d", addr);		// int_fd to char
		pstmt->setString(1, SQL_INIT_ADDR);
		pstmt->setString(2, c_addr);
		res = pstmt->executeQuery();
		return true;
	}
	bool ClearAllAddr()
	{
		pstmt = conn->prepareStatement("UPDATE userinfo set addr=(?)");
		pstmt->setString(1, SQL_INIT_ADDR);
		res = pstmt->executeQuery();
		return true;
	}
	bool UpdateAddr(string name, int addr)		// update user's chat_addr for chat
	{
		pstmt = conn->prepareStatement("UPDATE userinfo set addr=(?) where name=(?)");
		char c_addr[10];
		snprintf(c_addr, 10, "%d", addr);
		pstmt->setString(1, c_addr);
		pstmt->setString(2, name);
		res = pstmt->executeQuery();
		return true;
	}
	bool Login(login_info& usr, int addr)		// addr = sockfd
	{
		pstmt = conn->prepareStatement("SELECT name, pwd FROM userinfo where name=(?) and pwd=(?)");
		pstmt->setString(1, usr.name);
		pstmt->setString(2, usr.pwd);
		res = pstmt->executeQuery();

		if(!res->next())	// user is not exist
			return false;

		UpdateAddr(usr.name, addr);
		return true;
	}
	bool Register(register_info& usr)
	{	
		pstmt = conn->prepareStatement(("SELECT name, email FROM userinfo where name=(?) or email=(?)"));
		pstmt->setString(1, usr.name);
		pstmt->setString(2, usr.email);
		res = pstmt->executeQuery();
		
		if(res->next())	
			return false;

		pstmt = conn->prepareStatement("INSERT INTO userinfo(name, pwd, email, addr) VALUES (?,?,?,?)");
	//    pstmt->setInt(1,16);		// BUG: insert id into DB
	    pstmt->setString(1, usr.name);
	    pstmt->setString(2, usr.pwd);
	    pstmt->setString(3, usr.email);
	    pstmt->setString(4, SQL_INIT_ADDR);
	    pstmt->executeUpdate();

		return true;
	};
	vector<string> Search(search_info info)
	{
		vector<string> list;
		pstmt = conn->prepareStatement("SELECT name, email, date, start, end, price, seat, comment FROM userinfo where date=(?) and start=(?) and end=(?)");
		pstmt->setString(1, info.date);
		pstmt->setString(2, info.start);
		pstmt->setString(3, info.end);
		res = pstmt->executeQuery();

		while(res->next())
		{
			if(NULL != res)
			{
				list.push_back(res->getString("name"));
				list.push_back(res->getString("email"));
				list.push_back(res->getString("date"));
				list.push_back(res->getString("start"));
				list.push_back(res->getString("end"));
				list.push_back(res->getString("price"));
				list.push_back(res->getString("seat"));
				list.push_back(res->getString("comment"));
			}
		}
		return list;
	};
	bool Booking(booking_info info)
	{
		string seat;
		pstmt = conn->prepareStatement("SELECT seat FROM userinfo where name=(?) and date=(?) and start=(?) and end=(?)");
		pstmt->setString(1, info.name);
		pstmt->setString(2, info.date);
		pstmt->setString(3, info.start);
		pstmt->setString(4, info.end);
		res = pstmt->executeQuery();

		while(res->next())
			if(NULL != res)
				seat = res->getString("seat");

		int i_seat = atoi(seat.c_str());
		if(0 == i_seat)	return false;
		i_seat -= 1;		// donot use i_seat--

		stringstream tmp;
		tmp<<i_seat;
		tmp>>seat;
		pstmt = conn->prepareStatement("UPDATE userinfo set seat=(?) where name=(?) and date=(?) and start=(?) and end=(?)");
		pstmt->setString(1, seat);
		pstmt->setString(2, info.name);
		pstmt->setString(3, info.date);
		pstmt->setString(4, info.start);
		pstmt->setString(5, info.end);
		res = pstmt->executeQuery();

		return true;
	};
	vector<string> Check_booking(string name)
	{
		vector<string> list;
		pstmt = conn->prepareStatement("SELECT date, start, end, price, seat, comment FROM userinfo where name=(?)");
		pstmt->setString(1, name);
		res = pstmt->executeQuery();

		while(res->next())					// 如果存在此拼车信息,则返回true
		{
			if(NULL != res)
			{
				list.push_back(res->getString("date"));
				list.push_back(res->getString("start"));
				list.push_back(res->getString("end"));
				list.push_back(res->getString("price"));
				list.push_back(res->getString("seat"));
				list.push_back(res->getString("comment"));
			}
		}
		return list;
}
	bool Upload(carpool_info info)
	{
		pstmt = conn->prepareStatement("UPDATE userinfo set date=(?), start=(?), end=(?), price=(?), seat=(?), comment=(?) where name=(?)");
		pstmt->setString(1, info.date);
		pstmt->setString(2, info.start);
		pstmt->setString(3, info.end);
		pstmt->setString(4, info.price);
		pstmt->setString(5, info.seat);
		pstmt->setString(6, info.comment);
		pstmt->setString(7, info.name);
		res = pstmt->executeQuery();
		return true;
	};
	string FindAddrFromName(string name)	// detect chat destination by name
	{
		string addr;
		pstmt = conn->prepareStatement("SELECT * FROM userinfo where name=(?)");
		pstmt->setString(1, name);
		res = pstmt->executeQuery();
    	while(res->next())
			addr = res->getString("addr");
		return addr;
	};
	string FindNameFromAddr(int sockfd)		// detect chat source by sockfd
	{
		string addr;
		string name;
		stringstream tmp; 
		tmp<<sockfd; 
		tmp>>addr;		// int trans to string
		pstmt = conn->prepareStatement("SELECT * FROM userinfo where addr=(?)");
		pstmt->setString(1, addr);
		res = pstmt->executeQuery();
		
    	while(res->next())
			name = res->getString("name");
		return name;
	};
};

WeSQL wesql;

#endif