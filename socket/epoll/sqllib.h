/*
	MySQL cmd
	新增一列: alter table userinfo add column addr varchar(20) not null;
	更新数据: udate userinfo set login="NO" where name='we';

*/

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

class WeSQL
{
public:
	Driver *driver;
	Connection *conn;
	Statement *stmt;
	ResultSet *res;
	PreparedStatement *pstmt;
	
	WeSQL()
	{
		driver = get_driver_instance();
		conn = driver->connect(SQL_ADDRESS, SQL_USER, SQL_PASSWORD); 
		conn->setSchema("bt");
		stmt = conn->createStatement();
	}
	bool Create_table()
	{
		stmt = conn->createStatement();
		stmt->execute("DROP TABLE IF EXISTS userinfo");
		stmt->execute("CREATE TABLE userinfo(id int primary key, name varchar(20), pwd varchar(20), email varchar(50), sex varchar(2), age varchar(2))");
		stmt->execute("alter table userinfo change id id int auto_increment;");
		return true;
	}
	bool ClearAddr(int addr)	// 用户退出时,必须要清理用户本次登录时记录到数据库的addr为-1	
	{
		pstmt = conn->prepareStatement("UPDATE userinfo set addr=(?) where addr=(?)");
		char c_addr[10];
		snprintf(c_addr, 10, "%d", addr);		// int的fd转换为char,保存到数据库
		pstmt->setString(1, SQL_INIT_ADDR);
		pstmt->setString(2, c_addr);
		res = pstmt->executeQuery();
		return true;
	}
	bool UpdateAddr(string name, int addr)	// 更新用户本次登录时打开的fd,便于之后聊天
	{
		pstmt = conn->prepareStatement("UPDATE userinfo set addr=(?) where name=(?)");
		char c_addr[10];
		snprintf(c_addr, 10, "%d", addr);	// int的fd转换为char,保存到数据库
		pstmt->setString(1, c_addr);
		pstmt->setString(2, name);
		res = pstmt->executeQuery();
		return true;
	}
	bool Login(login_info& usr, int addr)	// addr是client此次登录时server打开的fd
	{
		pstmt = conn->prepareStatement("SELECT name, pwd FROM userinfo where name=(?) and pwd=(?)");
		pstmt->setString(1, usr.name);
		pstmt->setString(2, usr.pwd);
		res = pstmt->executeQuery();
		while (res->next())					// 如果存在此name & pwd的用户,则返回true
			if(NULL == res)
				return false;

		UpdateAddr(usr.name, addr);
		return true;
	}
	bool Register(register_info& usr)		// 确认此用户名或邮箱是否已经注册过,然后添加到数据库
	{	
		pstmt = conn->prepareStatement(("SELECT name, email FROM userinfo where name=(?) or email=(?)"));
		pstmt->setString(1, usr.name);
		pstmt->setString(2, usr.email);
		res = pstmt->executeQuery();
		while (res->next())	
			if(NULL != res)
				return false;	// 如果存在此name | email的用户,则返回false

		// 没有被注册过,添加此用户到数据库
		pstmt = conn->prepareStatement("INSERT INTO userinfo(id, name, pwd, email, sex, age, addr) VALUES (?,?,?,?,?,?,?)");
	    pstmt->setInt(1,16);		// BUG: 不录入id会报错,还需要录入id时自动获取自增id值
	    pstmt->setString(2, usr.name);
	    pstmt->setString(3, usr.pwd);
	    pstmt->setString(4, usr.email);
	    pstmt->setString(5, usr.sex);
	    pstmt->setString(6, usr.age);
	    pstmt->setString(7, SQL_INIT_ADDR);
	    pstmt->executeUpdate();

		return true;
	};
	string FindAddrFromName(string name)	// 通过name判断聊天信息发往何处(fd)
	{
		string addr;
		pstmt = conn->prepareStatement("SELECT * FROM userinfo where name=(?)");
		pstmt->setString(1, name);
		res = pstmt->executeQuery();
    	while(res->next())		// 获取结果必须使用while()
			addr = res->getString("addr");
		return addr;
	};
	string FindNameFromAddr(int sockfd)		// 通过fd判断当前发言的人是谁
	{
		string addr;
		string name;
		stringstream tmp; 
		tmp<<sockfd; 
		tmp>>addr;
		pstmt = conn->prepareStatement("SELECT * FROM userinfo where addr=(?)");
		pstmt->setString(1, addr);
		res = pstmt->executeQuery();
    	while(res->next())		// 获取结果必须使用while()
			name = res->getString("name");
		return name;
	};
	~WeSQL(){	cout<<"析构函数执行完毕"<<endl;};
};

WeSQL wesql;

#endif